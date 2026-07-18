#pragma once

#include <cstdio>
#include <cstdlib>

inline void testCheck(bool condition, const char* file, int line) {
    if (!condition) {
        std::fprintf(stderr, "テスト失敗: %s:%d\n", file, line);
        std::abort();
    }
}

#define check(condition) testCheck((condition), __FILE__, __LINE__)
