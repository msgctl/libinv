#ifndef LIBINV_OBJECT_HH
#define LIBINV_OBJECT_HH
#include <string>
#include <memory>
#include "index.hh"
#include "database.hh"
#include "index.hh"
#include "rpc.hh"
#include "datamodel.hh"
#include "exception.hh"
#include "jsonrpc.hh"

namespace inventory {
namespace exceptions {
    class NoSuchObject : public ExceptionBase {
    public:
        NoSuchObject(const std::string &type, const std::string &id)
        : ExceptionBase("No object of type " + type + " and id " + id + ".") {}

        virtual JSONRPC::ErrorCode ec() const {
            return JSONRPC::ErrorCode::INVALID_REQUEST;
        }
    };
}

template<class Database, class Derived>
class NullMixin : public RPC::MethodRoster<Database, NullMixin<Database,
                                                             Derived>> {
    typedef NullMixin<Database, Derived> self;

public:
    void get(Database &db) {}
    void commit(Database &db) {}

    static const std::vector<RPC::Method<Database, self>> &methods() {
        static const std::vector<RPC::Method<Database, self>> methods({
        });
        return methods;
    }

    static const std::string &mixin_type() {
        static const std::string type("null");
        return type;
    }

    rapidjson::Value repr(rapidjson::Document::AllocatorType &alloc) const {
        return rapidjson::Value(rapidjson::kNullType);
    }

    void from_repr(const rapidjson::Value &object) {}
};

template<class Database, template<class, class> class IndexType, class Derived,
                                        template<class, class> class ...Mixins>
class Object : public IndexType<Database, Derived>,
               public Mixins<Database, Derived>...,
               public NullMixin<Database, Derived>,
               public DatamodelObject<Database>,
               private RPC::MethodRoster<Database, Derived> {
    typedef Object<Database, IndexType, Derived, Mixins...> self;
    friend class RPC::MethodRoster<Database, Derived>;

    template<
        template<class, class> class T_ = NullMixin,
        template<class, class> class ...Mixins_>
    class Foreach {
    public:
        static void get(self &object, Database &db) {
            object.T_<Database, Derived>::get(db);
            if (sizeof...(Mixins_))
                Foreach<Mixins_...>::get(object, db);
        }

        static void commit(self &object, Database &db) {
            object.T_<Database, Derived>::commit(db);
            if (sizeof...(Mixins_))
                Foreach<Mixins_...>::commit(object, db);
        }

        static void rpc_method_list(std::vector<std::string> &ret) {
            RPC::MethodRoster<Database, T_<Database, Derived>>::rpc_method_list(ret);
            if (sizeof...(Mixins_))
                Foreach<Mixins_...>::rpc_method_list(ret);
        }

        static rapidjson::Value rpc_call(self &object, Database &db,
                                        const RPC::SingleCall &call,
                        rapidjson::Document::AllocatorType &alloc) {
            try {
                return object.RPC::MethodRoster<Database, T_<Database,
                                 Derived>>::rpc_call(db, call, alloc);
            } catch (RPC::exceptions::NoSuchMethod &e) {}

            if (sizeof...(Mixins_)) {
                return Foreach<Mixins_...>::rpc_call(object, db, call, alloc);
            } else {
                throw RPC::exceptions::NoSuchMethod(call.jsonrpc()->method());
            }
        }

        static void repr(const self &object, rapidjson::Value &obj_repr,
                            rapidjson::Document::AllocatorType &alloc) {
            rapidjson::Value mixin_repr = object.T_<Database,
                                       Derived>::repr(alloc);
            obj_repr.AddMember(
                rapidjson::StringRef(object.T_<Database,
                        Derived>::mixin_type().c_str()),
                mixin_repr,
                alloc
            );
            if (sizeof...(Mixins_))
                Foreach<Mixins_...>::repr(object, obj_repr, alloc);
        }

        static void from_repr(self &object, const rapidjson::Value &obj_repr) {
            if (!obj_repr.IsObject())
                throw exceptions::InvalidRepr("repr is not a JSON object");

            const std::string &mixin_type =
                T_<Database, Derived>::mixin_type();
            rapidjson::Value::ConstMemberIterator mixin_repr =
                      obj_repr.FindMember(mixin_type.c_str());
            if (mixin_repr == obj_repr.MemberEnd()) {
                throw exceptions::InvalidRepr("no \"" + mixin_type + "\" "
                                                 "member in repr object");
            }
            object.T_<Database, Derived>::from_repr(mixin_repr->value);
            if (sizeof...(Mixins_))
                Foreach<Mixins_...>::from_repr(object, obj_repr);
        }

        static void mixin_list(std::vector<std::string> &ret) {
            ret.push_back(T_<Database, Derived>::mixin_type());
            if (sizeof...(Mixins_))
                Foreach<Mixins_...>::mixin_list(ret);
        }
    };

public:
    Object() {}
    Object(const std::string &id) {
        self::assign_id(id);
    }
    Object(Database &db)
    : IndexType<Database, Derived>(db) {}

    void get(Database &db, std::string id) {
        this->IndexType<Database, Derived>::get(db, id);
        get(db);
    }

    void get(Database &db) {
        Foreach<Mixins...>::get(*this, db);
    }

    void commit(Database &db) {
        // TODO
        this->IndexType<Database, Derived>::commit(db);
        Foreach<Mixins...>::commit(*this, db);
    }

    // TODO write remove

    static std::vector<std::string> rpc_methods() {
        std::vector<std::string> ret;
        for (const RPC::Method<Database, Derived> &m : methods())
            ret.push_back(m.name());
        Foreach<Mixins...>::rpc_method_list(ret);
        return ret;
    }

    static std::vector<std::string> mixin_list() {
        std::vector<std::string> ret;
        ret.push_back(Derived::type());
        Foreach<Mixins...>::mixin_list(ret);
        return ret;
    }

    // For RPC string-type mapping, see: DatamodelObject
    virtual std::vector<std::string> virtual_rpc_methods() const {
        return rpc_methods();
    }

    rapidjson::Value rpc_call(Database &db, const RPC::SingleCall &call,
                            rapidjson::Document::AllocatorType &alloc) {
        // set the index id if specified in the request or else a new id will
        // be generated automatically
        if (!RPC::ObjectCallParams(call).id().empty()) {
            this->IndexType<Database, Derived>::get(db,
                     RPC::ObjectCallParams(call).id());
            
            if (!this->IndexType<Database, Derived>::exists(db)) {
                throw exceptions::NoSuchObject(
                    this->IndexType<Database, Derived>::type(),
                    this->IndexType<Database, Derived>::id()
                );
            }
        }

        try {
            return RPC::MethodRoster<Database, Derived>::rpc_call(db,
                                                        call, alloc);
        } catch (RPC::exceptions::NoSuchMethod &e) {}

        return Foreach<Mixins...>::rpc_call(*this, db, call, alloc);
    }

    rapidjson::Value rpc_repr_create(Database &db, const RPC::SingleCall &call,
                                   rapidjson::Document::AllocatorType &alloc) {
        const rapidjson::Value &jrepr = RPC::ObjectCallParams(call)["repr"];
        from_repr(jrepr);
        commit(db);
        return rapidjson::Value("OK");
    }

    rapidjson::Value rpc_repr_get(Database &db, const RPC::SingleCall &call,
                                rapidjson::Document::AllocatorType &alloc) {
        get(db);
        return repr(alloc);
    }

    rapidjson::Value repr(rapidjson::Document::AllocatorType &alloc) const {
        rapidjson::Value obj_repr(rapidjson::kObjectType);
        repr(obj_repr, alloc);
        return obj_repr;
    }

    rapidjson::Document repr() const {
        rapidjson::Document obj_repr(rapidjson::kObjectType);
        repr(obj_repr, obj_repr.GetAllocator());
        return obj_repr;
    }

    std::string repr_string() const {
        rapidjson::Document repr_obj = repr();
        rapidjson::StringBuffer esb;
        rapidjson::PrettyWriter<rapidjson::StringBuffer> ewriter(esb);
        repr_obj.Accept(ewriter);
        return esb.GetString();
    }

    void from_repr(const rapidjson::Value &obj_repr) {
        if (!obj_repr.IsObject())
            throw exceptions::InvalidRepr("repr is not a JSON object");

        rapidjson::Value::ConstMemberIterator id = obj_repr.FindMember("id"); 
        if (id == obj_repr.MemberEnd())
            throw exceptions::InvalidRepr("repr lacks \"id\" member");
        if (!id->value.IsString())
            throw exceptions::InvalidRepr("id is not a string");
        self::assign_id(id->value.GetString()); 

        rapidjson::Value::ConstMemberIterator type = obj_repr.FindMember("type");
        if (type == obj_repr.MemberEnd())
            throw exceptions::InvalidRepr("repr lacks \"type\" member");
        if (!type->value.IsString())
            throw exceptions::InvalidRepr("type is not a string");
        if (type->value.GetString() != Derived::type()) {
            throw exceptions::InvalidRepr("tried to initialize "
                           + Derived::type() + " instance with "
                           + type->value.GetString() + " repr");
        }

        Foreach<Mixins...>::from_repr(*this, obj_repr);
    }

    std::string virtual_type() const {
        return Derived::type();
    }

    static const std::vector<RPC::Method<Database, Derived>> &methods() {
        static const std::vector<RPC::Method<Database, Derived>> ret({
            RPC::Method<Database, Derived>("repr.get", &self::rpc_repr_get),
            RPC::Method<Database, Derived>("repr.create", &self::rpc_repr_create),
        });
        return ret;
    }

    static const std::string &mixin_type() {
        return Derived::type();
    }

private:
    void repr(rapidjson::Value &robj, rapidjson::Document::AllocatorType
                                                         &alloc) const {
        rapidjson::Value id;
        const std::string &sid = this->IndexType<Database, Derived>::id();
        id.SetString(sid.c_str(), sid.length(), alloc);
        robj.AddMember("id", id, alloc);

        rapidjson::Value type;
        type.SetString(Derived::type().c_str(), Derived::type().length(),
                                                                  alloc);
        robj.AddMember("type", type, alloc);

        Foreach<Mixins...>::repr(*this, robj, alloc);
    }
};

}

#endif
