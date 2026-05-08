#pragma once

#include "math/Vec2.h"

#include <cstddef>
#include <vector>

namespace cr {

struct RewindState {
  bool active = true;
  Vec2 pos{};
  Vec2 vel{};
};

class RewindBuffer {
public:
  explicit RewindBuffer(std::size_t capacityFrames);

  void SetObjectCount(std::size_t n);

  void PushFrame(const std::vector<RewindState>& states);
  bool PopFrame(std::vector<RewindState>& outStates);

  void Clear();
  std::size_t Capacity() const { return m_capacity; }
  std::size_t Size() const { return m_size; }

private:
  std::size_t m_capacity = 0;
  std::size_t m_objectCount = 0;

  std::size_t m_head = 0;
  std::size_t m_size = 0;

  std::vector<RewindState> m_storage;
};

} // namespace cr
