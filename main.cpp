#include <iostream>
#include <cassert>

#include <thread>

#include <bounded_queue.h>

int main() {
    using namespace bounded_queue;
    auto mem = std::make_shared<Memory>(4096*10);
    std::cout << mem->size() << '\n';
    Producer<Sep<uint32_t>> p{mem};

    size_t n = 1025;
    volatile Index back = 0;
    for (size_t i = 0; i < n; i++) {
        auto e = p.produce(8, back);
        if (e) {
            std::cout << "#" << i << " " << e.data() << '\n';
            *e.data<uint32_t>() = i;
        }
    }

    Index old_back = back;
    Consumer<Sep<uint32_t>> c{mem};
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
