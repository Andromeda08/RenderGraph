#pragma once

#define PASS_STR(b) b ? "\x1b[0;32mpassed\x1b[0m" : "\x1b[0;31mfailed\x1b[0m"
#define LOG_TEST(x) std::println("\x1b[1;97m{}\x1b[0m > {}", __builtin_FUNCTION(), PASS_STR(x));
#define RET_TEST(x) return x ? EXIT_SUCCESS : EXIT_FAILURE;

#define LOG_AND_RET_TEST(x) \
LOG_TEST(x) \
RET_TEST(x)

#define RUN_TEST(fn) results.push_back(fn())
