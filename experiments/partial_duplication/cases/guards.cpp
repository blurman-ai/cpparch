// CASE E — short guards below min_tokens. Even though clamp() and clampPos()
// are near-identical, both are far under the function-scale floor and must not
// become fragments at all (min_tokens protects against trivial-snippet noise).
namespace guards
{

int clamp(int v, int lo, int hi)
{
  if (v < lo) { return lo; }
  if (v > hi) { return hi; }
  return v;
}

int clampPos(int v, int hi)
{
  if (v < 0) { return 0; }
  if (v > hi) { return hi; }
  return v;
}

}  // namespace guards
