.PHONY: run
run: a.out
	./a.out

a.out: sleepy.cpp
	clang++ --std=c++20 -g -fsanitize=address -fsanitize=undefined -Wall -Wextra -pedantic -Werror -Wno-infinite-recursion sleepy.cpp
