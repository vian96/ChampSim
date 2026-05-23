#ifndef REPLACEMENT_LRU_BIP_H
#define REPLACEMENT_LRU_BIP_H

#include <algorithm>
#include <cassert>
#include <random>
#include <vector>

#include "cache.h"
#include "modules.h"

class lru_bip : public champsim::modules::replacement
{
  const long NUM_WAY;
  uint64_t cycle = 1;
  std::vector<uint64_t> last_used_cycles;

  // Random number generation for the 1/32 bimodal probability
  std::mt19937 rng{std::random_device{}()};
  std::bernoulli_distribution is_mru_insertion{1.0 / 32.0};

public:
  lru_bip(CACHE* cache) : lru_bip(cache, cache->NUM_SET, cache->NUM_WAY) {}

  lru_bip(CACHE* cache, long sets, long ways) : replacement(cache), NUM_WAY(ways), last_used_cycles(static_cast<std::size_t>(sets * ways), 0) {}

  long find_victim(uint32_t triggering_cpu, uint64_t instr_id, long set, const champsim::cache_block* current_set, champsim::address ip,
                   champsim::address full_addr, access_type type)
  {
    // Standard LRU victim selection: find the block with the oldest timestamp
    auto begin = std::next(std::begin(last_used_cycles), set * NUM_WAY);
    auto end = std::next(begin, NUM_WAY);

    auto victim = std::min_element(begin, end);
    return std::distance(begin, victim);
  }

  void replacement_cache_fill(uint32_t triggering_cpu, long set, long way, champsim::address full_addr, champsim::address ip, champsim::address victim_addr,
                              access_type type)
  {
    /*
     * BIP Logic:
     * With a high probability (31/32), we perform LIP (insert at LRU position).
     * With a low probability (1/32), we perform standard LRU (insert at MRU position).
     */
    bool mru_flavor = is_mru_insertion(rng);
    std::size_t idx = static_cast<std::size_t>(set * NUM_WAY + way);

    if (mru_flavor) {
      // Insert at MRU position
      last_used_cycles.at(idx) = ++cycle;
    } else {
      // Insert at LRU position (LIP)
      // We set timestamp to 0 so it remains the oldest in the set
      last_used_cycles.at(idx) = 0;
    }
  }

  void update_replacement_state(uint32_t triggering_cpu, long set, long way, champsim::address full_addr, champsim::address ip, champsim::address victim_addr,
                                access_type type, uint8_t hit)
  {
    /*
     * Standard LRU Promotion:
     * Whenever a block is hit, it is promoted to the MRU position.
     * This is crucial for BIP; a line inserted at the LRU position must
     * prove its utility via a hit before it is allowed to stay in the cache.
     */
    if (hit) {
      last_used_cycles.at(static_cast<std::size_t>(set * NUM_WAY + way)) = ++cycle;
    }
  }
};

#endif
