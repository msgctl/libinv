#ifndef LIBINV_HTTP_CLIENT_EX_HH
#define LIBINV_HTTP_CLIENT_EX_HH
#include <string>
#include "exception.hh"

namespace inventory::RPC::exceptions {
    class HTTPClientExceptionBase : public inventory::exceptions::ExceptionBase {
    public:
        using ExceptionBase::ExceptionBase;
    };

    class HTTPClientException : public HTTPClientExceptionBase {
        static const char *errclass() {
            return "HTTP client exception: ";
        }

    public:
        HTTPClientException(std::string method_name)
        : HTTPClientExceptionBase(errclass() + method_name) {}

        HTTPClientException(const char *method_name)
        : HTTPClientExceptionBase(errclass() + std::string(method_name)) {}

        virtual JSONRPC::ErrorCode ec() const {
            return JSONRPC::ErrorCode::INTERNAL_ERROR;
        }
    };
}

#endif
