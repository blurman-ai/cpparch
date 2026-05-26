#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>

namespace archcheck::graph
{

struct NodeId
{
   std::uint32_t value;
};

inline bool operator==(NodeId a, NodeId b)
{
   return a.value == b.value;
}

inline bool operator!=(NodeId a, NodeId b)
{
   return a.value != b.value;
}

} // namespace archcheck::graph

namespace std
{

template <>
struct hash<archcheck::graph::NodeId>
{
   std::size_t operator()(archcheck::graph::NodeId id) const noexcept
   {
      return std::hash<std::uint32_t>{}(id.value);
   }
};

} // namespace std
