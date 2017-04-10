#ifndef LIBINV_FACTORY_HH
#define LIBINV_FACTORY_HH
#include <vector>
#include <memory>
#include <utility>

namespace inventory {

template<class T>
class Factory {
public:
    static std::shared_ptr<T> create() {
        T* tptr = new T;
        return std::shared_ptr<T>(tptr);
    }

    template<typename... Args>
    static std::shared_ptr<T> create(Args&&... args) {
        T* tptr = new T(std::forward<Args>(args)...);
        return std::shared_ptr<T>(tptr);
    }
};

template<class T>
std::shared_ptr<T> make() {
    return Factory<T>::create();
}

template<class T, typename... Args>
std::shared_ptr<T> make(Args&&... args) {
    return Factory<T>::create(args...);
}

}

#endif
