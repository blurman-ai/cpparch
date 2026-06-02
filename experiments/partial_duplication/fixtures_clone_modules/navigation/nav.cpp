// LD.16 fixture — navigation module.
//   navProcess <-> weather/wx.cpp::wxProcess  => cross navigation<->weather
//   navRetry   <-> licensing/lic.cpp::licRetry => cross navigation<->licensing
int navProcess(int* buffer, int length)
{
  int ok = validateInput(buffer, length);
  int score = computeScore(buffer, length);
  int total = ok + score;
  int scaled = computeScore(total, 2);
  int result = validateInput(buffer, scaled);
  return result + total + score;
}

int navRetry(int* queue, int depth)
{
  int a = retryFetch(queue, depth);
  int b = logEvent(a, depth);
  int c = a + b;
  int d = retryFetch(c, 1);
  int e = logEvent(d, c);
  return a + b + c + d + e;
}
