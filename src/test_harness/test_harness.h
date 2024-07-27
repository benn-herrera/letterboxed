// BNG Light Weight Test Harness Header
// This file + test gen automation in src/cmake/lib.cmake constitute the testing system.
// usage: in a cpp file under tests/ directory of a lib project add a source file
// e.g. tests/my_math.cpp
// #include "my_math_api.h"
// #include "test_harness/test_harness.h"
// BNG_TEST(foo_test, { // names can be reused (e.g. name all tests _) 
//    BT_CHECK(my_math_func_a(val) == expected0);
//    BT_CHECK(my_math_func_b(val) == expected1); 
//  });
// BNG_TEST(0foo, { // names can start with number or be simply numeric
//    BT_CHECK(my_math_func_c(val) == expected2);
//  });
// tests are executed in file order.
// entry point for test exe is in test_harness.h. just add tests and you're done.
// cmake system will automatically add target test_my_math to the project.
// each test cpp file constitutes a test module. each module is a separate executable target.
// all of the modules for a given lib constitute a suite.
// each suite has a run target run_[lib_name]_tests. a single RUN_ALL_TESTS target runs all suites.
// you can manually invoke tests with RUN_ALL_TESTS target in IDE or cmake --build . --target RUN_ALL_TESTS
#pragma once

// * do not include anything but system utility headers in this file.
//   * include nothing from any authored directory (e.g. core, platform, etc - violates test independence.)
//   * do not include stdlib C++ headers if you can possibly help it.
//   * this test harness cheap and simple. it's ~150 lines including the cmake. let's keep it that way.
#include <stdio.h>
#include <assert.h>

namespace bng::test {
  class Test {
    using TestFunc = void(Test*);
  public:
    Test(const char* name, TestFunc* tf) 
      : run(tf), name(name) 
    {
      assert(run && name && *name);
      if (!first()) {
        first() = last() = this;
      } else {
        last()->next = this;
        last() = this;
      }
      ++test_count();
    }

    static int run_all(const char* suite_name) {
      assert(first());
      for (const char* sn = suite_name; *sn; ++sn) {
        if (*sn == '/' || *sn == '\\') { suite_name = sn + 1; }
      }

      printf("running suite %s\n", suite_name);
      int run_count = 0, failed_count = 0;
      for (auto t = first(); t; t = t->next) {
        ++run_count;
        t->run(t);
        if (!t->err_count && t->check_count) {
          printf("%s %s passed %d checks.\n", suite_name, t->name, t->check_count);
        }
        else {
          ++failed_count;
          if (t->check_count) {
            fprintf(stderr, "%s %s FAILED %d/%d checks.\n", suite_name, t->name, t->err_count, t->check_count);
          }
          else {
            fprintf(stderr, "%s %s FAILED by having 0 checks.\n", suite_name, t->name);
          }
        }
      }

      assert(run_count == test_count());
      if (!failed_count) {
        printf("%s passed %d tests.\n", suite_name, run_count);
        return 0;
      }
      fprintf(stderr,"%s FAILED %d of %d tests\n", suite_name, failed_count, run_count);
      return 1;
    }

  public:
    void inc_check_count() {
      ++check_count;
    }

    void inc_err_count() {
      ++err_count;
    }

  private:
    Test* next = nullptr;
    TestFunc* run = nullptr;
    const char* name = nullptr;
    int check_count = 0;
    int err_count = 0;

    static Test*& first() { static Test* t = nullptr; return t; }
    static Test*& last() { static Test* t = nullptr; return t; }
    static int& test_count() { static int count = 0; return count; }    
  };

 # define BNG_BEGIN_TEST(N) \
     auto test_##N = bng::test::Test(#N, [](bng::test::Test* _t_)
# define BNG_END_TEST() \
     );

 # define BNG_TEST(N, BODY) \
    BNG_BEGIN_TEST(N) \
    BODY \
    BNG_END_TEST()

 # define BT_CHECK(V) do { \
    _t_->inc_check_count(); \
    if (!(V)) { \
      _t_->inc_err_count(); \
      fprintf(stderr, "%s(%d): CHECK " #V " FAILED!\n", __FILE__, __LINE__); } \
    } while(0)
} // namepsace bng::test

int main(int /*argc*/, const char** argv) {
  return bng::test::Test::run_all((argv && argv[0]) ? argv[0] : "test_suite");
}
