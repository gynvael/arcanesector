#pragma once
#include <list>
#include <thread>
#include <mutex>

template<typename T>
class SyncedQueue {
 public:
  bool pop(T *el) {
    std::lock_guard<std::mutex> guard(m);
    if (queue.empty()) {
      return false;
    }

    *el = queue.front();
    queue.pop_front();
    return true;
  }

  bool empty() const {
    std::lock_guard<std::mutex> guard(m);
    return queue.empty();
  }

  size_t size() const {
    std::lock_guard<std::mutex> guard(m);
    return queue.size();
  }

  void push(T el) {
    std::lock_guard<std::mutex> guard(m);
    queue.push_back(el);
  }

 private:
  std::mutex m;
  std::list<T> queue;
};

