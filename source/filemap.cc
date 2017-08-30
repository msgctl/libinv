#include "filemap.hh"
#include "filemap_ex.hh"
#include <string>
#include <stdexcept>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>

namespace inventory::util {

FileMap::FileMap(std::string filename)
: m_filename(filename) {
}

FileMap::~FileMap() {
    unmap();
}

void FileMap::map() const {
    m_fd = open(m_filename.c_str(), O_RDONLY);
    if (m_fd == -1)
        throw exceptions::NoSuchFile(m_filename);

    if (fstat(m_fd, &m_sb) == -1) // TODO msgctl
        throw std::runtime_error("stat() failed: " __FILE__);

    m_mem = mmap(NULL, m_sb.st_size, PROT_READ, MAP_PRIVATE,
                                                   m_fd, 0);
    if (m_mem == MAP_FAILED)
        throw std::runtime_error("mmap() failed: " __FILE__);
}

void FileMap::unmap() const {
    if (m_mem) {
        munmap(m_mem, m_sb.st_size);
        close(m_fd);
        m_mem = nullptr;
    }
}

}
