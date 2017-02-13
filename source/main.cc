#include <iostream>
#include <memory>
#include <map>
#include <set>
#include <vector>
#include <string>
#include <stdexcept>
#include "datamodel.hh"
#include "stdtypes.hh"

using namespace kyotocabinet;
using namespace inventory;
using namespace inventory::types;

void generate_html(Database<> &db, Owner<> &owner) {
    std::ofstream ofs("db.html");
    ofs << "<html><body>" << std::endl;
    ofs << "<table>" << std::endl;

    for (auto &item : owner.get_assoc_objects<Item<>>(db)) {
        ofs << "<tr>" << std::endl;

        ofs << "<td>" << std::endl;
        for (auto &pic : item.get_assoc_objects<Picture<>>(db)) {
            ofs << "<img src='" << pic.id() << ".jpg' width=200 height=282/>"
                << std::endl;
        }
        ofs << "</td>" << std::endl;

        ofs << "<td>" << std::endl;
        for (auto &pair : item.attributes()) {
            ofs << pair.first << " " << pair.second << "<br/>" << std::endl;
        }
        ofs << "</td>" << std::endl;

        ofs << "</tr>" << std::endl;
    }

    ofs << "</table>" << std::endl;
    ofs << "</body></html>" << std::endl;
    ofs.close();
}

template<class Database, class Derived>
void print_attributes(Container<Database, Derived> &cnt) {
    for (auto &pair : cnt.attributes()) {
        std::cout << pair.first << "\t" << pair.second << std::endl;
    }
}

template<class Database, class Derived>
void print_categories(Database &db, Association<Database, Derived> &obj) {
    std::cout << "Categories: ";
    for (auto &cat : obj.template get_assoc_objects<Category<>>(db))
        std::cout << cat.id() << " ";
    std::cout << std::endl;
}

template<class Database, class Derived>
void print_contents(Database &db, Hierarchical<Database, Derived> &obj) {
    std::cout << "Contents: " << std::endl;
    for (auto &item : obj.down(db)) {
        print_attributes(item);
        std::cout << std::endl;
    }
}

void sticker_read_cli(Database<> &db) {
    for (;;) {
        std::cout << "Code: ";

        std::string sid;
        std::cin >> sid;

        try {
            Sticker<> stk(sid);
            if (!stk.exists(db)) {
                std::cerr << "Doesn't exist." << std::endl;
                continue;
            }
            stk.get(db);

            std::vector<Item<>> items = stk.get_assoc_objects<Item<>>(db);
            for (Item<> &i : items) {
                std::cout << "Item: " << i.id() << std::endl;
                print_attributes(i);
                print_categories(db, i);
                print_contents(db, i);
                std::cout << std::endl << i.repr_string() << std::endl;

                std::vector<Picture<>> pics = i.get_assoc_objects<Picture<>>(db);
                for (Picture<> &p : pics) {
                    p.show();
                }
            }
        } catch (std::runtime_error &err) {
            std::cerr << "Exception caught: " << err.what() << std::endl;
        }
    }
}

void item_add_cli(Database<> &db) {
    for (;;) {
        try {
            Item<> item(db);
            std::cout << "New item id: " << item.id() << std::endl;

            for (;;) {
                std::string akey, avalue;

                std::cout << "Attribute key: ";
                while (!akey.size())
                    std::getline(std::cin, akey);
                std::cout << "Attribute value: ";
                while (!avalue.size())
                    std::getline(std::cin, avalue);
                std::cout << std::endl;

                item[akey] = avalue;

                std::cout << "Another attribute? (y/n)" << std::endl;
                std::string act;
                std::cin >> act;
                if (act == "n")
                    break;        
            }

            {
                std::cout << "Any categories? (y/n)" << std::endl;
                std::string act;
                std::cin >> act;
                if (act == "y") {
                    for (;;) {
                        std::cout << "Category: ";
                        std::string catname;
                        while (!catname.size())
                            std::getline(std::cin, catname);

                        Category<> cat(catname);
                        if (cat.exists(db)) {
                            cat.associate(item);
                            cat.commit(db);
                        } else {
                            std::cerr << "No such category: " << catname << std::endl;
                        }

                        std::cout << "Another category? (y/n)" << std::endl;
                        std::string act;
                        std::cin >> act;
                        if (act == "n")
                            break;        
                   }
                }
            }

            {
                std::cout << "Any owners? (y/n)" << std::endl;
                std::string act;
                std::cin >> act;
                if (act == "y") {
                    for (;;) {
                        std::cout << "Owner: ";
                        std::string ownname;
                        while (!ownname.size())
                            std::getline(std::cin, ownname);

                        Owner<> own(ownname);
                        if (own.exists(db)) {
                            own.associate(item);
                            own.commit(db);
                        } else {
                            std::cerr << "No such owner: " << ownname << std::endl;
                        }

                        std::cout << "Another owner? (y/n)" << std::endl;
                        std::string act;
                        std::cin >> act;
                        if (act == "n")
                            break;        
                   }
                }
            }

            {
                std::cout << "ISBN? (y/n)" << std::endl;
                std::string act;
                std::cin >> act;
                if (act == "y") {
                    std::cout << "Scan ISBN: ";

                    std::string code;
                    while (!code.size())
                        std::getline(std::cin, code);

                    ISBN<> isbn(code);
                    isbn.associate(item);
                    isbn.commit(db);
                }
            }

            {
                std::cout << "GTIN? (y/n)" << std::endl;
                std::string act;
                std::cin >> act;
                if (act == "y") {
                    std::cout << "Scan GTIN: ";

                    std::string code;
                    while (!code.size())
                        std::getline(std::cin, code);

                    GTIN<> gtin(code);
                    gtin.associate(item);
                    gtin.commit(db);
                }
            }

            for (;;) {
                std::cout << "Scan object? (y/n)" << std::endl;
                std::string act;
                std::cin >> act;
                if (act == "y") {
                    Picture<> pic(db);
                    pic.scan(item);
                    pic.commit(db);
                } else if (act == "n") {
                    break;
                }
            }

            item.commit(db);

            StickerPrefix stp("LDB");
            Sticker<> stk(db, stp);
            stk.print(item);
            stk.commit(db);

        } catch (std::runtime_error &err) {
            std::cerr << "Exception caught: " << err.what() << std::endl;
        }

        std::cout << "Another item? (y/n)" << std::endl;
        std::string act;
        std::cin >> act;
        if (act == "n")
            break;        
    }
}

void create_customized_sticker(Database<> &db) {
    std::cout << "Input item id: ";
    std::string iname;
    while (!iname.size())
        std::getline(std::cin, iname);

    Item<> item(iname);
    if (!item.exists(db)) {
        std::cerr << "No such item." << std::endl;
        return;
    }

    std::cout << "Input sticker id: ";
    std::string sname;
    while (!sname.size())
        std::getline(std::cin, sname);
    Sticker<> stk(sname);
    stk.print(item);
    stk.commit(db);
}

void create_category_cli(Database<> &db) {
    std::cout << "Category name: ";
    std::string cname;
    while (!cname.size())
        std::getline(std::cin, cname);

    Category<> cat(cname);
    std::cout << "Any category attributes? (y/n)" << std::endl;
    std::string act;
    std::cin >> act;
    if (act == "y") {
        for (;;) {
            std::string akey, avalue;

            std::cout << "Attribute key: ";
            while (!akey.size())
                std::getline(std::cin, akey);
            std::cout << "Attribute value: ";
            while (!avalue.size())
                std::getline(std::cin, avalue);
            std::cout << std::endl;           

            cat[akey] = avalue;

            std::cout << "Another category attribute? (y/n)" << std::endl;
            std::string act;
            std::cin >> act;
            if (act == "n")
                break;
        }
    }
    cat.commit(db);
}

void edit_category_cli(Database<> &db) {
    std::string cname;
    std::cout << "Choose category: ";
    while (!cname.size())
        std::getline(std::cin, cname);

    Category<> cat(cname);
    if (!cat.exists(db)) {
        std::cerr << "No such category." << std::endl;
        return;
    }

    std::string cupname;
    std::cout << "Choose a parent category: ";
    while (!cupname.size())
        std::getline(std::cin, cupname);

    Category<> cup(cupname);
    if (!cup.exists(db)) {
        std::cerr << "No such category." << std::endl;
        return;
    }

    cat.get(db);
    cup += cat;

    cup.commit(db);
    cat.commit(db);

    std::cout << "Added." << std::endl;
}

void create_owner_cli(Database<> &db) {
    std::cout << "Owner name: ";
    std::string cname;
    while (!cname.size())
        std::getline(std::cin, cname);

    Owner<> cat(cname);
    std::cout << "Any owner attributes? (y/n)" << std::endl;
    std::string act;
    std::cin >> act;
    if (act == "y") {
        for (;;) {
            std::string akey, avalue;

            std::cout << "Attribute key: ";
            while (!akey.size())
                std::getline(std::cin, akey);
            std::cout << "Attribute value: ";
            while (!avalue.size())
                std::getline(std::cin, avalue);
            std::cout << std::endl;           

            cat[akey] = avalue;

            std::cout << "Another owner attribute? (y/n)" << std::endl;
            std::string act;
            std::cin >> act;
            if (act == "n")
                break;
        }
    }
    cat.commit(db);
}

void generate_html_cli(Database<> &db) {
    std::string sowner;
    std::cout << "Input owner: ";
    std::cin >> sowner;

    Owner<> me(sowner);
    if (!me.exists(db)) {
        std::cerr << "No such person." << std::endl;
        return;
    }

    std::cout << "Generating HTML..." << std::endl;
    me.get(db);
    generate_html(db, me);
}

void move_item_cli(Database<> &db) {
    std::string cstkr;
    std::cout << "Scan the container: ";
    while (!cstkr.size())
        std::getline(std::cin, cstkr);

    Sticker<> stk(cstkr);
    if (!stk.exists(db)) {
        std::cerr << "No such sticker." << std::endl;
        return;
    }

    std::vector<Item<>> containers = stk.get_assoc_objects<Item<>>(db);
    for (Item<> &i : containers) {
        std::cout << "Container: " << i.id() << std::endl;
        print_attributes(i);
    }

    int i = 0;
    if (containers.size() > 1) {
        do {
            std::cout << "Pick the container from above list: ";
            std::cin >> i;
        } while(i >= containers.size());
    }
    Item<> &container = containers[i];

    for (;;) {
        std::string istkc;
        std::cout << "Scan the item: ";
        while (!istkc.size())
            std::getline(std::cin, istkc);

        Sticker<> istk(istkc);
        if (!istk.exists(db)) {
            std::cerr << "No such sticker." << std::endl;
            return;
        }

        std::vector<Item<>> items = istk.get_assoc_objects<Item<>>(db);
        for (Item<> &i : items) {
            std::cout << "Item: " << i.id() << std::endl;
            print_attributes(i);
        }

        i = 0;
        if (items.size() > 1) {
            do {
                std::cout << "Pick the item from above list: ";
                std::cin >> i;
            } while (i >= items.size());
        }
        Item<> &item = items[i];

        container += item;
        item.commit(db);
        container.commit(db);

        std::cout << "Move another item to the same container? (y/n)" << std::endl;
        std::string act;
        std::cin >> act;
        if (act == "n")
            break;
    }
}

void cli(Database<> &db) {
    std::cout << "Add item, read sticker, create category, edit category, "
                      "create owner, print customized sticker, move item, "
                       "generate html? (a/r/c/ec/o/pcs/mi/g)" << std::endl;
    std::string act;
    std::cin >> act;
    if (act == "a")
        item_add_cli(db);
    else if (act == "r")
        sticker_read_cli(db);
    else if (act == "c")
        create_category_cli(db);
    else if (act == "ec")
        edit_category_cli(db);
    else if (act == "o")
        create_owner_cli(db);
    else if (act == "pcs")
        create_customized_sticker(db);
    else if (act == "mi")
        move_item_cli(db);
    else if (act == "g")
        generate_html_cli(db);
}

int main(int argc, char** argv) {
  Database<> db;
  db.open("casket.kct");

  /*
  Item<> test;
  test["mass"] = "33";

  Item<> test2;
  test2["mass"] = "42";
  test+=test2;

  test.commit(db);
  test2.commit(db);

  Sticker<> s;
  s.print(test2);
  s.commit(db);

  Category<> cat;
  cat.get(db, "Books");
  cat.associate(test2);
  cat.commit(db);
  */

  cli(db);

  return 0;
}
