#ifndef LIBINV_INDEX_HH
#define LIBINV_INDEX_HH
#include <string>
#include <kcutil.h>
#include <uuid/uuid.h>
#include "key.hh"
#include "uuid.hh"

/* google coding style */

namespace inventory {

template<class Database, template<class, class> class IndexImpl, class Derived>
class Index {
public:
    Index() {}

    static std::string prefix() {
        return Derived::type();
    }

    static std::string type() {
        return Derived::type();
    }

    IndexKey path() {
        Derived *index_impl = static_cast<Derived *>(this);
        return IndexKey({prefix(), index_impl->id()});
    }

    static bool prefix_match(IndexKey path) {
        return path.type_part() == type();
    }

    bool id_match(IndexKey path) {
        Derived *index_impl = static_cast<Derived *>(this);
        return path.id_part() == index_impl->id();
    }

    bool db_key_match(std::string pstr) {
        IndexKey path(pstr);
        return prefix_match(path) && id_match(path);
    }

    void commit(Database &db) {
        if (!db.impl().set(path(), ""))
            throw std::runtime_error("Couldn't set kv");
    }

    bool exists(Database &db) {
        Derived &index_impl = static_cast<Derived &>(*this);
        return db.impl().check(index_impl.path()) != -1;
    }

    bool remove(Database &db) {
        Derived &index_impl = static_cast<Derived &>(*this);
        return db.impl().remove(index_impl.path()) != -1;
    }
};

template<class Database, class Derived>
class StringIndexedObject : public Index<Database, StringIndexedObject, Derived> {
    typedef Index<Database, StringIndexedObject, Derived> super;

public:
    StringIndexedObject() {
        generate_id();
    }
    StringIndexedObject(Database &db) {
        Derived &derived = static_cast<Derived &>(*this);
        do {
            derived.generate_id();
        } while(exists(db) != -1);
    }

    std::string id() const {
        return m_id;
    }

    void assign_id(std::string id) {
        m_id = id;
    }

    void get(Database &db, std::string uuid) {
        id_from_string(uuid);
    }

    void generate_id() {
        m_id = Derived::type() + std::to_string(rand() % 1000);
    }

protected:
    void id_from_path(IndexKey path) {
        m_id = path.id_part();
    }

    void id_from_string(std::string id) {
        m_id = id;
    }

private:
    std::string m_id;
};

template<class Database, class Derived>
class Base64ID : public Index<Database, Base64ID, Derived> {
    typedef Index<Database, Base64ID, Derived> super;

public:
    Base64ID() {
        generate_id();
    }
    Base64ID(Database &db) {
        Derived &derived = static_cast<Derived &>(*this);
        do {
            derived.generate_id();
        } while(db.impl().check(derived.path()) != -1);
    }

    std::string id() const {
        return m_uuid;
    }

    void assign_id(std::string uuid) {
        id_from_string(uuid);
    }

    void get(Database &db, std::string uuid) {
        id_from_string(uuid);
    }

    void generate_id() {
        uuid_t uuid;
        uuid_generate(uuid);

        const char *suuid =
            kyotocabinet::baseencode((const void *)(&uuid), 6);
        m_uuid = suuid;
        delete suuid;
    }

protected:
    void id_from_path(IndexKey path) {
        id_from_string(path.id_part());
    }

    void id_from_string(std::string uuid) {
        m_uuid = uuid;
        // TODO check and throw bad uuid
    }

private:
    std::string m_uuid;
};

template<class Database, class Derived>
class UUIDIndexedObject : public Index<Database, UUIDIndexedObject, Derived> {
    typedef Index<Database, UUIDIndexedObject, Derived> super;

public:
    UUIDIndexedObject() {
        generate_id();
    }
    UUIDIndexedObject(Database &db) {
        Derived &derived = static_cast<Derived &>(*this);
        do {
            derived.generate_id();
        } while(db.impl().check(derived.path()) != -1);
    }

    std::string id() const {
        char uuid[32];
        uuid_unparse(m_uuid, uuid);
        return std::string(uuid);
    }

    void assign_id(std::string uuid) {
        id_from_string(uuid);
    }

    void get(Database &db, std::string uuid) {
        id_from_string(uuid);
    }

    void generate_id() {
        uuid_generate(m_uuid);
    }

protected:
    void id_from_path(IndexKey path) {
        id_from_string(path.id_part());
    }

    void id_from_string(std::string uuid) {
        if (uuid_parse(uuid.c_str(), m_uuid))
            throw std::runtime_error("Bad UUID: " + uuid);
    }

private:
    uuid_t m_uuid;
};

}

#endif
