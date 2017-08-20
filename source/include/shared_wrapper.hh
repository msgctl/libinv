#ifndef LIBINV_SHARED_WRAPPER_HH
#define LIBINV_SHARED_WRAPPER_HH
#include <memory>
#include "factory.hh"

namespace inventory {

template<class Type>
class Shared {
public:
    template<typename ...Args>
    Shared(const Args &...args) {
        m_shptr = make<Type>(args...);
    }

    Type &ref() {
        return *m_shptr;
    }

    const Type &cref() const {
        return ref();
    }

    Type *operator->() {
        return m_shptr.get();
    }

    operator Type() {
        return *m_shptr;
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
    auto operator*=( Arg &arg) {
        return *m_shptr *= arg;
    }

    template<class Arg>
    auto operator*=(Shared<Arg> &arg) {
        return *m_shptr *= arg.ref();
    }

    template<class Arg>
    auto operator/=( Arg &arg) {
        return *m_shptr /= arg;
    }

    template<class Arg>
    auto operator/=(Shared<Arg> &arg) {
        return *m_shptr /= arg.ref();
    }

protected:
    std::shared_ptr<Type> m_shptr;
};

}

#endif
