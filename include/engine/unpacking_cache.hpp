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
typedef unsigned char Generation;
typedef unsigned Timestamp;
class UnpackingCache
{
  private:
    boost::compute::detail::lru_cache<std::tuple<NodeID, NodeID, ExcludeIndex, Generation>,
                                      EdgeDuration>
        m_cache;
    boost::shared_mutex m_shared_access;
    Generation m_current_gen = 1;
    Generation m_old_gen = 2;
    Timestamp m_current_time = 0;
    Timestamp m_old_time = 0;
    std::tuple<NodeID, NodeID, ExcludeIndex, Generation> m_edge;

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

    bool IsEdgeInCache(std::tuple<NodeID, NodeID, ExcludeIndex, Timestamp> edge)
    {
        std::get<0>(m_edge) = std::get<0>(edge);
        std::get<1>(m_edge) = std::get<1>(edge);
        std::get<2>(m_edge) = std::get<2>(edge);
        std::get<3>(m_edge) = m_current_gen;

        if (std::get<3>(edge) > m_current_time)
        {
            std::get<3>(m_edge) = m_new_gen;
        }
        boost::shared_lock<boost::shared_mutex> lock(m_shared_access);
        return m_cache.contains(m_edge);
    }

    void AddEdge(std::tuple<NodeID, NodeID, ExcludeIndex, Timestamp> edge, EdgeDuration duration)
    {
        std::get<0>(m_edge) = std::get<0>(edge);
        std::get<1>(m_edge) = std::get<1>(edge);
        std::get<2>(m_edge) = std::get<2>(edge);
        std::get<3>(m_edge) = m_current_gen;

        if (std::get<3>(edge) > m_current_time)
        {
            std::get<3>(m_edge) = m_new_gen;
        }
        boost::unique_lock<boost::shared_mutex> lock(m_shared_access);
        m_cache.insert(m_edge, duration);
    }

    EdgeDuration GetDuration(std::tuple<NodeID, NodeID, ExcludeIndex, Timestamp> edge)
    {
        std::get<0>(m_edge) = std::get<0>(edge);
        std::get<1>(m_edge) = std::get<1>(edge);
        std::get<2>(m_edge) = std::get<2>(edge);
        std::get<3>(m_edge) = m_current_gen;

        if (std::get<3>(edge) > m_current_time)
        {
            std::get<3>(m_edge) = m_new_gen;
        }
        boost::shared_lock<boost::shared_mutex> lock(m_shared_access);
        boost::optional<EdgeDuration> duration = m_cache.get(m_edge);
        return duration ? *duration : MAXIMAL_EDGE_DURATION;
    }
};
} // engine
} // osrm

#endif // UNPACKING_CACHE_HPP