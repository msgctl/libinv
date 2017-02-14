#ifndef LIBINV_RPC_HH
#define LIBINV_RPC_HH
#include <vector>
#include <memory>
#include <functional>
#include <algorithm>
#include <stdexcept>
#include <rapidjson/document.h>
#include "datamodel.hh"
#include "jsonrpc.hh"

namespace inventory {
namespace RPC {
namespace exceptions {
    class ExceptionBase : public JSONRPC::exceptions::ExceptionBase {
    public:
        using JSONRPC::exceptions::ExceptionBase::ExceptionBase;
    };

    class NoSuchMethod : public ExceptionBase {
        static const char *errclass() {
            return "No such RPC method: ";
        }

    public:
        NoSuchMethod(const std::string &method_name)
        : ExceptionBase(errclass() + method_name) {}

        NoSuchMethod(const char *method_name)
        : ExceptionBase(errclass() + std::string(method_name)) {}

        virtual JSONRPC::ErrorCode ec() const {
            return JSONRPC::ErrorCode::METHOD_NOT_FOUND;
        }
    };

    class InvalidParameters : public ExceptionBase {
        static const char *errclass() {
            return "Invalid parameters: ";
        }

    public:
        InvalidParameters(const std::string &errdesc)
        : ExceptionBase(errclass() + errdesc) {}

        InvalidParameters(const char *errdesc)
        : ExceptionBase(errclass() + std::string(errdesc)) {}

        virtual JSONRPC::ErrorCode ec() const {
            return JSONRPC::ErrorCode::INVALID_PARAMS;
        }
    };
}

class Session {
public:
    virtual void reply(std::unique_ptr<JSONRPC::Response> response) = 0;
};

class CallBase {
public:
    CallBase(Session *session)
    : m_session(session) {}

    Session *session() {
        return m_session;
    }

protected:
    Session *m_session;
};

class SingleCall;
class BatchCall : public CallBase {
    friend class SingleCall;

public:
    BatchCall(const JSONRPC::BatchRequest *request, Session *session)
    : m_req(request), CallBase(session) {}

    const JSONRPC::BatchRequest *jsonrpc() const {
        return m_req;
    }

    // Returns an empty unique_ptr if there should be no response
    template<class Database, class Datamodel>
    std::unique_ptr<JSONRPC::Response> complete(Database &db) const;

private:
    const JSONRPC::BatchRequest *m_req;
};

class SingleCall : public CallBase {
    friend class BatchCall;

public:
    SingleCall(const JSONRPC::SingleRequest *request, Session *session)
    : m_req(request), CallBase(session) {}

    const JSONRPC::SingleRequest *jsonrpc() const {
        return m_req;
    }

    template<class Database, class Datamodel>
    std::unique_ptr<JSONRPC::Response> complete(Database &db,
        rapidjson::Document::AllocatorType *alloc = nullptr) const;

private:
    template<class Database, class Datamodel>
    rapidjson::Value complete_call(Database &db,
        rapidjson::Document::AllocatorType &alloc) const;

    template<class Database, class Datamodel>
    rapidjson::Value complete_datamodel_call(Database &db,
        rapidjson::Document::AllocatorType &alloc) const;

    const JSONRPC::SingleRequest *m_req;
};

class ObjectCallParams {
public:
    ObjectCallParams(const SingleCall &call) {
        rapidjson::Value::ConstMemberIterator params =
         call.jsonrpc()->value().FindMember("params");
        if (params == call.jsonrpc()->value().MemberEnd())
            throw RPC::exceptions::InvalidParameters("\"params\" is undefined "
                                                "in a datamodel object call.");
        if (!params->value.IsObject())
            throw RPC::exceptions::InvalidParameters("\"params\" is not a "
                             "JSONRPC object in a datamodel object call.");
        m_params = &params->value;
    }

    // makes operator[] available
    const rapidjson::Value &get() const {
        return *m_params;
    }

    // libinv API
    std::string id() const {
        rapidjson::Value::ConstMemberIterator id = m_params->FindMember("id");
        if (id == m_params->MemberEnd())
            return std::string();
        if (!id->value.IsString())
            throw RPC::exceptions::InvalidParameters("object id is not a string");
        return id->value.GetString();
    }

    std::string type() const {
        rapidjson::Value::ConstMemberIterator type = m_params->FindMember("type");
        if (type == m_params->MemberEnd())
            throw RPC::exceptions::InvalidParameters("\"type\" parameter undefined");
        if (!type->value.IsString())
            throw RPC::exceptions::InvalidParameters("object type is not a string");
        return type->value.GetString();
    }

    const rapidjson::Value &operator[](const std::string &member) const {
        rapidjson::Value::ConstMemberIterator mit =
              m_params->FindMember(member.c_str());
        if (mit == m_params->MemberEnd()) {
            throw RPC::exceptions::InvalidParameters("\"" + member +
                                          "\" parameter undefined");
        }
        return mit->value;
    }

    bool has_member(const char *member) {
        return m_params->HasMember(member);
    }

private:
    const rapidjson::Value *m_params;
};

template<class Database, class Datamodel>
std::unique_ptr<JSONRPC::Response> BatchCall::complete(Database &db) const {
    JSONRPC::BatchResponse *batch_response = new JSONRPC::BatchResponse;
    std::unique_ptr<JSONRPC::Response> response_uniqptr(batch_response);
    m_req->foreach([&, this](const JSONRPC::SingleRequest &single_request){
        try {
            const SingleCall scall(&single_request, m_session);
            rapidjson::Value single_result =
                scall.complete_call<Database, Datamodel>(db,
                               batch_response->allocator());
            batch_response->push_back(single_request, single_result);
        } catch (const inventory::exceptions::ExceptionBase &e) {
            batch_response->push_back(single_request, e);
        }
    });
    return response_uniqptr;
}

template<class Database, class Datamodel>
std::unique_ptr<JSONRPC::Response> SingleCall::complete(Database &db,
                   rapidjson::Document::AllocatorType *alloc) const {
    JSONRPC::SingleResponse *single_response = new JSONRPC::SingleResponse;
    std::unique_ptr<JSONRPC::Response> response_uniqptr(single_response);

    if (alloc == nullptr)
        alloc = &single_response->allocator();

    try {
        rapidjson::Value result = complete_call<Database, Datamodel>(db, *alloc);
        single_response->assign(*m_req, result);
    } catch (const inventory::exceptions::ExceptionBase &e) {
        // catch everything; subject to change
        single_response->assign(*m_req, e);
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
        if (_namespace == "datamodel")
            return complete_datamodel_call<Database, Datamodel>(db, alloc);
    } catch (const std::out_of_range &e) {}
    throw JSONRPC::exceptions::InvalidRequest("There's no \"" + _namespace +
                                                           "\" namespace.");
}

template<class Database, class Datamodel>
rapidjson::Value SingleCall::complete_datamodel_call(
        Database &db, rapidjson::Document::AllocatorType &alloc) const {
    std::string objtype = ObjectCallParams(*this).type();
    std::unique_ptr<DatamodelObject<Database>> obj( 
              Datamodel::template create<Database>(
                                         objtype));
    return obj->rpc_call(db, *this, alloc);
}

// TODO
template<class Database, class Datamodel>
class DatamodelHandler {
    DatamodelHandler() {}

public:
    rapidjson::Value complete(Database &db,
                rapidjson::Document::AllocatorType &alloc) {
    }
};

class Request { 
public:
    Request(const std::string &request, std::shared_ptr<Session> session)
    : m_session(session), m_reqstr(request) {}

    // TODO call from a workqueue
    template<class Database, class Datamodel>
    std::unique_ptr<JSONRPC::Response> complete(Database &db) { 
        std::unique_ptr<JSONRPC::Request> request(new JSONRPC::Request);
        std::unique_ptr<JSONRPC::Response> response;

        try {
            request->parse(m_reqstr);
            if (request->is_batch()) {
                JSONRPC::BatchRequest breq(std::move(*request.release()));
                BatchCall batch(&breq, m_session.get());
                response = batch.complete<Database, Datamodel>(db);
            } else {
                JSONRPC::SingleRequest sreq(std::move(*request.release()));
                SingleCall single(&sreq, m_session.get());
                response = single.complete<Database, Datamodel>(db);
            }
        } catch (const JSONRPC::exceptions::ParseError &e) {
            response.reset(new JSONRPC::Response);
            response->assign(e);
        } catch (const JSONRPC::exceptions::InvalidRequest &e) {
            response.reset(new JSONRPC::Response);
            response->assign(e);
        }

        if (response)
            m_session->reply(std::move(response));
    }

private:
    std::string m_reqstr;
    std::shared_ptr<Session> m_session;
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
