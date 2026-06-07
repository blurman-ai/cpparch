#pragma once

// Case: 5 bool fields with state-like naming → should FLAG (violation)
struct DownloadState
{
  bool started;        // state: download started
  bool running;        // state: currently downloading
  bool paused;         // state: paused
  bool failed;         // state: error occurred
  bool completed;      // state: finished
};

// Case: 6 bools, 4+ state-like → should FLAG
class ConnectionManager
{
public:
  void connect();
  void disconnect();

private:
  bool is_connecting;  // state
  bool is_connected;   // state
  bool is_authenticated;  // state
  bool is_ready;       // state
  int retry_count;
  bool should_reconnect;  // state-like (action to take)
};
