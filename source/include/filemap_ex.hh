#ifndef LIBINV_FILEMAP_EX_HH
#define LIBINV_FILEMAP_EX_HH
#include <string>
#include "exception.hh"

namespace inventory::util::exceptions {

class NoSuchFile : public inventory::exceptions::ExceptionBase {
public:
    NoSuchFile(std::string filename)
    : ExceptionBase("No such file: " + filename) {}

    virtual JSONRPC::ErrorCode ec() const {
        return JSONRPC::ErrorCode::NO_SUCH_FILE;
    }
};

}

#endif
