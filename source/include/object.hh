#ifndef LIBINV_OBJECT_HH
#define LIBINV_OBJECT_HH
#include <string>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <functional>
#include "index.hh"
#include "database.hh"
#include "index.hh"
#include "rpc.hh"
#include "datamodel.hh"
#include "exception.hh"
#include "jsonrpc.hh"
#include "factory.hh"
#include "object_ex.hh"
#include "mode.hh"

namespace inventory {

template<class Database, class Derived>
class NullMixin : public RPC::MethodRoster<Database, NullMixin<Database,
                                                             Derived>> {
    typedef NullMixin<Database, Derived> self;

public:
    void get(Database &db) {}
    void commit(Database &db) {}
    void clear() {}

    std::unique_ptr<JSONRPC::SingleRequest> build_update_request(
                          rapidjson::Document::AllocatorType &) {
        return nullptr;
    }

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

    bool modified() const {
        return false;
    }

    bool db_backed() const {
        return false;
    }

    void set_db_backed(bool state) {}
    void set_modified(bool state) {}

    void on_commit() {}
    void on_get() {}
};

extern std::shared_mutex g_object_rwlock;
template<class Database, template<class, class> class IndexType, class Derived,
                                        template<class, class> class ...Mixins>
class Object : public IndexType<Database, Derived>,
               public Mixins<Database, Derived>...,
               public NullMixin<Database, Derived>,
               public DatamodelObject<Database>,
               private RPC::MethodRoster<Database, Derived>,
               public std::enable_shared_from_this<Object<Database,
                                  IndexType, Derived, Mixins...>> {
    friend class RPC::MethodRoster<Database, Derived>;
    typedef Object<Database, IndexType, Derived, Mixins...> self;

    typedef std::function<void(std::unique_ptr<JSONRPC::SingleRequest>)>
                                                       RequestCombineCb;

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

        static bool modified(const self &object) {
            if (object.T_<Database, Derived>::modified())
                return true;
            if (sizeof...(Mixins_))
                Foreach<Mixins_...>::modified(object);
            return false;
        }

        static void on_commit(self &object) {
            object.T_<Database, Derived>::on_commit();
            if (sizeof...(Mixins_))
                Foreach<Mixins_...>::on_commit(object);
        }

        static void on_get(self &object) {
            object.T_<Database, Derived>::on_get();
            if (sizeof...(Mixins_))
                Foreach<Mixins_...>::on_get(object);
        }

        // TODO modification hash
        // TODO send requests with hash?
        // TODO redo requests if hash different?
        static void set_modified(self &object, bool state) {
            object.T_<Database, Derived>::set_modified(state);
            if (sizeof...(Mixins_))
                Foreach<Mixins_...>::set_modified(object, state);
        }

        static bool db_backed(const self &object) {
            if (object.T_<Database, Derived>::db_backed())
                return true;
            if (sizeof...(Mixins_))
                Foreach<Mixins_...>::db_backed(object);
            return false;
        }

        static void set_db_backed(self &object, bool state) {
            object.T_<Database, Derived>::set_db_backed(state);
            if (sizeof...(Mixins_))
                Foreach<Mixins_...>::set_db_backed(object, state);
        }

        static void build_update_request(self &object,
            rapidjson::Document::AllocatorType &alloc,
                                RequestCombineCb cb) {
            cb(object.T_<Database, Derived>::build_update_request(alloc));
            if (sizeof...(Mixins_))
                Foreach<Mixins_...>::build_update_request(object, alloc, cb);
        }
    };

public:
    typedef std::function<void(std::string, Mode)> ForeachModeCb;
    typedef std::map<std::string, Mode> ModeMap;

    Object() {}
    Object(std::string id) {
        self::assign_id(id);
    }
    Object(Database &db)
    : IndexType<Database, Derived>(db) {}

    virtual ~Object() {}

    bool exists(Database &db) {
        return this->IndexType<Database, Derived>::exists(db);
    }

    void get(Database &db, std::string id) {
        this->IndexType<Database, Derived>::get(db, id);
        get(db);
    }

    void get(Database &db) {
        std::shared_lock<std::shared_mutex> lock(g_object_rwlock);
        get_modes(db);
        Foreach<Mixins...>::get(*this, db);
    }

    void clear() {
        clear_modes();
        Foreach<Mixins...>::clear(*this);
    }

    void remove(Database &db) {
        std::unique_lock<std::shared_mutex> lock(g_object_rwlock);
        clear();
        Foreach<Mixins...>::commit(*this, db);
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
                clear();
                from_repr(sresp.result());
                Foreach<Mixins...>::on_get(*this);
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
        std::shared_lock<std::shared_mutex> lock(g_object_rwlock);
        this->IndexType<Database, Derived>::commit(db);
        commit_modes(db);
        Foreach<Mixins...>::commit(*this, db);
        on_commit();
    }

    void on_commit() {
        m_remove_modes.clear();
        m_add_modes.clear();       
    }

    void commit(std::shared_ptr<RPC::ClientSession> session,
                               bool force_push_id = false) {
        std::shared_ptr<RPC::ClientRequest> req_handle =
                   commit_async(session, force_push_id);
        req_handle->complete();
    }

   std::shared_ptr<RPC::ClientRequest> commit_async(std::shared_ptr<RPC::ClientSession>
                                                  session, bool force_push_id = false) {
        using namespace RPC;
        using namespace JSONRPC;

        if (db_backed()) {
            std::unique_ptr<JSONRPC::BatchRequest> jupdatereq =
                                        build_update_request();
            auto handler = wrap_refcount_check(
                [this](std::unique_ptr<Response> response) -> void {
                    const BatchResponse bresp(std::move(response));
                    bresp.foreach(
                        [](const SingleResponse &resp) -> void {
                            if (resp.has_error())
                                resp.throw_ec();
                        }
                    );
                    Foreach<Mixins...>::on_commit(*this);
                    on_commit();
                }
            );
            return Factory<RPC::ClientRequest>::create(std::move(jupdatereq),
                                                           session, handler);
        } else {
            // does not push ID if it was generated automatically
            std::unique_ptr<JSONRPC::SingleRequest> jsetreq = build_create_request(
                             !this->IndexType<Database, Derived>::generated_id() ||
                                                                    force_push_id);
            auto handler = wrap_refcount_check(
                [this](std::unique_ptr<Response> response) -> void {
                    const SingleResponse sresp(std::move(response));
                    if (sresp.has_error())
                        sresp.throw_ec();

                    self::assign_id(sresp.result().GetString());
                    Foreach<Mixins...>::on_commit(*this);
                    on_commit();
                }
            );
            return Factory<RPC::ClientRequest>::create(std::move(jsetreq), session,
                                                                          handler);
        }
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
        Derived &d = static_cast<Derived &>(*this); 
        const rapidjson::Value &jrepr = RPC::ObjectCallParams(call)["repr"];

        // generates database-unique id 
        this->IndexType<Database, Derived>::generate_id(db);
        from_repr(jrepr); // might overwrite above id
        if (d.exists(db))
            throw exceptions::ObjectExists(d.type(), d.id());
        commit(db);

        rapidjson::Value jid;
        jid.SetString(d.id().c_str(), alloc);
        return jid;
    }

    rapidjson::Value rpc_mode_update(Database &db, const RPC::SingleCall &call,
                                   rapidjson::Document::AllocatorType &alloc) {
        rpc_get_index(call);
        if (!exists(db)) {
            Derived &d = static_cast<Derived &>(*this); 
            throw exceptions::NoSuchObject(d.type(), d.id());
        }       

        const rapidjson::Value &jset = RPC::ObjectCallParams(call)["mode_set"];
        const rapidjson::Value &jremove = RPC::ObjectCallParams(call)["mode_remove"];

        modes_from_repr(jset);
        remove_modes(jremove);
    }

    rapidjson::Value rpc_remove(Database &db, const RPC::SingleCall &call,
                              rapidjson::Document::AllocatorType &alloc) {
        rpc_get_index(call);
        if (!exists(db)) {
            Derived &d = static_cast<Derived &>(*this); 
            throw exceptions::NoSuchObject(d.type(), d.id());
        }

        const rapidjson::Value &jrepr = RPC::ObjectCallParams(call)["repr"];
        get(db);
        remove(db);
        return rapidjson::Value("OK");
    }

    rapidjson::Value rpc_clear(Database &db, const RPC::SingleCall &call,
                              rapidjson::Document::AllocatorType &alloc) {
        rpc_get_index(call);
        if (!exists(db)) {
            Derived &d = static_cast<Derived &>(*this); 
            throw exceptions::NoSuchObject(d.type(), d.id());
        }

        const rapidjson::Value &jrepr = RPC::ObjectCallParams(call)["repr"];
        get(db);
        clear();
        commit(db);
        return rapidjson::Value("OK");
    }

    rapidjson::Value rpc_get(Database &db, const RPC::SingleCall &call,
                           rapidjson::Document::AllocatorType &alloc) {
        rpc_get_index(call);
        if (!exists(db)) {
            Derived &d = static_cast<Derived &>(*this); 
            throw exceptions::NoSuchObject(d.type(), d.id());
        }

        get(db);
        return repr(alloc);
    }

    rapidjson::Value repr(rapidjson::Document::AllocatorType &alloc,
                                        bool push_id = true) const {
        rapidjson::Value obj_repr(rapidjson::kObjectType);
        repr(obj_repr, alloc, push_id);
        return obj_repr;
    }

    rapidjson::Document repr(bool push_id = true) const {
        rapidjson::Document obj_repr(rapidjson::kObjectType);
        repr(obj_repr, obj_repr.GetAllocator(), push_id);
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
        using namespace rapidjson;

        if (!obj_repr.IsObject())
            throw exceptions::InvalidRepr("repr is not a JSON object");

        // if repr has no id, it will be generated
        if (repr_has_id(obj_repr))
            self::assign_id(id_from_repr(obj_repr)); // throws

        // checks type
        Value::ConstMemberIterator type = obj_repr.FindMember("type");
        if (type == obj_repr.MemberEnd())
            throw exceptions::InvalidRepr("repr lacks \"type\" member");
        if (!type->value.IsString())
            throw exceptions::InvalidRepr("type is not a string");
        if (type->value.GetString() != Derived::type()) {
            throw exceptions::InvalidRepr("tried to initialize "
                           + Derived::type() + " instance with "
                           + type->value.GetString() + " repr");
        }

        Value::ConstMemberIterator jmodes = obj_repr.FindMember("modes");
        if (jmodes != obj_repr.MemberEnd())
            modes_from_repr(jmodes->value);

        Foreach<Mixins...>::from_repr(*this, obj_repr);
    }

    std::string virtual_type() const {
        return Derived::type();
    }

    bool modified() const {
        return Foreach<Mixins...>::modified(*this);
    }

    bool db_backed() const {
        return Foreach<Mixins...>::db_backed(*this);
    }

    static const std::vector<RPC::Method<Database, Derived>> &methods() {
        static const std::vector<RPC::Method<Database, Derived>> ret({
            RPC::Method<Database, Derived>("repr.get", &self::rpc_get),
            RPC::Method<Database, Derived>("repr.create", &self::rpc_create),
            RPC::Method<Database, Derived>("mode.update", &self::rpc_mode_update),
            RPC::Method<Database, Derived>("remove", &self::rpc_remove),
            RPC::Method<Database, Derived>("clear", &self::rpc_clear),
        });
        return ret;
    }

    static const std::string &mixin_type() {
        return Derived::type();
    }

    void rpc_get_index(const RPC::SingleCall &call) {
        if (!RPC::ObjectCallParams(call).id().empty()) {
            this->IndexType<Database, Derived>::assign_id(
                        RPC::ObjectCallParams(call).id());
        } else {
            throw RPC::exceptions::InvalidParameters("No id supplied.");
        }
    }

    const ModeMap &modes() const {
        return m_modes;
    }

    void clear_modes() {
        m_modes.clear();
        m_add_modes.clear();
        m_remove_modes.clear();
    }

    bool access(std::string handle, enum Ownership owner,
                                enum Right right) const {
        ModeMap::const_iterator mit = m_modes.find(handle);
        if (mit == m_modes.end())
            return false;
        return mit->second.access(owner, right);
    }

    void set_mode(std::string handle, Mode mode) {
        m_modes[handle] = mode;
        m_add_modes[handle] = mode;
    }

    void remove_mode(std::string handle) {
        ModeMap::iterator mit = m_modes.find(handle);
        if (mit != m_modes.end()) {
            m_remove_modes[handle] = mit->second;
            m_modes.erase(mit);
        }
    }

private:
    void repr(rapidjson::Value &robj, rapidjson::Document::AllocatorType
                                    &alloc, bool push_id = true) const {
        using namespace rapidjson;

        if (push_id) {
            Value id;
            const std::string &sid = this->IndexType<Database, Derived>::id();
            id.SetString(sid.c_str(), sid.length(), alloc);
            robj.AddMember("id", id, alloc);
        }

        Value type;
        type.SetString(Derived::type().c_str(), Derived::type().length(),
                                                                  alloc);
        robj.AddMember("type", type, alloc);

        Value jmodes = modes_repr(alloc);
        robj.AddMember("modes", jmodes, alloc);

        Foreach<Mixins...>::repr(*this, robj, alloc);
    }

    bool repr_has_id(const rapidjson::Value &obj_repr) {
        rapidjson::Value::ConstMemberIterator id = obj_repr.FindMember("id"); 
        return id != obj_repr.MemberEnd();
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

    std::unique_ptr<JSONRPC::SingleRequest> build_create_request(
                                           bool push_id = true) {
        // TODO better
        auto jreq = std::make_unique<JSONRPC::SingleRequest>();
        jreq->id(self::id() + ":" + uuid_string());
        jreq->method("object.repr.create");
        jreq->params(true);

        using namespace rapidjson;
        Value jtype;
        jtype.SetString(self::type().c_str(), jreq->allocator());
        jreq->params().AddMember("type", jtype, jreq->allocator());

        Value jrepr = repr(jreq->allocator(), push_id);
        jreq->params().AddMember("repr", jrepr, jreq->allocator());

        return jreq;
    }

    std::unique_ptr<JSONRPC::BatchRequest> build_update_request() {
        auto jbreq = std::make_unique<JSONRPC::BatchRequest>();

        if (modes_modified()) {
            std::unique_ptr<JSONRPC::SingleRequest> modereq =
                                 build_mode_update_request();
            jbreq->push_back(std::move(modereq));
        }

        Foreach<Mixins...>::build_update_request(*this, jbreq->allocator(),
            [&](std::unique_ptr<JSONRPC::SingleRequest> jreq) -> void {
                if (jreq)
                    jbreq->push_back(std::move(jreq));
            }
        );
        return jbreq;
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

    std::unique_ptr<JSONRPC::SingleRequest> build_mode_update_request() {
        auto jreq = std::make_unique<JSONRPC::SingleRequest>();
        jreq->id(self::id() + ":" + uuid_string());
        jreq->method("object.mode.update");
        jreq->params(true);

        using namespace rapidjson;
        Value jid;
        jid.SetString(self::id().c_str(), jreq->allocator());
        jreq->params().AddMember("id", jid, jreq->allocator());

        Value jtype;
        jtype.SetString(self::type().c_str(), jreq->allocator());
        jreq->params().AddMember("type", jtype, jreq->allocator());

        Value jmode_set(kArrayType);
        foreach_mode(m_add_modes,
            [&](std::string handle, Mode mode) -> void {
                Value jrepr = mode_repr(handle, mode, jreq->allocator());
                jmode_set.PushBack(jrepr, jreq->allocator());
            }
        ); 
        jreq->params().AddMember("mode_set", jmode_set, jreq->allocator());

        Value jmode_remove(kArrayType);
        foreach_mode(m_remove_modes,
            [&](std::string handle, Mode mode) -> void {
                Value jrepr = mode_repr(handle, mode, jreq->allocator());
                jmode_remove.PushBack(jrepr, jreq->allocator());
            }
        ); 
        jreq->params().AddMember("mode_remove", jmode_remove, jreq->allocator());

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

    static void foreach_mode(const ModeMap &modes, ForeachModeCb cb) {
        for (const auto &pair : modes)
            cb(pair.first, pair.second);
    }

    void foreach_mode(Database &db, ForeachModeCb cb) {
        Derived &derived = static_cast<Derived &>(*this); 

        std::unique_ptr<kyotocabinet::DB::Cursor> cur(db.impl().cursor());
        if (!cur->jump(ModeKey::prefix(derived.path())))
            return;

        std::string path, modestr;
        while (cur->get(&path, &modestr, true)) {
            ModeKey key(path);
            if (key.path_part() != derived.path().string())
                break;
            if (!key.good())
                continue;

            Mode mode(modestr);
            cb(key.handle_part(), mode);
        }
    }

    void get_modes(Database &db) {
        foreach_mode(db,
            [this](std::string handle, Mode mode) -> void {
                m_modes[handle] = mode;
            }
        );
    }

    void commit_modes(Database &db) {
        for (const auto &pair : m_remove_modes)
            remove_mode(db, pair.first);
        for (const auto &pair : m_add_modes)
            set_mode(db, pair.first, pair.second);

        m_remove_modes.clear();
        m_add_modes.clear();
    }

    Mode get_mode(Database &db, std::string handle) const {
        Derived &derived = static_cast<Derived &>(*this); 
        Mode retv;

        ModeKey key({derived.path(), handle});
        std::string modestr;
        if (db.impl().get(key, &modestr))
            retv.from_string(modestr.c_str());
        return retv;
    }

    void set_mode(Database &db, std::string handle, Mode mode) {
        Derived &derived = static_cast<Derived &>(*this); 

        ModeKey key({derived.path(), handle});
        db.impl().set(key, mode.string()); // TODO throw
    }

    void remove_mode(Database &db, std::string handle) {
        Derived &derived = static_cast<Derived &>(*this); 

        ModeKey key({derived.path(), handle});
        db.impl().remove(key); // TODO throw
    }

    static rapidjson::Value mode_repr(std::string handle, Mode mode,
                        rapidjson::Document::AllocatorType &alloc) {
        using namespace rapidjson;

        Value jhandle;
        jhandle.SetString(handle.c_str(), alloc);

        std::string modestr = mode.string();
        Value jmode;
        jmode.SetString(modestr.c_str(), alloc);

        Value jobj(kObjectType);
        jobj.AddMember("handle", jhandle, alloc);
        jobj.AddMember("mode", jmode, alloc);
        return jobj;
    }

    rapidjson::Value modes_repr(rapidjson::Document::AllocatorType
                                                   &alloc) const {
        using namespace rapidjson;

        Value jmodes(kArrayType);
        foreach_mode(m_modes,
            [&](std::string handle, Mode mode) -> void {
                Value jrepr = mode_repr(handle, mode, alloc);
                jmodes.PushBack(jrepr, alloc);
            }
        );
        return jmodes;
    }

    void modes_from_repr(const rapidjson::Value &mode_repr) {
        using namespace rapidjson;

        for (Value::ConstValueIterator itr = mode_repr.Begin();
                               itr != mode_repr.End(); ++itr) {
            Mode mode;
            std::string handle;

            // TODO check values

            Value::ConstMemberIterator handle_ = itr->FindMember("handle");
            if (handle_ == itr->MemberEnd())
                throw exceptions::InvalidRepr("mode repr lacks \"handle\" member");
            handle = handle_->value.GetString();

            Value::ConstMemberIterator mode_ = itr->FindMember("mode");
            if (mode_ == itr->MemberEnd())
                throw exceptions::InvalidRepr("mode repr lacks \"mode\" member");
            mode.from_string(mode_->value.GetString());

            set_mode(handle, mode);
        }
    }

    void remove_modes(const rapidjson::Value &mode_repr) {
        using namespace rapidjson;

        for (Value::ConstValueIterator itr = mode_repr.Begin();
                               itr != mode_repr.End(); ++itr) {
            std::string handle;

            Value::ConstMemberIterator handle_ = itr->FindMember("handle");
            if (handle_ == itr->MemberEnd())
                throw exceptions::InvalidRepr("mode repr lacks \"handle\" member");
            handle = handle_->value.GetString();

            remove_mode(handle);
        }
    }

    bool modes_modified() const {
        return !m_add_modes.empty() || !m_remove_modes.empty();
    }

    ModeMap m_modes;
    ModeMap m_add_modes;
    ModeMap m_remove_modes;
};

}

#endif
