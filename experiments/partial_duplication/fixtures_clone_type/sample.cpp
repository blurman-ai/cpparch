// Fixture for LD.10 clone-type classifier (#072).
//
// Four families of two functions each. Within a family the two bodies share two
// distinctive callee names (kept by selective normalization => rare tokens, df=2)
// so the pair survives candidate filtering; across families no rare tokens are
// shared, so only the four intra-family pairs are reported.
//
// Expected clone-type label per pair (snapshot mode, default flags):
//   processA1 <-> processA2  => EXACT       (byte-for-byte identical)
//   processB1 <-> processB2  => RENAMED     (only local identifiers differ)
//   processC1 <-> processC2  => LITERAL     (only literals differ)
//   processD1 <-> processD2  => STRUCTURAL  (extra statement => stream diverges)
//   processE1 <-> processE2  => MIXED       (identifiers AND literals differ)

// --- Family A: EXACT -------------------------------------------------------
int processA1(int* buffer, int length)
{
  int ok = validateInput(buffer, length);
  int score = computeScore(buffer, length);
  int total = ok + score;
  int scaled = computeScore(total, 2);
  int result = validateInput(buffer, scaled);
  return result + total + score;
}

int processA2(int* buffer, int length)
{
  int ok = validateInput(buffer, length);
  int score = computeScore(buffer, length);
  int total = ok + score;
  int scaled = computeScore(total, 2);
  int result = validateInput(buffer, scaled);
  return result + total + score;
}

// --- Family B: RENAMED (locals renamed, literals + structure intact) -------
int processB1(char* data, int size)
{
  int head = parseHeader(data, size);
  int conf = loadConfig(data, head);
  int sum = head + conf;
  int adj = loadConfig(sum, 4);
  int out = parseHeader(data, adj);
  return out + sum + conf;
}

int processB2(char* data, int size)
{
  int alpha = parseHeader(data, size);
  int beta = loadConfig(data, alpha);
  int gamma = alpha + beta;
  int delta = loadConfig(gamma, 4);
  int omega = parseHeader(data, delta);
  return omega + gamma + beta;
}

// --- Family C: LITERAL (only literals differ, identifiers intact) ----------
float processC1(float* pixels, int count)
{
  float g = applyGamma(pixels, count);
  float c = clampRange(g, 0.5);
  float m = c * 2;
  float n = applyGamma(pixels, 8);
  float r = clampRange(n, 1.5);
  return g + c + m + n + r;
}

float processC2(float* pixels, int count)
{
  float g = applyGamma(pixels, count);
  float c = clampRange(g, 0.9);
  float m = c * 3;
  float n = applyGamma(pixels, 16);
  float r = clampRange(n, 4.5);
  return g + c + m + n + r;
}

// --- Family D: STRUCTURAL (extra statement, normalized stream diverges) -----
int processD1(int* queue, int depth)
{
  int a = retryFetch(queue, depth);
  int b = logEvent(a, depth);
  int c = a + b;
  int d = retryFetch(c, 1);
  int e = logEvent(d, c);
  return a + b + c + d + e;
}

int processD2(int* queue, int depth)
{
  int a = retryFetch(queue, depth);
  int b = logEvent(a, depth);
  int c = a + b;
  int d = retryFetch(c, 1);
  int x = retryFetch(d, depth);
  int e = logEvent(x, c);
  return a + b + c + d + e + x;
}

// --- Family E: MIXED (both identifiers and literals differ, structure intact) -
int processE1(unsigned char* blob, int len)
{
  int key = digest(blob, len);
  int hash = encrypt(blob, key);
  int mix = key ^ 7;
  int more = encrypt(mix, 3);
  int last = digest(blob, more);
  return key + hash + mix + more + last;
}

int processE2(unsigned char* blob, int len)
{
  int seed = digest(blob, len);
  int tag = encrypt(blob, seed);
  int fold = seed ^ 9;
  int extra = encrypt(fold, 5);
  int tail = digest(blob, extra);
  return seed + tag + fold + extra + tail;
}
