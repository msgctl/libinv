#include <assert.h>
#include <gtest/gtest.h>
#include <iostream>
#include <memory>
#include <typeinfo>
#include "stdtypes.hh"
#include "rpc.hh"
#include "jsonrpc.hh"

using namespace std;
using namespace inventory;
using namespace inventory::types;

static int g_argc;
static char **g_argv;

class MockSession : public RPC::Session {
public:
    virtual void reply(std::unique_ptr<JSONRPC::ResponseBase> response) {
        std::cout << std::string(*response) << std::endl;
    }
};

class RPCTest : public ::testing::Test {
public:
    RPCTest()
    : m_session(new MockSession) {
        m_db.open(g_argv[1]);
    }

    virtual void SetUp() {}
    virtual void TearDown() {}

    Database<> m_db;
    std::shared_ptr<MockSession> m_session;
};

TEST_F(RPCTest, DatamodelTypeList) {
    for (const std::string &name : StandardDataModel::type_list())
        std::cout << name << std::endl;
} 

TEST_F(RPCTest, DatamodelObjectFactory) {
    std::cout << StandardDataModel::create("Category")->virtual_type() << std::endl;
} 

TEST_F(RPCTest, CategoryMethodList) {
    for (const std::string &name : Category<>::rpc_methods())
        std::cout << name << std::endl;
} 

TEST_F(RPCTest, ParseError) {
    std::string reqstr = "blah!";
   
    RPC::Request req(reqstr, m_session);
    req.complete<Database<>, StandardDataModel>(m_db);
} 

TEST_F(RPCTest, InvalidRequest_nomethod) {
    std::string reqstr = "{\"jsonrpc\": \"2.0\", \"id\": 1}";
   
    RPC::Request req(reqstr, m_session);
    req.complete<Database<>, StandardDataModel>(m_db);
} 

TEST_F(RPCTest, InvalidRequest_badnamespace) {
    std::string reqstr = "{\"jsonrpc\": \"2.0\", \"id\": 1, \"method\": "
                                                    "\"badspace.blah\"}";
   
    RPC::Request req(reqstr, m_session);
    req.complete<Database<>, StandardDataModel>(m_db);
} 

TEST_F(RPCTest, InvalidRequest_bad_datamodel_call_no_params) {
    std::string reqstr = "{\"jsonrpc\": \"2.0\", \"id\": 1, \"method\": "
                                                   "\"datamodel.blah\"}";
   
    RPC::Request req(reqstr, m_session);
    req.complete<Database<>, StandardDataModel>(m_db);
} 

TEST_F(RPCTest, InvalidRequest_bad_datamodel_call_empty_params) {
    std::string reqstr = "{\"jsonrpc\": \"2.0\", \"id\": 1, \"method\": "
                                   "\"datamodel.blah\", \"params\": {}}";
   
    RPC::Request req(reqstr, m_session);
    req.complete<Database<>, StandardDataModel>(m_db);
} 

TEST_F(RPCTest, InvalidRequest_bad_datamodel_call_nosuchtype) {
    std::string reqstr = "{\"jsonrpc\": \"2.0\", \"id\": 1, \"method\": "
                   "\"datamodel.blah\", \"params\": {\"type\": \"bl\"}}";
   
    RPC::Request req(reqstr, m_session);
    req.complete<Database<>, StandardDataModel>(m_db);
} 

TEST_F(RPCTest, InvalidRequest_bad_datamodel_call_nosuchrpc) {
    std::string reqstr = "{\"jsonrpc\": \"2.0\", \"id\": 1, \"method\": "
                 "\"datamodel.blah\", \"params\": {\"type\": \"Item\"}}";
 
    RPC::Request req(reqstr, m_session);
    req.complete<Database<>, StandardDataModel>(m_db);
} 

TEST_F(RPCTest, RPC_attribute_list) {
    Item<> testobj;
    testobj["testattr"] = "test"; 
    testobj.commit(m_db); 

    std::string reqstr = "{\"jsonrpc\": \"2.0\", \"id\": 1, \"method\": "
         "\"datamodel.attribute.list\", \"params\": {\"type\": \"Item\","
                                   " \"id\": \"" + testobj.id() + "\"}}";
    std::cout << reqstr << std::endl;
    RPC::Request req(reqstr, m_session);
    req.complete<Database<>, StandardDataModel>(m_db);
} 

TEST_F(RPCTest, RPC_batch) {
    std::string reqstr = "[{\"jsonrpc\": \"2.0\", \"id\": 1, \"method\": "
             "\"datamodel.repr.get\", \"params\": {\"type\": \"Item\"}}]";
   
    RPC::Request req(reqstr, m_session);
    req.complete<Database<>, StandardDataModel>(m_db);
} 

TEST_F(RPCTest, RPC_repr_get) {
    Item<> testobj;
    testobj["testattr"] = "test"; 
    testobj.commit(m_db);

    std::string reqstr = "{\"jsonrpc\": \"2.0\", \"id\": 1, \"method\": "
               "\"datamodel.repr.get\", \"params\": {\"type\": \"Item\","
                                   " \"id\": \"" + testobj.id() + "\"}}";
    std::cout << reqstr << std::endl;
    RPC::Request req(reqstr, m_session);
    req.complete<Database<>, StandardDataModel>(m_db);
}

int main(int argc, char **argv) {
    assert(argc > 1);
    g_argc = argc;
    g_argv = argv;
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
