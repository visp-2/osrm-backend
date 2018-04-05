#ifndef UNPACKING_CACHE_HPP
#define UNPACKING_CACHE_HPP

#include <boost/optional/optional_io.hpp>
#include <boost/thread.hpp>

#include "../../third_party/compute_detail/lru_cache.hpp"
#include "util/typedefs.hpp"

namespace osrm
{
namespace engine
{
typedef unsigned char ExcludeIndex;
typedef unsigned Timestamp;
typedef std::tuple<NodeID, NodeID, ExcludeIndex, Timestamp> Key;
typedef std::size_t HashedKey;

struct HashKey
{
    std::size_t operator()(Key const &key) const noexcept
    {
        std::size_t h1 = std::hash<NodeID>{}(std::get<0>(key));
        std::size_t h2 = std::hash<NodeID>{}(std::get<1>(key));
        std::size_t h3 = std::hash<ExcludeIndex>{}(std::get<2>(key));
        std::size_t h4 = std::hash<Timestamp>{}(std::get<3>(key));

        std::size_t seed = 0;
        boost::hash_combine(seed, h1);
        boost::hash_combine(seed, h2);
        boost::hash_combine(seed, h3);
        boost::hash_combine(seed, h4);

        return seed;
    }
};
class UnpackingCache
{
  private:
    boost::compute::detail::lru_cache<HashedKey, EdgeDuration> m_cache;
    boost::shared_mutex m_shared_access;

  public:
    // TO FIGURE OUT HOW MANY LINES TO INITIALIZE CACHE TO:
    // Assume max cache size is 500mb (see bottom of OP here:
    // https://github.com/Project-OSRM/osrm-backend/issues/4798#issue-288608332)

    // LRU CACHE IMPLEMENTATION HAS THESE TWO STORAGE CONTAINERS
    // map: n * tuple_hash + n * EdgeDuration
    //    = n * std::size_t + n * std::int32_t
    //    = n * 8 bytes + n * 4 bytes
    //    = n * 12 bytes
    // list: n * HashedKey
    //     = n * std::size_t
    //     = n * 8 bytes
    // Total = n * 20 bytes
    // Total cache size: 500 mb = 1000 * 1024 *1024 bytes = 524288000 bytes

    // THREAD LOCAL STORAGE
    // Number of lines we need  = 524288000 / 20 / number of threads = 26214400 / number of threads
    // 16 threads: 26214400 / 16 = 1638400
    // 8 threads: 26214400 / 8 = 3276800
    // 4 threads: 26214400 / 4 = 6553600
    // 2 threads: 26214400 / 2 = 13107200

    // SHARED STORAGE CACHE
    // Number of lines for shared storage cache 1000 mb = 1048576000 / 20 = 52428800
    // Number of lines for shared storage cache 500 mb = 524288000 / 20 = 26214400
    // Number of lines for shared storage cache 250 mb = 262144000 / 20 = 13107200

    UnpackingCache() : m_cache(52428800){};

    UnpackingCache(std::size_t cache_size) : m_cache(cache_size){};

    bool IsEdgeInCache(Key edge)
    {
        HashedKey hashed_edge = HashKey{}(edge);
        boost::shared_lock<boost::shared_mutex> lock(m_shared_access);
        return m_cache.contains(hashed_edge);
    }

    void AddEdge(Key edge, EdgeDuration duration)
    {
        HashedKey hashed_edge = HashKey{}(edge);
        boost::unique_lock<boost::shared_mutex> lock(m_shared_access);
        m_cache.insert(hashed_edge, duration);
    }

    EdgeDuration GetDuration(Key edge)
    {
        HashedKey hashed_edge = HashKey{}(edge);
        boost::shared_lock<boost::shared_mutex> lock(m_shared_access);
        boost::optional<EdgeDuration> duration = m_cache.get(hashed_edge);
        return duration ? *duration : MAXIMAL_EDGE_DURATION;
    }
};
} // engine
} // osrm

#endif // UNPACKING_CACHE_HPP