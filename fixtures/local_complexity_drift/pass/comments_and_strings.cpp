// Control keywords in comments and literals must not create branch tokens:
// if (a) while (b) for (;;) switch (c) { case 1: }
const char *banner()
{
  /* do { dangerous(); } while (true); */
  return "if (x && y) { switch (z) }";
}
