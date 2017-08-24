#ifndef LIBINV_RPC_EX_HH
#define LIBINV_RPC_EX_HH
#include <string>
#include "exception.hh"
#include "jsonrpc_ex.hh"

namespace inventory::RPC::exceptions {
    class ExceptionBase : public JSONRPC::exceptions::ExceptionBase {
    public:
        using JSONRPC::exceptions::ExceptionBase::ExceptionBase;
    };

    class NoSuchMethod : public ExceptionBase {
        static const char *errclass() {
            return "No such RPC method: ";
        }

    public:
        NoSuchMethod(std::string method_name)
        : ExceptionBase(errclass() + method_name) {}

        NoSuchMethod(const char *method_name)
        : ExceptionBase(errclass() + std::string(method_name)) {}

        virtual JSONRPC::ErrorCode ec() const {
            return JSONRPC::ErrorCode::METHOD_NOT_FOUND;
        }
    };

    class InvalidParameters : public ExceptionBase {
        static const char *errclass() {
            return "Invalid parameters: ";
        }

    public:
        InvalidParameters(std::string errdesc)
        : ExceptionBase(errclass() + errdesc) {}

        InvalidParameters(const char *errdesc)
        : ExceptionBase(errclass() + std::string(errdesc)) {}

        virtual JSONRPC::ErrorCode ec() const {
            return JSONRPC::ErrorCode::INVALID_PARAMS;
        }
    };
}

#endif
