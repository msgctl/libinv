#ifndef LIBINV_DATABASE_HH
#define LIBINV_DATABASE_HH
#include <kcpolydb.h>
#include <stdexcept>

/* google coding style */

namespace inventory {

class NullDBBackend {
};

template<class kdb = kyotocabinet::TreeDB>
class Database {
public:
    ~Database() {
        close();
    }

    void open(std::string file) {
        using namespace kyotocabinet;

        if (!m_db.open(file, TreeDB::OWRITER | TreeDB::OCREATE))
            throw std::runtime_error("Couldn't open file: " + file);
    }

    void close() {
        m_db.close();
    }

    void clear() {
        m_db.clear();
    }

    kdb &impl() {
        return m_db;
    }

protected:
    kdb m_db;
};

template<>
class Database<NullDBBackend> {
public:
    void open(std::string file) {}
    void close() {}
    NullDBBackend &impl() {
        return m_db;
    }

protected:
    NullDBBackend m_db;
};

}

#endif
