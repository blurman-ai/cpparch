// LD.16 fixture — weather module.
//   wxProcess = EXACT clone of navigation/nav.cpp::navProcess => cross navigation<->weather
//   wxParse  <-> licensing/lic.cpp::licParse                 => cross weather<->licensing
int wxProcess(int* buffer, int length)
{
  int ok = validateInput(buffer, length);
  int score = computeScore(buffer, length);
  int total = ok + score;
  int scaled = computeScore(total, 2);
  int result = validateInput(buffer, scaled);
  return result + total + score;
}

int wxParse(char* data, int size)
{
  int head = parseHeader(data, size);
  int conf = loadConfig(data, head);
  int sum = head + conf;
  int adj = loadConfig(sum, 4);
  int out = parseHeader(data, adj);
  return out + sum + conf;
}
