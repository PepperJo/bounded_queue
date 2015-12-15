#include <iostream>
#include <cassert>

#include <thread>

#include <bounded_queue.h>

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

int main() {
    using namespace bounded_queue;
    Memory mem{128};
    std::cout << mem.size() << '\n';
    Producer<Sep> p{mem};

    size_t n = 10;
    volatile Index back = 0;
    for (size_t i = 0; i < n; i++) {
        auto e = p.produce(4, back);
        if (e) {
            std::cout << "#" << i << " " << e.data() << '\n';
            *e.data<uint32_t>() = i;
        }
    }

    Index old_back = back;
    Consumer<Sep> c{mem};
    for (size_t i = 0; i < n; i++) {
        auto e = c.consume();
        if (e) {
            std::cout << "#" << i << " " << e.data() << " " <<
                *e.data<uint32_t>() << '\n';
            if (c.back() - old_back > 1024) {
                back = c.back();
            }
        }
    }
    return 0;
}
