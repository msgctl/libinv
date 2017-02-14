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
        : ExceptionBase("Parse error.") {}

        virtual ErrorCode ec() const {
            return ErrorCode::PARSE_ERROR;
        }
    };

    class InvalidRequest : public ExceptionBase {
        static const char *errclass() {
            return "Invalid request: ";
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

    Namespace(std::string method) {
        boost::char_separator<char> bsep(".");
        boost::tokenizer<boost::char_separator<char>> tokens(method, bsep);
        for (const std::string &t : tokens)
            m_tokenized.push_back(t);
    }

    std::string operator[](int n) const {
        if (n >= m_tokenized.size())
            throw std::out_of_range("JSONRPC namespace lookup failed");
        return m_tokenized[n];
    }

    std::string first() const {
        if (m_tokenized.empty())
            throw std::out_of_range("JSONRPC namespace lookup failed");
        return m_tokenized.front();
    }

    std::string last() const {
        if (m_tokenized.empty())
            throw std::out_of_range("JSONRPC namespace lookup failed");
        return m_tokenized.back();
    }

    std::string path() const {
        std::string result;
        for (const std::string &iter : m_tokenized)
            result += iter + ".";
        result.pop_back();
        return result;
    }

    void pop() const {
        if (m_tokenized.empty())
            throw InvalidUse("one pop() too many");
        m_stack.push(m_tokenized.front());
        m_tokenized.pop_front();
    }

    void push() const {
        if (m_stack.empty())
            throw InvalidUse("one push() too many");
        m_tokenized.push_front(m_stack.top());
        m_stack.pop();
    }

    void rewind() const {
        while (!m_stack.empty())
            push();
    }

    int position() const {
        return m_stack.size();
    }

private:
    mutable std::stack<std::string> m_stack;
    mutable std::deque<std::string> m_tokenized;
}; 

class SingleRequest;
class BatchRequest;

class RequestBase {
public:
    // Not meant to be caught
    class InvalidUse : public std::runtime_error {
    public:
        InvalidUse(const char *desc)
        : std::runtime_error(std::string("Invalid use of Request class:") +
                                                                   desc) {}
    };

    rapidjson::Value &value() {
        return *m_jval;
    }

    const rapidjson::Value &value() const {
        return *m_jval;
    }

    std::unique_ptr<rapidjson::Document> &document() {
        return m_jreq;
    }

    rapidjson::Document::AllocatorType &allocator() const {
        if (m_alloc == nullptr) {
            throw InvalidUse("tried to modify a Request instance without "
                                                    "assigned allocator");
        }
        return *m_alloc;
    }

    operator std::string() const {
        rapidjson::StringBuffer esb;
        rapidjson::PrettyWriter<rapidjson::StringBuffer> ewriter(esb);
        m_jreq->Accept(ewriter);
        return esb.GetString();
    }

protected:
    static void validate(const rapidjson::Value &request);
    static void validate_single(const rapidjson::Value &request);

    static bool is_batch(const rapidjson::Value &request);
    static bool is_notification(const rapidjson::Value &request);
    static bool is_single(const rapidjson::Value &request);

    RequestBase() {}

    void alloc_document(enum rapidjson::Type type = rapidjson::kNullType) {
        m_jreq.reset(new rapidjson::Document(type));
        m_jval = m_jreq.get();
        m_alloc = &m_jreq->GetAllocator();
    }

    rapidjson::Value *m_jval;
    rapidjson::Document::AllocatorType *m_alloc;
    std::unique_ptr<rapidjson::Document> m_jreq;
};

class Request : public RequestBase {
public:
    bool is_batch() const;
    bool is_single() const;

    Request() {
        alloc_document();
    }

    void parse(const std::string &reqstr) {
        m_jreq->Parse(reqstr.c_str());
        if (m_jreq->HasParseError())
            throw JSONRPC::exceptions::ParseError();
        m_jval = m_jreq.get();
        validate(*m_jval); // throws
    }
};

class SingleRequest : public RequestBase {
public:
    SingleRequest() {
        alloc_document(rapidjson::kObjectType);
    }

    // See: BatchRequest::foreach()
    SingleRequest(rapidjson::Value *value, rapidjson::Document::AllocatorType
                                                          *alloc = nullptr) {
        m_jval = value;
        m_alloc = alloc;
        validate(*m_jval); // throws;
        if (is_batch(*m_jval)) {
            throw InvalidUse("tried to make a SingleRequest() instance out of "
                                                           "a batch request.");
        }
    }

    SingleRequest(Request &&req) {
        m_jreq = std::move(req.document());
        m_jval = m_jreq.get();
        m_alloc = &m_jreq->GetAllocator();

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
    }

    const Namespace &namespaces() const {
        if (!m_namespace)
            m_namespace.reset(new Namespace(method()));
        return *m_namespace;
    }

    void clear() {
        m_jreq->SetObject();
    }

private:
    void update_member(const char *key, const char *svalue) {
        rapidjson::Value::MemberIterator jid =
                     m_jval->FindMember("id");
        if (jid == m_jval->MemberEnd()) {
            m_jval->AddMember(rapidjson::StringRef(key),
             rapidjson::StringRef(svalue), allocator());
        }
        jid->value.SetString(svalue, allocator());
    }

    mutable std::unique_ptr<Namespace> m_namespace;
};

class BatchRequest : public RequestBase {
public:
    BatchRequest() {
        alloc_document(rapidjson::kArrayType);
    }

    BatchRequest(rapidjson::Value *value, rapidjson::Document::AllocatorType
                                                         *alloc = nullptr) {
        m_jval = value;
        m_alloc = nullptr;
        validate(*m_jval);
        if (is_single(*m_jval)) {
            throw InvalidUse("tried to make a BatchRequest() instance out of "
                                                         "a single request.");
        }
    }

    BatchRequest(Request &&req) {
        m_jreq = std::move(req.document());
        m_jval = m_jreq.get();
        m_alloc = &m_jreq->GetAllocator();

        if (is_single(*m_jval)) {
            throw InvalidUse("tried to make a BatchRequest() instance out of "
                                                         "a single request.");
        }
    }

    template<class Callback>
    void foreach(Callback cb) const {
        for (rapidjson::Value::ValueIterator itr = m_jreq->Begin();
                                     itr != m_jreq->End(); ++itr) {
            const SingleRequest sreq(&*itr);
            cb(sreq);
        }
    }

    template<class Callback>
    void foreach(Callback cb) {
        for (rapidjson::Value::ValueIterator itr = m_jreq->Begin();
                                     itr != m_jreq->End(); ++itr) {
            SingleRequest sreq(&*itr, m_alloc);
            cb(sreq);
        }
    }

    void clear() {
        m_jreq->SetArray();
    }
};

class Response {
public:
    Response()
    : m_response(new rapidjson::Document) {
        m_response->SetNull();
    }

    Response(rapidjson::Document::AllocatorType &allocator)
    : m_response(new rapidjson::Document(&allocator)) {
        m_response->SetNull();
    }

    virtual ~Response() {}

    operator std::string() const {
        if (!m_cache.GetSize()) {
            rapidjson::PrettyWriter<rapidjson::StringBuffer> ewriter(m_cache);
            m_response->Accept(ewriter);
        }
        return m_cache.GetString();
    }

    rapidjson::Document::AllocatorType &allocator() const {
        return m_response->GetAllocator();
    }

    bool empty() const {
        return m_response->IsNull();
    }

    void assign(const inventory::exceptions::ExceptionBase &e) {
        m_response->SetObject();
        add_jsonrpc_version(*m_response);
        add_error(*m_response, e);
    }

protected:
    void add_jsonrpc_version(rapidjson::Value &doc) {
        rapidjson::Value jversion;
        jversion.SetString(JSONRPC::gc_version, allocator());
        doc.AddMember("version", jversion, allocator());
    }

    void add_request_id(rapidjson::Value &doc,
                        rapidjson::Value &id) {
        doc.AddMember("id", id, allocator());
    }

    void add_result(rapidjson::Value &doc,
               rapidjson::Value &result) {
        doc.AddMember("result", result, allocator());
    }

    void add_error(rapidjson::Value &obj, const
        inventory::exceptions::ExceptionBase &e) {
        rapidjson::Value errobj(rapidjson::kObjectType);
        errobj.AddMember("ec", (int)(e.ec()), allocator());
        errobj.AddMember("message", rapidjson::Value(e.what(), allocator()),
                                                               allocator());
        obj.AddMember("error", errobj, allocator());
    }

    std::unique_ptr<rapidjson::Document> m_response;
    mutable rapidjson::StringBuffer m_cache;
};

class SingleResponse : public Response {
public:
    SingleResponse() {}

    SingleResponse(rapidjson::Document::AllocatorType &allocator)
    : Response(allocator) {}

    void assign(const SingleRequest &request, rapidjson::Value &result) {
        if (request.is_notification() || result.IsNull()) {
            m_response->SetNull();
            return;
        }

        m_response->SetObject();
        add_jsonrpc_version(*m_response);
        add_request_id(*m_response, request.id());
        add_result(*m_response, result);
    }

    void assign(const SingleRequest &request,
            const inventory::exceptions::ExceptionBase &e) {
        if (request.is_notification()) {
            m_response->SetNull();
            return;
        }

        m_response->SetObject();
        add_jsonrpc_version(*m_response);
        add_request_id(*m_response, request.id());
        add_error(*m_response, e);
    }
};

class BatchResponse : public Response {
public:
    BatchResponse() {}

    BatchResponse(rapidjson::Document::AllocatorType &allocator)
    : Response(allocator) {}

    void push_back(const SingleRequest &request, rapidjson::Value &result) {
        if (request.is_notification() || result.IsNull())
            return;
        if (empty())
            m_response->SetArray();

        rapidjson::Value single_response(rapidjson::kObjectType);
        add_jsonrpc_version(single_response);
        add_request_id(single_response, request.id());
        add_result(single_response, result);

        m_response->PushBack(single_response, allocator());
    }

    void push_back(const SingleRequest &request, const
            inventory::exceptions::ExceptionBase &e) {
        if (request.is_notification())
            return;
        if (empty())
            m_response->SetArray();

        rapidjson::Value single_error(rapidjson::kObjectType);
        add_jsonrpc_version(single_error);
        add_request_id(single_error, request.id());
        add_error(single_error, e);

        m_response->PushBack(single_error, allocator());
    }
};

}
}

#endif
