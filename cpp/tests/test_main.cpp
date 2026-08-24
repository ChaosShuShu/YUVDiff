#include "test_framework.hpp"

#include <iostream>

int main() {
    auto& tests = test_framework::get_tests();
    std::cout << "Running " << tests.size() << " tests...\n";

    int passed = 0;
    int failed = 0;

    for (const auto& t : tests) {
        try {
            t.func();
            std::cout << "  [PASS] " << t.name << "\n";
            passed++;
        } catch (const std::exception& e) {
            std::cout << "  [FAIL] " << t.name << ": " << e.what() << "\n";
            failed++;
        }
    }

    std::cout << "\nTest Results: " << passed << " passed, " << failed << " failed, "
              << tests.size() << " total.\n";

    return (failed == 0) ? 0 : 1;
}
