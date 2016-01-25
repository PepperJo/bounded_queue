
#include <bounded_queue.h>

#include <cerrno>

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <psl/align.h>

using namespace bounded_queue;

static void* rb_mmap(size_t size) {
    int fd = open("/tmp", O_RDWR | O_TMPFILE | O_EXCL, S_IRWXU);
    if (fd == -1) {
        throw std::system_error{errno, std::system_category()};
    }

    int ret = ftruncate(fd, size);
    if (ret == -1) {
        throw std::system_error{errno, std::system_category()};
    }

    void* m1 =
        mmap(nullptr, size * 2, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (m1 == nullptr) {
        throw std::system_error{errno, std::system_category()};
    }
    void* m2 = mmap(reinterpret_cast<char*>(m1) + size, size,
                    PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, fd, 0);
    if (m2 == nullptr) {
        throw std::system_error{errno, std::system_category()};
    }
    close(fd);
    return m1;
}

Memory::Memory(size_t size)
    : size_{psl::align<size_t>(size, getpagesize())}, mem_{rb_mmap(size_)} {}

Memory::~Memory() { munmap(mem_, raw_size()); }
