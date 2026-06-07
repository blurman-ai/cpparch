#pragma once

// Case: Matches pattern but name in exclude list → should NOT flag
struct ChordOptions
{
  bool dim;
  bool min;
  bool maj;
  bool sus;
  bool extended;
};

// Case: *Config pattern → should NOT flag
class AppOptions
{
  bool enable_animations;
  bool use_gpu;
  bool allow_network;
  bool verbose;
  bool debug;
};
