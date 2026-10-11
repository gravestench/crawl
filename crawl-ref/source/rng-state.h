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

    // Chooses a stream within one RNG owner. Scoped generator adapters still
    // use the process-wide instance until game sessions own this object.
    class selection_state
    {
    public:
        explicit selection_state(persistent_state &state);

        rng_type selected_type() const;
        rng_type select(rng_type type);
        PcgRNG *temporary() const;
        PcgRNG *set_temporary(PcgRNG *stream);
        PcgRNG &current();

    private:
        persistent_state &state;
        rng_type selected;
        PcgRNG *temporary_stream;
    };
}
