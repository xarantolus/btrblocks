// ---------------------------------------------------------------------------
// BtrBlocks
// ---------------------------------------------------------------------------
#include <iostream>
#include "common/SIMD.hpp"
#include "gtest/gtest.h"
#include "scheme/SchemePool.hpp"
// ---------------------------------------------------------------------------
using namespace btrblocks;
// ---------------------------------------------------------------------------
int main(int argc, char* argv[]) {
#ifdef BTR_USE_SIMD
  BTR_IFELSEARM_SVE(
      { std::cout << "\033[0;35mSIMD+SVE ENABLED\033[0m" << std::endl; },
      { std::cout << "\033[0;35mSIMD ENABLED\033[0m" << std::endl; });
#else
  std::cout << "\033[0;31mSIMD DISABLED\033[0m" << std::endl;
#endif
  testing::InitGoogleTest(&argc, argv);
  // -------------------------------------------------------------------------------------
  SchemePool::available_schemes = make_unique<SchemesCollection>();
  return RUN_ALL_TESTS();
}
// ---------------------------------------------------------------------------
