#include "archcheck/scan/local_complexity_metrics.h"

#include <algorithm>
#include <cctype>

#include "archcheck/scan/function_body_scan.h"

namespace archcheck::scan
{
namespace
{

using duplication::Token;

struct BraceFrame
{
  bool control = false;
  bool isDo = false;
};

// State machine port of the #102 prototype scorer (metric_for_span). The quirks
// of the prototype are kept on purpose — its numbers are corpus-validated.
struct ScoreState
{
  int score = 0;
  int increments = 0;
  int nesting = 0;
  int maxNesting = 0;
  int parenDepth = 0;
  std::vector<BraceFrame> braceStack;
  std::vector<std::string> lastOp = {""}; // logical-series tracker per paren depth
  bool pendingControl = false;            // next `{` / braceless stmt opens a control body
  bool pendingIsDo = false;
  bool awaitingHeader = false; // saw a control keyword, waiting for its `(`
  int headerBaseDepth = 0;
  bool inHeader = false;
  bool skipNextIf = false; // `else if`: the if was already counted by `else`
  bool elseArmed = false;  // saw `else`, resolving else vs else-if
  bool doTailPending = false;
  bool swallowHeader = false; // consume the do-while tail `while(...)` silently
  bool lambdaPending = false; // saw `[`, a following `{` is a lambda body
  int bracelessOpen = 0;

  void add(bool structural)
  {
    score += 1 + (structural ? nesting : 0);
    ++increments;
  }

  void enterNesting()
  {
    ++nesting;
    maxNesting = std::max(maxNesting, nesting);
  }
};

bool isStructuralKeyword(const std::string &s)
{
  return s == "if" || s == "for" || s == "while" || s == "switch" || s == "catch";
}

// Keywords that are free or already accounted for (case labels, the do-while
// tail, the `if` of an `else if`). Returns true when the token is consumed.
bool handleFreeKeyword(ScoreState &st, const std::string &sym)
{
  if (sym == "case" || sym == "default")
    return true;
  if (sym == "while" && (st.doTailPending || st.swallowHeader))
  {
    st.doTailPending = false;
    st.swallowHeader = true;
    return true;
  }
  if (st.skipNextIf && sym == "if")
  {
    st.skipNextIf = false;
    st.awaitingHeader = true;
    return true;
  }
  return false;
}

// Keyword dispatch; returns true when the token was a scoring keyword.
bool handleKeyword(ScoreState &st, const std::string &sym)
{
  if (handleFreeKeyword(st, sym))
    return true;
  if (isStructuralKeyword(sym))
  {
    st.add(true);
    st.awaitingHeader = true;
    return true;
  }
  if (sym == "do")
  {
    st.add(true);
    st.pendingControl = true;
    st.pendingIsDo = true;
    return true;
  }
  if (sym == "else")
  {
    st.add(false); // hybrid increment: +1, no nesting bonus
    st.elseArmed = true;
    return true;
  }
  if (sym == "goto" || sym == "co_await")
  {
    st.add(false);
    return true;
  }
  return false;
}

// `Type&&` is glued to the preceding token; logical `&&` is spaced (prototype
// heuristic, validated by review repro D2).
bool isGluedRvalueRef(const Token &tok, const Token &prev)
{
  const std::string text = prev.raw.empty() ? prev.sym : prev.raw;
  if (tok.off != prev.off + static_cast<int>(text.size()))
    return false;
  const char last = text.back();
  return (std::isalnum(static_cast<unsigned char>(last)) != 0) || last == '_' || last == '>';
}

void handleLogicalOp(ScoreState &st, bool isAnd)
{
  const std::string op = isAnd ? "&&" : "||";
  if (st.lastOp[static_cast<std::size_t>(st.parenDepth)] != op)
  {
    st.add(false);
    st.lastOp[static_cast<std::size_t>(st.parenDepth)] = op;
  }
}

void handleOpenBrace(ScoreState &st)
{
  if (st.pendingControl)
  {
    st.braceStack.push_back({true, st.pendingIsDo});
    st.enterNesting();
    st.pendingControl = false;
    st.pendingIsDo = false;
  }
  else if (st.lambdaPending)
  {
    st.braceStack.push_back({true, false});
    st.enterNesting();
    st.lambdaPending = false;
  }
  else
    st.braceStack.push_back({false, false});
}

void handleCloseBrace(ScoreState &st)
{
  if (st.braceStack.empty())
    return;
  const BraceFrame frame = st.braceStack.back();
  st.braceStack.pop_back();
  if (frame.control)
    st.nesting = std::max(0, st.nesting - 1);
  if (frame.isDo)
    st.doTailPending = true;
}

void handleParens(ScoreState &st, bool open)
{
  if (open)
  {
    ++st.parenDepth;
    st.lastOp.resize(static_cast<std::size_t>(st.parenDepth) + 1);
    st.lastOp.back().clear();
    if (st.awaitingHeader)
    {
      st.inHeader = true;
      st.headerBaseDepth = st.parenDepth - 1;
      st.awaitingHeader = false;
    }
    return;
  }
  st.parenDepth = std::max(0, st.parenDepth - 1);
  st.lastOp.resize(static_cast<std::size_t>(st.parenDepth) + 1);
  if (st.inHeader && st.parenDepth == st.headerBaseDepth)
  {
    st.inHeader = false;
    if (st.swallowHeader)
      st.swallowHeader = false; // do-while tail header consumed, no body follows
    else
      st.pendingControl = true;
  }
}

void handlePunct(ScoreState &st, const std::string &sym)
{
  if (sym == "?")
    st.add(true); // ternary: structural increment, opens no nesting frame
  else if (sym == "!")
    st.lastOp[static_cast<std::size_t>(st.parenDepth)].clear(); // breaks a logical series
  else if (sym == "(")
    handleParens(st, true);
  else if (sym == ")")
    handleParens(st, false);
  else if (sym == "[")
    st.lambdaPending = true;
  else if (sym == "=")
    st.lambdaPending = false; // `x[] = {…}` is an initializer, not a lambda
  else if (sym == "{")
    handleOpenBrace(st);
  else if (sym == "}")
    handleCloseBrace(st);
  else if (sym == ";" && st.parenDepth == 0 && st.bracelessOpen > 0)
  {
    st.nesting = std::max(0, st.nesting - st.bracelessOpen);
    st.bracelessOpen = 0;
  }
}

// Resolve `else` vs `else if`, and open a braceless control body at the first
// statement token after a header.
void resolvePendingControl(ScoreState &st, const std::string &sym)
{
  if (st.elseArmed)
  {
    st.elseArmed = false;
    if (sym == "if")
      st.skipNextIf = true;
    else
      st.pendingControl = true;
  }
  if (st.pendingControl && sym != "{")
  {
    st.enterNesting();
    ++st.bracelessOpen;
    st.pendingControl = false;
    st.pendingIsDo = false;
  }
}

void scoreToken(ScoreState &st, const Token &tok, const Token *prev)
{
  resolvePendingControl(st, tok.sym);
  if (tok.sym == "&&" || tok.sym == "||")
  {
    if (prev == nullptr || !isGluedRvalueRef(tok, *prev))
      handleLogicalOp(st, tok.sym == "&&");
    return;
  }
  if (tok.sym == "and" || tok.sym == "or")
  {
    handleLogicalOp(st, tok.sym == "and");
    return;
  }
  if (!handleKeyword(st, tok.sym))
    handlePunct(st, tok.sym);
}

FunctionComplexity scoreSpan(const std::vector<Token> &tokens, const FunctionSpan &span)
{
  ScoreState st;
  int lines = 0;
  int prevLine = -1;
  for (std::size_t k = span.bodyBegin + 1; k < span.bodyEnd; ++k)
  {
    scoreToken(st, tokens[k], k > span.bodyBegin + 1 ? &tokens[k - 1] : nullptr);
    if (tokens[k].line != prevLine)
    {
      ++lines;
      prevLine = tokens[k].line;
    }
  }
  return {span.qualifiedName, span.paramArity, span.startLine, span.endLine, lines,
          st.score,           st.increments,   st.maxNesting};
}

} // namespace

std::vector<Token> stripDirectiveTokens(const std::vector<Token> &tokens)
{
  std::vector<Token> out;
  out.reserve(tokens.size());
  std::size_t i = 0;
  while (i < tokens.size())
  {
    const bool lineStart = i == 0 || tokens[i - 1].line != tokens[i].line;
    if (tokens[i].sym != "#" || !lineStart)
    {
      out.push_back(tokens[i]);
      ++i;
      continue;
    }
    int line = tokens[i].line;
    while (i < tokens.size())
    {
      if (tokens[i].line != line)
      {
        if (tokens[i - 1].sym != "\\")
          break;
        line = tokens[i].line; // backslash continuation: directive spans lines
      }
      ++i;
    }
  }
  return out;
}

std::vector<FunctionComplexity> computeFileComplexity(const std::string &source)
{
  const auto tokens = stripDirectiveTokens(duplication::lex(source));
  std::vector<FunctionComplexity> out;
  for (const FunctionSpan &span : discoverFunctions(tokens))
    out.push_back(scoreSpan(tokens, span));
  return out;
}

} // namespace archcheck::scan
