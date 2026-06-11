// Preprocessor branches do not raise the branch score by default.
int mode(int x)
{
#if defined(FAST_PATH)
  return x;
#elif defined(SLOW_PATH)
  return -x;
#else
  return 0;
#endif
}
