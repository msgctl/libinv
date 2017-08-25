#ifndef LIBINV_CXX_PATTERNS_HH
#define LIBINV_CXX_PATTERNS_HH

namespace inventory::patterns {

template<class Derived>
class Singleton {
    Singleton() {}

public:
    static Derived &instance() {
        static Derived dobj;
        return dobj;
    }
};

}

#endif
