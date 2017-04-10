#include <assert.h>
#include <gtest/gtest.h>
#include <iostream>
#include <memory>
#include <typeinfo>
#include <rapidjson/document.h>
#include "stdtypes.hh"
#include "rpc.hh"
#include "jsonrpc.hh"

using namespace std;
using namespace inventory;
using namespace inventory::types;

static int g_argc;
static char **g_argv;

class DatamodelTest : public ::testing::Test {
public:
    DatamodelTest() {
        m_db.open(g_argv[1]);
    }

    virtual void SetUp() {}
    virtual void TearDown() {}

    Database<> m_db;
};

TEST_F(DatamodelTest, repr_test) {
    std::shared_ptr<Item<>> first = make<Item<>>();
    first->attribute("testattr") = "test";    

    // TODO
    // make a wrapper template with a std::shared_ptr which will take charge
    // of calling the factory to construct the objects, with perfect forwarding
    // and ref(), cref() methods to return temporary references for operator()s

    // if you're still thinking about async db update events, your long gone
    // gut feeling is that these should be opt-in with object granularity

    // test weak_ptr use in ClientRequest

    std::shared_ptr<Owner<>> link = make<Owner<>>("fred");
    link->associate(first);

    std::shared_ptr<Item<>> contents = make<Item<>>();
    first->insert(contents);

    std::shared_ptr<Item<>> up = make<Item<>>();
    up->insert(first);

    rapidjson::Document first_repr = first->repr();

    std::shared_ptr<Item<>> second = make<Item<>>();
    second->from_repr(first_repr);

    std::cout << first->repr_string() << std::endl;
    std::cout << second->repr_string() << std::endl;
    EXPECT_EQ(first->repr_string(), second->repr_string());
}

int main(int argc, char **argv) {
    assert(argc > 1);
    g_argc = argc;
    g_argv = argv;
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
