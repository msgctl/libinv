#ifndef LIBINV_HTTP_CLIENT_HH
#define LIBINV_HTTP_CLIENT_HH
#include <string>
#include <memory>
#include <vector>
#include <mutex>
#include <functional>
extern "C" {
    #include <curl/curl.h>
}
#include "rpc.hh"
#include "jsonrpc.hh"
#include "exception.hh"
#include "http_client_ex.hh"

namespace inventory::RPC {

class HTTPClientSession;
class HTTPClient : public Client {
public:
    HTTPClient(std::string url,
               std::shared_ptr<Workqueue<JSONRPC::RequestBase>> workqueue,
                                              bool tls_verify_peer = true)
    : Client(workqueue), m_url(url), m_tls_verify_peer(tls_verify_peer) {}
    virtual ~HTTPClient() {}

    virtual std::shared_ptr<ClientSession> create_session();

    std::string url() const {
        return m_url;
    }

    bool tls_verify_peer() const {
        return m_tls_verify_peer;
    }

private:
    std::string m_url;
    bool m_tls_verify_peer;
};

class HTTPClientSession : public ClientSession {
public:
    HTTPClientSession(HTTPClient *client);

    virtual void notify(const JSONRPC::RequestBase &request);
    virtual void notify_async(std::unique_ptr<JSONRPC::RequestBase> request);

    virtual std::unique_ptr<JSONRPC::Response> call(
               const JSONRPC::RequestBase &request);
    virtual void call_async(std::unique_ptr<JSONRPC::RequestBase> request,
                                                ResponseHandler response);
    virtual void upload_file(std::string id, std::string path);

private:
    class CURLWrapper {
    public:
        CURLWrapper() {
            using namespace exceptions;

            m_curl = curl_easy_init();
            if (m_curl == nullptr)
                throw HTTPClientException("Couldn't initialize libcurl.");
        }

        ~CURLWrapper() {
            curl_easy_cleanup(m_curl);
        }

        CURL *curl() {
            return m_curl;
        }

    private:
        CURL *m_curl;
    };

    void set_curl_defaults(CURL *handle);

    // allows the curl handle to be reused
    std::mutex m_curl_rpc_lock;
    CURLWrapper m_rpc_handle;

    std::mutex m_curl_upload_lock;
    CURLWrapper m_upload_handle;
};

}

#endif
