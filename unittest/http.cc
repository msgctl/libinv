#include <assert.h>
#include <gtest/gtest.h>
#include <iostream>
#include <memory>
#include <typeinfo>
#include <functional>
#include <unistd.h>
#include "stdtypes.hh"
#include "rpc.hh"
#include "jsonrpc.hh"
#include "http_server.hh"
#include "http_client.hh"

using namespace std;
using namespace inventory;
using namespace inventory::stdtypes;

static int g_argc;
static char **g_argv;

using namespace inventory::RPC;

class HTTPTest : public ::testing::Test {
public:
    HTTPTest() {
        unlink(g_argv[1]);
        m_db.open(g_argv[1]);
    }

    virtual void SetUp() {
        m_swq = std::make_shared<Workqueue<ServerRequest>>(4);
        m_cwq = std::make_shared<Workqueue<JSONRPC::RequestBase>>(2);
        m_client = std::make_unique<HTTPClient>("https://localhost:8080", m_cwq,
                                   "client.crt", "client.key", "ca.crt", false);
        m_server = std::make_unique<HTTPServer>(8080, m_swq, 
            [this](ServerRequest &request) -> void {
                request.complete<Database<>, types::StandardDataModel>(m_db);
                //std::cout << "completed" << std::endl;
            }, "ca.key", "ca.crt"
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

    Item first;
    first["testattr"] = "test";
    first->commit(session);

    Item second;
    second->get(session, first->id());
    EXPECT_EQ(first->repr_string(), second->repr_string());
} 

TEST_F(HTTPTest, HTTPClient_curl_keepalive) {
    using namespace std;
    using namespace RPC;
    std::shared_ptr<ClientSession> session = m_client->create_session();

    for (int i = 0; i < 10; i++) {
        Item first;
        first["testattr"] = "test";
        first->commit(session);

        Item second;
        second->get(session, first->id());
        EXPECT_EQ(first->repr_string(), second->repr_string());
    }
} 

TEST_F(HTTPTest, HTTPClient_complete_async) {
    using namespace RPC;
    using namespace std;
    std::shared_ptr<ClientSession> session = m_client->create_session();

    Item first;
    first["testattr"] = "test";
    std::shared_ptr<ClientRequest> req_hnd = first->commit_async(session);
    req_hnd->complete_async();
    req_hnd->future().wait();

    Item second;
    std::shared_ptr<ClientRequest> req2_hnd = second->get_async(session, first->id());
    req2_hnd->complete_async();
    req2_hnd->future().wait();
    EXPECT_EQ(first->repr_string(), second->repr_string());
} 

TEST_F(HTTPTest, HTTPClient_async_complete_cb) {
    using namespace RPC;
    using namespace std;
    std::shared_ptr<ClientSession> session = m_client->create_session();

    Item first;
    first["testattr"] = "test";
    std::shared_ptr<ClientRequest> req_hnd = first->commit_async(session);
    req_hnd->complete_async();
    req_hnd->future().wait();

    Item second;
    std::shared_ptr<ClientRequest> req2_hnd = second->get_async(session, first->id());

    bool complete_cb = false;
    req2_hnd->push_complete_cb(
        [&]() -> void {
            complete_cb = true;
        }
    );
    req2_hnd->complete_async();
    req2_hnd->future().wait();

    EXPECT_EQ(first->repr_string(), second->repr_string());
    EXPECT_EQ(complete_cb, true);
}

TEST_F(HTTPTest, HTTPClient_exception_nosuchobject) {
    using namespace std;
    using namespace RPC;
    using namespace ::inventory::exceptions;
    std::shared_ptr<ClientSession> session = m_client->create_session();

    Item first;
    try {
        first->get(session, "0fe93648-8984-11e7-88c0-00173e539aaa");
    } catch (const NoSuchObject &) {}
} 

TEST_F(HTTPTest, HTTPClient_rpc_integration_test) {
    using namespace std;
    using namespace RPC;
    std::shared_ptr<ClientSession> session = m_client->create_session();

    Item first;

    Mode access_mode;
    access_mode.set(USER, READ | WRITE | LIST);
    first->set_mode("user_handle", access_mode);

    first["testattr"] = "test";
    first->commit(session);

    access_mode.set(USER, READ);
    first->set_mode("user_handle", access_mode);
    first->commit(session); // update

    Owner owner("jones");
    owner->commit(session);

    owner *= first;
    first->commit(session);

    Item second;
    second->get(session, first->id());
    EXPECT_EQ(first->repr_string(), second->repr_string());

    Owner owner_("jones");
    owner_->get(session);
    EXPECT_EQ(owner->repr_string(), owner_->repr_string());

    Item box;
    box["name"] = "boxen";
    box->commit(session);
    box += first;
    box->commit(session);

    Item box_;
    box_->get(session, box->id());
    EXPECT_EQ(box->repr_string(), box_->repr_string());

    Item third;
    third->get(session, first->id());
    EXPECT_EQ(first->repr_string(), third->repr_string());

    Item second_box;
    second_box->commit(session);
    second_box["name"] = "box2";
    second_box += box;
    second_box->commit(session);
    EXPECT_NE(box->repr_string(), box_->repr_string());

    box_->get(session);
    EXPECT_EQ(box->repr_string(), box_->repr_string());

    Item auto_id;
    std::string id_before = auto_id->id();
    auto_id["color"] = "yellow";
    auto_id->commit(session);
    std::string id_after = auto_id->id();
    EXPECT_NE(id_before, id_after);
    auto_id->get(session);
}

int main(int argc, char **argv) {
    assert(argc > 1);
    g_argc = argc;
    g_argv = argv;
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
