#include <uuid/uuid.h>
#include <kcutil.h>
#include "uuid.hh"

namespace inventory {

std::string uuid_string() {
    uuid_t uuid;
    uuid_generate(uuid);

    char cuuid[40];
    uuid_unparse(uuid, cuuid);
    return std::string(cuuid);
}

}
