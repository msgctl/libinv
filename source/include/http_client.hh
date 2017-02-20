#ifndef LIBINV_HTTP_CLIENT_HH
#define LIBINV_HTTP_CLIENT_HH
#include <string>
#include <memory>
#include <vector>
#include <functional>
#include "rpc.hh"
#include "jsonrpc.hh"
#include "exception.hh"

namespace inventory::RPC::exceptions {
    class HTTPClientExceptionBase : public ExceptionBase {
    public:
        using RPC::exceptions::ExceptionBase::ExceptionBase;
    };

    class HTTPClientException : public HTTPClientExceptionBase {
        static const char *errclass() {
            return "HTTP client exception: ";
        }

    public:
        HTTPClientException(const std::string &method_name)
        : HTTPClientExceptionBase(errclass() + method_name) {}

        HTTPClientException(const char *method_name)
        : HTTPClientExceptionBase(errclass() + std::string(method_name)) {}

        virtual JSONRPC::ErrorCode ec() const {
            return JSONRPC::ErrorCode::INTERNAL_ERROR;
        }
    };
}

namespace inventory::RPC {

class HTTPClientSession;
class HTTPClient : public Client {
public:
    HTTPClient(std::string url,
               std::shared_ptr<Workqueue<JSONRPC::RequestBase>> workqueue)
    : Client(workqueue), m_url(url) {}
    virtual ~HTTPClient() {}

    virtual std::shared_ptr<ClientSession> create_session();

    std::string url() const {
        return m_url;
    }

protected:
    std::string m_url;
};

class HTTPClientSession : public ClientSession {
public:
    HTTPClientSession(HTTPClient *client)
    : ClientSession(static_cast<HTTPClient *>(client)) {}

    virtual void notify(const JSONRPC::RequestBase &request);
    virtual void notify_async(std::unique_ptr<JSONRPC::RequestBase> request);

    virtual std::unique_ptr<JSONRPC::Response> call(
               const JSONRPC::RequestBase &request);
    virtual void call_async(std::unique_ptr<JSONRPC::RequestBase> request,
                                                ResponseHandler response);
};

}

#endif
