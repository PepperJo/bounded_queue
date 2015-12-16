#ifndef COMMON_H
#define COMMON_H

#include <cstdint>
#include <cstddef>
#include <cassert>

struct ServerConnectionData {
    uint64_t address;
    /* queue size! */
    uint64_t size;
    uint32_t rkey;
};

struct ClientConnectionData {
    uint64_t address;
    uint32_t rkey;
};

struct Sep {
    uint32_t size : 31;
    uint32_t is_footer : 1;

    void header(size_t s) volatile {
        is_footer = 0;
        size = static_cast<decltype(size)>(s);
        assert (size == s);
    }
    void footer() volatile {
        size = 0;
        is_footer = 1;
    }

    bool valid() volatile {
        return (!is_footer && size != 0) || (is_footer && size == 0);
    }
};

#endif /* COMMON_H */
