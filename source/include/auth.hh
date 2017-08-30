#ifndef LIBINV_AUTH_HH
#define LIBINV_AUTH_HH
#include <string>
#include "database.hh"
#include "shared_wrapper.hh"

namespace inventory {

class User {
public:
    User(std::string handle)
    : m_handle(handle) {}

    std::string handle() const {
        return m_handle;
    }

private:
    std::string m_handle;
};

}

#endif
