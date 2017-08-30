#include <mutex>
#include <functional>
extern "C" {
    #include <curl/curl.h>
}
#include "http_client.hh"
#include "exception.hh"
#include "jsonrpc.hh"
#include "rpc.hh"
#include "filemap.hh"

namespace inventory::RPC {

static size_t _curl_rpc_write_cb(void *ptr, size_t size, size_t nmemb,
                                      std::string *reply_buffer) {
    reply_buffer->append((const char *)(ptr), size * nmemb);
    return size * nmemb;
}

static size_t _curl_rpc_read_cb(void *ptr, size_t size, size_t nmemb,
                             std::stringstream *request_stream) {
    request_stream->read((char *)(ptr), size * nmemb);
    return request_stream->gcount();
}

static size_t _curl_upload_read_cb(void *ptr, size_t size, size_t nmemb,
                                                           void *data) {
    memcpy(ptr, data, size * nmemb);
    return size * nmemb;
}

static size_t _curl_upload_write_cb(void *ptr, size_t size, size_t nmemb,
                                                            void *data) {
    return size * nmemb;
}

HTTPClientSession::HTTPClientSession(HTTPClient *client)
: ClientSession(static_cast<HTTPClient *>(client)) {
    CURL *rpc_handle = m_rpc_handle.curl();
    set_curl_defaults(rpc_handle);
    curl_easy_setopt(rpc_handle, CURLOPT_READFUNCTION, _curl_rpc_read_cb);
    curl_easy_setopt(rpc_handle, CURLOPT_WRITEFUNCTION, _curl_rpc_write_cb);
    curl_easy_setopt(rpc_handle, CURLOPT_SSL_VERIFYPEER,
                      (int)(client->tls_verify_peer()));
    curl_easy_setopt(rpc_handle, CURLOPT_SSLCERTTYPE, "PEM");
    curl_easy_setopt(rpc_handle, CURLOPT_SSLCERT, "client.crt");
    curl_easy_setopt(rpc_handle, CURLOPT_SSLKEYTYPE, "PEM");
    curl_easy_setopt(rpc_handle, CURLOPT_SSLKEY, "client.key");
    curl_easy_setopt(rpc_handle, CURLOPT_CAINFO, "ca.crt");

    CURL *upload_handle = m_upload_handle.curl();
    set_curl_defaults(upload_handle);
    curl_easy_setopt(upload_handle, CURLOPT_READFUNCTION, _curl_upload_read_cb);
    curl_easy_setopt(upload_handle, CURLOPT_SSL_VERIFYPEER,
                         (int)(client->tls_verify_peer()));
    curl_easy_setopt(upload_handle, CURLOPT_SSLCERTTYPE, "PEM");
    curl_easy_setopt(upload_handle, CURLOPT_SSLCERT, "client.crt");
    curl_easy_setopt(upload_handle, CURLOPT_SSLKEYTYPE, "PEM");
    curl_easy_setopt(upload_handle, CURLOPT_SSLKEY, "client.key");
    curl_easy_setopt(upload_handle, CURLOPT_CAINFO, "ca.crt");
}

void HTTPClientSession::set_curl_defaults(CURL *handle) {
    curl_easy_setopt(handle, CURLOPT_POST, 1);
    curl_easy_setopt(handle, CURLOPT_TCP_KEEPALIVE, 0);
    curl_easy_setopt(handle, CURLOPT_TCP_KEEPIDLE, 600);
    curl_easy_setopt(handle, CURLOPT_TCP_KEEPINTVL, 30);
}

void HTTPClientSession::notify(const JSONRPC::RequestBase &request) {
    call(request);
}

void HTTPClientSession::notify_async(std::unique_ptr<JSONRPC::RequestBase>
                                                                request) {
    call_async(std::move(request),
        [](std::unique_ptr<JSONRPC::Response>) -> void {});
}

std::unique_ptr<JSONRPC::Response> HTTPClientSession::call(
                     const JSONRPC::RequestBase &request) {
    using namespace exceptions;
    char errbuf[CURL_ERROR_SIZE];
    std::stringstream request_stream;
    std::string reply_buffer;

    request_stream.str(request);

    curl_easy_setopt(m_rpc_handle.curl(), CURLOPT_URL,
        static_cast<HTTPClient *>(m_client)->url().c_str()); 
    curl_easy_setopt(m_rpc_handle.curl(), CURLOPT_READDATA, &request_stream);
    curl_easy_setopt(m_rpc_handle.curl(), CURLOPT_POSTFIELDSIZE,
                                request_stream.str().size());
    curl_easy_setopt(m_rpc_handle.curl(), CURLOPT_WRITEDATA, &reply_buffer);
    curl_easy_setopt(m_rpc_handle.curl(), CURLOPT_ERRORBUFFER, errbuf);

    CURLcode result;
    {
        std::unique_lock<std::mutex> lock(m_curl_rpc_lock);
        result = curl_easy_perform(m_rpc_handle.curl());
    }
    if (result != CURLE_OK) {
        throw HTTPClientException(std::string("Couldn't complete request: ")
                                                                  + errbuf);
    }

    return std::make_unique<JSONRPC::Response>(reply_buffer);
}

void HTTPClientSession::call_async(std::unique_ptr<JSONRPC::RequestBase>
             request, ClientSession::ResponseHandler response_handler) {
    m_client->workqueue().push(std::move(request),
        [response_handler, this](JSONRPC::RequestBase &request) -> void {
            std::unique_ptr<JSONRPC::Response> response = call(request);
            response_handler(std::move(response));
        }
    );
}

void HTTPClientSession::upload_file(std::string id, std::string path) {
    using namespace exceptions;
    using namespace util;

    FileMap file(path);
    const char *data = file.data();
    char errbuf[CURL_ERROR_SIZE];

    std::string url = static_cast<HTTPClient *>(m_client)->url() + "/upload";
    curl_easy_setopt(m_upload_handle.curl(), CURLOPT_URL, url.c_str());
    curl_easy_setopt(m_upload_handle.curl(), CURLOPT_READDATA, data);
    curl_easy_setopt(m_upload_handle.curl(), CURLOPT_POSTFIELDSIZE, file.size());
    curl_easy_setopt(m_upload_handle.curl(), CURLOPT_ERRORBUFFER, errbuf);

    CURLcode result;
    {
        std::unique_lock<std::mutex> lock(m_curl_upload_lock);
        result = curl_easy_perform(m_upload_handle.curl());
    }
    if (result != CURLE_OK) {
        throw HTTPClientException(std::string("Couldn't complete upload: ")
                                                                 + errbuf);
    }
}

std::shared_ptr<ClientSession> HTTPClient::create_session() {
    std::shared_ptr<ClientSession> session =
        std::make_shared<HTTPClientSession>(this);
    m_sessions.push_back(session);
    return session;
}

}
