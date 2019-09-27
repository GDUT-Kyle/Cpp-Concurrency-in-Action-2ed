#include <memory>
#include <mutex>
#include <utility>

template <typename T>
class ConcurrentList {
 public:
  ConcurrentList() = default;

  ~ConcurrentList() {
    remove_if([](const Node&) { return true; });
  }

  ConcurrentList(const ConcurrentList&) = delete;

  ConcurrentList& operator=(const ConcurrentList&) = delete;

  void push_front(const T& x) {
    std::unique_ptr<Node> newNode(new Node(x));
    std::lock_guard<std::mutex> l(head_.m);
    newNode->next = std::move(head_.next);
    head_.next = std::move(newNode);
  }

  template <typename F>
  void for_each(F f) {
    Node* cur = &head_;
    std::unique_lock<std::mutex> l(head_.m);
    while (Node* const next = cur->next.get()) {
      std::unique_lock<std::mutex> nextLock(next->m);
      l.unlock();  // 锁住了下一节点，因此可以释放上一节点的锁
      f(*next->data);
      cur = next;               // 当前节点指向下一节点
      l = std::move(nextLock);  // 转交下一节点锁的所有权，循环上述过程
    }
  }

  template <typename F>
  std::shared_ptr<T> find_first_if(F f) {
    Node* cur = &head_;
    std::unique_lock<std::mutex> l(head_.m);
    while (Node* const next = cur->next.get()) {
      std::unique_lock<std::mutex> nextLock(next->m);
      l.unlock();
      if (f(*next->data)) {
        return next->data;  // 返回目标值，无需继续查找
      }
      cur = next;
      l = std::move(nextLock);
    }
    return nullptr;
  }

  template <typename F>
  void remove_if(F f) {
    Node* cur = &head_;
    std::unique_lock<std::mutex> l(head_.m);
    while (Node* const next = cur->next.get()) {
      std::unique_lock<std::mutex> nextLock(next->m);
      if (f(*next->data)) {  // 为 true 则移除下一节点
        std::unique_ptr<Node> oldNext = std::move(cur->next);
        cur->next = std::move(next->next);  // 下一节点设为下下节点
        nextLock.unlock();
      } else {  // 否则继续转至下一节点
        l.unlock();
        cur = next;
        l = std::move(nextLock);
      }
    }
  }

 private:
  struct Node {
    std::mutex m;
    std::shared_ptr<T> data;
    std::unique_ptr<Node> next;
    Node() = default;
    Node(const T& x) : data(std::make_shared<T>(x)) {}
  };

  Node head_;
};
