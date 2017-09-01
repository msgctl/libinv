#ifndef LIBINV_STDTYPES_HH
#define LIBINV_STDTYPES_HH
// temporary
#include <stdlib.h>

#include <string>
#include "object.hh"
#include "database.hh"
#include "key.hh"
#include "index.hh"
#include "association.hh"
#include "hierarchical.hh"
#include "container.hh"
#include "rpc.hh"
#include "datamodel.hh"
#include "shared_wrapper.hh"

namespace inventory {
namespace types {

template<class Database = Database<>>
class Category : public Object<Database, StringIndexedObject,
                  Category<Database>, Association, Container,
                                              Hierarchical> {
public:
    typedef Object<Database, StringIndexedObject, Category<Database>,
                          Association, Container, Hierarchical> impl;
    using impl::Object;

    static const std::string &type() {
        static const std::string type("Category");
        return type;
    }
};

class StickerPrefix {
public:
    StickerPrefix(const std::string &value)
    : m_prefix(value) {}

    operator std::string() const {
        return m_prefix;
    }

    std::string string() const {
        return m_prefix;
    }

private:
    std::string m_prefix;
};

template<class Database = Database<>>
class Sticker : public Object<Database, Base64ID,
     Sticker<Database>, Association, Container> {
public:
    typedef Object<Database, Base64ID, Sticker<Database>,
                            Association, Container> impl;

    Sticker() {}
    Sticker(const StickerPrefix &prefix) {
        impl::assign_id(prefix.string() + impl::id());
    }
    Sticker(const std::string &id)
    : impl::Object(id) {}
    Sticker(Database &db)
    : impl::Object(db) {}
    Sticker(Database &db, const StickerPrefix &prefix) {
        do {
            impl::generate_id();
            impl::assign_id(prefix.string() + impl::id());
        } while(db.impl().check(impl::path()) != -1);
    }

    static const std::string &type() {
        static const std::string type("Sticker");
        return type;
    }

    template<class DBObject>
    void print(DBObject &obj) {
        impl::associate(obj);

        system(std::string("barcode -b '" + impl::id()
            + "' -e 'code128b' -u mm -p 62x20 -o barcode.ps").c_str());
        system("lp -d QL-570 barcode.ps");
    }
};

template<class Database = Database<>>
class Picture : public Object<Database, UUIDIndexedObject,
              Picture<Database>, Association, Container> {
public:
    typedef Object<Database, UUIDIndexedObject, Picture<Database>,
                                     Association, Container> impl;
    using impl::Object;

    static const std::string &type() {
        static const std::string type("Picture");
        return type;
    }

    template<class DBObject>
    void scan(DBObject &obj) {
        impl::associate(obj);

        system(std::string("scanimage --resolution 300 > " + impl::id()
                                                    + ".pbm").c_str());
        system(std::string("convert " + impl::id() + ".pbm " + impl::id()
                                                      + ".jpg").c_str());
    }

    void show() {
        system(std::string("feh -. " + impl::id() + ".jpg &").c_str());
    }
};

template<class Database = Database<>>
class Item : public Object<Database, UUIDIndexedObject,
                Item<Database>, Association, Container,
                                        Hierarchical> {
public:
    typedef Object<Database, UUIDIndexedObject, Item<Database>, Association,
                                             Container, Hierarchical> impl;
    using impl::Object;

    static const std::string &type() {
        static const std::string type("Item");
        return type;
    }
};

template<class Database = Database<>>
class GTIN : public Object<Database, StringIndexedObject,
                GTIN<Database>, Association, Container> {
public:
    typedef Object<Database, StringIndexedObject, GTIN<Database>,
                                     Association,Container> impl;
    using impl::Object;

    static const std::string &type() {
        static const std::string type("GTIN");
        return type;
    }
};

template<class Database = Database<>>
class ISBN : public Object<Database, StringIndexedObject,
                ISBN<Database>, Association, Container> {
public:
    typedef Object<Database, StringIndexedObject, ISBN<Database>,
                                     Association,Container> impl;
    using impl::Object;

    static const std::string &type() {
        static const std::string type("ISBN");
        return type;
    }
};

template<class Database = Database<>>
class Owner : public Object<Database, StringIndexedObject,
                  Owner<Database>, Association, Container,
                                           Hierarchical> {
public:
    typedef Object<Database, StringIndexedObject, Owner<Database>,
                       Association, Container, Hierarchical> impl;
    using impl::Object;

    static const std::string &type() {
        static const std::string type("Owner");
        return type;
    }
};

using StandardDataModel = Datamodel<Category, Sticker, Picture,
                                      Item, GTIN, ISBN, Owner>;
}

namespace stdtypes {
    typedef Shared<types::Category<>> Category;
    typedef Shared<types::StickerPrefix> StickerPrefix;
    typedef Shared<types::Sticker<>> Sticker;
    typedef Shared<types::Picture<>> Picture;
    typedef Shared<types::Item<>> Item;
    typedef Shared<types::GTIN<>> GTIN;
    typedef Shared<types::ISBN<>> ISBN;
    typedef Shared<types::Owner<>> Owner;
}

}

#endif
