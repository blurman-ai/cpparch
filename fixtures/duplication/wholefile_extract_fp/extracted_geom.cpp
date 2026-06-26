// Synthetic fixture (#151): a freshly EXTRACTED module — three helpers lifted out of a
// large original file (original_geom.cpp) which STILL keeps its copies. This is the
// "extract-to-module, leave the original" TP — a missing reuse edge, NOT a file move.
#include <cmath>

namespace geom
{

double triMinAngleDeg(double ax, double ay, double bx, double by, double cx, double cy)
{
  const double lab = std::hypot(bx - ax, by - ay);
  const double lbc = std::hypot(cx - bx, cy - by);
  const double lca = std::hypot(ax - cx, ay - cy);
  const double ca = (lab * lab + lca * lca - lbc * lbc) / (2.0 * lab * lca);
  const double cb = (lab * lab + lbc * lbc - lca * lca) / (2.0 * lab * lbc);
  const double cc = (lbc * lbc + lca * lca - lab * lab) / (2.0 * lbc * lca);
  const double deg = 57.29577951308232;
  return std::min({std::acos(ca), std::acos(cb), std::acos(cc)}) * deg;
}

double quantileSorted(double *v, int n, double q)
{
  int i = static_cast<int>(q * (n - 1) + 0.5);
  if (i < 0) i = 0;
  if (i >= n) i = n - 1;
  return v[i];
}

double bracketGap(const double *lv, int n, double d)
{
  if (n == 0 || d < lv[0] || d > lv[n - 1]) return -1.0;
  for (int i = 1; i < n; ++i)
  {
    if (d <= lv[i]) return lv[i] - lv[i - 1];
  }
  return -1.0;
}

} // namespace geom
