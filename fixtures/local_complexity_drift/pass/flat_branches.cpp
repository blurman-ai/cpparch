// Flat sequential branching: many decision points, no nesting growth.
int classify(int v)
{
  if (v < 0) return -1;
  if (v == 0) return 0;
  if (v < 10) return 1;
  return 2;
}
