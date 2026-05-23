#ifndef BRANCH_PAP_H
#define BRANCH_PAP_H

#include <array>
#include <cstdint>

#include "address.h"
#include "modules.h"
#include "msl/fwcounter.h"

class pap : champsim::modules::branch_predictor
{
  // --- Design Constants ---
  static constexpr std::size_t BHT_SIZE = 1024;
  static constexpr std::size_t HISTORY_LENGTH = 10;
  static constexpr uint32_t HISTORY_MASK = (1 << HISTORY_LENGTH) - 1;

  static constexpr std::size_t PHT_SIZE = 8192;
  static constexpr std::size_t COUNTER_BITS = 2;

  // --- Structures ---
  // Per-address History Table: Stores local histories for branches
  std::array<uint32_t, BHT_SIZE> bht{};

  // Per-address Pattern History Table: Stores 2-bit saturating counters
  std::array<champsim::msl::fwcounter<COUNTER_BITS>, PHT_SIZE> pht;

  // --- Helpers ---
  [[nodiscard]] static constexpr std::size_t hash_bht(champsim::address ip)
  {
    // Standard fold/prime-modulo is less common for BHTs than simple bit extraction
    // But for ChampSim compatibility and reliability, we use a clean shift/mask
    return (ip.to<std::size_t>() >> 2) % BHT_SIZE;
  }

  [[nodiscard]] static constexpr std::size_t hash_pht(champsim::address ip, uint32_t history)
  {
    // PAp indexing: mix the PC and the local history.
    // We XOR some PC bits with the history to distribute patterns across the PHT.
    auto pc_part = ip.to<std::size_t>() >> 2;
    return (pc_part ^ history) % PHT_SIZE;
  }

public:
  using branch_predictor::branch_predictor;

  /**
   * Predict the branch direction based on local history.
   */
  bool predict_branch(champsim::address ip)
  {
    const auto bht_idx = hash_bht(ip);
    const auto local_history = bht[bht_idx] & HISTORY_MASK;

    const auto pht_idx = hash_pht(ip, local_history);
    const auto counter = pht[pht_idx];

    // Predict taken if the counter is in the upper half of its range
    return counter.value() > (counter.maximum / 2);
  }

  /**
   * Update the predictor state with the actual outcome.
   */
  void last_branch_result(champsim::address ip, [[maybe_unused]] champsim::address branch_target, bool taken, [[maybe_unused]] uint8_t branch_type)
  {
    const auto bht_idx = hash_bht(ip);
    auto& local_history = bht[bht_idx];

    // 1. Update the PHT entry using the history that was used for the prediction
    const auto pht_idx = hash_pht(ip, local_history & HISTORY_MASK);
    if (taken) {
      pht[pht_idx]++;
    } else {
      pht[pht_idx]--;
    }

    // 2. Update the local history in the BHT (Shift in the new outcome)
    local_history = ((local_history << 1) | (taken ? 1 : 0)) & HISTORY_MASK;
  }
};

#endif
