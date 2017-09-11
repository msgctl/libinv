#include <string.h>
#include "jsonrpc.hh"

namespace inventory {
namespace JSONRPC {
const char *gc_version = "2.0";

const char *util::error_message(ErrorCode ec) {
    switch (ec) {
        case ErrorCode::PARSE_ERROR:
            return "Parse error";
        case ErrorCode::INVALID_REQUEST:
            return "Invalid request";
        case ErrorCode::METHOD_NOT_FOUND:
            return "Method not found";
        case ErrorCode::INVALID_PARAMS:
            return "Invalid parameters";
        case ErrorCode::INTERNAL_ERROR:
            return "Internal error";    
    }
    return "";
}

template<class Exception>
void validate_jsonrpc_version(const rapidjson::Value &object) {
    if (!object.HasMember("jsonrpc"))
        throw Exception("\"jsonrpc\" member is undefined.");
    if (!object["jsonrpc"].IsString())
        throw Exception("\"jsonrpc\" member isn't a string.");
    if (strcmp(object["jsonrpc"].GetString(),gc_version))
        throw Exception("unsupported JSONRPC version.");
}

template<class Exception>
void validate_id(const rapidjson::Value &object,
                                bool required) {
    if (object.HasMember("id")) {
        if (!object["id"].IsString() && !object["id"].IsInt())
            throw Exception("\"id\" must be a string or an "
                                                 "integer");
    } else if (required) {
        throw Exception("\"id\" member must be defined");
    }
}

void RequestBase::validate_single(const rapidjson::Value &request) {
    if (!request.IsObject())
        throw exceptions::InvalidRequest("request is not a JSON object");

    validate_jsonrpc_version<exceptions::InvalidRequest>(request);
    validate_id<exceptions::InvalidRequest>(request, false);

    if (!request.HasMember("method"))
        throw exceptions::InvalidRequest("\"method\" member is undefined.");
    if (!request["method"].IsString())
        throw exceptions::InvalidRequest("\"method\" member isn't a string");
}

void ResponseBase::validate_single(const rapidjson::Value &response) {
    if (!response.IsObject())
        throw exceptions::InvalidResponse("response is not a JSON object");

    validate_jsonrpc_version<exceptions::InvalidResponse>(response);
    validate_id<exceptions::InvalidResponse>(response, true);

    rapidjson::Value::ConstMemberIterator jresult =
                     response.FindMember("result");
    rapidjson::Value::ConstMemberIterator jerror =
                     response.FindMember("error");
    const bool result_present = jresult != response.MemberEnd();
    const bool error_present = jerror != response.MemberEnd();

    if (result_present && error_present ||
        !result_present && !error_present) {
        throw exceptions::InvalidResponse("Either \"result\" or \"error\""
                                              " member must be present.");
    } 

    if (error_present) {
        if (!jerror->value.IsObject()) {
            throw exceptions::InvalidResponse("JSONRPC error (\"error\") "
                                                     "is not an object.");
        }

        rapidjson::Value::ConstMemberIterator jec =
                    jerror->value.FindMember("ec");
        rapidjson::Value::ConstMemberIterator jmessage =
                    jerror->value.FindMember("message");
        const bool ec_present = jec != jerror->value.MemberEnd();
        const bool message_present = jmessage != jerror->value.MemberEnd();

        if (!ec_present) {
            throw exceptions::InvalidResponse("Error code (\"ec\") is not "
                                   "present in the JSONRPC error object.");
        } else if (!jec->value.IsInt()) {
            throw exceptions::InvalidResponse("Error code (\"ec\") is not "
                                                           " an integer.");
        }

        if (!message_present) {
            throw exceptions::InvalidResponse("Error message (\"message\") "
                             "is not present in the JSONRPC error object.");
        } else if (!jmessage->value.IsString()) {
            throw exceptions::InvalidResponse("Error message (\"message\") "
                                                        "is not a string.");
        }
    } 
}

void RequestBase::validate(const rapidjson::Value &request) {
    if (is_batch(request)) {
        for (rapidjson::Value::ConstValueIterator itr = request.Begin();
                                          itr != request.End(); ++itr) {
            validate_single(*itr);
        }
    } else if (is_single(request)) {
        validate_single(request);
    } else {
        throw JSONRPC::exceptions::InvalidRequest("the request must be an "
                                                     "object or an array");
    }
}

void ResponseBase::validate(const rapidjson::Value &request) {
    if (is_batch(request)) {
        for (rapidjson::Value::ConstValueIterator itr = request.Begin();
                                          itr != request.End(); ++itr) {
            validate_single(*itr);
        }
    } else if (is_single(request)) {
        validate_single(request);
    } else {
        throw JSONRPC::exceptions::InvalidResponse("the response must be an "
                                                       "object or an array");
    }
}

bool SingleRequest::has_id() const {
    return m_jval->HasMember("id");
}

bool JSONRPCBase::is_batch(const rapidjson::Value &val) {
    return val.IsArray();
}

bool JSONRPCBase::is_single(const rapidjson::Value &val) {
    return val.IsObject();
}

bool JSONRPCBase::is_batch() const {
    return RequestBase::is_batch(*m_jval);
}

bool JSONRPCBase::is_single() const {
    return RequestBase::is_single(*m_jval);
}

rapidjson::Value &SingleResponse::error() const {
    rapidjson::Value::MemberIterator jerror =
                 m_jval->FindMember("error");
    if (jerror == m_jval->MemberEnd())
       throw InvalidUse("Requested a nonexistent error object."); 
    return jerror->value;
}

bool SingleResponse::has_id() const {
    return m_jval->HasMember("id");
}

bool SingleResponse::has_error() const {
    return m_jval->HasMember("error");
}

bool SingleResponse::has_result() const {
    return m_jval->HasMember("result");
}

Namespace::Namespace(std::string method) {
    boost::char_separator<char> bsep(".");
    boost::tokenizer<boost::char_separator<char>> tokens(method, bsep);
    for (const std::string &t : tokens)
        m_tokenized.push_back(t);
}

std::string Namespace::operator[](int n) const {
    if (n >= m_tokenized.size())
        throw std::out_of_range("JSONRPC namespace lookup failed");
    return m_tokenized[n];
}

std::string Namespace::first() const {
    if (m_tokenized.empty())
        throw std::out_of_range("JSONRPC namespace lookup failed");
    return m_tokenized.front();
}

std::string Namespace::last() const {
    if (m_tokenized.empty())
        throw std::out_of_range("JSONRPC namespace lookup failed");
    return m_tokenized.back();
}

std::string Namespace::path() const {
    std::string result;
    for (const std::string &iter : m_tokenized)
        result += iter + ".";
    result.pop_back();
    return result;
}

void Namespace::pop() const {
    if (m_tokenized.empty())
        throw InvalidUse("one pop() too many");
    m_stack.push(m_tokenized.front());
    m_tokenized.pop_front();
}

void Namespace::push() const {
    if (m_stack.empty())
        throw InvalidUse("one push() too many");
    m_tokenized.push_front(m_stack.top());
    m_stack.pop();
}

void Namespace::rewind() const {
    while (!m_stack.empty())
        push();
}

int Namespace::position() const {
    return m_stack.size();
}

rapidjson::Document::AllocatorType &JSONRPCBase::allocator() const {
    if (m_alloc == nullptr) {
        throw InvalidUse("Invalid use: tried to modify a JSONRPCBase "
                      "subclass instance without assigned allocator");
    }
    return *m_alloc;
}

JSONRPCBase::operator std::string() const {
    rapidjson::StringBuffer esb;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> ewriter(esb);
    m_jval->Accept(ewriter);
    return esb.GetString();
}

JSONRPCBase::JSONRPCBase(JSONRPCBase &&base) {
    m_jdoc.reset(new rapidjson::Document(std::move(base.document())));
    m_jval = m_jdoc.get();
    m_alloc = &m_jdoc->GetAllocator();
}

void JSONRPCBase::alloc_document(enum rapidjson::Type type,
               rapidjson::Document::AllocatorType *alloc) {
    m_jdoc.reset(new rapidjson::Document(type, alloc));
    m_jval = m_jdoc.get();
    m_alloc = &m_jdoc->GetAllocator();
}

void JSONRPCBase::parse(std::string reqstr) {
    if (!m_jdoc) {
        throw InvalidUse("Invalid use: tried to parse into a transient "
                                          "JSONRPC subclass instance.");
    }

    m_jdoc->Parse(reqstr.c_str());
    if (m_jdoc->HasParseError())
        throw JSONRPC::exceptions::ParseError(reqstr);
}

const rapidjson::Value &SingleRequest::params() const {
    rapidjson::Value::ConstMemberIterator params =
                     m_jval->FindMember("params");
    if (params == m_jval->MemberEnd()) {
        throw InvalidUse("Called const params() on a SingleRequest "
                                             "without parameters.");
    }
    return params->value;
}

rapidjson::Value &SingleRequest::params(bool create) {
    rapidjson::Value::MemberIterator params =
                m_jval->FindMember("params");
    if (!create && params == m_jval->MemberEnd()) {
        throw InvalidUse("Called params() on a SingleRequest "
                                       "without parameters.");
    } else if (create) {
        rapidjson::Value empty_params(rapidjson::kObjectType);
        m_jval->AddMember("params", empty_params, allocator());
    }
    return params->value;
}

const Namespace &SingleRequest::namespaces() const {
    if (!m_namespace)
        update_namespaces();
    return *m_namespace;
}

void SingleRequest::update_member(std::string key, std::string svalue) {
    rapidjson::Value::MemberIterator jmemb =
            m_jval->FindMember(key.c_str());
    if (jmemb == m_jval->MemberEnd()) {
        // TODO try RAPIDJSON_HAS_STDSTRING, do not copy
        rapidjson::Value jkey(key.c_str(), allocator());
        rapidjson::Value jvalue(svalue.c_str(), allocator());

        m_jval->AddMember(jkey, jvalue, allocator());
    } else {
        jmemb->value.SetString(svalue.c_str(), allocator());
    }
}

void SingleRequest::update_namespaces() const {
    delete m_namespace.release();
    m_namespace.reset(new Namespace(method()));
}

void ResponseBase::add_jsonrpc_version(rapidjson::Value &doc) {
    rapidjson::Value jversion;
    jversion.SetString(JSONRPC::gc_version, allocator());
    doc.AddMember("jsonrpc", jversion, allocator());
}

void ResponseBase::add_request_id(rapidjson::Value &doc,
                                 rapidjson::Value &id) {
    // TODO rapidjson move semantics, same for RequestBase
    rapidjson::Value id_copy(id, allocator());
    doc.AddMember("id", id_copy, allocator());
}

void ResponseBase::add_result(rapidjson::Value &doc,
                         rapidjson::Value &result) {
    // TODO rapidjson move semantics
    // TODO broken code, need to explicitly copy or move
    doc.AddMember("result", result, allocator());
}

void ResponseBase::add_error(rapidjson::Value &obj, const
               inventory::exceptions::ExceptionBase &e) {
    rapidjson::Value errobj(rapidjson::kObjectType);
    errobj.AddMember("ec", (int)(e.ec()), allocator());
    errobj.AddMember("message", rapidjson::Value(e.what(), allocator()),
                                                           allocator());
    obj.AddMember("error", errobj, allocator());
}

void SingleResponse::assign(const inventory::exceptions::ExceptionBase &e) {
    m_jval->SetObject();
    add_jsonrpc_version(*m_jval);
    add_error(*m_jval, e);
}

void SingleResponse::assign(const SingleRequest &request,
                              rapidjson::Value &result) {
    if (request.is_notification() || result.IsNull()) {
        m_jval->SetNull();
        return;
    }

    m_jval->SetObject();
    add_jsonrpc_version(*m_jval);
    add_request_id(*m_jval, request.id());
    add_result(*m_jval, result);
}

void SingleResponse::assign(const SingleRequest &request,
         const inventory::exceptions::ExceptionBase &e) {
    if (request.is_notification()) {
        m_jval->SetNull();
        return;
    }

    m_jval->SetObject();
    add_jsonrpc_version(*m_jval);
    add_request_id(*m_jval, request.id());
    add_error(*m_jval, e);
}

const rapidjson::Value &SingleResponse::result() const {
    rapidjson::Value::ConstMemberIterator jresult =
                      m_jval->FindMember("result");
    if (jresult == m_jval->MemberEnd())
        throw InvalidUse("Requested a nonexistent result object.");
    return jresult->value; 
}

// Remember to construct SingleResponse with a BatchResponse instance
// allocator if it's ever going to be a part of BatchResponse.
void BatchResponse::push_back(SingleResponse &&response) {
    if (&allocator() != &response.allocator()) {
        throw InvalidUse("moved SingleResponse allocator != BatchResponse"
                                                           " allocator.");
    }
    if (response.empty())
        return;
    if (empty())
        m_jval->SetArray();

    // postcondition: response.document().IsNull() == true
    m_jval->PushBack(response.document(), allocator());
}

}
}
