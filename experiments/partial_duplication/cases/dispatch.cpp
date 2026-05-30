// CASE D — two large switch statements. They share the switch/case/break/:
// skeleton (a big plain-Jaccard overlap), but dispatch to genuinely different
// work: arithmetic vs method calls. Rarity weighting must pull their weighted
// overlap below threshold so two unrelated dispatch tables are not reported.
#include <cstddef>
#include <string>

namespace dispatch
{

struct Sink;
void emit(Sink& s, const std::string& name);

// Arithmetic opcode evaluator.
int handleOpcode(int op, int a, int b)
{
  int r = 0;
  switch (op)
  {
    case 0: r = a + b; break;
    case 1: r = a - b; break;
    case 2: r = a * b; break;
    case 3: r = a / (b | 1); break;
    case 4: r = a % (b | 1); break;
    case 5: r = a << (b & 31); break;
    case 6: r = a >> (b & 31); break;
    case 7: r = a & b; break;
    case 8: r = a | b; break;
    case 9: r = a ^ b; break;
    case 10: r = ~a; break;
    case 11: r = -a; break;
    default: r = 0; break;
  }
  return r;
}

// Event router — same case skeleton, but it calls methods, never arithmetic.
void routeEvent(Sink& s, int kind)
{
  switch (kind)
  {
    case 0: emit(s, "open"); break;
    case 1: emit(s, "close"); break;
    case 2: emit(s, "read"); break;
    case 3: emit(s, "write"); break;
    case 4: emit(s, "flush"); break;
    case 5: emit(s, "seek"); break;
    case 6: emit(s, "lock"); break;
    case 7: emit(s, "unlock"); break;
    case 8: emit(s, "retry"); break;
    case 9: emit(s, "abort"); break;
    case 10: emit(s, "commit"); break;
    case 11: emit(s, "reset"); break;
    default: emit(s, "noop"); break;
  }
}

}  // namespace dispatch
