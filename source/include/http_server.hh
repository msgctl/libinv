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

//TODO move to .cc
#include <algorithm>

namespace inventory::RPC::exceptions {
    class HTTPServerExceptionBase : public ExceptionBase {
    public:
        using RPC::exceptions::ExceptionBase::ExceptionBase;
    };

    class HTTPServerException : public HTTPServerExceptionBase {
        static const char *errclass() {
            return "HTTP client exception: ";
        }

    public:
        HTTPServerException(const std::string &method_name)
        : HTTPServerExceptionBase(errclass() + method_name) {}

        HTTPServerException(const char *method_name)
        : HTTPServerExceptionBase(errclass() + std::string(method_name)) {}

        virtual JSONRPC::ErrorCode ec() const {
            return JSONRPC::ErrorCode::INTERNAL_ERROR;
        }
    };
}

namespace inventory::RPC {

class HTTPServerSession;
class HTTPServer : public Server {
public:
    HTTPServer(int port, std::shared_ptr<Workqueue<ServerRequest>> workqueue,
                                              RequestHandler request_handler)
    : Server(workqueue, request_handler), m_port(port) {
        init_daemon();
    }

    virtual ~HTTPServer() {
        MHD_stop_daemon(m_daemon);
    }

    std::shared_ptr<HTTPServerSession> create_session(
                   struct MHD_Connection *connection);

private:
    void init_daemon();

    struct MHD_Daemon *m_daemon;
    int m_port;
};

class HTTPServerSession : public ServerSession {
    friend ssize_t _http_response_reader(void *session,
                  uint64_t pos, char *buf, size_t max);

public:
    HTTPServerSession(HTTPServer *server, struct MHD_Connection *connection)
    : ServerSession(static_cast<Server *>(server)),
      m_connection(connection) {}

    virtual void terminate();
    virtual void reply_async(std::unique_ptr<JSONRPC::ResponseBase>
                                                         response);

private:
    std::string m_response; // HTTP sessions are one-shot
    struct MHD_Connection *m_connection;
};

}

#endif
