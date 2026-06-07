#pragma once

// Case: Only 4 bool fields (under threshold of 5) → should NOT flag
struct SmallState
{
  bool started;
  bool running;
  bool paused;
  bool completed;
};
