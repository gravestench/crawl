#pragma once

#include <cstdint>
#include <vector>

#include "fixedvector.h"
#include "pcg.h"
#include "rng-type.h"

class CrawlVector;

namespace rng
{
    // Saved streams for one game session. Generator selection and temporary
    // subgenerators remain separate so their existing lifetimes are unchanged.
    class persistent_state
    {
    public:
        CrawlVector to_vector();
        void load(const CrawlVector &states);
        std::vector<uint64_t> draw_counts();

        void seed(uint64_t value);
        void seed(uint64_t value, uint64_t sequence);
        PcgRNG &stream(rng_type type);

    private:
        void seed_from(PcgRNG &master);
        FixedVector<PcgRNG, NUM_RNGS> streams;
    };
}
