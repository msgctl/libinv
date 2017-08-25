#ifndef LIBINV_HIERARCHICAL_HH
#define LIBINV_HIERARCHICAL_HH
#include <set>
#include <vector>
#include <memory>
#include <string>
#include <mutex>
#include <shared_mutex>
#include <kcdb.h>
#include <rapidjson/document.h>
#include "key.hh"
#include "database.hh"
#include "rpc.hh"
#include "exception.hh"

namespace inventory {

extern std::shared_mutex g_hierarchical_rwlock;
template<class Database, class Derived>
class Hierarchical : public RPC::MethodRoster<Database,
                     Hierarchical<Database, Derived>> {
    typedef Hierarchical<Database, Derived> self;

public:
    template<class AssocObject>
    void operator+=(AssocObject &object) {
        Derived &derived = static_cast<Derived &>(*this);

        *this += object.path();

        if (object.m_up_id) {
            HierarchyDownKey dkey({object.m_up_id, object.path()});
            m_remove_dkeys.insert(dkey);
        }
        object.m_up_id = derived.path();
    }

    template<class AssocObject>
    void insert(AssocObject &object) {
        *this += object;
    }

    template<class T>
    void insert(std::shared_ptr<T> aoptr) {
        *this += *aoptr;
    }

    void operator+=(const IndexKey &key) {
        m_down_ids.insert(key);
        m_add_down_ids.insert(key);
        m_remove_down_ids.erase(key);
    }

    template<class AssocObject>
    void operator-=(AssocObject &object) {
        *this -= object.path();

        // TODO check integrity
        object.m_up_id.clear();
    }

    template<class AssocObject>
    void remove(AssocObject &object) {
        *this -= object;
    }

    template<class T>
    void remove(std::shared_ptr<T> aoptr) {
        *this -= *aoptr;
    }

    void operator-=(const IndexKey &key) {
        m_down_ids.erase(key);
        m_add_down_ids.erase(key);
        m_remove_down_ids.insert(key);
    }

    void get(Database &db) {
        std::shared_lock<std::shared_mutex> lock(g_hierarchical_rwlock);
        Derived &derived = static_cast<Derived &>(*this);

        HierarchyUpKey upkey(derived.path());
        std::string up_id;
        db.impl().get(upkey, &up_id);
        m_up_id.from_string(up_id);

        std::unique_ptr<kyotocabinet::DB::Cursor> cur(db.impl().cursor());
        if (!cur->jump(HierarchyDownKey::prefix(derived.path())))
            return;

        std::string path, value;
        while (cur->get(&path, &value, true)) {
            HierarchyDownKey dkey(path);
            if (!dkey.good() || dkey.local_part() != derived.path())
                break;

            m_down_ids.insert(dkey.remote_part());
        }
    }

    void commit(Database &db) {
        std::unique_lock<std::shared_mutex> lock(g_hierarchical_rwlock);
        Derived &derived = static_cast<Derived &>(*this);

        HierarchyUpKey upkey(derived.path());
        if (m_up_id) {
            db.impl().set(upkey, m_up_id);
        } else {
            db.impl().remove(upkey);
        }

        for (const IndexKey &p : m_add_down_ids) {
            HierarchyDownKey dkey({derived.path(), p.string()});
            HierarchyUpKey ukey(p.string());

            db.impl().set(ukey, derived.path());
            db.impl().set(dkey, "");
        }
        m_add_down_ids.clear();

        for (const IndexKey &p : m_remove_down_ids) {
            HierarchyDownKey dkey({derived.path(), p.string()});
            HierarchyUpKey ukey(p.string());

            if (!db.impl().remove(dkey))
                throw std::runtime_error("Couldn't remove keys");
            if (!db.impl().remove(ukey))
                throw std::runtime_error("Couldn't remove keys");
        }
        m_remove_down_ids.clear();

        for (const HierarchyDownKey &dkey : m_remove_dkeys) {
             if (!db.impl().remove(dkey))
                throw std::runtime_error("Couldn't remove keys");           
        }
        m_remove_dkeys.clear();
    }

    void set_up_id(const IndexKey &key) {
        m_up_id = key;
    }

    std::string up_id() {
        return m_up_id;
    }

    void clear_up() {
        m_up_id.clear();
    }

    void clear_down() {
        m_remove_down_ids = m_down_ids;
        m_down_ids.clear();
    }

    Derived up(Database &db) {
        Derived obj;
        obj.get(db, m_up_id.id_part());
        return obj;
    }

    std::set<IndexKey> down_ids() {
        return m_down_ids;
    }

    std::vector<Derived> down(Database &db) {
        std::vector<Derived> result;
        std::set<IndexKey> dids = down_ids();
        for (const IndexKey &key : dids) {
            Derived obj(db);
            obj.get(db, key.id_part());
            result.push_back(obj);
        }
        return result;
    }

    static const std::vector<RPC::Method<Database, self>> &methods() {
        static const std::vector<RPC::Method<Database, self>> ret({
        });
        return ret;
    }

    static const std::string &mixin_type() {
        static const std::string type("hierarchical");
        return type;
    }

    void from_repr(const rapidjson::Value &object) {
        if (!object.IsObject())
            throw exceptions::InvalidRepr("repr is not a json object");

        rapidjson::Value::ConstMemberIterator jup_id =
                           object.FindMember("up_id");
        if (jup_id != object.MemberEnd()) {
            if (!jup_id->value.IsString())
                throw exceptions::InvalidRepr("up_id is not a string");
            m_up_id.from_string(jup_id->value.GetString());
        }

        rapidjson::Value::ConstMemberIterator jdown_ids =
                           object.FindMember("down_ids");
        if (jdown_ids != object.MemberEnd()) {
            if (!jdown_ids->value.IsArray())
                throw exceptions::InvalidRepr("down_ids is not an array");
            for (auto &v : jdown_ids->value.GetArray()) {
                if (!v.IsString()) {
                    throw exceptions::InvalidRepr("down_ids member is not a "
                                                                   "string");
                }
                *this += IndexKey(v.GetString());
            }
        }
    }

    rapidjson::Value repr(rapidjson::Document::AllocatorType &alloc) const {
        rapidjson::Value rarr(rapidjson::kObjectType);
        repr(rarr, alloc);
        return rarr;
    }

    rapidjson::Document repr() const {
        rapidjson::Document rdoc(rapidjson::kObjectType);
        repr(rdoc, rdoc.GetAllocator());
        return rdoc;
    }

    void clear() {
        clear_up();
        clear_down();
    }

private:
    void repr(rapidjson::Value &robj, rapidjson::Document::AllocatorType
                                                         &alloc) const {
        if (m_up_id) {
            rapidjson::Value jup_id;
            std::string sup_id(m_up_id);
            jup_id.SetString(sup_id.c_str(), sup_id.length(), alloc);
            robj.AddMember("up_id", jup_id, alloc);
        }

        if (!m_down_ids.empty()) {
            rapidjson::Value jdown_ids(rapidjson::kArrayType);
            for (const IndexKey &key : m_down_ids) {
                rapidjson::Value jdown_id;
                std::string sdown_id(key);
                jdown_id.SetString(sdown_id.c_str(), sdown_id.length(), alloc);
                jdown_ids.PushBack(jdown_id, alloc);
            }
            robj.AddMember("down_ids", jdown_ids, alloc);
        }
    }

    IndexKey m_up_id;
    std::set<IndexKey> m_down_ids;
    std::set<IndexKey> m_add_down_ids;
    std::set<IndexKey> m_remove_down_ids;
    std::set<HierarchyDownKey> m_remove_dkeys;
};

}

#endif
