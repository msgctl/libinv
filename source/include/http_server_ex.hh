#ifndef LIBINV_HTTP_SERVER_EX_HH
#define LIBINV_HTTP_SERVER_EX_HH
#include <string>
#include "exception.hh"

namespace inventory::RPC::exceptions {
    class HTTPServerExceptionBase : public inventory::exceptions::ExceptionBase {
    public:
        using ExceptionBase::ExceptionBase;
    };

    class HTTPServerException : public HTTPServerExceptionBase {
        static const char *errclass() {
            return "HTTP server exception: ";
        }

    public:
        HTTPServerException(std::string method_name)
        : HTTPServerExceptionBase(errclass() + method_name) {}

        HTTPServerException(const char *method_name)
        : HTTPServerExceptionBase(errclass() + std::string(method_name)) {}

        virtual JSONRPC::ErrorCode ec() const {
            return JSONRPC::ErrorCode::INTERNAL_ERROR;
        }
    };
}

#endif
