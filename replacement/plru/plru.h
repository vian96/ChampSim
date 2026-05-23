#ifndef REPLACEMENT_PSEUDO_LRU_H
#define REPLACEMENT_PSEUDO_LRU_H

#include <algorithm>
#include <cassert>
#include <cmath>
#include <vector>

#include "cache.h"
#include "modules.h"

class pseudo_lru : public champsim::modules::replacement
{
  const long NUM_SET;
  const long NUM_WAY;
  const uint32_t num_levels;

  // We store N-1 bits per set.
  // For standard caches (up to 64-way), a single uint64_t per set is highly efficient.
  std::vector<uint64_t> tree_bits;

  // Helper to calculate log2 for power-of-2 dimensions
  static uint32_t lg2(long n)
  {
    uint32_t l = 0;
    while (n >>= 1)
      ++l;
    return l;
  }

public:
  explicit pseudo_lru(CACHE* cache) : pseudo_lru(cache, cache->NUM_SET, cache->NUM_WAY) {}

  pseudo_lru(CACHE* cache, long sets, long ways)
      : replacement(cache), NUM_SET(sets), NUM_WAY(ways), num_levels(lg2(ways)), tree_bits(static_cast<std::size_t>(sets), 0)
  {
    // Tree-PLRU requires power-of-two associativity
    assert(ways > 0 && (ways & (ways - 1)) == 0);
    // uint64_t can support up to 64-way (63 bits needed)
    assert(ways <= 64);
  }

  long find_victim(uint32_t triggering_cpu, uint64_t instr_id, long set, const champsim::cache_block* current_set, champsim::address ip,
                   champsim::address full_addr, access_type type)
  {
    long node_idx = 0;
    long way = 0;
    uint64_t current_tree = tree_bits.at(static_cast<std::size_t>(set));

    for (uint32_t level = 0; level < num_levels; ++level) {
      // If bit is 0, we go left (way bit stays 0 at this position)
      // If bit is 1, we go right (way bit becomes 1 at this position)
      bool go_right = (current_tree >> node_idx) & 1;

      if (go_right) {
        way |= (1L << (num_levels - 1 - level));
        node_idx = 2 * node_idx + 2; // Right child in heap-indexed tree
      } else {
        node_idx = 2 * node_idx + 1; // Left child in heap-indexed tree
      }
    }

    return way;
  }

  void replacement_cache_fill(uint32_t triggering_cpu, long set, long way, champsim::address full_addr, champsim::address ip, champsim::address victim_addr,
                              access_type type)
  {
    // On a fill, the new block becomes the MRU. Update the tree bits to point away from it.
    update_replacement_state(triggering_cpu, set, way, full_addr, ip, victim_addr, type, false);
  }

  void update_replacement_state(uint32_t triggering_cpu, long set, long way, champsim::address full_addr, champsim::address ip, champsim::address victim_addr,
                                access_type type, uint8_t hit)
  {
    // Optimization: Skip writebacks if preferred by the microarchitecture
    if (hit && type == access_type::WRITE)
      return;

    long node_idx = 0;
    uint64_t& current_tree = tree_bits.at(static_cast<std::size_t>(set));

    for (uint32_t level = 0; level < num_levels; ++level) {
      // Determine if the way being accessed is in the left or right subtree
      bool is_right = (way >> (num_levels - 1 - level)) & 1;

      if (is_right) {
        // Way is on the right, so make the tree bit point LEFT (0)
        current_tree &= ~(1ULL << node_idx);
        node_idx = 2 * node_idx + 2;
      } else {
        // Way is on the left, so make the tree bit point RIGHT (1)
        current_tree |= (1ULL << node_idx);
        node_idx = 2 * node_idx + 1;
      }
    }
  }
};

#endif
