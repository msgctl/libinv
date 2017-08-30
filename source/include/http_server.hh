#ifndef LIBINV_HTTP_SERVER_HH
#define LIBINV_HTTP_SERVER_HH 
extern "C" {
    #include <sys/types.h>
    #include <sys/select.h>
    #include <sys/socket.h>
    /*
     * libmicrohttpd documentation suggests including above before microhttpd.h
     */
    #include <microhttpd.h>
}
#include <string>
#include <memory>
#include "rpc.hh"
#include "jsonrpc.hh"
#include "exception.hh"
#include "workqueue.hh"
#include "http_server.hh"
#include "filemap.hh"

//TODO move to .cc
#include <algorithm>

namespace inventory::RPC {

class HTTPServerSession;
class HTTPServer : public Server {
public:
    HTTPServer(int port, std::shared_ptr<Workqueue<ServerRequest>> workqueue,
                                              RequestHandler request_handler)
    : Server(workqueue, request_handler), m_port(port) {
        init_daemon();
    }

    HTTPServer(int port, std::shared_ptr<Workqueue<ServerRequest>> workqueue,
                        RequestHandler request_handler, std::string key_path,
                                                       std::string cert_path)
    : Server(workqueue, request_handler), m_port(port), m_key_map(key_path),
                                                     m_cert_map(cert_path) {
        init_daemon();
    }

    virtual ~HTTPServer() {
        MHD_stop_daemon(m_daemon);
    }

    std::shared_ptr<HTTPServerSession> create_session(
                    struct MHD_Connection *connection,
                             std::string user_handle);

private:
    void init_daemon();
    bool use_tls() const {
        return !(m_key_map.path().empty() || m_cert_map.path().empty());
    }

    struct MHD_Daemon *m_daemon;
    int m_port;
    util::FileMap m_key_map;
    util::FileMap m_cert_map;

    int m_conn_limit = 64;
    int m_conn_timeout = 600;
    int m_threadpool_size = 2;
};

class HTTPServerSession : public ServerSession {
    friend ssize_t _http_response_reader(void *session,
                  uint64_t pos, char *buf, size_t max);

public:
    HTTPServerSession(HTTPServer *server, struct MHD_Connection *connection,
                                                         std::string handle)
    : ServerSession(static_cast<Server *>(server)),
      m_connection(connection) {
        m_handle = handle;
    }

    virtual void terminate();
    virtual void reply_async(std::unique_ptr<JSONRPC::ResponseBase>
                                                         response);

    std::string &request() {
        return m_request;
    }

private:
    std::string m_request;
    std::string m_response; // HTTP sessions are one-shot
    std::string m_handle;
    struct MHD_Connection *m_connection;
};

}

#endif
