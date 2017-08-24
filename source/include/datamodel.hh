#ifndef LIBINV_DATAMODEL_HH
#define LIBINV_DATAMODEL_HH
// temporary
#include <stdlib.h>

#include <string>
#include <memory>
#include <vector>
#include <stdexcept>
#include <rapidjson/document.h>
#include "database.hh"
#include "exception.hh"
#include "jsonrpc.hh"
#include "datamodel_ex.hh"

namespace inventory {
namespace RPC {
    class Session;
    class SingleCall;
    class Request;
}

template<class Database>
class DatamodelObject {
public:
    virtual void get(const rapidjson::Document &desc) {};
    virtual rapidjson::Document describe() const {};

    virtual void get(Database &db, std::string id) {};
    virtual void get(Database &db) {};
    virtual void commit(Database &db) {};

    virtual rapidjson::Value rpc_call(Database &db,
                        const RPC::SingleCall &call,
          rapidjson::Document::AllocatorType &alloc) {}

    virtual std::vector<std::string> virtual_rpc_methods() const {};
    virtual std::string virtual_type() const {};
};

template<class Database>
class NullType : public DatamodelObject<Database> {
public:
    static std::string type() {
        throw std::runtime_error("NullType");
    }
};

template<template<class> class ...Types>
class Datamodel {
public:
    template<
        template<class> class T_ = NullType,
        template<class> class ...Types_>
    class Foreach {
    public:
        template <class Database_ = Database<>>
        static DatamodelObject<Database_> *create(std::string type) {
             if (T_<Database_>::type() == type)
                return new T_<Database_>;
             if (sizeof...(Types_))
                return Foreach<Types_...>::create(type);
             throw inventory::exceptions::NoSuchType(type);
        }

        static void *type_list(std::vector<std::string> &ret) {
             ret.push_back(T_<Database<NullDBBackend>>::type());
             if (sizeof...(Types_))
                Foreach<Types_...>::type_list(ret);
        }
    };

    template <class Database_ = Database<>>
    static DatamodelObject<Database_> *create(std::string type) {
        return Foreach<Types...>::template create<Database_>(type);
    }

    static std::vector<std::string> type_list() {
        std::vector<std::string> ret;
        Foreach<Types...>::type_list(ret);
        return ret;
    }

    static bool type_exists(std::string type) {
        for (const std::string &iter : type_list())
            if (iter == type)
                return true;
        return false;
    }
};

}

#endif
