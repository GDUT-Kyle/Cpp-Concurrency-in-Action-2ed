#include <algorithm>
#include <functional>
#include <future>
#include <iterator>
#include <numeric>
#include <thread>
#include <utility>
#include <vector>

class threads_guard {
 public:
  explicit threads_guard(std::vector<std::thread>& t) : threads(t) {}
  ~threads_guard() {
    for (auto& x : threads) {
      if (x.joinable()) {
        x.join();
      }
    }
  }

 private:
  std::vector<std::thread>& threads;
};

template <typename Iterator, typename T>
struct accumulate_block {
  T operator()(Iterator first, Iterator last) {
    return std::accumulate(first, last, T{});
  }
};

template <typename Iterator, typename T>
T parallel_accumulate(Iterator first, Iterator last, T init) {
  const unsigned long len = std::distance(first, last);
  if (!len) return init;
  const unsigned long min_per_thread = 25;
  const unsigned long max_threads = (len + min_per_thread - 1) / min_per_thread;
  const unsigned long hardware_threads = std::thread::hardware_concurrency();
  const unsigned long num_threads =
      std::min(hardware_threads != 0 ? hardware_threads : 2, max_threads);
  const unsigned long block_size = len / num_threads;
  std::vector<std::future<T>> fts(num_threads - 1);
  std::vector<std::thread> threads(num_threads - 1);
  threads_guard g(threads);
  Iterator block_start = first;
  for (unsigned long i = 0; i < num_threads - 1; ++i) {
    Iterator block_end = block_start;
    std::advance(block_end, block_size);
    std::packaged_task<T(Iterator, Iterator)> pt(
        accumulate_block<Iterator, T>{});
    fts[i] = pt.get_future();
    threads[i] = std::thread(std::move(pt), block_start, block_end);
    block_start = block_end;
  }
  T last_res = accumulate_block<Iterator, T>{}(block_start, last);
  std::for_each(threads.begin(), threads.end(),
                std::mem_fn(&std::thread::join));
  T res = init;
  for (unsigned long i = 0; i < num_threads - 1; ++i) res += fts[i].get();
  res += last_res;
  return res;
}