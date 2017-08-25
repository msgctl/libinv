#include <functional>
extern "C" {
    #include <curl/curl.h>
}
#include "http_client.hh"
#include "exception.hh"
#include "jsonrpc.hh"
#include "rpc.hh"

namespace inventory::RPC {

static size_t _curl_write_cb(void *ptr, size_t size, size_t nmemb,
                                      std::string *reply_buffer) {
    reply_buffer->append((const char *)(ptr), size * nmemb);
    return size * nmemb;
}

static size_t _curl_read_cb(void *ptr, size_t size, size_t nmemb,
                             std::stringstream *request_stream) {
    request_stream->read((char *)(ptr), size * nmemb);
    return request_stream->gcount();
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

    struct CURLWrap {
        CURL *ptr;

        CURLWrap() {
            ptr = curl_easy_init();
            if (ptr == nullptr)
                throw HTTPClientException("Couldn't initialize libcurl.");
        }
        ~CURLWrap() {
            curl_easy_cleanup(ptr);
        }
    } curl;

    request_stream.str(request);

    curl_easy_setopt(curl.ptr, CURLOPT_URL,
        static_cast<HTTPClient *>(m_client)->url().c_str()); 
    curl_easy_setopt(curl.ptr, CURLOPT_POST, 1);
    curl_easy_setopt(curl.ptr, CURLOPT_READFUNCTION, _curl_read_cb);
    curl_easy_setopt(curl.ptr, CURLOPT_READDATA, &request_stream);
    curl_easy_setopt(curl.ptr, CURLOPT_POSTFIELDSIZE,
                        request_stream.str().size());
    curl_easy_setopt(curl.ptr, CURLOPT_WRITEFUNCTION, _curl_write_cb);
    curl_easy_setopt(curl.ptr, CURLOPT_WRITEDATA, &reply_buffer);
    curl_easy_setopt(curl.ptr, CURLOPT_ERRORBUFFER, errbuf);
    curl_easy_setopt(curl.ptr, CURLOPT_TCP_KEEPALIVE, 1);
    curl_easy_setopt(curl.ptr, CURLOPT_TCP_KEEPIDLE, 600);
    curl_easy_setopt(curl.ptr, CURLOPT_TCP_KEEPINTVL, 30);

    CURLcode result = curl_easy_perform(curl.ptr);
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

std::shared_ptr<ClientSession> HTTPClient::create_session() {
    std::shared_ptr<ClientSession> session =
        std::make_shared<HTTPClientSession>(this);
    m_sessions.push_back(session);
    return session;
}

}
