#ifndef BRANCH_GAG_H
#define BRANCH_GAG_H

#include <array>
#include <cstdint>

#include "address.h"
#include "modules.h"
#include "msl/fwcounter.h"

class gag : champsim::modules::branch_predictor
{
  // GAg configuration
  static constexpr std::size_t GHR_BITS = 14;
  static constexpr std::size_t TABLE_SIZE = 1 << GHR_BITS; // 16384 entries
  static constexpr std::size_t COUNTER_BITS = 2;

  // Mask to keep GHR within the bit width
  static constexpr std::uint32_t GHR_MASK = TABLE_SIZE - 1;

  // Global History Register: tracks the direction of the last 14 branches
  std::uint32_t ghr = 0;

  // Pattern History Table: indexed by the GHR
  std::array<champsim::msl::fwcounter<COUNTER_BITS>, TABLE_SIZE> pht;

public:
  using branch_predictor::branch_predictor;

  bool predict_branch(champsim::address ip);
  void last_branch_result(champsim::address ip, champsim::address branch_target, bool taken, uint8_t branch_type);
};

/**
 * Predictor logic for GAg:
 * The GHR provides the history of recent branches, which is used as an index
 * into the PHT. We ignore the Instruction Pointer (IP) in GAg.
 */
bool gag::predict_branch(champsim::address ip)
{
  // In GAg, the index is purely the Global History Register
  auto index = ghr & GHR_MASK;
  auto counter = pht[index];

  // Return true (taken) if counter is in the upper half of its range (2 or 3)
  return counter.value() > (counter.maximum / 2);
}

/**
 * Update logic:
 * 1. Update the 2-bit counter in the PHT using the actual outcome.
 * 2. Shift the actual outcome into the GHR.
 */
void gag::last_branch_result(champsim::address ip, champsim::address branch_target, bool taken, uint8_t branch_type)
{
  // 1. Update the PHT entry that was used for the prediction
  auto index = ghr & GHR_MASK;
  if (taken) {
    pht[index]++;
  } else {
    pht[index]--;
  }

  // 2. Update the Global History Register (Shift and Insert)
  ghr = ((ghr << 1) | (taken ? 1 : 0)) & GHR_MASK;
}

#endif
