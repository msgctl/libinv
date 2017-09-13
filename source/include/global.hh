#ifndef LIBINV_GLOBAL_HH
#define LIBINV_GLOBAL_HH
#include <memory>
#include <vector>
#include <string>
#include <functional>
#include <rapidjson/document.h>
#include "rpc.hh"

namespace inventory {

// Keeps a global object index
template<class Database, class Derived>
class Global : public RPC::MethodRoster<Database,
                     Global<Database, Derived>> {
    typedef Global<Database, Derived> self;

public:
    typedef std::function<void(SharedVector<Derived> &&)> GlobalIndexCb;

    void get(Database &db) {}
    void commit(Database &db) {
        using namespace rapidjson;
        Derived &derived = static_cast<Derived &>(*this);
        Document jindex = get_index(db);

        if (m_clear) {
            for (rapidjson::Value::ValueIterator itr = jindex.Begin();
                                         itr != jindex.End(); ++itr) {
                if (itr->GetString() == derived.path().string()) {
                    jindex.Erase(itr);
                    break;
                }
            } 
        } else {
            bool found = false;
            for (rapidjson::Value::ValueIterator itr = jindex.Begin();
                                         itr != jindex.End(); ++itr) {
                if (itr->GetString() == derived.path().string()) {
                    found = true;
                    break;
                }
            } 
            if (!found) {
                Value jindexkey;
                std::string path = derived.path();
                jindexkey.SetString(path.c_str(), jindex.GetAllocator());
                jindex.PushBack(jindexkey, jindex.GetAllocator());
            }
        }

        put_index(db, jindex);
    }
    void clear() {
        m_clear = true;
    }

    static std::shared_ptr<RPC::ClientRequest> get_global_index(std::shared_ptr<
                                RPC::ClientSession> session, GlobalIndexCb cb) {
        using namespace inventory::RPC;
        using namespace inventory::JSONRPC;
        using namespace rapidjson;
        using namespace std;

        unique_ptr<JSONRPC::SingleRequest> jreq = build_index_request();
        auto handler = [cb](unique_ptr<Response> response) -> void {
            const SingleResponse sresp(move(response));
            if (sresp.has_error())
                sresp.throw_ec();

            SharedVector<Derived> index_objs;
            for (auto itr = sresp.result().Begin();
                       itr != sresp.result().End();
                                           itr++) {
                IndexKey objk(itr->GetString());
                auto obj = Shared<Derived>(objk);
                index_objs.push_back(obj);
            }
            cb(move(index_objs));
        };
        return Factory<SingleClientRequest>::create(move(jreq), session,
                                                               handler);
    }

    std::unique_ptr<JSONRPC::SingleRequest> build_update_request(
                          rapidjson::Document::AllocatorType &) {
        return nullptr;
    }

    static const std::vector<RPC::Method<Database, self>> &methods() {
        static const std::vector<RPC::Method<Database, self>> methods({
            RPC::Method<Database, self>("global.index", &self::rpc_index),
        });
        return methods;
    }

    rapidjson::Value rpc_index(Database &db, const RPC::SingleCall &call,
                             rapidjson::Document::AllocatorType &alloc) {
        using namespace rapidjson;

        Document jindex = get_index(db, &alloc);
        Value jindexv;
        jindexv.Swap(jindex);
        return jindexv;
    }

    static const std::string &mixin_type() {
        static const std::string type("global");
        return type;
    }

    rapidjson::Value repr(rapidjson::Document::AllocatorType &alloc) const {
        return rapidjson::Value(rapidjson::kNullType);
    }

    void from_repr(const rapidjson::Value &object) {}

    bool modified() const {
        return false;
    }

    bool db_backed() const {
        return false;
    }

    void set_db_backed(bool state) {}
    void set_modified(bool state) {}

    void on_commit() {
        m_clear = false;
    }
    void on_get() {
        m_clear = false;
    }

private:
    static std::unique_ptr<JSONRPC::SingleRequest> build_index_request() {
        using namespace rapidjson;

        auto jreq = std::make_unique<JSONRPC::SingleRequest>();
        jreq->id(uuid_string());
        jreq->method("object.global.index");
        jreq->params(true);

        Value jtype;
        jtype.SetString(Derived::type().c_str(), jreq->allocator());
        jreq->params().AddMember("type", jtype, jreq->allocator());

        return jreq;
    }

    rapidjson::Document get_index(Database &db, rapidjson::Document::
                                    AllocatorType *alloc = nullptr) {
        using namespace rapidjson;
        Derived &derived = static_cast<Derived &>(*this);

        Document jindex(alloc);
        std::string index_repr;
        if (db.impl().get(derived.type(), &index_repr))
            jindex.Parse(index_repr.c_str()); // TODO check parse error
        else
            jindex.SetArray();
        return jindex;
    }

    void put_index(Database &db, rapidjson::Value &jindex) {
        using namespace rapidjson;
        Derived &derived = static_cast<Derived &>(*this);

        StringBuffer esb;
        PrettyWriter<rapidjson::StringBuffer> ewriter(esb);
        jindex.Accept(ewriter);

        db.impl().set(derived.type(), esb.GetString());
    }

    bool m_clear = false;
};

}

#endif
