#include "rewind/RewindBuffer.h"

namespace cr {

RewindBuffer::RewindBuffer(std::size_t capacityFrames) : m_capacity(capacityFrames) {}

void RewindBuffer::SetObjectCount(std::size_t n) {
  m_objectCount = n;
  m_storage.resize(m_capacity * m_objectCount);
  Clear();
}

void RewindBuffer::Clear() {
  m_head = 0;
  m_size = 0;
}

void RewindBuffer::PushFrame(const std::vector<RewindState>& states) {
  if (m_objectCount == 0 || m_capacity == 0) return;
  if (states.size() != m_objectCount) return;

  const std::size_t writeIndex = m_head % m_capacity;
  for (std::size_t i = 0; i < m_objectCount; i++) {
    m_storage[writeIndex * m_objectCount + i] = states[i];
  }

  m_head = (m_head + 1) % m_capacity;
  if (m_size < m_capacity) m_size++;
}

bool RewindBuffer::PopFrame(std::vector<RewindState>& outStates) {
  if (m_objectCount == 0 || m_size == 0) return false;

  if (outStates.size() != m_objectCount) outStates.resize(m_objectCount);

  // most recent frame is at (head-1)
  const std::size_t readIndex = (m_head + m_capacity - 1) % m_capacity;
  for (std::size_t i = 0; i < m_objectCount; i++) {
    outStates[i] = m_storage[readIndex * m_objectCount + i];
  }

  m_head = readIndex;
  m_size--;
  return true;
}

} // namespace cr
