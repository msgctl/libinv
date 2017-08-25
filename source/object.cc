#include "object.hh"
#include <mutex>
#include <shared_mutex>

namespace inventory {
    std::shared_mutex g_object_rwlock;
}
