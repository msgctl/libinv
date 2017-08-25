#include "hierarchical.hh"
#include <mutex>
#include <shared_mutex>

namespace inventory {
    std::shared_mutex g_hierarchical_rwlock;
}
