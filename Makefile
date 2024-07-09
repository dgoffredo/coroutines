.PHONY: dietmar
dietmar: kuhl
	./kuhl

kuhl: dietmar.cpp
	clang++ --std=c++20 -g -fsanitize=address -fsanitize=undefined -Wall -Wextra -pedantic -Werror -Wno-infinite-recursion -o $@ $^


.PHONY: run
run: sleepy
	./sleepy

sleepy: sleepy.cpp
	clang++ --std=c++20 -g -fsanitize=address -fsanitize=undefined -Wall -Wextra -pedantic -Werror -Wno-infinite-recursion -o $@ $^
