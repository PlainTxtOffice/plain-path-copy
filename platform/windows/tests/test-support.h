// A minimal test harness: no third-party framework is needed to build.
#pragma once

#include <cstdio>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace test_support {

struct TestCase {
  const char* name;
  std::function<void()> body;
};

inline std::vector<TestCase>& Registry() {
  static std::vector<TestCase> tests;
  return tests;
}

inline int& FailureCount() {
  static int failures = 0;
  return failures;
}

struct Registrar {
  Registrar(const char* name, std::function<void()> body) {
    Registry().push_back({name, std::move(body)});
  }
};

inline std::string Narrow(std::wstring_view text) {
  std::string result;
  for (wchar_t c : text) {
    if (c == L'\r') {
      result += "\\r";
    } else if (c == L'\n') {
      result += "\\n";
    } else if (c < 0x80) {
      result += static_cast<char>(c);
    } else {
      result += '?';
    }
  }
  return result;
}

inline void ExpectEqual(std::wstring_view actual, std::wstring_view expected,
                        const char* file, int line) {
  if (actual != expected) {
    ++FailureCount();
    std::printf("%s(%d): expected \"%s\" but got \"%s\"\n", file, line,
                Narrow(expected).c_str(), Narrow(actual).c_str());
  }
}

inline void ExpectTrue(bool condition, const char* text, const char* file,
                       int line) {
  if (!condition) {
    ++FailureCount();
    std::printf("%s(%d): expected %s\n", file, line, text);
  }
}

inline int RunAll() {
  for (const TestCase& test : Registry()) {
    const int before = FailureCount();
    test.body();
    std::printf("%s %s\n", FailureCount() == before ? "PASS" : "FAIL",
                test.name);
  }
  std::printf("%zu tests, %d failures\n", Registry().size(), FailureCount());
  return FailureCount() == 0 ? 0 : 1;
}

}  // namespace test_support

#define TEST_CONCAT_INNER(a, b) a##b
#define TEST_CONCAT(a, b) TEST_CONCAT_INNER(a, b)
#define TEST(name)                                                  \
  static void name();                                               \
  static const test_support::Registrar TEST_CONCAT(name, _registrar)( \
      #name, name);                                                 \
  static void name()
#define EXPECT_EQ(actual, expected) \
  test_support::ExpectEqual((actual), (expected), __FILE__, __LINE__)
#define EXPECT_TRUE(condition) \
  test_support::ExpectTrue((condition), #condition, __FILE__, __LINE__)
