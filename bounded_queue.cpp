
#include <bounded_queue.h>

#include <cerrno>

#include <sys/mman.h>
#include <unistd.h>

#include <psl/align.h>

using namespace bounded_queue;

Memory::Memory(size_t size) :
    size_{psl::align<size_t>(size, getpagesize())},
    mem_{mmap(NULL, raw_size(), PROT_READ | PROT_WRITE,
            MAP_SHARED | MAP_ANONYMOUS, -1, 0)} {
    if (mem_ == nullptr) {
        throw std::system_error{errno, std::system_category()};
    }
    int ret = remap_file_pages(reinterpret_cast<char*>(mem_) + size_,
            size_, 0, size_/getpagesize(), 0);
    if (ret != 0) {
        ret = errno;
        munmap(mem_, raw_size());
        throw std::system_error{ret, std::system_category()};
    }
}

Memory::~Memory() {
    munmap(mem_, raw_size());
}

