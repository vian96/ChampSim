#ifndef REPLACEMENT_LRU_LIP_H
#define REPLACEMENT_LRU_LIP_H

#include <algorithm>
#include <cassert>
#include <vector>

#include "cache.h"
#include "modules.h"

class lru_lip : public champsim::modules::replacement
{
  const long NUM_WAY;
  uint64_t cycle = 1; // Start from 1 to distinguish from initial 0
  std::vector<uint64_t> last_used_cycles;

public:
  lru_lip(CACHE* cache) : lru_lip(cache, cache->NUM_SET, cache->NUM_WAY) {}

  lru_lip(CACHE* cache, long sets, long ways) : replacement(cache), NUM_WAY(ways), last_used_cycles(static_cast<std::size_t>(sets * ways), 0) {}

  long find_victim(uint32_t triggering_cpu, uint64_t instr_id, long set, const champsim::cache_block* current_set, champsim::address ip,
                   champsim::address full_addr, access_type type)
  {
    // Standard LRU logic: find the block with the oldest timestamp (minimum value)
    auto begin = std::next(std::begin(last_used_cycles), set * NUM_WAY);
    auto end = std::next(begin, NUM_WAY);

    auto victim = std::min_element(begin, end);
    return std::distance(begin, victim);
  }

  void replacement_cache_fill(uint32_t triggering_cpu, long set, long way, champsim::address full_addr, champsim::address ip, champsim::address victim_addr,
                              access_type type)
  {
    /*
     * LIP Logic: On a miss, insert at the LRU position.
     *
     * In this timestamp-based implementation, we achieve this by giving the new block
     * the oldest possible timestamp (0), ensuring it remains the next candidate for
     * eviction unless it receives a cache hit.
     */
    last_used_cycles.at(static_cast<std::size_t>(set * NUM_WAY + way)) = 0;
  }

  void update_replacement_state(uint32_t triggering_cpu, long set, long way, champsim::address full_addr, champsim::address ip, champsim::address victim_addr,
                                access_type type, uint8_t hit)
  {
    /*
     * LRU Promotion Logic:
     * Only on a HIT (or a hit to a previously filled line) do we promote it to MRU.
     * Note: ChampSim calls update_replacement_state on hits.
     */
    if (hit) {
      // Promotion to MRU: Update timestamp to current global cycle
      last_used_cycles.at(static_cast<std::size_t>(set * NUM_WAY + way)) = ++cycle;
    }
  }
};

#endif
