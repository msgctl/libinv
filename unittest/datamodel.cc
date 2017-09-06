#include <assert.h>
#include <gtest/gtest.h>
#include <iostream>
#include <memory>
#include <typeinfo>
#include <rapidjson/document.h>
#include "stdtypes.hh"
#include "rpc.hh"
#include "jsonrpc.hh"
#include "mode.hh"

using namespace std;
using namespace inventory;
using namespace inventory::types::shared;

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
    Item first;
    first["testattr"] = "test";    

    Mode access_mode;
    access_mode.set(USER, READ | WRITE);
    first->set_mode("user_handle", access_mode);

    // TODO
    // if you're still thinking about async db update events, your long gone
    // gut feeling is that these should be opt-in with object granularity

    // test weak_ptr use in ClientRequest

    Owner link("fred");
    link *= first;

    Item contents;
    first += contents;

    Item up;
    up += first;

    rapidjson::Document first_repr = first->repr();

    Item second;
    second->from_repr(first_repr);

    std::cout << first->repr_string() << std::endl;
    std::cout << second->repr_string() << std::endl;
    EXPECT_EQ(first->repr_string(), second->repr_string());

    first->commit(m_db);
    contents->commit(m_db);
    up->commit(m_db);
}

int main(int argc, char **argv) {
    assert(argc > 1);
    g_argc = argc;
    g_argv = argv;
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
