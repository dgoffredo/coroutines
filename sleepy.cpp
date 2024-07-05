// This code is based on
// <https://gist.github.com/Qix-/09532acd0f6c9a57c09bd9ce31b3023f>.

#include <iostream>
#include <chrono>
#include <coroutine>
#include <map>
#include <thread>
#include <typeinfo>

const int DELAY_MS = 250;

using TimePoint = std::chrono::time_point<std::chrono::steady_clock>;

struct EventLoop {
  std::multimap<TimePoint, void*> events;
  
  void run();
};

void EventLoop::run() {
  std::cout << "Starting event loop.\n";

  while (!events.empty()) {
    auto current = events.begin();
    auto [deadline, data] = *current;
    // TODO: check if `deadline` is in the past. For now this will do.
    std::this_thread::sleep_until(deadline);
    auto co = std::coroutine_handle<>::from_address(data);
		events.erase(current);
		co.resume();
  }

  std::cout << "Stopping event loop.\n";
}

struct Sleep {
  TimePoint deadline;
	explicit Sleep(int ms)
  : deadline(std::chrono::steady_clock::now() + std::chrono::milliseconds(ms)) {}
};

Sleep sleep(int ms) { return Sleep{ms}; }

struct Coroutine {
	struct Awaiter;
	struct Promise;
	using CoroutineHandle = std::coroutine_handle<Promise>;
  using promise_type = Promise; // required by the C++ coroutine protocol

	CoroutineHandle _co;

  // This constructor must be implicit, per the C++ coroutine protocol.
	Coroutine(CoroutineHandle co) : _co(co) {}

	Coroutine(const Coroutine &) = delete;
	Coroutine(Coroutine &&) = delete;

	Awaiter operator co_await();
};

// TODO: This part I understand the least.
struct FinalAwaitable {
	std::coroutine_handle<> _co;
	// std::coroutine_handle<> _co = std::noop_coroutine();

	explicit FinalAwaitable(std::coroutine_handle<> co) : _co(co) {}

	bool await_ready() noexcept { return false; }

	std::coroutine_handle<> await_suspend(std::coroutine_handle<>) noexcept {
		if (_co) {
			return _co;
		} else {
			return std::noop_coroutine();
		}
	}

	/*std::coroutine_handle<> await_suspend(std::coroutine_handle<>) noexcept {
    return _co;
  }*/

	void await_resume() noexcept {
		// TODO?
		if (_co) {
			_co.destroy();
		}
	}
};

struct Coroutine::Promise {
	using CoroutineHandle = std::coroutine_handle<Promise>;

	EventLoop *_loop;
	std::coroutine_handle<> _continuation;

	explicit Promise(EventLoop *loop) : _loop(loop) {}

	auto get_return_object() { return CoroutineHandle::from_promise(*this); }
	std::suspend_never initial_suspend() { return {}; }
	auto final_suspend() noexcept {
		return FinalAwaitable{ _continuation };
	}
	void return_void() {}

	template <typename T>
	auto && await_transform(T &&obj) const noexcept {
		return std::forward<T>(obj);
	}

	auto await_transform(Sleep sleep_cmd) {
		void *data = CoroutineHandle::from_promise(*this).address();
    _loop->events.emplace(sleep_cmd.deadline, data);
		return std::suspend_always{};
	}

	void unhandled_exception() {
		std::terminate();
	}
};

struct Coroutine::Awaiter {
	using CoroutineHandle = std::coroutine_handle<Coroutine::Promise>;

	CoroutineHandle _co;

	explicit Awaiter(CoroutineHandle co) : _co(co) {};
	bool await_ready() {
		return false;
	}

	auto await_suspend(std::coroutine_handle<> co_cont) {
		_co.promise()._continuation = co_cont;
		return true;
	}

	void await_resume() {
		// TODO?
		_co.destroy();
	}
};

Coroutine::Awaiter Coroutine::operator co_await() {
	return Coroutine::Awaiter{_co};
}

Coroutine third(EventLoop *) {
	std::cout << "4" << '\n';
	co_await sleep(DELAY_MS);
	std::cout << "5" << '\n';
	co_await sleep(DELAY_MS);
	std::cout << "6" << '\n';
	co_await sleep(DELAY_MS);
}

Coroutine second(EventLoop *loop) {
	std::cout << "2" << '\n';
	co_await sleep(DELAY_MS);
	std::cout << "3" << '\n';
	co_await sleep(DELAY_MS);
	for (int i = 0; i < 3; i++) {
		co_await third(loop);
	}
	std::cout << "7" << '\n';
	co_await sleep(DELAY_MS);
}

Coroutine sleepy_main(EventLoop *loop) {
	std::cout << "1" << '\n';
	co_await sleep(DELAY_MS);
	co_await second(loop);
	std::cout << "8" << '\n';
	co_await sleep(DELAY_MS);
	std::cout << "9" << '\n';
}

int main() {
	EventLoop loop;

	// TODO: I guess something needs to "drive" `sleepy_main`.
	auto main_coro = sleepy_main(&loop);
	std::cout << typeid(main_coro).name() << '\n';
  
	loop.run();
	main_coro._co.destroy(); // TODO?

	std::cout << "====== done ======\n";
	return 0;
}
