#ifndef LIBINV_DATAMODEL_EX_HH
#define LIBINV_DATAMODEL_EX_HH
#include <string>
#include "exception.hh"

namespace inventory {
namespace exceptions {
    class NoSuchType : public ExceptionBase {
    public:
        NoSuchType(const std::string type)
        : ExceptionBase("No such type: " + type) {}

        virtual JSONRPC::ErrorCode ec() const {
            return JSONRPC::ErrorCode::INVALID_REQUEST;
        }
    };
}
}

#endif
