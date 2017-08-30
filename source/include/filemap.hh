#ifndef LIBINV_FILEMAP_HH
#define LIBINV_FILEMAP_HH
#include <string>
#include <sys/stat.h>
#include "filemap_ex.hh"

namespace inventory::util {

class FileMap {
public:
    FileMap() {}
    FileMap(std::string filename);
    ~FileMap();

    const char *data() const {
        if (!m_mem)
            map();
        return (const char *)(m_mem);
    }

    size_t size() const {
        if (m_mem)
            return m_sb.st_size;
        return 0;
    }

    std::string path() const {
        return m_filename;
    }

    void path(std::string p) {
        if (m_mem)
            unmap();
        m_filename = p;
    }

private:
    void map() const;
    void unmap() const;

    std::string m_filename;
    mutable void *m_mem = nullptr;
    mutable struct stat m_sb;
    mutable int m_fd = -1;
};
}

#endif
