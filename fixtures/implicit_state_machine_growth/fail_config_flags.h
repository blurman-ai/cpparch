#pragma once

// Case: 5+ bool fields but all config-named → should NOT flag (no state pattern match)
struct LoggerConfig
{
  bool enable_console;
  bool enable_file;
  bool enable_syslog;
  bool verbose;
  bool debug;
};

// Case: Mix but config-ratio too high → should NOT flag
struct AudioSettings
{
  bool use_stereo;
  bool allow_surround;
  bool enable_compression;
  bool force_mono;
  bool started;  // only 1 state-like, 4 config → below 60% threshold
};
