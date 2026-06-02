// LD.16 fixture — licensing module.
//   licParse = EXACT clone of weather/wx.cpp::wxParse     => cross weather<->licensing
//   licRetry = EXACT clone of navigation/nav.cpp::navRetry => cross navigation<->licensing
int licParse(char* data, int size)
{
  int head = parseHeader(data, size);
  int conf = loadConfig(data, head);
  int sum = head + conf;
  int adj = loadConfig(sum, 4);
  int out = parseHeader(data, adj);
  return out + sum + conf;
}

int licRetry(int* queue, int depth)
{
  int a = retryFetch(queue, depth);
  int b = logEvent(a, depth);
  int c = a + b;
  int d = retryFetch(c, 1);
  int e = logEvent(d, c);
  return a + b + c + d + e;
}
