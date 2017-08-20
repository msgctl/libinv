#include <assert.h>
#include <gtest/gtest.h>
#include <iostream>
#include <memory>
#include <typeinfo>
#include <functional>
#include "stdtypes.hh"
#include "rpc.hh"
#include "jsonrpc.hh"
#include "http_server.hh"
#include "http_client.hh"

using namespace std;
using namespace inventory;
using namespace inventory::types;

static int g_argc;
static char **g_argv;

using namespace inventory::RPC;

class HTTPTest : public ::testing::Test {
public:
    HTTPTest() {
        m_db.open(g_argv[1]);
    }

    virtual void SetUp() {
        m_swq = std::make_shared<Workqueue<ServerRequest>>(4);
        m_cwq = std::make_shared<Workqueue<JSONRPC::RequestBase>>(2);
        m_client = std::make_unique<HTTPClient>("http://localhost:8080", m_cwq);
        m_server = std::make_unique<HTTPServer>(8080, m_swq, 
            [this](ServerRequest &request) -> void {
                request.complete<Database<>, StandardDataModel>(m_db);
                std::cout << "completed" << std::endl;
            }
        );
    }

    virtual void TearDown() {}

    Database<> m_db;
    std::shared_ptr<Workqueue<ServerRequest>> m_swq;
    std::shared_ptr<Workqueue<JSONRPC::RequestBase>> m_cwq;
    std::unique_ptr<HTTPServer> m_server;
    std::unique_ptr<HTTPClient> m_client;
};

TEST_F(HTTPTest, HTTPServer) {
    // Initialized in a test fixture;
}

TEST_F(HTTPTest, HTTPClient_complete_sync) {
    using namespace std;
    using namespace RPC;

    std::shared_ptr<ClientSession> session = m_client->create_session();
    Shared<Item<>> first;
    first["testattr"] = "test";
    first->commit(session);

    Shared<Item<>> second;
    second->get(session, first->id());
    cout << second["testattr"];

    session->terminate();
    

/*
    std::unique_ptr<JSONRPC::SingleRequest> jreq(new JSONRPC::SingleRequest);
    jreq->id("42");
    jreq->method("datamodel.repr.get");
    cout << "request: " << jreq->string() << endl;
   
    auto req_handle = Factory<ClientRequest>::create(std::move(jreq), session);

    cout << "calling complete() on ClientRequest instance" << endl;
    req_handle->complete();

    cout << "response: " << req_handle->jsonrpc_response().string() << endl;

    session->terminate();
*/
} 

TEST_F(HTTPTest, HTTPClient_complete_async) {
    using namespace RPC;
    using namespace std;

    shared_ptr<ClientSession> session = m_client->create_session();

    Shared<Item<>> item;
    auto req_handle = item.ref().get_async(session);

    cout << "request: " << req_handle->string() << endl;
    
    cout << "calling complete_async() on ClientRequest instance" << endl;
    req_handle->complete_async();
    cout << "waiting for the asynchronous request to finish" << endl;
    req_handle->future().wait();
    cout << "response: " << req_handle->jsonrpc_response().string() << endl;

    session->terminate();
} 

TEST_F(HTTPTest, HTTPClient_async_response_handler) {
    using namespace RPC;

    std::unique_ptr<JSONRPC::SingleRequest> jreq(new JSONRPC::SingleRequest);
    jreq->id("42");
    jreq->method("datamodel.repr.get");
    jreq->params(true);
    jreq->params()["id"] = "obj-id";
    jreq->params()["type"] = "Item";
    cout << "request: " << jreq->string() << endl;
   
    std::string response_string;
    std::shared_ptr<ClientSession> session = m_client->create_session();

    auto req_handle = Factory<ClientRequest>::create(std::move(jreq), session,
        [&](std::unique_ptr<JSONRPC::Response> response) -> void {
            response_string = *response;
        }
    );

    cout << "calling complete_async() on ClientRequest instance" << endl;
    req_handle->complete_async();
    cout << "waiting for the asynchronous request to finish" << endl;
    req_handle->future().wait();
    cout << "response: " << response_string << endl;

    session->terminate();
}

int main(int argc, char **argv) {
    assert(argc > 1);
    g_argc = argc;
    g_argv = argv;
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
