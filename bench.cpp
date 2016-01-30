#include <algorithm>
#include <fstream>
#include <iostream>

#include <sys/resource.h>

#include "cpptoml.h"
#include "../../builds/smhasher/src/metahash.h"
#include "meta/hashing/probe_map.h"
#include "meta/util/random.h"
#include "meta/util/time.h"
#include "meta/util/progress.h"
#include "meta/logging/logger.h"

// configuration
using elem_type = std::string;
using hash_fun = std::hash<elem_type>;
#define USE_PROBING 0

// fix this if you're on another platform
std::size_t max_rss()
{
    struct rusage ru;
    ::getrusage(RUSAGE_SELF, &ru);

    // Linux: ru_maxrss is in kilobytes
    return ru.ru_maxrss * 1024;
}

template <class T>
T make_elem(uint64_t);

template <>
uint64_t make_elem(uint64_t i)
{
    return i;
}

template <>
std::string make_elem(uint64_t i)
{
    return std::to_string(i);
}

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
    printing::progress prog{"> Querying: ", input.size()};
    uint64_t i = 0;
    for (auto begin = std::begin(input); begin != std::end(input); ++begin)
    {
        prog(i++);
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
            auto miss = make_elem<elem_type>(
                limit + random::bounded_rand(rng, limit));
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
    std::vector<elem_type> input(size);
    std::mt19937 engine(seed);
    std::generate(input.begin(), input.end(), [&]()
                  {
                      return make_elem<elem_type>(
                          random::bounded_rand(engine, limit));
                  });
    std::cout << " done.\n";

// building time
#if USE_PROBING
    using probing = hashing::probing::binary_hybrid<hashing::kv_pair<elem_type,
                                                                     uint64_t>>;
    // using probing = hashing::probing::binary;
    hashing::probe_map<elem_type, uint64_t, probing, hash_fun> table;
    table.max_load_factor(0.85);
    std::ofstream output{"probe-map.tsv", std::ios::app};
#else
    std::unordered_map<elem_type, uint64_t, hash_fun> table;
    std::ofstream output{"unordered-map.tsv", std::ios::app};
#endif
    auto build_time = common::time([&]()
                                   {
                                       fill(table, input);
                                   });

    std::cout << "Total inserts: " << size << "\n";
    std::cout << "Unique inserts: " << table.size() << "\n";
    std::cout << "Elapsed time: " << build_time.count() << "ms\n";
    std::cout << "Max RSS: " << max_rss() << "\n" << std::endl;

    output << size << '\t' << build_time.count() << '\t' << max_rss();

    // query time
    for (auto hp : {1.0, 0.95, 0.75, 0.5, 0.25, 0.05, 0.0})
    {
        std::cout << "Querying (hit prob " << hp << ")..." << std::endl;
        uint64_t counts = 0;
        engine.seed(seed);
        auto query_time
            = common::time([&]()
                           {
                               query(table, input, limit, engine, 1.0);
                           });
        std::cout << "\n";

        auto qps = size / (query_time.count() / 1000.0);

        std::cout << "Total lookups: " << size << "\n";
        std::cout << "Elapsed time: " << query_time.count() << "ms\n";
        std::cout << "Lookups/sec: " << qps << "\n" << std::endl;
        output << '\t' << qps;
    }
    output << std::endl;

    return 0;
}
