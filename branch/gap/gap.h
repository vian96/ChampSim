#ifndef BRANCH_GAP_H
#define BRANCH_GAP_H

#include <array>
#include <cstdint>

#include "address.h"
#include "modules.h"
#include "msl/fwcounter.h"

class gap : champsim::modules::branch_predictor
{
  // GAp Configuration
  static constexpr std::size_t GHR_WIDTH = 10;                      // 10-bit global history
  static constexpr std::size_t NUM_PHTS = 16;                       // 16 distinct tables (per-address)
  static constexpr std::size_t PHT_SIZE = (1 << GHR_WIDTH);         // 1024 entries per table
  static constexpr std::size_t TOTAL_ENTRIES = NUM_PHTS * PHT_SIZE; // 16,384 entries total

  // 16,384 entries * 2 bits = 32,768 bits (Exactly 32k bits)
  std::array<champsim::msl::fwcounter<2>, TOTAL_ENTRIES> pht;

  std::uint64_t ghr = 0;
  static constexpr std::uint64_t GHR_MASK = PHT_SIZE - 1;

  [[nodiscard]] std::size_t get_index(champsim::address ip) const
  {
    // Select the PHT using the lower bits of the PC (Address)
    // and the entry within that PHT using the GHR.
    auto pht_select = ip.to<std::uint64_t>() % NUM_PHTS;
    auto ghr_select = ghr & GHR_MASK;
    return (pht_select << GHR_WIDTH) | ghr_select;
  }

public:
  using branch_predictor::branch_predictor;

#include "gap.h"

  /**
   * Predicts the branch direction based on the Global History and Branch Address.
   *
   * @param ip The instruction pointer (PC) of the branch.
   * @return True if predicted taken, false otherwise.
   */
  bool predict_branch(champsim::address ip)
  {
    auto index = get_index(ip);
    auto counter = pht[index];

    // Standard 2-bit counter logic: Predict taken if counter is 2 or 3
    return counter.value() >= (counter.maximum / 2 + 1);
  }

  /**
   * Updates the predictor state after the actual branch outcome is known.
   *
   * @param ip The instruction pointer (PC) of the branch.
   * @param branch_target The actual target address (not used in GAp).
   * @param taken The actual outcome of the branch.
   * @param branch_type The type of branch (conditional, call, etc.).
   */
  void last_branch_result(champsim::address ip, [[maybe_unused]] champsim::address branch_target, bool taken, uint8_t branch_type)
  {
    // Only update for conditional branches
    // Note: Some GAp implementations update GHR for all branches,
    // but PHT only for conditional ones.
    auto index = get_index(ip);

    // Update the 2-bit saturating counter in the PHT
    if (taken) {
      pht[index] += 1;
    } else {
      pht[index] -= 1;
    }

    // Update Global History Register (Shift and Mask)
    ghr = (ghr << 1) | (taken ? 1 : 0);
    ghr &= GHR_MASK;
  }
};

#endif
