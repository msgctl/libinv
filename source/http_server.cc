#include <string>
#include <memory>
#include "http_server.hh"
#include "exception.hh"
#include "jsonrpc.hh"
#include "rpc.hh"

namespace inventory::RPC {

void HTTPServerSession::terminate() {
    // TODO do something with m_connection
    ServerSession::terminate();
}

void HTTPServerSession::reply_async(std::unique_ptr<JSONRPC::ResponseBase>
                                                               response) {
    m_response = *response;
    MHD_resume_connection(m_connection);
}

std::shared_ptr<HTTPServerSession> HTTPServer::create_session(
                          struct MHD_Connection *connection) {
    std::shared_ptr<HTTPServerSession> session(
      new HTTPServerSession(this, connection));
    m_sessions.push_back(session);
    return session;
}

void _request_free_cb(void *priv) {
    HTTPServerSession *session = (HTTPServerSession *)(priv);
    session->terminate();
}

ssize_t _http_response_reader(void *cls, uint64_t pos,
                              char *buf, size_t max) {
    HTTPServerSession *session = (HTTPServerSession *)(cls);

    if (!max)
        return 0;
    if (pos >= session->m_response.length())
        return MHD_CONTENT_READER_END_OF_STREAM;
    session->m_response.copy(buf, max, pos);
}

static int _http_handler(void *handler_cls,
                  struct MHD_Connection *connection,
                const char *url, const char *method,
         const char *version, const char *post_data,
        size_t *post_data_size, void **cls) {
    HTTPServer *server = (HTTPServer *)(handler_cls);

    if (strcmp(method, MHD_HTTP_METHOD_POST))
        return MHD_NO; // abort
  
    if (*cls == nullptr) {
        *cls = (void *)(new std::string);
        return MHD_YES; // continue
    }

    // second+ pass
    int status;
    std::string *srequest = *(std::string **)(cls);
    if (*post_data_size) {
        srequest->append(post_data, *post_data_size);
        *post_data_size = 0;
        status = MHD_YES;
    } else { // last pass
        /* 
         * Suspend the connection to avoid busywaiting on
         * _http_response_reader.
         */
        MHD_suspend_connection(connection);

        std::shared_ptr<HTTPServerSession> session =
                 server->create_session(connection);
        std::unique_ptr<JSONRPC::Request> jrequest(new JSONRPC::Request(
                                                            *srequest));
        delete srequest;
        std::unique_ptr<RPC::ServerRequest> request(new RPC::ServerRequest(
                                            std::move(jrequest), session));

        server->workqueue().push(std::move(request),
                         server->request_handler());

        struct MHD_Response *mhd_response =
             MHD_create_response_from_callback(MHD_SIZE_UNKNOWN, 4096, 
                                &_http_response_reader, session.get(),
                                                   &_request_free_cb);
        status = MHD_queue_response(connection, MHD_HTTP_OK,
                                              mhd_response);
        MHD_destroy_response(mhd_response);
    }
    return status;
}

void HTTPServer::init_daemon() {
    m_daemon = MHD_start_daemon(MHD_USE_SELECT_INTERNALLY |
                   MHD_USE_SUSPEND_RESUME | MHD_USE_DEBUG,
                 m_port, NULL, NULL, &_http_handler, this, 
                          MHD_OPTION_CONNECTION_LIMIT, 64,
                        MHD_OPTION_CONNECTION_TIMEOUT, 10,
                           MHD_OPTION_THREAD_POOL_SIZE, 2,
//          MHD_OPTION_EXTERNAL_LOGGER, _httpd_logger, NULL,
                                          MHD_OPTION_END);
}

}
