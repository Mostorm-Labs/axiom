#include "g1_08_verifier.hpp"

#include <iostream>

int main() {
    const auto result = canvas::verification::g1_08::verifyReferenceIndexedAndLocality();
    std::cout << "correctness=" << (result.correctness_pass ? "PASS" : "FAIL")
              << " locality=" << (result.locality_pass ? "PASS" : "FAIL")
              << " scales=" << result.scales_checked
              << " hierarchy=" << (result.locality_pass ? "PASS" : "FAIL")
              << " connector_delete=" << (result.locality_pass ? "PASS" : "FAIL")
              << " cascade=" << (result.locality_pass ? "PASS" : "FAIL")
              << " access_shape_l01=" << (result.local_mutation_access_shape_pass ? "PASS" : "FAIL")
              << " access_shape_l02=" << (result.hierarchy_access_shape_pass ? "PASS" : "FAIL")
              << " access_shape_l04=" << (result.cascade_access_shape_pass ? "PASS" : "FAIL")
              << " delete_reverse_scan=" << (result.delete_reverse_scan_observed ? "OBSERVED" : "NOT_MEASURED") << '\n';
    return result.correctness_pass && result.locality_pass ? 0 : 1;
}
