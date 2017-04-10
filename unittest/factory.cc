#include <assert.h>
#include <gtest/gtest.h>
#include <iostream>
#include "stdtypes.hh"
#include "factory.hh"

using namespace std;
using namespace inventory;

static int g_argc;
static char **g_argv;

class FactoryTest : public ::testing::Test {
public:
    FactoryTest() {
        m_db.open(g_argv[1]);
    }

    virtual void SetUp() {}
    virtual void TearDown() {}

    Database<> m_db;
};

TEST_F(FactoryTest, one) {
    std::shared_ptr<std::string> str = Factory<std::string>::create("blah!");
    std::cout << *str << std::endl;
}

int main(int argc, char **argv) {
    assert(argc > 1);
    g_argc = argc;
    g_argv = argv;
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
