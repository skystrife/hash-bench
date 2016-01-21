#include <algorithm>
#include <iostream>

#include "cpptoml.h"
#include "meta/hashing/probe_map.h"
#include "meta/util/random.h"
#include "meta/util/time.h"
#include "meta/util/progress.h"
#include "meta/logging/logger.h"

template <class HashTable, class InputContainer>
void fill(HashTable& table, const InputContainer& input)
{
    using namespace meta;

    printing::progress prog{"> Building table: ", input.size()};
    uint64_t i = 0;
    for (auto begin = std::begin(input); begin != std::end(input); ++begin)
    {
        prog(i++);
        ++table[*begin];
    }
}

template <class HashTable, class InputContainer, class RandomEngine>
void query(const HashTable& table, const InputContainer& input, uint64_t limit,
           RandomEngine&& rng, double hit_prob)
{
    using namespace meta;

    uint64_t found = 0;
    uint64_t missed = 0;
    for (auto begin = std::begin(input); begin != std::end(input); ++begin)
    {
        auto rnd = random::bounded_rand(rng, 1000);
        if (rnd / 1000.0 < hit_prob)
        {
            if (table.find(*begin) != table.end())
                ++found;
            else
                throw std::runtime_error{"hash table bug discovered"};
        }
        else
        {
            auto miss = limit + random::bounded_rand(rng, limit);
            if (table.find(miss) != table.end())
                throw std::runtime_error{"hash table bug discovered"};
            else
                ++missed;
        }
    }
    if (missed + found != input.size())
        throw std::runtime_error{"benchmark code bug discovered"};
}

int main(int argc, char** argv)
{
    using namespace meta;

    if (argc < 2)
    {
        std::cout << "Usage: " << argv[0] << " config.toml" << std::endl;
        return 1;
    }

    logging::set_cerr_logging();

    auto cfg = cpptoml::parse_file(argv[1]);
    uint64_t size = *cfg->get_as<int64_t>("input-size");
    uint64_t limit = *cfg->get_as<int64_t>("input-range");
    auto seed = *cfg->get_as<int64_t>("seed");

    std::cout << "Generating input..." << std::flush;
    std::vector<uint64_t> input(size);
    std::mt19937 engine(seed);
    std::generate(input.begin(), input.end(), [&]()
                  {
                      return random::bounded_rand(engine, limit);
                  });
    std::cout << " done.\n";

// building time
#if 1
    hashing::probe_map<uint64_t, uint64_t> table;
#else
    std::unordered_map<uint64_t, uint64_t> table;
#endif
    auto build_time = common::time([&]()
                                   {
                                       fill(table, input);
                                   });

    std::cout << "Total inserts: " << size << "\n";
    std::cout << "Unique inserts: " << table.size() << "\n";
    std::cout << "Elapsed time: " << build_time.count() << "ms\n" << std::endl;

    // query time
    for (auto hp : {1.0, 0.95, 0.75, 0.5, 0.25, 0.05, 0.0})
    {
        std::cout << "Querying (hit prob " << hp << ")..." << std::flush;
        uint64_t counts = 0;
        engine.seed(seed);
        auto query_time
            = common::time([&]()
                           {
                               query(table, input, limit, engine, 1.0);
                           });
        std::cout << " done.\n";

        std::cout << "Total lookups: " << size << "\n";
        std::cout << "Elapsed time: " << query_time.count() << "ms\n";
        std::cout << "Lookups/sec: " << size / (query_time.count() / 1000.0)
                  << "\n" << std::endl;
    }

    return 0;
}
