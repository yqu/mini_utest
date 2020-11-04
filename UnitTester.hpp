/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://www.mozilla.org/en-US/MPL/2.0/.
 * Copyright © 2020 Yann Quelen */


// Unit testing facility
//
// Design goals :
// - easy to write tests
// - easy to read tests
// - exception safe
// - simple minimal code
// - zero macro, no preprocessor
// - no external dependency
// - C++17. possibly supports some earlier versions.
//
// Usage:
// - create a UnitTester instance.
// - use the expect_* methods to write tests. They run immediately.
// - access global results with count_pass(), count_fail() and summary().
// - alternatively the tests can be written more naturally with
//   test("id").expect_XX(test) instead of test.expect_XX("id", test)

#include <iostream>
#include <exception>
#include <functional>

class UnitTester
{
  private:
    std::ostream& out_;
    uintmax_t count_pass_;
    uintmax_t count_fail_;
    uintmax_t count_skip_;
    bool color_output_;
    bool hide_pass_;
    std::function<bool(std::string const&)> filter_;

    static constexpr char c_red[] = "\x1B[31m";
    static constexpr char c_green[] = "\x1B[32m";
    static constexpr char c_reset[] = "\x1B[0m";

    const char *red_() const { return color_output_ ? c_red : ""; }
    const char *green_() const { return color_output_ ? c_green : ""; }
    const char *reset_() const { return color_output_ ? c_reset : ""; }

    // Counts a PASS and print pass message
    void pass_(std::string const& id) {
      ++count_pass_;
      if (!hide_pass_) {
        out_ << green_() << "☑  PASS  " << reset_() << id << '\n';
      }
    }

    // Counts a FAIL and prints fail message
    void fail_(std::string const& id) {
      ++count_fail_;
      out_ << red_() << "☒  FAIL  " << reset_() << id << '\n';
    }

    // RAII thing to save and restore iostream flags in a scope
    class local_iostream_flags {
      private:
        std::ios_base& io_;
        std::ios::fmtflags f_;
      public:
        local_iostream_flags(std::ios_base& io) : io_(io), f_(io_.flags()) { }
        ~local_iostream_flags() { io_.flags(f_); }
    };

  public:
    /** Create a UnitTester instance, with output to the specified std::ostream,
     * by default std::cout. */
    UnitTester(std::ostream& out = std::cout)
    : out_(out), count_pass_(0), count_fail_(0), count_skip_(0),
      color_output_(true), hide_pass_(false), filter_()
    { }

    /** Returns the number of tests passed so far in this UnitTester instance. */
    uintmax_t count_pass() const { return count_pass_; }
    /** Returns the number of tests failed so far in this UnitTester instance. */
    uintmax_t count_fail() const { return count_fail_; }

    /** Outputs a count of passed tests, and failed tests if any test failed. */
    void summary() {
      if (count_skip_ > 0) {
        out_ << count_skip_ << " tests skipped.\n";
      }
      out_ << count_pass_ << " tests passed.\n";
      if (count_fail_ > 0) {
        out_ << count_fail_ << " tests " << red_() << "FAILED !" << reset_() << '\n';
      }
    }

    /** Returns whether color output is enabled. */
    bool color_output() const { return color_output_; }
    /** Enable or disable color output. Enabled by default. */
    UnitTester& color_output(bool c) { color_output_ = c; return *this; }

    UnitTester& hide_pass() { hide_pass_ = true; return *this; }
    UnitTester& show_pass() { hide_pass_ = false; return *this; }

    /** Run tests only if a specified condition is true.
     * The id string of a test is passed to the function.
     * A call to only_if replaces the previous condition.
     * The condition is always true by default. */
    UnitTester& only_if(decltype(filter_) f) {
      filter_ = f;
      return *this;
    }

    /** Remove any condition set by only_if(). */
    UnitTester& always() {
      decltype(filter_) empty_func;
      filter_.swap(empty_func);
      return *this;
    }

    /** Execute a test that is expected to return the boolean value true.
     * A PASS or FAIL indication will be output to the std::ostream associated with
     * this UnitTester
     * @param id A string identifying this test case in the output.
     * @param t A callable that is expected to return true if the test is successful.
     * @return true if the test succeeded.
     * Example:
     *     UnitTester test;
     *
     *     test("1+1 equals 2").expect_true([] {
     *       return 1+1 == 2;
     *     });
     */
    template <typename Test>
    bool expect_true(std::string const& id, Test t) {
      if (filter_ && !filter_(id)) { ++count_skip_; return false; }
      local_iostream_flags f(out_);
      out_ << std::boolalpha;
      return expect_value(id, true, [&t] { return !!t(); });
    }

    /** Execute a test that is expected to return the boolean value false.
     * A PASS or FAIL indication will be output to the std::ostream associated with
     * this UnitTester
     * @param id A string identifying this test case in the output.
     * @param t A callable that is expected to return false if the test is successful.
     * @return true if the test succeeded.
     * Example:
     *     UnitTester test;
     *
     *     test.expect_false("1+1 does not equals 3", [] {
     *       return 1+1 == 3;
     *     });
     */
    template <typename Test>
    bool expect_false(std::string const& id, Test t) {
      if (filter_ && !filter_(id)) { ++count_skip_; return false; }
      local_iostream_flags f(out_);
      out_ << std::boolalpha;
      return expect_value(id, false, [&t] { return !!t(); });
    }

    /** Execute a test that is expected to return a specified value.
     * A PASS or FAIL indication will be output to the std::ostream associated with
     * this UnitTester
     * @param id A string identifying this test case in the output.
     * @param value The value expected to be returned.
     * @param t A callable that is expected to return something equal to 'value' if
     *          the test is successful.
     * @return true if the test succeeded.
     * Example:
     *     UnitTester test;
     *
     *     test("1+1 equals 2").expect_value(2, [] { return 1+1; });
     */
    template <typename Test, typename T>
    bool expect_value(std::string const& id, T const& value, Test t) {
      if (filter_ && !filter_(id)) { ++count_skip_; return false; }
      try {
        auto test_value = t();
        bool result = (test_value == value);
        if (result) {
          pass_(id);
        } else {
          fail_(id);
          out_ << "  expected value " << value << ", found " << test_value << " instead.\n";
        }
        return result;
      } catch(std::exception& e) {
        fail_(id);
        out_ << "  expected value " << value << ", got exception: " << e.what() << '\n';
        return false;
      } catch (...) {
        fail_(id);
        out_ << "  expected value " << value << ", got exception not derived from std::exception\n";
        return false;
      }
    }

    /** Execute a test that is expected to return a value within a specified range.
     * A PASS or FAIL indication will be output to the std::ostream associated with
     * this UnitTester.
     * The type of the value should implement usual comparison operators.
     * Use with caution, since a test with a random result passing this once is not
     * a guarantee that the test will always pass.
     * Can be useful for testing floating point results.
     * @param id A string identifying this test case in the output.
     * @param min The minimum value expected to be returned (included).
     * @param max The maximum value expected to be returned (included).
     * @param t A callable that is expected to return a value within a range if the test is successful.
     * @return true of the test succeeded.
     * Example:
     *     UnitTester test;
     *
     *     test.expect_in_range("rng test", 0, RAND_MAX, [] { return rand(); });
     *     test("one third").expect_in_range(0.333, 0.334, []{ return 1.0 / 3.0; });
     */
    template <typename Test, typename T>
    bool expect_in_range(std::string const& id, T const& min, T const& max, Test t) {
      if (filter_ && !filter_(id)) { ++count_skip_; return false; }
      try {
        auto test_value = t();
        bool result = (min <= test_value && test_value <= max);
        if (result) {
          pass_(id);
        } else {
          fail_(id);
          out_ << "  value " << test_value << " is not in expected range [" << min << ", " << max << "]\n";
        }
        return result;
      } catch(std::exception& e) {
        fail_(id);
        out_ << "  expected a value in [" << min << ", " << max << "], got exception: " << e.what() << '\n';
        return false;
      } catch (...) {
        fail_(id);
        out_ << "  expected a value in [" << min << ", " << max << "], got exception not derived from std::exception\n";
        return false;
      }
    }

    /** Execute a test that is expected to throw an exception of any type.
     * A PASS or FAIL indication will be output to the std::ostream associated with
     * this UnitTester
     * @param id A string identifying this test case in the output.
     * @param t A callable that is expected to throw an exception if the test is successful.
     * @return true of the test succeeded.
     * Example:
     *     UnitTester test;
     *
     *     test.expect_any_exception("exception example", [] {
     *       throw "something";
     *     });
     */
    template <typename Test>
    bool expect_any_exception(std::string const& id, Test t) {
      if (filter_ && !filter_(id)) { ++count_skip_; return false; }
      bool exception_happened = false;
      try {
        t();
      } catch (...) {
        exception_happened = true;
      }
      if (exception_happened) {
        pass_(id);
      } else {
        fail_(id);
        out_ << "  expected exception was not thrown.\n";
      }
      return exception_happened;
    }

    /** Execute a test that is expected to throw an exception of a specified type.
     * The expected exception type is given as a template parameter of this method.
     * A PASS or FAIL indication will be output to the std::ostream associated with
     * this UnitTester
     * @param id A string identifying this test case in the output.
     * @param t A callable that is expected to throw an exception if the test is successful.
     * @return true of the test succeeded.
     * Example:
     *     UnitTester test;
     *
     *     test.expect_exception<std::logic_error>("exception example", [] {
     *       throw std::logic_error("this example is not realistic");
     *     });
     */
    template <typename Except, typename Test>
    bool expect_exception(std::string const& id, Test t) {
      if (filter_ && !filter_(id)) { ++count_skip_; return false; }
      bool exception_happened = false;
      bool other_exception_happened = false;
      try {
        t();
      } catch (Except& e) {
        exception_happened = true;
      } catch (...) {
        other_exception_happened = true;
      }
      if (exception_happened) {
        pass_(id);
      } else if (other_exception_happened) {
        fail_(id);
        out_ << "  an exception happened but not of the correct type.\n";
      } else {
        fail_(id);
        out_ << "  expected exception was not thrown.\n";
      }
      return exception_happened;
    }

    // This class holds an id for a test and is returned by UnitTester::operator(),
    // which enables the notation test("test id").expect_value(42, [] { return 40 + 2; });
    struct UnitTestNamer {
      UnitTester& tester;
      std::string id;
      UnitTestNamer(UnitTester& t, std::string const& i) : tester(t), id(i) { }
      template <typename Test> bool expect_true(Test t) { return tester.expect_true(id, t); }
      template <typename Test> bool expect_false(Test t) { return tester.expect_false(id, t); }
      template <typename Test, typename T> bool expect_value(const T& value, Test t) { return tester.expect_value(id, value, t); }
      template <typename Test, typename T> bool expect_in_range(T const& min, T const& max, Test t) {
        return tester.expect_in_range(id, min, max, t);
      }
      template <typename Test> bool expect_any_exception(Test t) { return tester.expect_any_exception(id, t); }
      template <typename Except, typename Test> bool expect_exception(Test t) { return tester.expect_exception<Except>(id, t); }
    };
    UnitTestNamer operator()(std::string const& id) { return UnitTestNamer(*this, id); }
};
