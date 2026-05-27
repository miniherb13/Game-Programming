#include "rewind/RewindBuffer.h"

namespace cr {

RewindBuffer::RewindBuffer(std::size_t capacityFrames) : m_capacity(capacityFrames) {
  m_storage.resize(m_capacity);
  Clear();
}

void RewindBuffer::Clear() {
  m_head = 0;
  m_size = 0;
}

void RewindBuffer::PushFrame(const GameSnapshot& frame) {
  if (m_capacity == 0) return;

  m_storage[m_head] = frame;
  m_head = (m_head + 1) % m_capacity;
  if (m_size < m_capacity) m_size++;
}

bool RewindBuffer::PopFrame(GameSnapshot& outFrame) {
  if (m_size == 0) return false;

  const std::size_t readIndex = (m_head + m_capacity - 1) % m_capacity;
  outFrame = m_storage[readIndex];

  m_head = readIndex;
  m_size--;
  return true;
}

} // namespace cr
