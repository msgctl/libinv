#ifndef LIBINV_SHARED_VECTOR_HH
#define LIBINV_SHARED_VECTOR_HH
#include <functional>
#include <memory>
#include <vector>
#include "rpc.hh"
#include "shared_wrapper.hh"

namespace inventory {

template<class Type>
class SharedVector {
public:
    typedef std::function<void(Shared<Type>)> ForeachCb;

    void push_back(Shared<Type> object) {
        m_vec.push_back(object);
    }

    void clear() {
        m_vec.clear();
    }

    void foreach(ForeachCb cb) {
        for (auto &shptr : m_vec)
            cb(shptr);
    }

    std::vector<Shared<Type>> &vec() {
        return m_vec;
    }

    std::shared_ptr<RPC::BatchClientRequest> get_async(
         std::shared_ptr<RPC::ClientSession> session) {
        using namespace RPC;
        using namespace JSONRPC;

        auto bcreq = std::make_shared<BatchClientRequest>(session);
        for (Shared<Type> &obj : m_vec) {
            std::unique_ptr<SingleRequest> jgetreq = obj->build_get_request(
                                            obj->id(), &bcreq->allocator());
            auto handler = obj->build_batch_get_async_handler();
            bcreq->push_back(std::move(jgetreq), handler);
        }
        return bcreq;
    }

    void get(std::shared_ptr<RPC::ClientSession> session) {
        auto bcreq = get_async(session);
        bcreq->complete();
    }

protected:
    std::vector<Shared<Type>> m_vec;
};

}

#endif
