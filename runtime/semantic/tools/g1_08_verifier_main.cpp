#include "g1_08_verifier.hpp"

#include <iostream>

int main() {
    const auto result = canvas::verification::g1_08::verifyReferenceIndexedAndLocality();
    std::cout << "correctness=" << (result.correctness_pass ? "PASS" : "FAIL")
              << " locality=" << (result.locality_pass ? "PASS" : "FAIL")
              << " scales=" << result.scales_checked
              << " delete_reverse_scan=" << (result.delete_reverse_scan_observed ? "OBSERVED" : "NOT_MEASURED") << '\n';
    return result.correctness_pass && result.locality_pass ? 0 : 1;
}
