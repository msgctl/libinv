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

void RequestBase::validate_single(const rapidjson::Value &request) {
    if (!request.IsObject())
        throw exceptions::InvalidRequest("request is not a JSON object");

    if (!request.HasMember("jsonrpc"))
        throw exceptions::InvalidRequest("\"jsonrpc\" is undefined.");
    if (!request["jsonrpc"].IsString())
        throw exceptions::InvalidRequest("\"jsonrpc\" member isn't a string.");
    if (strcmp(request["jsonrpc"].GetString(),gc_version))
        throw exceptions::InvalidRequest("unsupported JSONRPC version.");

    if (!request.HasMember("method"))
        throw exceptions::InvalidRequest("\"method\" member is undefined.");
    if (!request["method"].IsString())
        throw exceptions::InvalidRequest("\"method\" member isn't a string");

    if (request.HasMember("id")) {
        if (!request["id"].IsString() && !request["id"].IsInt())
            throw exceptions::InvalidRequest("\"id\" must be a string or an "
                                                                  "integer");
    }
}

void RequestBase::validate(const rapidjson::Value &request) {
    if (is_batch(request)) {
        for (rapidjson::Value::ConstValueIterator itr = request.Begin();
                                          itr != request.End(); ++itr) {
            if (!itr->IsObject())
                throw JSONRPC::exceptions::InvalidRequest(
                    "batch call array member is not an object");
            validate_single(*itr);
        }
    } else if (is_single(request)) {
        validate_single(request);
    } else {
        throw JSONRPC::exceptions::InvalidRequest("the request must be an "
                                                     "object or an array");
    }
}

bool SingleRequest::has_id() const {
    return m_jval->HasMember("id");
}

bool RequestBase::is_batch(const rapidjson::Value &val) {
    return val.IsArray();
}

bool RequestBase::is_single(const rapidjson::Value &val) {
    return val.IsObject();
}

bool Request::is_batch() const {
    return RequestBase::is_batch(*m_jval);
}

bool Request::is_single() const {
    return RequestBase::is_single(*m_jval);
}

}
}
