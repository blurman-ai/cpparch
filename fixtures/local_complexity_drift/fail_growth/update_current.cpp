// Current: the same function grew nested control flow (score 1 -> 10).
int pick(int a)
{
  if (a > 0) {
    if (a > 1) {
      if (a > 2) {
        if (a > 3) { return 4; }
      }
    }
  }
  return 0;
}
