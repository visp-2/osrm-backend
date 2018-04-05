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
    // Total cache size: 500 mb = 500 * 1024 *1024 bytes = 524288000 bytes
    // Assume unsigned char is 1 byte (my local machine this is the case):
    // Current cache line = NodeID * 2 + unsigned char * 1 + EdgeDuration * 1
    //                    = std::uint32_t * 2 + unsigned char * 1 + std::int32_t * 1
    //                    = 4 bytes * 3 + 1 byte = 13 bytes
    // Number of cache lines is 500 mb = 500 * 1024 *1024 bytes = 524288000 bytes / 13 = 40329846
    // For threadlocal cache, Number of cache lines = max cache size / number of threads
    //                                              (Assume that the number of threads is 16)
    //                                              = 40329846 / 16 = 2520615

    UnpackingCache() : m_cache(40329846){};

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