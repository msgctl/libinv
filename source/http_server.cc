#include <string>
#include <memory>
#include <gnutls/gnutls.h>
#include <gnutls/x509.h>
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
                            struct MHD_Connection *connection,
                                    std::string user_handle) {
    std::shared_ptr<HTTPServerSession> session(
      new HTTPServerSession(this, connection, user_handle));
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

static int _http_upload_handler(void *handler_cls,
                  struct MHD_Connection *connection,
                const char *url, const char *method,
         const char *version, const char *post_data,
          size_t *post_data_size, void **conn_cls) {
}

static int _http_rpc_handler(void *handler_cls,
             struct MHD_Connection *connection,
           const char *url, const char *method,
    const char *version, const char *post_data,
     size_t *post_data_size, void **conn_cls) {
    HTTPServer *server = (HTTPServer *)(handler_cls);
    std::shared_ptr<HTTPServerSession> **session =
        (std::shared_ptr<HTTPServerSession> **)(conn_cls);

    if (strcmp(method, MHD_HTTP_METHOD_POST))
        return MHD_NO; // abort
  
    // second+ pass
    int status;
    if (*post_data_size) {
        (**session)->request().append(post_data, *post_data_size);
        *post_data_size = 0;
        status = MHD_YES;
    } else { // last pass
        /* 
         * Suspend the connection to avoid busywaiting on
         * _http_response_reader.
         */
        MHD_suspend_connection(connection);

        std::unique_ptr<JSONRPC::Request> jrequest(new JSONRPC::Request(
                                               (**session)->request()));
        std::unique_ptr<RPC::ServerRequest> request(new RPC::ServerRequest(
                                          std::move(jrequest), **session));

        server->workqueue().push(std::move(request),
                         server->request_handler());

        struct MHD_Response *mhd_response =
             MHD_create_response_from_callback(MHD_SIZE_UNKNOWN, 4096, 
                            &_http_response_reader, (**session).get(),
                                                   &_request_free_cb);
        status = MHD_queue_response(connection, MHD_HTTP_OK,
                                              mhd_response);
        MHD_destroy_response(mhd_response);
        delete *session;
    }
    return status;
}

static gnutls_x509_crt_t get_client_certificate(gnutls_session_t tls_session) {
    unsigned int listsize;
    const gnutls_datum_t *pcert;
    unsigned int client_cert_status;
    gnutls_x509_crt_t client_cert;

    if (tls_session == NULL)
        throw exceptions::HTTPServerException("NULL gnutls session");
    if (gnutls_certificate_verify_peers2(tls_session,
                              &client_cert_status)) {
        // TODO
        throw exceptions::HTTPServerException("gnutls_certificate_verify_peers2()");
    }
    pcert = gnutls_certificate_get_peers(tls_session,
                                          &listsize);
    if (pcert == NULL || !listsize) {
        throw exceptions::HTTPServerException("Couldn't retrieve client\
                                                   certificate chain.");
    }
    if (gnutls_x509_crt_init(&client_cert)) {
        throw exceptions::HTTPServerException("Couldn't initialize client\
                                                           certificate.");
    } 
    if (gnutls_x509_crt_import(client_cert, &pcert[0],
                              GNUTLS_X509_FMT_DER)) {
        gnutls_x509_crt_deinit(client_cert);
        throw exceptions::HTTPServerException("Couldn't import client\
                                                       certificate.");
    }
}

static std::string cert_auth_get_dn(gnutls_x509_crt_t client_cert) {
    char *buf;
    size_t bufsize = 0;

    // TODO use string buffer, reserve
    gnutls_x509_crt_get_dn(client_cert, NULL, &bufsize);
    buf = (char *)malloc(bufsize);
    gnutls_x509_crt_get_dn(client_cert, buf, &bufsize);
    return std::string(buf);
}

static int _http_dispatch_handler(void *handler_cls,
                  struct MHD_Connection *connection,
                const char *url, const char *method,
         const char *version, const char *post_data,
          size_t *post_data_size, void **conn_cls) {
    HTTPServer *server = (HTTPServer *)(handler_cls);
    std::shared_ptr<HTTPServerSession> **session =
        (std::shared_ptr<HTTPServerSession> **)(conn_cls);

    if (*session == nullptr) {
        gnutls_session_t tls_session;
        const union MHD_ConnectionInfo *ci; 

        ci = MHD_get_connection_info (connection,
             MHD_CONNECTION_INFO_GNUTLS_SESSION);
        tls_session = (gnutls_session_t)(ci->tls_session);

        gnutls_x509_crt_t client_cert = get_client_certificate(tls_session);
        std::string client_handle = cert_auth_get_dn(client_cert);

        *session = new std::shared_ptr<HTTPServerSession>;
        **session = server->create_session(connection, client_handle);
        return MHD_YES;
    }

    // digest auth?
    // …

    if (!strcmp(url, "/upload")) {
        return _http_upload_handler(handler_cls, connection, url, method,
                           version, post_data, post_data_size, conn_cls);
    }
    return _http_rpc_handler(handler_cls, connection, url, method,
                    version, post_data, post_data_size, conn_cls);
}

void HTTPServer::init_daemon() {
    if (use_tls()) {
        const char *key_pem = m_key_map.data();
        const char *cert_pem = m_cert_map.data();

        m_daemon = MHD_start_daemon(MHD_USE_SELECT_INTERNALLY |
         MHD_USE_SUSPEND_RESUME | MHD_USE_DEBUG | MHD_USE_SSL,
            m_port, NULL, NULL, &_http_dispatch_handler, this, 
                    MHD_OPTION_CONNECTION_LIMIT, m_conn_limit,
                MHD_OPTION_CONNECTION_TIMEOUT, m_conn_timeout,
               MHD_OPTION_THREAD_POOL_SIZE, m_threadpool_size,
                            MHD_OPTION_HTTPS_MEM_KEY, key_pem,
                          MHD_OPTION_HTTPS_MEM_CERT, cert_pem,
                        MHD_OPTION_HTTPS_MEM_TRUST, cert_pem,
    //          MHD_OPTION_EXTERNAL_LOGGER, _httpd_logger, NULL,
                                              MHD_OPTION_END);
    } else {
        m_daemon = MHD_start_daemon(MHD_USE_SELECT_INTERNALLY |
                       MHD_USE_SUSPEND_RESUME | MHD_USE_DEBUG,
            m_port, NULL, NULL, &_http_dispatch_handler, this, 
                    MHD_OPTION_CONNECTION_LIMIT, m_conn_limit,
                MHD_OPTION_CONNECTION_TIMEOUT, m_conn_timeout,
               MHD_OPTION_THREAD_POOL_SIZE, m_threadpool_size,
    //          MHD_OPTION_EXTERNAL_LOGGER, _httpd_logger, NULL,
                                              MHD_OPTION_END);
    }
}

}
