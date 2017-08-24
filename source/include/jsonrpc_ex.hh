#ifndef LIBINV_JSONRPC_EX_HH
#define LIBINV_JSONRPC_EX_HH
#include <string>
#include "exception.hh"

namespace inventory::JSONRPC::exceptions {
    class ExceptionBase : public inventory::exceptions::ExceptionBase {
    public:
        using inventory::exceptions::ExceptionBase::ExceptionBase;
    };

    class ParseError : public ExceptionBase {
    public:
        ParseError()
        : ExceptionBase("JSONRPC: parse error.") {}

        virtual ErrorCode ec() const {
            return ErrorCode::PARSE_ERROR;
        }
    };

    class InvalidRequest : public ExceptionBase {
        static const char *errclass() {
            return "Invalid JSONRPC request: ";
        }

    public:
        InvalidRequest(const std::string description)
        : ExceptionBase(errclass() + description) {}

        InvalidRequest(const char *description)
        : ExceptionBase(errclass() + std::string(description)) {}

        virtual ErrorCode ec() const {
            return ErrorCode::INVALID_REQUEST;
        }
    };

    class InvalidResponse : public ExceptionBase {
        static const char *errclass() {
            return "Invalid JSONRPC response: ";
        }

    public:
        InvalidResponse(const std::string description)
        : ExceptionBase(errclass() + description) {}

        InvalidResponse(const char *description)
        : ExceptionBase(errclass() + std::string(description)) {}

        virtual ErrorCode ec() const {
            return ErrorCode::INTERNAL_ERROR;
        }
    };
}

#endif
