#pragma once

// Header-only template component: declaration here, definition in foo_impl.hpp.
// foo.hpp <-> foo_impl.hpp is one component split, not an SF.9 cycle (mlpack/Boost idiom).

template <typename T>
T identity(T value);

// Include implementation.
#include "foo_impl.hpp"
