#pragma once

class Writer
{
  void emit()
  {
    log(flushed_);
  }
  bool flushed_ = false;
};
