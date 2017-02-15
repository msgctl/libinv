#ifndef LIBINV_JSONRPC_HH
#define LIBINV_JSONRPC_HH
#include <string>
#include <deque>
#include <stack>
#include <memory>
#include <stdexcept>
#include <rapidjson/rapidjson.h>
#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <boost/tokenizer.hpp>
#include <boost/algorithm/string/join.hpp>
#include "exception.hh"

namespace inventory {
namespace JSONRPC {
extern const char *gc_version;

namespace util {
    const char *error_message(ErrorCode e);
}

namespace exceptions {
    class ExceptionBase : public inventory::exceptions::ExceptionBase {
    public:
        using inventory::exceptions::ExceptionBase::ExceptionBase;
    };

    class ParseError : public ExceptionBase {
    public:
        ParseError()
        : ExceptionBase("JSONRPC: parse error.") {}

        virtual ErrorCode ec() const {
            return ErrorCode::PARSE_ERROR;
        }
    };

    class InvalidRequest : public ExceptionBase {
        static const char *errclass() {
            return "Invalid JSONRPC request: ";
        }

    public:
        InvalidRequest(const std::string &description)
        : ExceptionBase(errclass() + description) {}

        InvalidRequest(const char *description)
        : ExceptionBase(errclass() + std::string(description)) {}

        virtual ErrorCode ec() const {
            return ErrorCode::INVALID_REQUEST;
        }
    };

    class InvalidResponse : public ExceptionBase {
        static const char *errclass() {
            return "Invalid JSONRPC response: ";
        }

    public:
        InvalidResponse(const std::string &description)
        : ExceptionBase(errclass() + description) {}

        InvalidResponse(const char *description)
        : ExceptionBase(errclass() + std::string(description)) {}

        virtual ErrorCode ec() const {
            return ErrorCode::INTERNAL_ERROR;
        }
    };
}

class Namespace {
public:
    // Not meant to be caught
    class InvalidUse : public std::runtime_error {
    public:
        InvalidUse(const char *desc)
        : std::runtime_error(std::string("Invalid use of Namespace class:") +
                                                                     desc) {}
    };

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

    void alloc_document(enum rapidjson::Type type = rapidjson::kNullType,
                    rapidjson::Document::AllocatorType *alloc = nullptr);
    void parse(const std::string &reqstr);

    rapidjson::Value *m_jval;
    rapidjson::Document::AllocatorType *m_alloc;
    std::unique_ptr<rapidjson::Document> m_jdoc;
};

class RequestBase : public JSONRPCBase {
protected:
    static void validate(const rapidjson::Value &request);
    static void validate_single(const rapidjson::Value &request);

    static bool is_notification(const rapidjson::Value &request);

    using JSONRPCBase::JSONRPCBase;
};

class Request : public RequestBase {
public:
    Request() 
    : RequestBase(rapidjson::kNullType) {}

    void parse(const std::string &reqstr) {
        RequestBase::parse(reqstr);
        validate(*m_jval);
    }
};

class SingleRequest : public RequestBase {
public:
    // SingleRequest instance with its separate Allocator (refcounting)
    SingleRequest()
    : RequestBase(rapidjson::kObjectType) {}

    // Building from scratch to include in BatchRequest, with a BatchRequest
    // instance's allocator to elide a copy.
    // usecase: batch calls
    SingleRequest(rapidjson::Document::AllocatorType *alloc)
    : RequestBase(rapidjson::kObjectType, alloc) {}

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

    void erase_id() {
        m_jval->EraseMember("id");
    }

    void id(const std::string &sid) {
        update_member("id", sid.c_str());
    }

    std::string method() const {
        return (*m_jval)["method"].GetString();
    }

    void method(const std::string &sid) {
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
    void update_member(const char *key, const char *svalue);
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

    void clear() {
        m_jval->SetArray();
    }

    bool empty() const {
        return !m_jval->Size();
    }
};

class ResponseBase : public JSONRPCBase {
protected:
    static void validate(const rapidjson::Value &request);
    static void validate_single(const rapidjson::Value &request);

    static bool is_notification(const rapidjson::Value &request);

    using JSONRPCBase::JSONRPCBase;

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

    bool empty() const {
        return m_jval->IsNull();
    }

    void parse(const std::string &reqstr) {
        ResponseBase::parse(reqstr);
        validate(*m_jval);
    }
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
        if (is_batch(*m_jval)) {
            throw InvalidUse("tried to make a SingleResponse() instance out of"
                                                         " a batch response.");
        }
    }

    // Converting from a generic, just-parsed Response instance.
    // usecase: single calls
    SingleResponse(Response &&resp)
    : ResponseBase(std::move(resp)) {
        if (is_batch(*m_jval)) {
            throw InvalidUse("tried to make a SingleResponse() instance out of "
                                                           "a batch response.");
        }
    }

    void assign(const inventory::exceptions::ExceptionBase &e);
    void assign(const SingleRequest &request, rapidjson::Value &result);

    void assign(const SingleRequest &request,
            const inventory::exceptions::ExceptionBase &e);

    bool empty() const {
        return !m_jval->MemberCount();
    }

    bool has_error() const;
    bool has_result() const;

    ErrorCode ec() const {
        return ErrorCode(error()["ec"].GetInt());
    }

    const char *error_message() const {
        return error()["message"].GetString();
    }

    const rapidjson::Value &result() const;

protected:
    rapidjson::Value &error() const;
};

class BatchResponse : public ResponseBase {
public:
    // BatchResponse instance with its separate Allocator (refcounting)
    BatchResponse()
    : ResponseBase(rapidjson::kArrayType) {}

    // Converting from a generic, just-parsed Response instance.
    BatchResponse(Response &&resp)
    : ResponseBase(std::move(resp)) {
        if (is_single(*m_jval)) {
            throw InvalidUse("tried to make a BatchResponse instance out of "
                                                       "a single response.");
        }
    } 

    // Remember to construct SingleResponse with a BatchResponse instance
    // allocator if it's ever going to be a part of BatchResponse.
    void push_back(SingleResponse &&response);

    bool empty() const {
        return !m_jval->Size();
    }
};

}
}

#endif
