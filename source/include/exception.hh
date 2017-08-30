#ifndef LIBINV_EXCEPTION_HH
#define LIBINV_EXCEPTION_HH
#include <stdexcept>
#include <string>

namespace inventory {

namespace JSONRPC {
    enum class ErrorCode : int {
        NO_SUCH_OBJECT = -32000,
        NO_SUCH_FILE = -32001,

        PARSE_ERROR = -32700,
        INVALID_REQUEST = -32600,
        METHOD_NOT_FOUND = -32601,
        INVALID_PARAMS = -32602,
        INTERNAL_ERROR = -32603
    };
}

namespace exceptions {
    class ExceptionBase : public std::runtime_error {
    public:
        ExceptionBase()
        : std::runtime_error("") {}

        ExceptionBase(std::string str)
        : std::runtime_error(str) {}

        ExceptionBase(const char *str)
        : std::runtime_error(str) {}

        virtual JSONRPC::ErrorCode ec() const = 0;
    };

    class InvalidRepr : public ExceptionBase {
    public:
        using ExceptionBase::ExceptionBase;

        virtual JSONRPC::ErrorCode ec() const {
            return JSONRPC::ErrorCode::INTERNAL_ERROR;
        }
    };
}
}

#endif
