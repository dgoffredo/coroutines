// This code is based on
// <https://gist.github.com/Qix-/09532acd0f6c9a57c09bd9ce31b3023f>.

#include <iostream>
#include <coroutine>

const int DELAY_MS = 1000;

struct EventLoop {
  // TODO
};

struct Sleep {
	int _delay;
	explicit Sleep(int delay) : _delay(delay) {}
};

Sleep sleep(int delay) { return Sleep{delay}; }

class Coroutine {
public:
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

/* TODO void on_sleep_done(uv_timer_t *timer) {
	auto co = std::coroutine_handle<>::from_address(timer->data);
	delete timer;
	co.resume();
}*/

// TODO: This part I understand the least.
struct FinalAwaitable {
	// std::coroutine_handle<> _co;
	std::coroutine_handle<> _co = std::noop_coroutine();

	explicit FinalAwaitable(std::coroutine_handle<> co) : _co(co) {}

	bool await_ready() noexcept { return false; }
  /*
	std::coroutine_handle<> await_suspend(std::coroutine_handle<>) noexcept {
		if (_co) {
			return _co;
		} else {
			return std::noop_coroutine();
		}
	}
  */
	std::coroutine_handle<> await_suspend(std::coroutine_handle<>) noexcept {
    return _co;
  }

	void await_resume() noexcept {}
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
		/* TODO
    uv_timer_t *timer = new uv_timer_t{};
		timer->data = CoroutineHandle::from_promise(*this).address();
    */
    (void)sleep_cmd;
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

	void await_resume() {}
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

	sleepy_main(&loop);
  // TODO

	return 0;
}
