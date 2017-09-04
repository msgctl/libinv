#ifndef LIBINV_RPC_HH
#define LIBINV_RPC_HH
#include <vector>
#include <memory>
#include <functional>
#include <algorithm>
#include <stdexcept>
#include <future>
#include <rapidjson/document.h>
#include "datamodel.hh"
#include "jsonrpc.hh"
#include "workqueue.hh"
#include "factory.hh"
#include "rpc_ex.hh"
#include "auth.hh"

namespace inventory {
namespace RPC {

class Client;
class ClientSession {
public:
    typedef std::function<void(std::unique_ptr<JSONRPC::Response>)>
                                                   ResponseHandler;

    virtual ~ClientSession() {}

    virtual void notify(const JSONRPC::RequestBase &request) = 0;
    virtual void notify_async(std::unique_ptr<JSONRPC::RequestBase>
                                                      request) = 0;

    virtual std::unique_ptr<JSONRPC::Response> call(
           const JSONRPC::RequestBase &request) = 0;
    virtual void call_async(std::unique_ptr<JSONRPC::RequestBase> request,
                                            ResponseHandler response) = 0;
    virtual void upload_file(std::string id, std::string path) = 0;

    virtual void terminate();

protected:
    ClientSession(Client *client)
    : m_client(client) {}

    Client *m_client;
};

class Server;
class ServerSession : public std::enable_shared_from_this<ServerSession> {
public:
    // called by RPC::Request instances
    virtual void reply_async(std::unique_ptr<JSONRPC::ResponseBase>
                                                     response) = 0;
    virtual void terminate();

    User &user() const {
        return *mp_user;
    }

    Server &server() const {
        return *m_server;
    }

protected:
    ServerSession(Server *server)
    : m_server(server) {}

    Server *m_server;
    std::unique_ptr<User> mp_user;
};

class ClientRequest;
class Client {
    friend class ClientSession;
public:
    virtual ~Client() {}

    Workqueue<JSONRPC::RequestBase> &workqueue() {
        return *m_wq;
    }

    virtual std::shared_ptr<ClientSession> create_session() = 0;

protected:
    Client(std::shared_ptr<Workqueue<JSONRPC::RequestBase>> workqueue)
    : m_wq(workqueue) {}

    void remove_session(ClientSession *session);

    std::shared_ptr<Workqueue<JSONRPC::RequestBase>> m_wq;
    std::vector<std::shared_ptr<ClientSession>> m_sessions;
};

class ServerRequest;
class Server {
    friend class ServerSession;

public:
    typedef std::function<void(ServerRequest &)> RequestHandler;

    virtual ~Server() {}

    Workqueue<ServerRequest> &workqueue() {
        return *m_wq;
    }

    RequestHandler request_handler() {
        return m_request_handler;
    }

protected:
    Server(std::shared_ptr<Workqueue<ServerRequest>> workqueue,
                                RequestHandler request_handler)
    : m_wq(workqueue), m_request_handler(request_handler) {}

    void remove_session(ServerSession *session);

    std::shared_ptr<Workqueue<ServerRequest>> m_wq;
    RequestHandler m_request_handler;
    std::vector<std::shared_ptr<ServerSession>> m_sessions;
};

class CallBase {
public:
    CallBase(ServerSession *session)
    : m_session(session) {}

    ServerSession *session() {
        return m_session;
    }

protected:
    ServerSession *m_session;
};

class SingleCall;
class BatchCall : public CallBase {
    friend class SingleCall;

public:
    BatchCall(JSONRPC::BatchRequest *request, ServerSession *session)
    : CallBase(session) {
        m_req.ptr = request;
    }

    BatchCall(const JSONRPC::BatchRequest *request, ServerSession *session)
    : CallBase(session) {
        m_req.cptr = request;
    }

    const JSONRPC::BatchRequest *jsonrpc() const {
        return m_req.cptr;
    }

    JSONRPC::BatchRequest *jsonrpc() {
        return m_req.ptr;
    }

    // Returns an empty unique_ptr if there should be no response
    template<class Database, class Datamodel>
    std::unique_ptr<JSONRPC::ResponseBase> complete(Database &db) const;

private:
    union {
        JSONRPC::BatchRequest *ptr;
        const JSONRPC::BatchRequest *cptr;
    } m_req;
};

class SingleCall : public CallBase {
    friend class BatchCall;

public:
    SingleCall(JSONRPC::SingleRequest *request, ServerSession *session)
    : CallBase(session) {
        m_req.ptr = request;
    }

    SingleCall(const JSONRPC::SingleRequest *request, ServerSession *session)
    : CallBase(session) {
        m_req.cptr = request;
    }

    const JSONRPC::SingleRequest *jsonrpc() const {
        return m_req.cptr;
    }

    JSONRPC::SingleRequest *jsonrpc() {
        return m_req.ptr;
    }

    template<class Database, class Datamodel>
    std::unique_ptr<JSONRPC::ResponseBase> complete(Database &db,
        rapidjson::Document::AllocatorType *alloc = nullptr) const;

private:
    template<class Database, class Datamodel>
    rapidjson::Value complete_call(Database &db,
        rapidjson::Document::AllocatorType &alloc) const;

    template<class Database, class Datamodel>
    rapidjson::Value complete_datamodel_call(Database &db,
        rapidjson::Document::AllocatorType &alloc) const;

    union {
        JSONRPC::SingleRequest *ptr;
        const JSONRPC::SingleRequest *cptr;
    } m_req;
};

// transient object - doesn't allocate or free heap memory, doesn't increment
// refcounts.
class ObjectCallParams {
public:
    ObjectCallParams(const SingleCall &call) {
        if (!call.jsonrpc()->has_params()) {
            throw RPC::exceptions::InvalidParameters("\"params\" is undefined "
                                                "in a datamodel object call.");
        }

        const rapidjson::Value &params = call.jsonrpc()->params();
        if (!params.IsObject())
            throw RPC::exceptions::InvalidParameters("\"params\" is not a "
                             "JSONRPC object in a datamodel object call.");
        m_params.cptr = &params;
    }

    ObjectCallParams(SingleCall &call) {
        // creates params member if it doesn't exist and takes a pointer to it
        m_params.ptr = &call.jsonrpc()->params(true);
        if (!m_params.ptr->IsObject()) {
            throw RPC::exceptions::InvalidParameters("\"params\" is undefined "
                                                "in a datamodel object call.");
        }
    }

    // makes operator[] available
    const rapidjson::Value &get() const {
        return *m_params.cptr;
    }

    rapidjson::Value &get() {
        return *m_params.ptr;
    }

    // libinv API
    std::string id() const {
        rapidjson::Value::ConstMemberIterator id =
                  m_params.cptr->FindMember("id");
        if (id == m_params.cptr->MemberEnd())
            return std::string();
        if (!id->value.IsString())
            throw RPC::exceptions::InvalidParameters("object id is not a string");
        return id->value.GetString();
    }

    std::string type() const {
        rapidjson::Value::ConstMemberIterator type =
                  m_params.cptr->FindMember("type");
        if (type == m_params.cptr->MemberEnd())
            throw RPC::exceptions::InvalidParameters("\"type\" parameter undefined");
        if (!type->value.IsString())
            throw RPC::exceptions::InvalidParameters("object type is not a string");
        return type->value.GetString();
    }

    const rapidjson::Value &operator[](const std::string &member) const {
        rapidjson::Value::ConstMemberIterator mit =
         m_params.cptr->FindMember(member.c_str());
        if (mit == m_params.cptr->MemberEnd()) {
            throw RPC::exceptions::InvalidParameters("\"" + member +
                                          "\" parameter undefined");
        }
        return mit->value;
    }

    rapidjson::Value &operator[](const std::string &member) {
        rapidjson::Value::MemberIterator mit =
          m_params.ptr->FindMember(member.c_str());
        if (mit == m_params.ptr->MemberEnd()) {
            throw RPC::exceptions::InvalidParameters("\"" + member +
                                          "\" parameter undefined");
        }
        return mit->value;
    }

    bool has_member(const char *member) const {
        return m_params.cptr->HasMember(member);
    }

private:
    union {
        rapidjson::Value *ptr;
        const rapidjson::Value *cptr;
    } m_params;
};

template<class Database, class Datamodel>
std::unique_ptr<JSONRPC::ResponseBase> BatchCall::complete(Database &db)
                                                                 const {
    JSONRPC::BatchResponse *bresp = new JSONRPC::BatchResponse;
    std::unique_ptr<JSONRPC::ResponseBase> resp_uniqptr(bresp);
    m_req.cptr->foreach([&, this](const JSONRPC::SingleRequest &srequest){
        try {
            JSONRPC::SingleResponse sresp(&bresp->allocator());
            const SingleCall scall(&srequest, m_session);

            rapidjson::Value sresult =
              scall.complete_call<Database, Datamodel>(db, bresp->allocator());
            sresp.assign(srequest, sresult);

            //std::cout << "debug: " << sresp.string() << std::endl;

            bresp->push_back(std::move(sresp));
        } catch (const inventory::exceptions::ExceptionBase &e) {
            JSONRPC::SingleResponse sresp(&bresp->allocator());

            sresp.assign(srequest, e);
            bresp->push_back(std::move(sresp));
        }
    });
    return resp_uniqptr;
}

template<class Database, class Datamodel>
std::unique_ptr<JSONRPC::ResponseBase> SingleCall::complete(Database &db,
                       rapidjson::Document::AllocatorType *alloc) const {
    JSONRPC::SingleResponse *single_response = new JSONRPC::SingleResponse;
    std::unique_ptr<JSONRPC::ResponseBase> response_uniqptr(single_response);

    if (alloc == nullptr)
        alloc = &single_response->allocator();

    try {
        //std::cout << "debug request: " << m_req.cptr->string() << std::endl;

        rapidjson::Value result = complete_call<Database, Datamodel>(db, *alloc);
        single_response->assign(*m_req.cptr, result);

        //std::cout << "debug: " << single_response->string() << std::endl;
    } catch (const inventory::exceptions::ExceptionBase &e) {
        // catch everything; subject to change
        single_response->assign(*m_req.cptr, e);
    }
    return response_uniqptr;
}

template<class Database, class Datamodel>
rapidjson::Value SingleCall::complete_call(Database &db,
      rapidjson::Document::AllocatorType &alloc) const {
    std::string _namespace = jsonrpc()->namespaces().first();
    // jsonrpc()->namespaces() object will be used later on by specialized
    // RPC handlers. Here only the first namespace specifier is removed.
    jsonrpc()->namespaces().pop();
    try {
        if (_namespace == "object")
            return complete_datamodel_call<Database, Datamodel>(db, alloc);
    } catch (const std::out_of_range &e) {}
    throw JSONRPC::exceptions::InvalidRequest("There's no \"" + _namespace +
                                                           "\" namespace.");
}

template<class Database, class Datamodel>
rapidjson::Value SingleCall::complete_datamodel_call(
        Database &db, rapidjson::Document::AllocatorType &alloc) const {

    //std::cout << "debug request: " << m_req.cptr->string() << std::endl;
    std::string objtype = ObjectCallParams(*this).type();
    std::unique_ptr<DatamodelObject<Database>> obj( 
              Datamodel::template create<Database>(
                                         objtype));
    return obj->rpc_call(db, *this, alloc);
}

template<class Database, class Datamodel>
class CallHandler {
public:
    virtual ~CallHandler() {}

    virtual rapidjson::Value complete(Database &db, const SingleCall &call,
                            rapidjson::Document::AllocatorType &alloc) = 0;
};

template<class Database, class Datamodel>
class DatamodelHandler : public CallHandler<Database, Datamodel> {
public:
    virtual rapidjson::Value complete(Database &db, const SingleCall &call,
                               rapidjson::Document::AllocatorType &alloc) {
        std::string objtype = ObjectCallParams(call).type();
        std::unique_ptr<DatamodelObject<Database>> obj( 
                  Datamodel::template create<Database>(
                                             objtype));
        return obj->rpc_call(db, call, alloc);
    } 
};

class ClientRequest : public std::enable_shared_from_this<ClientRequest> {
    // Not meant to be caught
    class InvalidUse : public std::runtime_error {
    public:
        InvalidUse(const char *desc)
        : std::runtime_error(std::string("Invalid use of ClientRequest class:") +
                                                                         desc) {}
    };

public:
    typedef std::function<void(std::unique_ptr<JSONRPC::Response>)> ResponseHandler;
    typedef std::function<void()> CompleteCallback;

protected:
    ClientRequest(std::weak_ptr<ClientSession> session,
       std::vector<CompleteCallback> complete_cbs = {})
    : m_session(session), m_complete_cbs(complete_cbs) {}

public:
    virtual ~ClientRequest() {};

    void push_complete_cb(CompleteCallback handler) {
        m_complete_cbs.push_back(handler);
    }

    virtual void complete() = 0;
    virtual void complete_async() = 0;
        return (bool)(m_response);
    }

    const JSONRPC::Response &jsonrpc_response() const {
        if (!completed()) {
            throw InvalidUse("Called jsonrpc_response() on a not yet "
                                                "completed request.");
        }
        return *m_response;
    }

    const JSONRPC::RequestBase &jsonrpc_request() const {
        // TODO exceptions
        return *m_request;
    }

    std::future<void> future() {
        return m_promise.get_future();
    }

    virtual std::string string() const = 0;

protected:
    std::weak_ptr<ClientSession> m_session;
    std::vector<CompleteCallback> m_complete_cbs;
    std::unique_ptr<JSONRPC::Response> m_response;
    std::promise<void> m_promise;
    std::unique_ptr<JSONRPC::RequestBase> m_request;
};

class SingleClientRequest : public ClientRequest {
public:
    friend class inventory::Factory<SingleClientRequest>;

    SingleClientRequest(std::unique_ptr<JSONRPC::RequestBase> request,
                           std::weak_ptr<ClientSession> session)
    : ClientRequest(session), m_request(std::move(request)) {}

    SingleClientRequest(std::unique_ptr<JSONRPC::RequestBase> request,
                           std::weak_ptr<ClientSession> session,
                                        ResponseHandler handler,
                std::vector<CompleteCallback> complete_cbs = {})
    : ClientRequest(session, complete_cbs), m_request(std::move(request)),
                                                     m_handler(handler) {}

    virtual void complete() {
        {
            auto session = m_session.lock();
            m_response = session->call(*m_request);
        }

        m_response->parse();
        if (m_handler)
            m_handler(std::move(m_response));
        for (CompleteCallback &cb : m_complete_cbs)
            cb();
        m_promise.set_value();
    }

    virtual void complete_async() {
        std::weak_ptr<ClientRequest> this_weakptr = shared_from_this();
        auto session = m_session.lock();
        if (!session) // TODO throw
            return;

        session->call_async(std::move(m_request),
            [this_weakptr, this](std::unique_ptr<JSONRPC::Response>
                                                response) -> void {
                auto request = this_weakptr.lock();
                if (!request)
                    return;

                m_response = std::move(response);
                m_response->parse();
                if (m_handler)
                    m_handler(std::move(m_response));
                for (CompleteCallback &cb : m_complete_cbs)
                    cb();
                m_promise.set_value();
            }
        );
    }

    virtual std::string string() const {
        return m_request->string();
    }

    std::unique_ptr<JSONRPC::RequestBase> release() {
        return m_request.release();
    }

private:
    ResponseHandler m_handler;
};

class BatchClientRequest : public ClientRequest {
public:
    typedef std::map<std::string, ResponseHandler> HandlerMap;

    BatchClientRequest(std::weak_ptr<ClientSession> session)
    : ClientRequest(session) {
        m_request.reset(new JSONRPC::BatchRequest);
    }

    BatchClientRequest(std::unique_ptr<JSONRPC::RequestBase> request,
                           std::weak_ptr<ClientSession> session)
    : ClientRequest(session), m_request(std::move(request)) {}

    BatchClientRequest(std::unique_ptr<JSONRPC::RequestBase> request,
                           std::weak_ptr<ClientSession> session,
                                             HandlerMap hnd_map,
                std::vector<CompleteCallback> complete_cbs = {})
    : ClientRequest(session, complete_cbs), m_request(std::move(request)),
                                                 m_handler_map(hnd_map) {}

    void push_back(std::unique_ptr<JSONRPC::SingleRequest> sreq,
                                      ResponseHandler handler) {
        using namespace JSONRPC;

        m_handlers[sreq->id_string()] = std::move(handler);
        BatchRequest *breq = static_cast<BatchRequest *>(m_request.get());
        breq->push_back(sreq);
    }

    virtual void complete() {
        /*
        {
            auto session = m_session.lock();
            m_response = session->call(*m_request);
        }

        m_response->parse();
        if (m_handler)
            m_handler(std::move(m_response));
        for (CompleteCallback &cb : m_complete_cbs)
            cb();
        m_promise.set_value();
        */
    }

    virtual void complete_async() {
        /*
        std::weak_ptr<ClientRequest> this_weakptr = shared_from_this();
        auto session = m_session.lock();
        if (!session) // TODO throw
            return;

        session->call_async(std::move(m_request),
            [this_weakptr](std::unique_ptr<JSONRPC::Response>
                                          response) -> void {
                auto request = this_weakptr.lock();
                if (!request)
                    return;

                request->m_response = std::move(response);
                request->m_response->parse();
                if (request->m_handler)
                    request->m_handler(std::move(request->m_response));
                for (CompleteCallback &cb : request->m_complete_cbs)
                    cb();
                request->m_promise.set_value();
            }
        );
        */
    }


private:
    HandlerMap m_handler_map;
};

class ServerRequest { 
public:
    ServerRequest(std::unique_ptr<JSONRPC::Request> request,
                       std::shared_ptr<ServerSession> session) 
    : m_session(session), m_request(std::move(request)) {}

    template<class Database, class Datamodel>
    void complete(Database &db) { 
        std::unique_ptr<JSONRPC::ResponseBase> response;

        try {
            m_request->parse(); // throws
            //std::cout << "debug request (server): " << m_request->string() << std::endl;
            if (m_request->is_batch()) {
                JSONRPC::BatchRequest breq(std::move(*m_request.release()));
                BatchCall batch(&breq, m_session.get());
                response = batch.complete<Database, Datamodel>(db);
            } else {
                JSONRPC::SingleRequest sreq(std::move(*m_request.release()));
                SingleCall single(&sreq, m_session.get());
                response = single.complete<Database, Datamodel>(db);
            }
        } catch (const JSONRPC::exceptions::ParseError &e) {
            JSONRPC::SingleResponse *sresp = new JSONRPC::SingleResponse;
            sresp->assign(e);
            response.reset(sresp);
        } catch (const JSONRPC::exceptions::InvalidRequest &e) {
            JSONRPC::SingleResponse *sresp = new JSONRPC::SingleResponse;
            sresp->assign(e);
            response.reset(sresp);
        }

        //std::cout << "debug response (server): " << response->string() << std::endl;

        if (response)
            m_session->reply_async(std::move(response));
    }

private:
    std::shared_ptr<ServerSession> m_session;
    std::unique_ptr<JSONRPC::Request> m_request;
};

template<class Database, class Mixin>
using Impl = rapidjson::Value (Mixin::*)(Database &,
    const SingleCall &, rapidjson::Document::AllocatorType &);

template<class Database, class Mixin>
class Method { 
public:
    Method(std::string name, Impl<Database, Mixin> impl)
    : m_name(name), m_impl(impl) {}

    std::string name() const {
        return m_name;
    }

    Impl<Database, Mixin> impl() const {
        return m_impl;
    }

    rapidjson::Value operator()(Mixin &object, Database &db,
                                     const SingleCall &call,
          rapidjson::Document::AllocatorType &alloc) const {
        return (object.*m_impl)(db, call, alloc);
    }

private:
    std::string m_name;
    Impl<Database, Mixin> m_impl;
};

template<class Database, class Mixin>
class MethodRoster {
public:
    rapidjson::Value rpc_call(Database &db, const SingleCall &call,
                       rapidjson::Document::AllocatorType &alloc) {
        Mixin &object = static_cast<Mixin &>(*this); 
        return rpc_method_get(call.jsonrpc()->namespaces().path())(object,
                                                         db, call, alloc);
    }

    static void rpc_method_list(std::vector<std::string> &methods) {
        for (const Method<Database, Mixin> &m : Mixin::methods())
            methods.push_back(m.name());
    }

    static bool rpc_method_exists(std::string name) {
        for (const Method<Database, Mixin> &m : Mixin::methods())
            if (m.name() == name)
                return true;
        return false;
    }

    static const Method<Database, Mixin> &rpc_method_get(std::string name) {
        for (const Method<Database, Mixin> &m : Mixin::methods()) {
            if (m.name() == name)
                return m;
        }
        throw RPC::exceptions::NoSuchMethod(name);
    }
};

}
}

#endif
