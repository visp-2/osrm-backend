#ifndef UNPACKING_CACHE_HPP
#define UNPACKING_CACHE_HPP

#include <boost/optional/optional_io.hpp>

#include "../../third_party/compute_detail/lru_cache.hpp"
#include "util/typedefs.hpp"

namespace osrm
{
namespace engine
{
using PathAnnotation = std::pair<EdgeDuration, EdgeDistance>;
class UnpackingCache
{
  private:
    boost::compute::detail::lru_cache<std::tuple<NodeID, NodeID, std::size_t>, PathAnnotation>
        cache;
    unsigned current_data_timestamp = 0;

  public:
    UnpackingCache(unsigned timestamp) : cache(16000000), current_data_timestamp(timestamp){};

    void Clear(unsigned new_data_timestamp)
    {
        if (current_data_timestamp != new_data_timestamp)
        {
            cache.clear();
            current_data_timestamp = new_data_timestamp;
        }
    }

    bool IsEdgeInCache(std::tuple<NodeID, NodeID, std::size_t> edge)
    {
        return cache.contains(edge);
    }

    void AddEdge(std::tuple<NodeID, NodeID, std::size_t> edge, PathAnnotation annotation)
    {
        cache.insert(edge, annotation);
    }

    PathAnnotation GetAnnotation(std::tuple<NodeID, NodeID, std::size_t> edge)
    {
        boost::optional<PathAnnotation> annotation = cache.get(edge);
        return annotation ? *annotation
                          : std::make_pair(MAXIMAL_EDGE_DURATION, MAXIMAL_EDGE_DISTANCE);
    }
};
} // engine
} // osrm

#endif // UNPACKING_CACHE_HPP
