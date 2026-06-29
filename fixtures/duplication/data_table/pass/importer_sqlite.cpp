// Pass fixture: corresponding importer with real logic (should still be reported).
// This has enough complexity to not be suppressed as whole-file clone.

#include <string>
#include <vector>
#include <map>

class SQLiteImporter
{
public:
  bool importData(const std::string &source)
  {
    std::vector<std::string> rows = parseRows(source);
    if (rows.empty())
    {
      return false;
    }

    for (const auto &row : rows)
    {
      if (!processRow(row))
      {
        logError("Failed to process row: " + row);
        return false;
      }
    }

    commitChanges();
    return true;
  }

private:
  std::vector<std::string> parseRows(const std::string &source)
  {
    std::vector<std::string> result;
    size_t pos = 0;
    while (pos < source.size())
    {
      size_t next = source.find('\n', pos);
      if (next == std::string::npos)
      {
        next = source.size();
      }
      result.push_back(source.substr(pos, next - pos));
      pos = next + 1;
    }
    return result;
  }

  bool processRow(const std::string &row)
  {
    if (row.empty())
    {
      return true;
    }
    // SQLite uses different escaping rules
    std::string escaped = row;
    size_t pos = 0;
    while ((pos = escaped.find('\'', pos)) != std::string::npos)
    {
      escaped.replace(pos, 1, "''");
      pos += 2;
    }
    executeQuery("INSERT INTO table VALUES ('" + escaped + "')");
    return true;
  }

  void commitChanges() { executeQuery("COMMIT"); }
  void logError(const std::string &msg) { /* log */ }
  void executeQuery(const std::string &sql) { /* execute */ }
};
