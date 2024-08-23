#include <unordered_map>
#include "benchmark/benchmark.h"
#include "btrblocks.hpp"
#include "common/Exceptions.hpp"
#include "common/Units.hpp"
#include "compression/Datablock.hpp"
#include "scheme/SchemePool.hpp"
#include "storage/MMapVector.hpp"
#include <algorithm>
#include "storage/Relation.hpp"
// ---------------------------------------------------------------------------------------------------

using namespace btrblocks;
using namespace std;

namespace btrbench {
static const std::vector<std::string> integer_datasets{
    "10_Semana.integer",         "12_Venta_uni_hoy.integer", "1_Agencia_ID.integer",
    "2_Canal_ID.integer",        "3_Cliente_ID.integer",     "4_Demanda_uni_equil.integer",
    "6_Dev_uni_proxima.integer", "9_Ruta_SAK.integer"};

static const vector<vector<IntegerSchemeType> > benchmarkedIntegerSchemes{
    {IntegerSchemeType::DICT, IntegerSchemeType::BP},
    {IntegerSchemeType::RLE}};

static void SetupSchemesAndDepth(const vector<IntegerSchemeType>& schemes) {
  BtrBlocksConfig::get().integers.schemes = SchemeSet<IntegerSchemeType>({});
  BtrBlocksConfig::get().integers.schemes.enable(IntegerSchemeType::UNCOMPRESSED);
  BtrBlocksConfig::get().integers.schemes.enable(IntegerSchemeType::ONE_VALUE);
  for (auto& scheme : schemes) {
    BtrBlocksConfig::get().integers.schemes.enable(scheme);
  }

  BtrBlocksConfig::get().integers.max_cascade_depth = 1;

  SchemePool::refresh();
}

static std::unordered_map<std::string, std::vector<BytesArray>> benchmark_chunks{};
static std::unordered_map<std::string, Relation> benchmark_relation{};

static void BtrBlocksBenchmark(benchmark::State& state,
                               const std::string unique_identifier,
                               const function<void()>& setup) {
  setup();

  const auto& compressed_chunks = benchmark_chunks.at(unique_identifier);
  const auto& relation = benchmark_relation.at(unique_identifier);
  if (compressed_chunks.empty()) {
    std::cerr << "Error: compressed_chunks.empty() is true" << std::endl;
    std::exit(1);
  }

  for (auto _ : state) {
    Datablock datablock{relation};
    for (const auto& chunk : compressed_chunks) {
      auto decompressed_chunk = datablock.decompress(chunk);
      asm volatile("" : : "r"(decompressed_chunk.tuple_count) : "memory");
    }
  }
}

void RegisterSingleBenchmarks() {
  //
  // Integer schemes
  //

  for (auto& dataset : integer_datasets) {
    // Run for all individually
    for (auto& schemes : benchmarkedIntegerSchemes) {
      SetupSchemesAndDepth(schemes);
      auto name = "INTEGER_" + ConvertSchemeTypeToString(schemes[0]) + "/" + dataset;

      auto unique_identifier = dataset;
      for (auto scheme : schemes) {
        unique_identifier += " " + ConvertSchemeTypeToString(scheme);
      }

      Relation relation;
      relation.addColumn(BENCHMARK_DATASET() + dataset);
      Datablock datablock(relation);
      auto ranges = relation.getRanges(btrblocks::SplitStrategy::SEQUENTIAL, 9999999);

      vector<BytesArray> compressed_chunks;
      compressed_chunks.resize(ranges.size());

      for (u32 chunk_i = 0; chunk_i < ranges.size(); chunk_i++) {
        auto chunk = relation.getChunk(ranges, chunk_i);
        auto db_meta = datablock.compress(chunk, compressed_chunks[chunk_i]);
      }

      benchmark_chunks.insert(std::make_pair(unique_identifier, std::move(compressed_chunks)));
      benchmark_relation.insert(std::make_pair(unique_identifier, std::move(relation)));

      benchmark::RegisterBenchmark(name.c_str(), BtrBlocksBenchmark, unique_identifier, [&]() { SetupSchemesAndDepth(schemes); })
          ->UseRealTime()
          ->MinTime(10);
    }
  }
}
}  // namespace btrbench
