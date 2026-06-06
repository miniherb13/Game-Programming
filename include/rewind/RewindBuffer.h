#pragma once

#include "rewind/Snapshot.h"

#include <cstddef>
#include <vector>

namespace cr {

class RewindBuffer {
public:
  explicit RewindBuffer(std::size_t capacityFrames);

  void PushFrame(const GameSnapshot& frame);
  bool PopFrame(GameSnapshot& outFrame);

  void Clear();
  std::size_t Capacity() const { return m_capacity; }
  std::size_t Size() const { return m_size; }

private:
  std::size_t m_capacity = 0;
  std::size_t m_head = 0;
  std::size_t m_size = 0;
  std::vector<GameSnapshot> m_storage;
};

} // namespace cr
