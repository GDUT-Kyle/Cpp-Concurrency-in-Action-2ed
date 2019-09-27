#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <utility>

class thread_pool {
 public:
  explicit thread_pool(int n) {
    for (int i = 0; i < n; ++i) {
      std::thread{[this] {
        std::unique_lock<std::mutex> l(m);
        while (true) {
          if (!q.empty()) {
            auto task = std::move(q.front());
            q.pop();
            l.unlock();
            task();
            l.lock();
          } else if (done) {
            break;
          } else {
            cv.wait(l);
          }
        }
      }}.detach();
    }
  }

  ~thread_pool() {
    {
      std::lock_guard<std::mutex> l(m);
      done = true;
    }
    cv.notify_all();
  }

  template <typename F>
  void submit(F&& f) {
    {
      std::lock_guard<std::mutex> l(m);
      q.emplace(std::forward<F>(f));
    }
    cv.notify_one();
  }

 private:
  std::mutex m;
  std::condition_variable cv;
  bool done = false;
  std::queue<std::function<void()>> q;
};