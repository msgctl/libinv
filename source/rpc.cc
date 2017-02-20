#include "rpc.hh"

namespace inventory::RPC {

    void ClientSession::terminate() {
        m_client->remove_session(this);
        m_client = nullptr;
    }

    void ServerSession::terminate() {
        m_server->remove_session(this);
        m_server = nullptr;
    }

    void Client::remove_session(ClientSession *session) {
        std::remove_if(m_sessions.begin(), m_sessions.end(),
            [&](std::shared_ptr<ClientSession> &iter) {
                return session == iter.get();
            }
        );
    }

    void Server::remove_session(ServerSession *session) {
        std::remove_if(m_sessions.begin(), m_sessions.end(),
            [&](std::shared_ptr<ServerSession> &iter) {
                return session == iter.get();
            }
        );
    }
}
