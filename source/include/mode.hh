#ifndef LIBINV_MODE_HH
#define LIBINV_MODE_HH
#include <string>
#include <string.h>

namespace inventory {
    enum Ownership {
        USER,
        GROUP,
        OTHER
    };

    enum Right {
        LIST = 1,
        WRITE = 2,
        READ = 4
    };

    class Mode {
        static int ctoi(char c) {
            return c - '0';
        }

    public:
        Mode() {}
        Mode(std::string str) {
            from_string(str.c_str());
        }

        void from_string(const char *c) {
            if (strlen(c) < 3)
                return; // TODO throw

            m_mode |= ctoi(c[0]) & 7;
            m_mode |= (ctoi(c[1]) & 7) << 3;
            m_mode |= (ctoi(c[2]) & 7) << 6;
        }

        std::string string() const {
            char mstr[4];
            mstr[0] = '0' + (m_mode & 7);
            mstr[1] = '0' + (m_mode >> 3 & 7);
            mstr[2] = '0' + (m_mode >> 6 & 7);
            mstr[3] = 0;
            return mstr;
        }

        bool access(enum Ownership owner, int right) const {
            return ((m_mode >> (owner * 3)) & 7) & right;
        }

        void set(enum Ownership owner, int right) {
            m_mode |= ((right & 7) << (owner * 3));
        }

        void clear(enum Ownership owner, int right) {
            m_mode &= ~(short)((right & 7) << (owner * 3));
        }

        bool zero() const {
            return m_mode;
        }

    private:
        short m_mode = 0;
    };
}

#endif
