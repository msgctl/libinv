#ifndef LIBINV_OBJECT_EX_HH
#define LIBINV_OBJECT_EX_HH
#include <string>
#include "exception.hh"

namespace inventory {
namespace exceptions {
    class NoSuchObject : public ExceptionBase {
    public:
        NoSuchObject() {}

        NoSuchObject(const std::string &type, const std::string &id)
        : ExceptionBase("No object of type " + type + " and id " + id + ".") {}

        virtual JSONRPC::ErrorCode ec() const {
            return JSONRPC::ErrorCode::NO_SUCH_OBJECT;
        }
    };
}
}

#endif
