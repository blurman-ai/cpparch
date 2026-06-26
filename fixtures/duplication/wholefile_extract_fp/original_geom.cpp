// Synthetic fixture (#151): the LARGE original that the module was extracted FROM. It still
// holds verbatim copies of the three helpers (now also in extracted_geom.cpp) PLUS a lot of
// other, unrelated code. So the smaller file is a SUBSET of this one (~80% of the small file
// matches, but only a small fraction of THIS file) -> must NOT be treated as a whole-file move.
#include <cmath>
#include <vector>

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

// --- below: unrelated original code, NOT extracted (makes this file much larger) ---

double polygonArea(const std::vector<double> &xs, const std::vector<double> &ys)
{
  double a = 0.0;
  const int n = static_cast<int>(xs.size());
  for (int i = 0, j = n - 1; i < n; j = i++)
  {
    a += (xs[j] + xs[i]) * (ys[j] - ys[i]);
  }
  return std::fabs(a) * 0.5;
}

double perimeter(const std::vector<double> &xs, const std::vector<double> &ys)
{
  double p = 0.0;
  const int n = static_cast<int>(xs.size());
  for (int i = 0, j = n - 1; i < n; j = i++)
  {
    p += std::hypot(xs[i] - xs[j], ys[i] - ys[j]);
  }
  return p;
}

double centroidX(const std::vector<double> &xs)
{
  double s = 0.0;
  for (double x : xs) s += x;
  return xs.empty() ? 0.0 : s / static_cast<double>(xs.size());
}

double boundingDiagonal(const std::vector<double> &xs, const std::vector<double> &ys)
{
  double x0 = xs[0], x1 = xs[0], y0 = ys[0], y1 = ys[0];
  for (double x : xs) { x0 = std::min(x0, x); x1 = std::max(x1, x); }
  for (double y : ys) { y0 = std::min(y0, y); y1 = std::max(y1, y); }
  return std::hypot(x1 - x0, y1 - y0);
}

} // namespace geom
