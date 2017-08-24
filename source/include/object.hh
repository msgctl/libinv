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
#include "factory.hh"
#include "object_ex.hh"

namespace inventory {

template<class Database, class Derived>
class NullMixin : public RPC::MethodRoster<Database, NullMixin<Database,
                                                             Derived>> {
    typedef NullMixin<Database, Derived> self;

public:
    void get(Database &db) {}
    void commit(Database &db) {}
    void clear() {}

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
               private RPC::MethodRoster<Database, Derived>,
               public std::enable_shared_from_this<Object<Database,
                                  IndexType, Derived, Mixins...>> {
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

        static void clear(self &object) {
            object.T_<Database, Derived>::clear();
            if (sizeof...(Mixins_))
                Foreach<Mixins_...>::clear(object);
        }
    };

public:
    Object() {}
    Object(std::string id) {
        self::assign_id(id);
    }
    Object(Database &db)
    : IndexType<Database, Derived>(db) {}

    virtual ~Object() {}

    void get(Database &db, std::string id) {
        this->IndexType<Database, Derived>::get(db, id);
        get(db);
    }

    void get(Database &db) {
        Foreach<Mixins...>::get(*this, db);
    }

    void clear() {
        Foreach<Mixins...>::clear(*this);
    }

    void remove(Database &db) {
        clear();
        IndexType<Database, Derived>::remove(db);
    }

    void get(std::shared_ptr<RPC::ClientSession> session, std::string id) {
        std::shared_ptr<RPC::ClientRequest> req_handle = get_async(session, id);
        req_handle->complete();
    }

    void get(std::shared_ptr<RPC::ClientSession> session) {
        get(session, self::id());
    }

    std::shared_ptr<RPC::ClientRequest> get_async(std::shared_ptr<
                    RPC::ClientSession> session, std::string id) {
        using namespace RPC;
        using namespace JSONRPC;

        std::unique_ptr<SingleRequest> jgetreq = build_get_request(id);
        // called (asynchronously) only if object haven't been destroyed
        // in the meantime
        auto handler = wrap_refcount_check(
            [this](std::unique_ptr<Response> response) -> void {
                const SingleResponse sresp(std::move(response));
                if (sresp.has_error())
                    sresp.throw_ec();
                else
                    from_repr(sresp.result());
            }
        );
        return Factory<ClientRequest>::create(std::move(jgetreq), session,
                                                                 handler);
    }

    std::shared_ptr<RPC::ClientRequest> get_async(std::shared_ptr<
                                    RPC::ClientSession> session) {
        return get_async(session, self::id());
    }

    void commit(Database &db) {
        // TODO
        this->IndexType<Database, Derived>::commit(db);
        Foreach<Mixins...>::commit(*this, db);
    }

    void commit(std::shared_ptr<RPC::ClientSession> session) {
        std::shared_ptr<RPC::ClientRequest> req_handle = commit_async(session);
        req_handle->complete();
    }

    std::shared_ptr<RPC::ClientRequest> commit_async(std::shared_ptr<RPC::ClientSession>
                                                                              session) {
        // TODO foreach commit w/ session, coalesce requests
        std::unique_ptr<JSONRPC::SingleRequest> jsetreq = build_create_request();
        return Factory<RPC::ClientRequest>::create(std::move(jsetreq), session);
    }

    static std::vector<std::string> rpc_methods() {
        std::vector<std::string> ret;
        // Derived classes can redefine methods()
        for (const RPC::Method<Database, Derived> &m : Derived::methods())
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
        // set the index id if specified in the request or else a new id (and
        // object) will be generated automatically 
        if (!RPC::ObjectCallParams(call).id().empty()) {
            this->IndexType<Database, Derived>::get(db,
                     RPC::ObjectCallParams(call).id());
            
            if (!this->IndexType<Database, Derived>::exists(db)) {
                throw exceptions::NoSuchObject(
                    this->IndexType<Database, Derived>::type(),
                    this->IndexType<Database, Derived>::id()
                );
            }
        } else {
            // create a new object
            commit(db);
        }

        try {
            // for RPC implementation in Derived classes
            Derived &derived = static_cast<Derived &>(*this); 
            return derived.RPC::MethodRoster<Database, Derived>::rpc_call(db,
                                                                call, alloc);
        } catch (RPC::exceptions::NoSuchMethod &e) {}

        return Foreach<Mixins...>::rpc_call(*this, db, call, alloc);
    }

    rapidjson::Value rpc_create(Database &db, const RPC::SingleCall &call,
                              rapidjson::Document::AllocatorType &alloc) {
        const rapidjson::Value &jrepr = RPC::ObjectCallParams(call)["repr"];
        from_repr(jrepr);
        remove_existing(db); 
        // no need to check for preexisting objects here
        commit(db);
        return rapidjson::Value("OK");
    }

    rapidjson::Value rpc_remove(Database &db, const RPC::SingleCall &call,
                              rapidjson::Document::AllocatorType &alloc) {
        const rapidjson::Value &jrepr = RPC::ObjectCallParams(call)["repr"];
        get(db);
        remove(db);
        return rapidjson::Value("OK");
    }

    rapidjson::Value rpc_clear(Database &db, const RPC::SingleCall &call,
                              rapidjson::Document::AllocatorType &alloc) {
        const rapidjson::Value &jrepr = RPC::ObjectCallParams(call)["repr"];
        get(db);
        clear();
        commit(db);
        return rapidjson::Value("OK");
    }

    rapidjson::Value rpc_get(Database &db, const RPC::SingleCall &call,
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

        self::assign_id(id_from_repr(obj_repr)); // throws

        // checks type
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

        clear();
        Foreach<Mixins...>::from_repr(*this, obj_repr);
    }

    std::string virtual_type() const {
        return Derived::type();
    }

    static const std::vector<RPC::Method<Database, Derived>> &methods() {
        static const std::vector<RPC::Method<Database, Derived>> ret({
            RPC::Method<Database, Derived>("repr.get", &self::rpc_get),
            RPC::Method<Database, Derived>("repr.create", &self::rpc_create),
            RPC::Method<Database, Derived>("remove", &self::rpc_remove),
            RPC::Method<Database, Derived>("clear", &self::rpc_clear),
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

    std::string id_from_repr(const rapidjson::Value &obj_repr) {
        rapidjson::Value::ConstMemberIterator id = obj_repr.FindMember("id"); 
        if (id == obj_repr.MemberEnd())
            throw exceptions::InvalidRepr("repr lacks \"id\" member");
        if (!id->value.IsString())
            throw exceptions::InvalidRepr("id is not a string");
        return id->value.GetString();
    }

    // removes any previous information about this object from the database
    void remove_existing(Database &db) {
        self ex_obj(self::id());
        ex_obj.get(db);
        ex_obj.remove(db);
    }

    std::unique_ptr<JSONRPC::SingleRequest> build_get_request(std::string id) {
        // TODO better
        auto jreq = std::make_unique<JSONRPC::SingleRequest>();
        jreq->id(self::id() + ":" + uuid_string());
        jreq->method("object.repr.get");
        jreq->params(true);

        using namespace rapidjson;
        Value jid;
        jid.SetString(id.c_str(), jreq->allocator());
        jreq->params().AddMember("id", jid, jreq->allocator());

        Value jtype;
        jtype.SetString(self::type().c_str(), jreq->allocator());
        jreq->params().AddMember("type", jtype, jreq->allocator());

        return jreq;
    }

    std::unique_ptr<JSONRPC::SingleRequest> build_create_request() {
        // TODO better
        auto jreq = std::make_unique<JSONRPC::SingleRequest>();

        std::string req_id = self::id() + ":" + uuid_string();
        jreq->id(req_id);
        jreq->method("object.repr.create");
        jreq->params(true);

        using namespace rapidjson;
        Value jtype;
        jtype.SetString(self::type().c_str(), jreq->allocator());
        jreq->params().AddMember("type", jtype, jreq->allocator());

        Value jrepr = repr(jreq->allocator());
        jreq->params().AddMember("repr", jrepr, jreq->allocator());

        return jreq;
    }

    std::unique_ptr<JSONRPC::SingleRequest> build_clear_request() {
        auto jreq = std::make_unique<JSONRPC::SingleRequest>();
        jreq->id(self::id() + ":" + uuid_string());
        jreq->method("object.clear");
        jreq->params(true);

        using namespace rapidjson;
        Value jid;
        jid.SetString(self::id().c_str(), jreq->allocator());
        jreq->params().AddMember("id", jid, jreq->allocator());

        Value jtype;
        jtype.SetString(self::type().c_str(), jreq->allocator());
        jreq->params().AddMember("type", jtype, jreq->allocator());

        return jreq;
    }

    std::unique_ptr<JSONRPC::SingleRequest> build_remove_request() {
        auto jreq = std::make_unique<JSONRPC::SingleRequest>();
        jreq->id(self::id() + ":" + uuid_string());
        jreq->method("object.remove");
        jreq->params(true);

        using namespace rapidjson;
        Value jid;
        jid.SetString(self::id().c_str(), jreq->allocator());
        jreq->params().AddMember("id", jid, jreq->allocator());

        Value jtype;
        jtype.SetString(self::type().c_str(), jreq->allocator());
        jreq->params().AddMember("type", jtype, jreq->allocator());

        return jreq;
    }

    RPC::ClientRequest::ResponseHandler wrap_refcount_check(
                  RPC::ClientRequest::ResponseHandler hnd) {
        std::weak_ptr<self> this_weakptr = self::shared_from_this();
        return [this_weakptr, hnd](std::unique_ptr<JSONRPC::Response>
                                                  response) -> void {
            auto sp = this_weakptr.lock();
            if (sp)
                hnd(std::move(response));
        };
    }
};

}

#endif
