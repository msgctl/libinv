#ifndef LIBINV_ASSOCIATION_HH
#define LIBINV_ASSOCIATION_HH
#include <set>
#include <vector>
#include <string>
#include <stdexcept>
#include <algorithm>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <rapidjson/document.h>
#include "key.hh"
#include "database.hh"
#include "rpc.hh"
#include "exception.hh"
#include "jsonrpc.hh"
#include "uuid.hh"

namespace inventory {

extern std::shared_mutex g_association_rwlock;
template<class Database, class Derived>
class Association : public RPC::MethodRoster<Database,
                     Association<Database, Derived>> {
    typedef Association<Database, Derived> self;

public:
    template<class AssocObject>
    void operator*=(AssocObject &object) {
        associate(object);
    }

    template<class AssocObject>
    void operator/=(AssocObject &object) {
        disassociate(object);
    }

    template<class AssocObject>
    void associate(AssocObject &object) {
        Derived &derived = static_cast<Derived &>(*this);        

        associate(object.path());
        object.associate(derived.path());
    }

    template<class T>
    void associate(std::shared_ptr<T> aoptr) {
        this->associate(*aoptr);
    }

    void associate(const IndexKey &key) {
        m_add.insert(key);
        m_remove.erase(key);
        m_assoc.insert(key);
        m_modified = true;
    }

    template<class AssocObject>
    void disassociate(AssocObject &object) {
        Derived &derived = static_cast<Derived &>(*this);        

        disassociate(object.path());
        object.disassociate(derived.path());
    }

    template<class T>
    void disassociate(std::shared_ptr<T> aoptr) {
        this->disassociate(*aoptr);
    }

    void disassociate(const IndexKey &key) {
        m_remove.insert(key);
        m_add.erase(key);
        m_assoc.erase(key);
        m_modified = true;
    }

    void get(Database &db) {
        std::shared_lock<std::shared_mutex> lock(g_association_rwlock);
        Derived &derived = static_cast<Derived &>(*this);

        std::unique_ptr<kyotocabinet::DB::Cursor> cur(db.impl().cursor());
        if (!cur->jump(LinkKey::prefix(derived.path())))
            return;

        std::string path, value;
        while (cur->get(&path, &value, true)) {
            LinkKey lkey(path);
            if (!lkey.good() || lkey.local_part() != derived.path())
                break;

            m_assoc.insert(lkey.remote_part());
        }

        m_from_db = true;
    }

    void commit(Database &db) {
        std::unique_lock<std::shared_mutex> lock(g_association_rwlock);
        Derived &derived = static_cast<Derived &>(*this);

        for (const IndexKey &p : m_remove) {
            LinkKey link({derived.path(), p.string()});
            if (!db.impl().remove(link))
                throw std::runtime_error("Couldn't remove keys");
            if (!db.impl().remove(link.inverted()))
                throw std::runtime_error("Couldn't remove keys");
        }
        m_remove.clear();

        for (const IndexKey &p : m_add) {
            LinkKey link({derived.path(), p.string()});
            if (!db.impl().set(link, ""))
                throw std::runtime_error("Couldn't set kv");
            if (!db.impl().set(link.inverted(), ""))
                throw std::runtime_error("Couldn't set kv");
        }
        m_add.clear();

        m_from_db = true;
        m_modified = false;
    }

    std::unique_ptr<JSONRPC::SingleRequest> build_update_request(
                     rapidjson::Document::AllocatorType &alloc) {
        if (!modified())
            return nullptr;

        Derived &derived = static_cast<Derived &>(*this);        
        auto jreq = std::make_unique<JSONRPC::SingleRequest>(&alloc);
        jreq->id(derived.id() + ":" + uuid_string());
        jreq->method("link.update");
        jreq->params(true);

        using namespace rapidjson;
        Value jadd(kArrayType);
        for (const IndexKey &key : m_add) {
            Value jkey;
            // TODO zerocopy
            jkey.SetString(key.string().c_str(), jreq->allocator());
            jadd.PushBack(jkey, jreq->allocator());
        }
        jreq->params().AddMember("add", jadd, jreq->allocator());

        Value jremove(kArrayType);
        for (const IndexKey &key : m_remove) {
            Value jkey;
            jkey.SetString(key.string().c_str(), jreq->allocator());
            jremove.PushBack(jkey, jreq->allocator());
        }
        jreq->params().AddMember("remove", jremove, jreq->allocator());

        return jreq;
    }

    template<class AssocObject>
    std::vector<IndexKey> get_assoc_ids() {
        std::vector<IndexKey> result;
        std::copy_if(m_assoc.begin(), m_assoc.end(),
            std::back_inserter(result), [](const IndexKey &k) {
                return k.type_part() == AssocObject::type();
            });
        return result;
    }

    template<class AssocObject>
    std::vector<AssocObject> get_assoc_objects(Database &db) {
        std::vector<AssocObject> result;
        std::vector<IndexKey> assoc_ids = get_assoc_ids<AssocObject>();
        for (IndexKey &key : assoc_ids) {
            AssocObject obj;
            obj.get(db, key.id_part());
            result.push_back(obj);
        }
        return result;
    }

    static const std::vector<RPC::Method<Database, self>> &methods() {
        static const std::vector<RPC::Method<Database, self>> ret({
            RPC::Method<Database, self>("link.update", &self::rpc_update),
        });
        return ret;
    }

    rapidjson::Value rpc_update(Database &db, const RPC::SingleCall &call,
                              rapidjson::Document::AllocatorType &alloc) {
        assoc_remove_batch(RPC::ObjectCallParams(call)["remove"]);
        assoc_set_batch(RPC::ObjectCallParams(call)["add"]);
        commit(db);

        // TODO better
        if (call.jsonrpc()->is_notification())
            return rapidjson::Value(rapidjson::kNullType);
        return rapidjson::Value("OK");
    }

    static const std::string &mixin_type() {
        static const std::string type("associative");
        return type;
    }

    rapidjson::Value repr(rapidjson::Document::AllocatorType &alloc) const {
        rapidjson::Value rarr(rapidjson::kArrayType);
        repr(rarr, alloc);
        return rarr;
    }

    rapidjson::Document repr() const {
        rapidjson::Document rdoc(rapidjson::kArrayType);
        repr(rdoc, rdoc.GetAllocator());
        return rdoc;
    }

    void from_repr(const rapidjson::Value &array) {
        clear();
        assoc_set_batch(array);
    }

    void clear() {
        for (const IndexKey &key : m_assoc)
            m_remove.insert(key);
        m_assoc.clear();
        m_add.clear();

        m_modified = true;
    }

    bool modified() const {
        return m_modified;
    }

    bool from_db() const {
        return m_from_db;
    }

    void set_from_db(bool state) {
        m_from_db = state;
    }

    void set_modified(bool state) {
        m_modified = state;
    }

private:
    void assoc_set_single(const rapidjson::Value &object) {
        if (!object.IsString())
            throw exceptions::InvalidRepr("array member is not a string");
        associate(IndexKey(object.GetString()));
    }

    void assoc_remove_single(const rapidjson::Value &object) {
        if (!object.IsString())
            throw exceptions::InvalidRepr("array member is not a string");
        disassociate(IndexKey(object.GetString()));
    }

    void assoc_set_batch(const rapidjson::Value &array) {
        if (!array.IsArray())
            throw exceptions::InvalidRepr("value is not an array");
        for (auto &v : array.GetArray())
            assoc_set_single(v);
    }

    void assoc_remove_batch(const rapidjson::Value &array) {
        if (!array.IsArray())
            throw exceptions::InvalidRepr("value is not an array");
        for (auto &v : array.GetArray())
            assoc_remove_single(v);
    }

    void repr(rapidjson::Value &rarr, rapidjson::Document::AllocatorType
                                                         &alloc) const {
        for (auto &ikey : m_assoc) {
            rapidjson::Value jikey;
            std::string sikey(ikey);
            jikey.SetString(sikey.c_str(), sikey.length(), alloc);
            rarr.PushBack(jikey, alloc);
        }
    }

    std::set<IndexKey> m_assoc;
    std::set<IndexKey> m_add;
    std::set<IndexKey> m_remove;
    bool m_modified = false;
    bool m_from_db = false;
};

}

#endif
