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
    Item<> first;
    first["testattr"] = "test";    

    Owner<> link("fred");
    link *= first;

    Item<> contents;
    first += contents;

    Item<> up;
    up += first;

    rapidjson::Document first_repr = first.repr();

    Item<> second;
    second.from_repr(first_repr);

    std::cout << first.repr_string() << std::endl;
    std::cout << second.repr_string() << std::endl;
    EXPECT_EQ(first.repr_string(), second.repr_string());
}

int main(int argc, char **argv) {
    assert(argc > 1);
    g_argc = argc;
    g_argv = argv;
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
