#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <functional>
#include <stdexcept>
#include <cmath>

namespace test_framework {

struct TestCase {
    std::string name;
    std::function<void()> func;
};

inline std::vector<TestCase>& get_tests() {
    static std::vector<TestCase> tests;
    return tests;
}

struct TestRegistrar {
    TestRegistrar(const std::string& name, std::function<void()> func) {
        get_tests().push_back({name, func});
    }
};

#define TEST_CASE(name) \
    void name(); \
    static test_framework::TestRegistrar registrar_##name(#name, name); \
    void name()

#define ASSERT_TRUE(cond) \
    do { \
        if (!(cond)) { \
            throw std::runtime_error(std::string("Assertion failed: ") + #cond + " at " + __FILE__ + ":" + std::to_string(__LINE__)); \
        } \
    } while (0)

#define ASSERT_FALSE(cond) ASSERT_TRUE(!(cond))

#define ASSERT_EQ(a, b) \
    do { \
        if ((a) != (b)) { \
            throw std::runtime_error(std::string("Assertion failed: ") + #a + " == " + #b + " at " + __FILE__ + ":" + std::to_string(__LINE__)); \
        } \
    } while (0)

#define ASSERT_NEAR(a, b, eps) \
    do { \
        if (std::abs((a) - (b)) > (eps)) { \
            throw std::runtime_error(std::string("Assertion failed: |") + #a + " - " + #b + "| <= " + #eps + \
                                     " (got " + std::to_string(a) + " vs " + std::to_string(b) + ") at " + __FILE__ + ":" + std::to_string(__LINE__)); \
        } \
    } while (0)

#define ASSERT_THROWS(expr, ExceptionType) \
    do { \
        bool caught = false; \
        try { \
            expr; \
        } catch (const ExceptionType&) { \
            caught = true; \
        } catch (...) { \
            throw std::runtime_error(std::string("Wrong exception thrown by: ") + #expr + " at " + __FILE__ + ":" + std::to_string(__LINE__)); \
        } \
        if (!caught) { \
            throw std::runtime_error(std::string("Expected exception ") + #ExceptionType + " for " + #expr + " at " + __FILE__ + ":" + std::to_string(__LINE__)); \
        } \
    } while (0)

} // namespace test_framework
