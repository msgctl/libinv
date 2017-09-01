#include <assert.h>
#include <gtest/gtest.h>
#include <iostream>
#include <memory>
#include <typeinfo>
#include "stdtypes.hh"
#include "rpc.hh"
#include "jsonrpc.hh"
#include "shared_wrapper.hh"

using namespace std;
using namespace inventory;
using namespace inventory::types;

static int g_argc;
static char **g_argv;

class MockSession : public RPC::ServerSession {
public:
    MockSession()
    : RPC::ServerSession(nullptr) {}

    virtual void reply(const JSONRPC::ResponseBase &response) {
        std::cout << std::string(response) << std::endl;
    }

    virtual void reply_async(std::unique_ptr<JSONRPC::ResponseBase> response) {
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
  
    std::unique_ptr<JSONRPC::Request> jreq(new JSONRPC::Request(reqstr)); 
    RPC::ServerRequest req(std::move(jreq), m_session);
    req.complete<Database<>, StandardDataModel>(m_db);
} 

TEST_F(RPCTest, InvalidRequest_nomethod) {
    std::string reqstr = "{\"jsonrpc\": \"2.0\", \"id\": 1}";
   
    std::unique_ptr<JSONRPC::Request> jreq(new JSONRPC::Request(reqstr)); 
    RPC::ServerRequest req(std::move(jreq), m_session);
    req.complete<Database<>, StandardDataModel>(m_db);
} 

TEST_F(RPCTest, InvalidRequest_badnamespace) {
    std::string reqstr = "{\"jsonrpc\": \"2.0\", \"id\": 1, \"method\": "
                                                    "\"badspace.blah\"}";

    std::unique_ptr<JSONRPC::Request> jreq(new JSONRPC::Request(reqstr)); 
    RPC::ServerRequest req(std::move(jreq), m_session);
    req.complete<Database<>, StandardDataModel>(m_db);
} 

TEST_F(RPCTest, InvalidRequest_bad_datamodel_call_no_params) {
    std::string reqstr = "{\"jsonrpc\": \"2.0\", \"id\": 1, \"method\": "
                                                   "\"datamodel.blah\"}";
   
    std::unique_ptr<JSONRPC::Request> jreq(new JSONRPC::Request(reqstr)); 
    RPC::ServerRequest req(std::move(jreq), m_session);
    req.complete<Database<>, StandardDataModel>(m_db);
} 

TEST_F(RPCTest, InvalidRequest_bad_datamodel_call_empty_params) {
    std::string reqstr = "{\"jsonrpc\": \"2.0\", \"id\": 1, \"method\": "
                                   "\"datamodel.blah\", \"params\": {}}";
   
    std::unique_ptr<JSONRPC::Request> jreq(new JSONRPC::Request(reqstr)); 
    RPC::ServerRequest req(std::move(jreq), m_session);
    req.complete<Database<>, StandardDataModel>(m_db);
} 

TEST_F(RPCTest, InvalidRequest_bad_datamodel_call_nosuchtype) {
    std::string reqstr = "{\"jsonrpc\": \"2.0\", \"id\": 1, \"method\": "
                   "\"datamodel.blah\", \"params\": {\"type\": \"bl\"}}";
   
    std::unique_ptr<JSONRPC::Request> jreq(new JSONRPC::Request(reqstr)); 
    RPC::ServerRequest req(std::move(jreq), m_session);
    req.complete<Database<>, StandardDataModel>(m_db);
} 

TEST_F(RPCTest, InvalidRequest_bad_datamodel_call_nosuchrpc) {
    std::string reqstr = "{\"jsonrpc\": \"2.0\", \"id\": 1, \"method\": "
                 "\"datamodel.blah\", \"params\": {\"type\": \"Item\"}}";
 
    std::unique_ptr<JSONRPC::Request> jreq(new JSONRPC::Request(reqstr)); 
    RPC::ServerRequest req(std::move(jreq), m_session);
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
    std::unique_ptr<JSONRPC::Request> jreq(new JSONRPC::Request(reqstr)); 
    RPC::ServerRequest req(std::move(jreq), m_session);
    req.complete<Database<>, StandardDataModel>(m_db);
} 

TEST_F(RPCTest, RPC_batch) {
    std::string reqstr = "[{\"jsonrpc\": \"2.0\", \"id\": 1, \"method\": "
             "\"datamodel.repr.get\", \"params\": {\"type\": \"Item\"}}]";
   
    std::unique_ptr<JSONRPC::Request> jreq(new JSONRPC::Request(reqstr)); 
    RPC::ServerRequest req(std::move(jreq), m_session);
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
    std::unique_ptr<JSONRPC::Request> jreq(new JSONRPC::Request(reqstr)); 
    RPC::ServerRequest req(std::move(jreq), m_session);
    req.complete<Database<>, StandardDataModel>(m_db);
}

class DummySession : public RPC::ClientSession {
public:
    DummySession()
    : ClientSession(nullptr) {}

    virtual void notify(const JSONRPC::RequestBase &request) {}
    virtual void notify_async(std::unique_ptr<JSONRPC::RequestBase>
                                                       request) {};

    virtual std::unique_ptr<JSONRPC::Response> call(
            const JSONRPC::RequestBase &request) {};
    virtual void call_async(std::unique_ptr<JSONRPC::RequestBase> request,
                                             ResponseHandler response) {};
    virtual void upload_file(std::string id, std::string path) {};

    virtual void terminate() {};
};

TEST_F(RPCTest, RPC_commit_reqs) {
    Shared<Item<>> one;
    one["test"] = "test";

    // first commit
    auto client_session = std::make_shared<DummySession>();
    {
        std::shared_ptr<RPC::ClientRequest> commit_req =
                       one->commit_async(client_session);
        // should be a *create* request
        std::cout << commit_req->string() << std::endl;
    }

    // simulate remote commit (sets m_from_db)
    one->commit(m_db);

    // modify
    one["abc"] = "bca";
    {
        std::shared_ptr<RPC::ClientRequest> update_req =
                       one->commit_async(client_session);
        // should be an *update* request (attribute-only)
        std::cout << update_req->string() << std::endl;
    }

    Shared<Owner<>> two("jones");
    {
        std::shared_ptr<RPC::ClientRequest> commit_req =
                       two->commit_async(client_session);
        // should be an *update* request (attribute-only)
        std::cout << commit_req->string() << std::endl;
    }
    two->commit(m_db);
    two *= one;
    {
        std::shared_ptr<RPC::ClientRequest> update_req =
                       one->commit_async(client_session);
        // should be an *update* request (attribute-only)
        std::cout << update_req->string() << std::endl;
    }

}

int main(int argc, char **argv) {
    assert(argc > 1);
    g_argc = argc;
    g_argv = argv;
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
