#include <algorithm>
#include <atomic>
#include <exception>
#include <future>
#include <list>
#include <memory>
#include <mutex>
#include <stack>
#include <thread>
#include <utility>
#include <vector>

struct EmptyStack : std::exception {
  const char* what() const noexcept { return "empty stack!"; }
};

template <typename T>
class ConcurrentStack {
 public:
  ConcurrentStack() : s_(std::stack<T>()) {}

  ConcurrentStack(const ConcurrentStack& rhs) {
    std::lock_guard<std::mutex> l(rhs.m_);
    s_ = rhs.s_;
  }

  ConcurrentStack& operator=(const ConcurrentStack&) = delete;

  void push(T n) {
    std::lock_guard<std::mutex> l(m_);
    s_.push(std::move(n));
  }

  bool empty() const {
    std::lock_guard<std::mutex> l(m_);
    return s_.empty();
  }

  std::shared_ptr<T> pop() {
    std::lock_guard<std::mutex> l(m_);
    if (s_.empty()) {
      throw EmptyStack();
    }
    auto res = std::make_shared<T>(std::move(s_.top()));
    s_.pop();
    return res;
  }

  void pop(T& res) {
    std::lock_guard<std::mutex> l(m_);
    if (s_.empty()) {
      throw EmptyStack();
    }
    res = std::move(s_.top());
    s_.pop();
  }

 private:
  mutable std::mutex m_;
  std::stack<T> s_;
};

template <typename T>
struct sorter {
  struct chunk_to_sort {
    std::list<T> data;
    std::promise<std::list<T>> promise;
  };

  ConcurrentStack<chunk_to_sort> chunks;
  std::vector<std::thread> threads;
  const unsigned max_thread_count;
  std::atomic<bool> end_of_data;

  sorter()
      : max_thread_count(std::thread::hardware_concurrency() - 1),
        end_of_data(false) {}

  ~sorter() {
    end_of_data = true;
    for (auto& x : threads) {
      x.join();
    }
  }

  void try_sort_chunk() {
    std::shared_ptr<chunk_to_sort> chunk = chunks.pop();
    if (chunk) {
      sort_chunk(chunk);
    }
  }

  std::list<T> do_sort(std::list<T>& v) {
    if (v.empty()) {
      return v;
    }
    std::list<T> res;
    res.splice(res.begin(), v, v.begin());
    const T& firstVal = *res.begin();
    auto it = std::partition(v.begin(), v.end(),
                             [&](const T& val) { return val < firstVal; });
    chunk_to_sort low;
    low.data.splice(low.data.end(), v, v.begin(), it);
    std::future<std::list<T>> l = low.promise.get_future();
    chunks.push(std::move(low));
    if (threads.size() < max_thread_count) {
      threads.emplace_back(&sorter<T>::sort_thread, this);
    }
    auto r(do_sort(v));
    res.splice(res.end(), r);
    while (l.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
      try_sort_chunk();
    }
    res.splice(res.begin(), l.get());
    return res;
  }

  void sort_chunk(const std::shared_ptr<chunk_to_sort>& chunk) {
    chunk->promise.set_value(do_sort(chunk->data));
  }

  void sort_thread() {
    while (!end_of_data) {
      try_sort_chunk();
      std::this_thread::yield();
    }
  }
};

template <typename T>
std::list<T> parallel_quick_sort(std::list<T> v) {
  if (v.empty()) {
    return v;
  }
  sorter<T> s;
  return s.do_sort(v);
}