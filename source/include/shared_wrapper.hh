#ifndef LIBINV_SHARED_WRAPPER_HH
#define LIBINV_SHARED_WRAPPER_HH
#include <memory>
#include <vector>
#include "factory.hh"

namespace inventory {

template<class Type_>
class Shared {
    Shared(const Type_ &) {}
    Shared(Type_ &&) {}

public:
    typedef Type_ Type;

    template<typename ...Args>
    Shared(const Args &...args) {
        m_shptr = make<Type_>(args...);
    }

    Type_ &ref() {
        return *m_shptr;
    }

    const Type_ &cref() const {
        return ref();
    }

    Type_ *operator->() {
        return m_shptr.get();
    }

    const Type_ *operator->() const {
        return m_shptr.get();
    }

    operator Type_() {
        return *m_shptr;
    }

    operator bool() const {
        return (bool)(m_shptr);
    }

    template<class Arg>
    auto operator[](Arg &arg) {
        return (*m_shptr)[arg];
    }

    template<class Arg>
    auto operator+=(Arg &arg) {
        return *m_shptr += arg;
    }

    template<class Arg>
    auto operator+=(Shared<Arg> &arg) {
        return *m_shptr += arg.ref();
    }

    template<class Arg>
    auto operator-=(Arg &arg) {
        return *m_shptr -= arg;
    }

    template<class Arg>
    auto operator-=(Shared<Arg> &arg) {
        return *m_shptr -= arg.ref();
    }

    template<class Arg>
    auto operator*=(Arg &arg) {
        return *m_shptr *= arg;
    }

    template<class Arg>
    auto operator*=(Shared<Arg> &arg) {
        return *m_shptr *= arg.ref();
    }

    template<class Arg>
    auto operator/=(Arg &arg) {
        return *m_shptr /= arg;
    }

    template<class Arg>
    auto operator/=(Shared<Arg> &arg) {
        return *m_shptr /= arg.ref();
    }

protected:
    std::shared_ptr<Type_> m_shptr;
};

}

#endif
