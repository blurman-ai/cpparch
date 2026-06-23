#pragma once

// The bool field exists already; a method body holds a '{' inside a string literal
// (#136). A naive brace counter would miscount depth and miss the field in one version.
class Writer
{
  void emit()
  {
    out_ << "block={ ";
  }
  bool flushed_ = false;
};
