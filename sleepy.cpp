// This code is based on
// <https://gist.github.com/Qix-/09532acd0f6c9a57c09bd9ce31b3023f>
// and on <https://github.com/dietmarkuehl/co_await-all-the-things/blob/main/task-using-std.cpp>.

#include <chrono>
#include <coroutine>
#include <cstddef>
#include <iostream>
#include <memory>
#include <new>
#include <queue>
#include <string>
#include <thread>
#include <utility>

const int DELAY_MS = 100;

using TimePoint = std::chrono::time_point<std::chrono::steady_clock>;

struct EventLoop {
  struct ScheduledEvent {
    TimePoint when;
    void *what;

    ScheduledEvent(TimePoint when, void *what)
    : when(std::move(when)), what(what) {}
  };
  using Compare = decltype([](const ScheduledEvent&left, const ScheduledEvent& right) {
      return left.when > right.when;
  });
  std::priority_queue<ScheduledEvent, std::vector<ScheduledEvent>, Compare> events;

  void schedule(TimePoint deadline, void *data);

  void run();
};

void EventLoop::schedule(TimePoint deadline, void *data) {
  events.emplace(deadline, data);
}

void EventLoop::run() {
  while (!events.empty()) {
    auto [deadline, data] = events.top();
    events.pop();
    std::this_thread::sleep_until(deadline);
    auto co = std::coroutine_handle<>::from_address(data);
    co.resume();
  }
}

struct Sleep {
  TimePoint deadline;
  explicit Sleep(int ms)
      : deadline(std::chrono::steady_clock::now() +
                 std::chrono::milliseconds(ms)) {}
};

Sleep sleep(int ms) { return Sleep{ms}; }

// Rather than having a special case for `void`, just use `Void`.
struct Void {};

template <typename Ret = Void>
struct Coroutine {
  struct Awaiter;
  struct Promise;
  using promise_type = Promise; // required by the C++ coroutine protocol

  using Deleter = decltype([](Promise* p) {
    std::coroutine_handle<Promise>::from_promise(*p).destroy();
  });
  using UniqueHandle= std::unique_ptr<Promise, Deleter>;
  UniqueHandle _promise;

  explicit Coroutine(UniqueHandle promise) : _promise(std::move(promise)) {}

  Coroutine(Coroutine&&) = default;

  Coroutine() = delete;
  Coroutine(const Coroutine&) = delete;

  // See definition below. It has to be after the definition of `Awaiter`.
  Awaiter operator co_await();
};

struct FinalAwaitable {
  std::coroutine_handle<> _co;

  explicit FinalAwaitable(std::coroutine_handle<> co) : _co(co) {}

  bool await_ready() noexcept { return false; }

  std::coroutine_handle<> await_suspend(std::coroutine_handle<>) noexcept {
    return _co;
  }

  void await_resume() noexcept {}
};

template <typename Ret>
struct Coroutine<Ret>::Promise {
  using CoroutineHandle = std::coroutine_handle<Promise>;

  EventLoop *_loop;
  // `_continuation` is what runs after we're done.
  // By default, it's a no-op handle, which means "nothing to do after."
  // Sometimes, `Coroutine::Awaiter::await_suspend` will assign a non-no-op
  // handle to `_continuation`.
  // `_continuation` is passed to `FinalAwaitable` in `final_suspend`.
  // `FinalAwaitable` will then return it in `FinalAwaitable::await_suspend`,
  // which will cause it to be `.resume()`d.
  std::coroutine_handle<> _continuation = std::noop_coroutine();

  alignas(Ret) std::byte _value[sizeof(Ret)];

  explicit Promise(EventLoop *loop) : _loop(loop) {}
  ~Promise() {
    std::launder(reinterpret_cast<Ret*>(&_value[0]))->~Ret();
  }

  Coroutine get_return_object() { return Coroutine{UniqueHandle(this)}; }
  std::suspend_never initial_suspend() { return {}; }
  auto final_suspend() noexcept { return FinalAwaitable{_continuation}; }
  void return_value(Ret value) {
    new (_value) Ret(std::move(value));
  }

  // If we `co_await` a `Sleep`, schedule ourselves as a continuation for when
  // the sleep is done.
  std::suspend_always await_transform(Sleep sleep_cmd) {
    void *data = CoroutineHandle::from_promise(*this).address();
    _loop->schedule(sleep_cmd.deadline, data);
    return {};
  }

  // If we `co_await` a something else, just use the value as-is.
  template <typename T>
  auto &&await_transform(T &&obj) const noexcept {
    return std::forward<T>(obj);
  }

  void unhandled_exception() { std::abort(); }
};

template <typename Ret>
struct Coroutine<Ret>::Awaiter {
  Promise *_promise;

  explicit Awaiter(Promise *promise) : _promise(promise){};
  bool await_ready() { return false; }

  auto await_suspend(std::coroutine_handle<> continuation) {
    _promise->_continuation = continuation;
    return true;
  }

  Ret await_resume() {
    return std::move(*std::launder(reinterpret_cast<Ret*>(&_promise->_value[0])));
  }
};

template <typename Ret>
typename Coroutine<Ret>::Awaiter Coroutine<Ret>::operator co_await() {
  return Coroutine::Awaiter(_promise.get());
}

Coroutine<> sleepy_main(EventLoop *loop);
Coroutine<> second(EventLoop *loop);
Coroutine<> third(EventLoop *loop);
Coroutine<std::string> fourth(EventLoop *loop);

Coroutine<std::string> fourth(EventLoop *loop) {
  std::cout << "        waahhh!\n";
  co_await sleep(DELAY_MS);
  std::cout << "        ooooo!\n";
  co_return "fish sticks";
}

Coroutine<> third(EventLoop *loop) {
  std::cout << "    4" << '\n';
  co_await sleep(DELAY_MS);
  std::cout << "    5" << '\n';
  co_await sleep(DELAY_MS);
  std::cout << "    6" << '\n';
  co_await sleep(DELAY_MS);
  std::cout << "fourth returned: " << co_await fourth(loop) << '\n';
  co_return Void{}; // TODO
}

Coroutine<> second(EventLoop *loop) {
  std::cout << "  2" << '\n';
  co_await sleep(DELAY_MS);
  std::cout << "  3" << '\n';
  co_await sleep(DELAY_MS);
  for (int i = 0; i < 1; i++) {
    co_await third(loop);
  }
  std::cout << "  7" << '\n';
  co_await sleep(DELAY_MS);
  co_return Void{}; // TODO
}

Coroutine<> sleepy_main(EventLoop *loop) {
  static int i = 0;
  if (++i == 2) {
    std::cout << "Goodbye!\n";
    co_return Void{}; // TODO
  }
  std::cout << "1" << '\n';
  co_await sleep(DELAY_MS);
  co_await second(loop);
  std::cout << "8" << '\n';
  co_await sleep(DELAY_MS);
  std::cout << "9" << '\n';
  co_await sleepy_main(loop);
  std::cout << "I finished awaiting myself.\n";
  co_return Void{}; // TODO
}

int main() {
  EventLoop loop;

  auto main_coro = sleepy_main(&loop);

  loop.run();

  std::cout << "====== done ======\n";
  return 0;
}
