#ifndef LIBINV_CONTAINER_HH
#define LIBINV_CONTAINER_HH
#include <string>
#include <map>
#include <vector>
#include <stdexcept>
#include <memory>
#include <kcdb.h>
#include <rapidjson/document.h>
#include "key.hh"
#include "database.hh"
#include "rpc.hh"
#include "exception.hh"

namespace inventory {

template<class Container>
class Attribute {
    friend Container;

    static AttributeKey db_key(std::string container_path, std::string key) {
        return AttributeKey({container_path, key});
    }

public:
    Attribute(std::string key, Container &c)
    : m_key(key), m_container(c) {}

    std::string operator=(std::string value) {
        return m_container.m_attrs[m_key] = value;
    }

    const bool exists() {
        return m_container.m_attrs.find(m_key) != m_container.m_attrs.end();
    }

    operator const char*() {
        if (!exists())
            return "";
        return m_container.m_attrs[m_key].c_str();
    }

    void remove() {
        if (!exists())
            return;
        m_container.m_attrs.erase(m_key);
        m_container.m_delete.push_back(m_key);
    }

protected:
    std::string m_key;
    Container &m_container;
};

template<class Database, class Derived>
class Container : public RPC::MethodRoster<Database,
                     Container<Database, Derived>> {
    typedef Container<Database, Derived> self;

public:
    friend class Attribute<self>;

    typedef std::map<std::string, std::string> AttrMap;
    typedef std::vector<std::string> IdVec;

    // TODO rewrite commit and get for rapidjson obj
    void get(Database &db) {
        Derived &derived = static_cast<Derived &>(*this);

        std::unique_ptr<kyotocabinet::DB::Cursor> cur(db.impl().cursor());
        if (!cur->jump(AttributeKey::prefix(derived.path())))
            return;

        std::string path, value;
        while (cur->get(&path, &value, true)) {
            if (!derived.db_key_match(AttributeKey(path).container_part()))
                break;
//TODO
            try {
                std::string ak = AttributeKey(path).attribute_part();
                m_attrs[ak] = value;
            } catch (std::out_of_range &oo) {}
        }
    }

    void commit(Database &db) {
        Derived *derived = static_cast<Derived *>(this);
        std::string container_path = derived->path();

        for (auto &kv : m_attrs) {
            std::string attribute_path = Attribute<self>::db_key(container_path,
                                                                      kv.first);
            if (!db.impl().set(attribute_path, kv.second))
                throw std::runtime_error("Couldn't set kv (" + attribute_path
                      + "," + kv.second + ")" + db.impl().error().message());
        }

        for (std::string &id : m_delete) {
            std::string attribute_path = Attribute<self>::db_key(container_path,
                                                                            id);
            if (!db.impl().remove(attribute_path))
                throw std::runtime_error("Couldn't remove key");
        }
        m_delete.clear();
    } 

    AttrMap &attributes() {
        return m_attrs;
    }

    Attribute<self> operator[](std::string key) {
        return Attribute<self>(key, *this);
    }

    rapidjson::Value rpc_attribute_list(Database &db, RPC::SingleCall &call,
                                rapidjson::Document::AllocatorType &alloc) {
        // never generate responses to notifications
        if (call.jsonrpc()->is_notification())
            return rapidjson::Value(rapidjson::kNullType);

        get(db);
        return repr(alloc);
    }

    rapidjson::Value rpc_attribute_set(Database &db, RPC::SingleCall &call,
                               rapidjson::Document::AllocatorType &alloc) {
        const char *attrn = RPC::ObjectCallParams(call)["key"].GetString();
        const char *attrv = RPC::ObjectCallParams(call)["value"].GetString();
        (*this)[attrn] = attrv;
        commit(db);

        // never generate responses to notifications
        if (call.jsonrpc()->is_notification())
            return rapidjson::Value(rapidjson::kNullType);
        return rapidjson::Value("OK");
    }

    rapidjson::Value rpc_attribute_get(Database &db, RPC::SingleCall &call,
                               rapidjson::Document::AllocatorType &alloc) {
        // never generate responses to notifications
        if (call.jsonrpc()->is_notification())
            return rapidjson::Value(rapidjson::kNullType);

        // TODO optimize
        get(db);
        return rapidjson::Value(
            (*this)[RPC::ObjectCallParams(call)["key"].GetString()],
            alloc
        );
    }

    static const std::vector<RPC::Method<Database, self>> &methods() {
        static const std::vector<RPC::Method<Database, self>> ret({
            RPC::Method<Database, self>("attribute.list", &self::rpc_attribute_list),
            RPC::Method<Database, self>("attribute.get", &self::rpc_attribute_set),
            RPC::Method<Database, self>("attribute.set", &self::rpc_attribute_get),
            RPC::Method<Database, self>("attribute.repr.get", &self::rpc_repr_get),
        });
        return ret;
    }

    static const std::string &mixin_type() {
        static const std::string type("kv");
        return type;
    }

    rapidjson::Value rpc_repr_get(Database &db, RPC::SingleCall &call,
                      rapidjson::Document::AllocatorType &alloc) {
        return repr(alloc);
    }

    rapidjson::Value repr(rapidjson::Document::AllocatorType &alloc) const {
        rapidjson::Value rarr(rapidjson::kObjectType);
        repr(rarr, alloc);
        return rarr;
    }

    rapidjson::Document repr() const {
        rapidjson::Document doc(rapidjson::kObjectType);
        repr(doc, doc.GetAllocator());
        return doc;
    }

    void from_repr(const rapidjson::Value &object) {
        attribute_set_batch(object);
    }

private:
    void attribute_set_batch(const rapidjson::Value &obj) {
        if (!obj.IsObject())
            throw exceptions::InvalidRepr("kv dict is not an object");
       
        for (rapidjson::Value::ConstMemberIterator it = obj.MemberBegin();
                                            it != obj.MemberEnd(); it++) { 
            if (!it->name.IsString())
                throw exceptions::InvalidRepr("key is not a string");
            if (!it->value.IsString())
                throw exceptions::InvalidRepr("value is not a string");
            (*this)[it->name.GetString()] = it->value.GetString();
        }
    }

    void repr(rapidjson::Value &robj, rapidjson::Document::AllocatorType
                                                         &alloc) const {
        for (auto &kv : m_attrs) {
            rapidjson::Value attrn(kv.first.c_str(), alloc);
            rapidjson::Value attrv(kv.second.c_str(), alloc);
            robj.AddMember(attrn, attrv, alloc);
        }
    }

    AttrMap m_attrs;
    IdVec m_delete;
};

}

#endif
