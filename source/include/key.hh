#ifndef LIBINV_KEY_HH
#define LIBINV_KEY_HH
#include <vector>
#include <initializer_list>
#include <string>
#include <boost/tokenizer.hpp>
#include <boost/algorithm/string/join.hpp>
#include <uuid/uuid.h>

/* google coding style */

namespace inventory {

class IndexSeparator {
public:
    constexpr static const char *string() {
        return ":";
    }
};

class AttributeSeparator {
public:
    constexpr static const char *string() {
        return ".";
    }
};

class LinkSeparator {
public:
    constexpr static const char *string() {
        return "*";
    }
};

class HierarchyDownSeparator {
public:
    constexpr static const char *string() {
        return ">";
    }
};

class HierarchyUpSeparator {
public:
    constexpr static const char *string() {
        return "<";
    }
};

template<class S>
class Key {
public:
    typedef S Separator;

    Key() {}

    Key(std::string path) {
        boost::char_separator<char> bsep(Separator::string());
        boost::tokenizer<boost::char_separator<char>> tokens(path, bsep);
        for (const std::string &t : tokens)
            m_path.push_back(t);
    }

    Key(std::initializer_list<std::string> tokens) {
        m_path.insert(m_path.end(), tokens);
    }

    bool compare(Key &p, int upto) {
        if (Separator::string() != p.Separator::string())
            return false;
        for (int i = 0; i < upto; i++)
            if ((*this)[i] != p[i])
                return false;
        return true;
    }

    operator std::string() const {
        return string();
    }

    std::string string() const  {
        return boost::algorithm::join(m_path, Separator::string());
    }

    std::string operator[](int n) const {
        if (n >= m_path.size())
            throw std::out_of_range("No such token");
        return m_path[n];
    }

    bool operator==(const Key<S> &k) const {
        return m_path == k.m_path;
    }

    bool operator!=(const Key<S> &k) const {
        return !operator==(k);
    }

    // TODO optimize
    bool operator<(const Key<S> &k) const {
        return m_path < k.m_path;
    }

    bool operator>(const Key<S> &k) const {
        return m_path > k.m_path;
    }

    void clear() {
        m_path.clear();
    }

    int elements() const {
        return m_path.size();
    }

    bool empty() const {
        return !elements();
    }

    operator bool() const {
        return elements();
    }

    void from_string(const std::string &s) {
        *this = s;
    }

protected:
    std::vector<std::string> m_path;
};

class IndexKey : public Key<IndexSeparator> {
public:
    IndexKey() {}

    IndexKey(std::string path)
    : Key(path) {}

    IndexKey(std::initializer_list<std::string> tokens)
    : Key(tokens) {}

    std::string type_part() const {
        return (*this)[0];
    }

    std::string id_part() const {
        return (*this)[1];
    }

    bool good() const {
        return m_path.size() == 2;
    }
};

class AttributeKey : public Key<AttributeSeparator> {
public:
    AttributeKey(std::string path)
    : Key(path) {}

    AttributeKey(std::initializer_list<std::string> tokens)
    : Key(tokens) {}

    IndexKey container_part() {
        return (*this)[0];
    }

    static std::string prefix(std::string local_part) {
        return local_part + AttributeSeparator::string();
    }

    std::string attribute_part() {
        return (*this)[1];
    }

    bool good() const {
        return m_path.size() == 2;
    }
};

class LinkKey : public Key<LinkSeparator> {
public:
    LinkKey(std::string key)
    : Key(key) {}

    LinkKey(std::initializer_list<std::string> tokens)
    : Key(tokens) {}

    IndexKey local_part() {
        return (*this)[0];
    }

    static std::string prefix(std::string local_part) {
        return local_part + LinkSeparator::string();
    }

    IndexKey remote_part() {
        return (*this)[1];
    }

    LinkKey inverted() {
        return LinkKey({remote_part(), local_part()});
    }

    bool good() const {
        return m_path.size() == 2;
    }
};

class HierarchyDownKey : public Key<HierarchyDownSeparator> {
public:
    HierarchyDownKey(std::string key)
    : Key(key) {}

    HierarchyDownKey(std::initializer_list<std::string> tokens)
    : Key(tokens) {}

    IndexKey local_part() {
        return (*this)[0];
    }

    static std::string prefix(std::string local_part) {
        return local_part + HierarchyDownSeparator::string();
    }

    IndexKey remote_part() {
        return (*this)[1];
    }

    bool good() const {
        return m_path.size() == 2;
    }
};

class HierarchyUpKey : public Key<HierarchyUpSeparator> {
public:
    HierarchyUpKey(std::string key)
    : Key(key) {
        m_path.push_back("up");
    }

    HierarchyUpKey(std::initializer_list<std::string> tokens)
    : Key(tokens) {
        m_path.push_back("up");
    }

    IndexKey local_part() {
        return (*this)[0];
    }

    static std::string prefix(std::string local_part) {
        return local_part + HierarchyUpSeparator::string();
    }

    bool good() const {
        return m_path.size() == 2;
    }
};

}

#endif
