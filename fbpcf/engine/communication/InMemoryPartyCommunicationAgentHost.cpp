/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "fbpcf/engine/communication/InMemoryPartyCommunicationAgentHost.h"

namespace fbpcf::engine::communication {

void InMemoryPartyCommunicationAgent::sendImpl(const void* data, int nBytes) {
  std::vector<unsigned char> buffer(nBytes);
  memcpy(buffer.data(), data, nBytes);
  host_.send(myId_, buffer);
  sentData_ += nBytes;
}

void InMemoryPartyCommunicationAgent::send(
    const std::vector<unsigned char>& data) {
  sendImpl(static_cast<const void*>(data.data()), data.size());
}

void InMemoryPartyCommunicationAgent::recvImpl(void* data, int nBytes) {
  auto result = host_.receive(myId_, nBytes);

  if (result.size() != nBytes) {
    throw std::runtime_error("unexpected message size!");
  }
  memcpy(data, result.data(), nBytes);
  receivedData_ += nBytes;
}

std::vector<unsigned char> InMemoryPartyCommunicationAgent::receive(
    size_t size) {
  std::vector<unsigned char> v(size);
  recvImpl(static_cast<void*>(v.data()), size);
  return v;
}

InMemoryPartyCommunicationAgentHost::InMemoryPartyCommunicationAgentHost() {
  agents_[0] = std::make_unique<InMemoryPartyCommunicationAgent>(*this, 0);
  agents_[1] = std::make_unique<InMemoryPartyCommunicationAgent>(*this, 1);
}

std::unique_ptr<InMemoryPartyCommunicationAgent>
InMemoryPartyCommunicationAgentHost::getAgent(int Id) {
  if (agents_[Id]) {
    return std::move(agents_[Id]);
  } else {
    throw std::runtime_error("Already extract the agent!");
  }
}

void InMemoryPartyCommunicationAgentHost::send(
    int myId,
    const std::vector<unsigned char>& data) {
  std::unique_lock<std::mutex> lock(bufferLock_[myId]);
  buffers_[myId].push(std::make_unique<std::vector<unsigned char>>(data));
  bufferEmptyVariable_[myId].notify_one();
}

std::vector<unsigned char> InMemoryPartyCommunicationAgentHost::receive(
    int myId,
    size_t size) {
  std::vector<unsigned char> result;

  while (result.size() < size) {
    std::unique_lock<std::mutex> lock(bufferLock_[1 - myId]);
    while (buffers_[1 - myId].empty()) {
      bufferEmptyVariable_[1 - myId].wait(
          lock, [&]() { return !buffers_[1 - myId].empty(); });
    }
    auto& buf = *buffers_[1 - myId].front();
    if (size - result.size() >= buf.size()) {
      result.insert(result.end(), buf.begin(), buf.end());
      buffers_[1 - myId].pop();
    } else {
      result.insert(
          result.end(), buf.begin(), buf.begin() + size - result.size());
      buf.erase(buf.begin(), buf.begin() + size - result.size());
    }
  }
  return result;
}

} // namespace fbpcf::engine::communication
