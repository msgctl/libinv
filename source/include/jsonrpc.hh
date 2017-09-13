#ifndef LIBINV_JSONRPC_HH
#define LIBINV_JSONRPC_HH
#include <string>
#include <deque>
#include <stack>
#include <memory>
#include <stdexcept>
#include <functional>
#include <rapidjson/rapidjson.h>
#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <boost/tokenizer.hpp>
#include <boost/algorithm/string/join.hpp>
#include "exceptions.hh"

namespace inventory {
namespace JSONRPC {
extern const char *gc_version;

namespace util {
    const char *error_message(ErrorCode e);
}

class Namespace {
    // Not meant to be caught
    class InvalidUse : public std::runtime_error {
    public:
        InvalidUse(const char *desc)
        : std::runtime_error(std::string("Invalid use of Namespace class:") +
                                                                     desc) {}
    };

public:
    Namespace(std::string method);

    std::string operator[](int n) const;
    std::string first() const;
    std::string last() const;
    std::string path() const;

    void pop() const;
    void push() const;
    void rewind() const;
    int position() const;

private:
    mutable std::stack<std::string> m_stack;
    mutable std::deque<std::string> m_tokenized;
}; 

class SingleRequest;
class BatchRequest;

class JSONRPCBase {
protected:
    // Not meant to be caught
    class InvalidUse : public std::runtime_error {
    public:
        InvalidUse(const char *desc)
        : std::runtime_error(std::string("Invalid use: ") + desc) {}
    };

public:
    rapidjson::Value &value() {
        return *m_jval;
    }

    const rapidjson::Value &value() const {
        return *m_jval;
    }

    rapidjson::Document &document() {
        return *m_jdoc;
    }

    rapidjson::Document::AllocatorType &allocator() const;

    operator std::string() const;
    std::string string() const {
        return operator std::string();
    }

    bool is_batch() const;
    bool is_single() const;

protected:
    static bool is_batch(const rapidjson::Value &request);
    static bool is_single(const rapidjson::Value &request);

    JSONRPCBase() {}
    JSONRPCBase(enum rapidjson::Type type, rapidjson::Document::AllocatorType
                                                          *alloc = nullptr) {
        alloc_document(type, alloc);
    }
    JSONRPCBase(JSONRPCBase &&base);
    virtual ~JSONRPCBase() {}

    void alloc_document(enum rapidjson::Type type = rapidjson::kNullType,
                    rapidjson::Document::AllocatorType *alloc = nullptr);
    void parse(std::string reqstr);

    rapidjson::Value *m_jval;
    rapidjson::Document::AllocatorType *m_alloc;
    std::unique_ptr<rapidjson::Document> m_jdoc;
};

class RequestBase : public JSONRPCBase {
public:
    virtual ~RequestBase() {}

protected:
    static void validate(const rapidjson::Value &request);
    static void validate_single(const rapidjson::Value &request);

    static bool is_notification(const rapidjson::Value &request);

    using JSONRPCBase::JSONRPCBase;
    RequestBase() {}
    RequestBase(RequestBase &&req)
    : JSONRPCBase(std::move(req)) {};
};

class Request : public RequestBase {
public:
    Request() 
    : RequestBase(rapidjson::kNullType) {}

    Request(const std::string reqstr) 
    : RequestBase(rapidjson::kNullType), m_text(reqstr) {}

    // TODO msgctl
    //Request(std::string &&reqstr) 
    //: RequestBase(rapidjson::kNullType), m_text(std::move(reqstr)) {}

    virtual ~Request() {}

    void assign(const std::string reqstr) {
        m_text = reqstr;
    }

    void assign(std::string &&reqstr) {
        m_text = std::move(reqstr);
    }

    void parse() {
        RequestBase::parse(m_text);
        validate(*m_jval);
    }

protected:
    std::string m_text;
};

class SingleRequest : public RequestBase {
public:
    // SingleRequest instance with its separate Allocator (refcounting)
    SingleRequest()
    : RequestBase(rapidjson::kObjectType) {
        update_member("jsonrpc", gc_version);
    }

    // Building from scratch to include in BatchRequest, with a BatchRequest
    // instance's allocator to elide a copy.
    // usecase: batch calls
    SingleRequest(rapidjson::Document::AllocatorType *alloc)
    : RequestBase(rapidjson::kObjectType, alloc) {
        update_member("jsonrpc", gc_version);
    }

    // transient instance; see: BatchRequest::foreach()
    // Doesn't allocate or free heap memory.
    // usecase: iterating over BatchRequest members, batch calls
    SingleRequest(rapidjson::Value *value, rapidjson::Document::AllocatorType
                                                          *alloc = nullptr) {
        m_jval = value;
        m_alloc = alloc;
        if (is_batch(*m_jval)) {
            throw InvalidUse("tried to make a SingleRequest() instance out of "
                                                           "a batch request.");
        }
    }

    // Converting from a generic, just-parsed Request instance.
    SingleRequest(Request &&req)
    : RequestBase(std::move(req)) {
        if (is_batch(*m_jval)) {
            throw InvalidUse("tried to make a SingleRequest() instance out of "
                                                           "a batch request.");
        }
    }

    virtual ~SingleRequest() {}

    bool has_id() const;

    bool is_notification() const {
        return !has_id();
    }

    // jsonrpc, id and method are the only _required_ parameters in each
    // request. The rest is up to the application API implementation.
    rapidjson::Value &id() const {
        if (!has_id())
            throw InvalidUse("Called id() on an object without one.");
        return (*m_jval)["id"];
    }

    std::string id_string() const {
        if (!has_id())
            throw InvalidUse("Called id_string() on an object without one.");
        return (*m_jval)["id"].GetString();
    }

    void erase_id() {
        m_jval->EraseMember("id");
    }

    void id(std::string sid) {
        update_member("id", sid);
    }

    std::string method() const {
        return (*m_jval)["method"].GetString();
    }

    void method(const std::string sid) {
        update_member("method", sid.c_str());
        update_namespaces();
    }

    bool has_params() const {
        rapidjson::Value::ConstMemberIterator params =
                         m_jval->FindMember("params");
        return params != m_jval->MemberEnd();
    }

    const rapidjson::Value &params() const;
    rapidjson::Value &params(bool create = false);
    const Namespace &namespaces() const;

    void clear() {
        m_jval->SetObject();
    }

    bool empty() const {
        return !m_jval->MemberCount();
    }

private:
    void update_member(std::string key, std::string svalue);
    void update_namespaces() const;

    mutable std::unique_ptr<Namespace> m_namespace;
};

class BatchRequest : public RequestBase {
public:
    // BatchRequest instance with its separate Allocator (refcounting)
    BatchRequest() 
    : RequestBase(rapidjson::kArrayType) {}

    // Converting from a generic, just-parsed Request instance
    BatchRequest(Request &&req)
    : RequestBase(std::move(req)) {
        if (is_single(*m_jval)) {
            throw InvalidUse("tried to make a BatchRequest instance out of "
                                                       "a single request.");
        }
    }

    virtual ~BatchRequest() {}

    template<class Callback>
    void foreach(Callback cb) const {
        for (rapidjson::Value::ValueIterator itr = m_jval->Begin();
                                     itr != m_jval->End(); ++itr) {
            const SingleRequest sreq(&*itr);
            cb(sreq);
        }
    }

    template<class Callback>
    void foreach(Callback cb) {
        for (rapidjson::Value::ValueIterator itr = m_jval->Begin();
                                     itr != m_jval->End(); ++itr) {
            SingleRequest sreq(&*itr, m_alloc);
            cb(sreq);
        }
    }

    void push_back(std::unique_ptr<SingleRequest> req) { // TODO zero-copy
        m_jval->PushBack(req->value().Move(), allocator());
    }

    void clear() {
        m_jval->SetArray();
    }

    bool empty() const {
        return !m_jval->Size();
    }
};

class ResponseBase : public JSONRPCBase {
public:
    virtual ~ResponseBase() {}

protected:
    static void validate(const rapidjson::Value &request);
    static void validate_single(const rapidjson::Value &request);

    static bool is_notification(const rapidjson::Value &request);

    using JSONRPCBase::JSONRPCBase;
    ResponseBase() {}
    ResponseBase(ResponseBase &&req)
    : JSONRPCBase(std::move(req)) {};

    void add_jsonrpc_version(rapidjson::Value &doc);
    void add_request_id(rapidjson::Value &doc,
                        rapidjson::Value &id);
    void add_result(rapidjson::Value &doc,
                rapidjson::Value &result);
    void add_error(rapidjson::Value &obj, const
      inventory::exceptions::ExceptionBase &e);
};

class Response : public ResponseBase {
public:
    Response() 
    : ResponseBase(rapidjson::kNullType) {}

    Response(const std::string reqstr) 
    : ResponseBase(rapidjson::kNullType), m_text(reqstr) {}

    virtual ~Response() {}

    bool empty() const {
        return m_jval->IsNull();
    }

    void assign(const std::string reqstr) {
        m_text = reqstr;
    }

    void parse() {
        ResponseBase::parse(m_text);
        validate(*m_jval);
    }

    // TODO move semantics
    void parse(const std::string reqstr) {
        m_text = reqstr;
        parse();
    }

protected:
    std::string m_text;
};

class SingleResponse : public ResponseBase {
public:
    // SingleResponse instance with its separate Allocator (refcounting)
    // usecase: single calls
    SingleResponse()
    : ResponseBase(rapidjson::kObjectType) {};

    // Building from scratch to include in BatchResponse, with a BatchResponse
    // instance's allocator to elide a copy.
    // usecase: batch calls
    SingleResponse(rapidjson::Document::AllocatorType *alloc)
    : ResponseBase(rapidjson::kObjectType, alloc) {}

    // transient instance; see: BatchResponse::foreach()
    // Doesn't allocate or free heap memory.
    // usecase: iterating over BatchResponse members, batch calls
    SingleResponse(rapidjson::Value *value, rapidjson::Document::AllocatorType
                                                           *alloc = nullptr) {
        m_jval = value;
        m_alloc = alloc;
        check_batch();
    }

    // Converting from a generic, just-parsed Response instance.
    // usecase: single calls
    SingleResponse(Response &&resp)
    : ResponseBase(std::move(resp)) {
        check_batch();
    }

    SingleResponse(std::unique_ptr<Response> response)
    : ResponseBase(std::move(*(response.release()))) {
        check_batch();
    }

    virtual ~SingleResponse() {}

    void assign(const inventory::exceptions::ExceptionBase &e);
    void assign(const SingleRequest &request, rapidjson::Value &result);

    void assign(const SingleRequest &request,
            const inventory::exceptions::ExceptionBase &e);

    bool empty() const {
        return !m_jval->MemberCount();
    }

    bool has_id() const;
    bool has_error() const;
    bool has_result() const;

    rapidjson::Value &id() const {
        if (!has_id())
            throw InvalidUse("Called id() on an object without one.");
        return (*m_jval)["id"];
    }

    std::string id_string() const {
        if (!has_id())
            throw InvalidUse("Called id_string() on an object without one.");
        return (*m_jval)["id"].GetString();
    }

    ErrorCode ec() const {
        return ErrorCode(error()["ec"].GetInt());
    }

    void throw_ec() const {
        // TODO

        switch (ec()) {
        case JSONRPC::ErrorCode::NO_SUCH_OBJECT:
            throw ::inventory::exceptions::NoSuchObject(error_message());
        break;
        default: {
            std::string errstr = "Unhandled exception: " __FILE__ " ("
                                 + std::to_string((int)(ec())) + "): "
                                                    + error_message();
            throw std::runtime_error(errstr);
        } break;
        }
    }

    std::string error_message() const {
        return error()["message"].GetString();
    }

    const rapidjson::Value &result() const;

protected:
    void check_batch() {
        if (is_batch(*m_jval)) {
            throw InvalidUse("tried to make a SingleResponse() instance out of "
                                                           "a batch response.");
        } 
    }

    rapidjson::Value &error() const;
};

class BatchResponse : public ResponseBase {
public:
    typedef std::function<void(const SingleResponse &)> ForeachCb;

    // BatchResponse instance with its separate Allocator (refcounting)
    BatchResponse()
    : ResponseBase(rapidjson::kArrayType) {}

    // Converting from a generic, just-parsed Response instance.
    BatchResponse(Response &&resp)
    : ResponseBase(std::move(resp)) {
        check_single();
    } 

    BatchResponse(std::unique_ptr<Response> response)
    : ResponseBase(std::move(*(response.release()))) {
        check_single();
    }

    virtual ~BatchResponse() {}

    // Remember to construct SingleResponse with a BatchResponse instance
    // allocator if it's ever going to be a part of BatchResponse.
    void push_back(SingleResponse &&response);

    void foreach(ForeachCb cb) const {
        for (rapidjson::Value::ValueIterator itr = m_jval->Begin();
                                     itr != m_jval->End(); ++itr) {
            const SingleResponse sresp(&*itr);
            cb(sresp);
        }
    }

    bool empty() const {
        return !m_jval->Size();
    }

private:
    void check_single() const {
        if (is_single(*m_jval)) {
            throw InvalidUse("tried to make a BatchResponse instance out of "
                                                       "a single response.");
        }
    }
};

}
}

#endif
