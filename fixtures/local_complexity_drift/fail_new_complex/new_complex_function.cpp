// A brand-new function above the absolute threshold 25 (26 decision points).
int saturate(int v)
{
  if (v > 1) v += 1;
  if (v > 2) v += 2;
  if (v > 3) v += 3;
  if (v > 4) v += 4;
  if (v > 5) v += 5;
  if (v > 6) v += 6;
  if (v > 7) v += 7;
  if (v > 8) v += 8;
  if (v > 9) v += 9;
  if (v > 10) v += 1;
  if (v > 11) v += 2;
  if (v > 12) v += 3;
  if (v > 13) v += 4;
  if (v > 14) v += 5;
  if (v > 15) v += 6;
  if (v > 16) v += 7;
  if (v > 17) v += 8;
  if (v > 18) v += 9;
  if (v > 19) v += 1;
  if (v > 20) v += 2;
  if (v > 21) v += 3;
  if (v > 22) v += 4;
  if (v > 23) v += 5;
  if (v > 24) v += 6;
  if (v > 25) v += 7;
  if (v > 26) v += 8;
  return v;
}
