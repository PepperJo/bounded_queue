#ifndef COMMON_H
#define COMMON_H

#include <iostream>
#include <system_error>
#include <cstdint>
#include <cstddef>
#include <cassert>

constexpr uint16_t default_port = 20123;

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

struct Bytes {
    size_t value;
};

inline std::ostream& operator<<(std::ostream& out, const Bytes& size) {
    out << size.value;
    return out;
}

inline std::istream& operator>>(std::istream& in, Bytes& size) {
    std::string str;
    in >> str;
    std::stringstream ss(str);
    ss >> size.value;
    char x = '\0';
    ss >> x;
    size_t order = 0;
    switch (x) {
    case 'G':
        order++;
    case 'M':
        order++;
    case 'K':
        order++;
        break;
    case '\0':
        break;
    default:
        in.setstate(std::ios_base::failbit);
        break;
    }
    while (order-- > 0) {
        size.value *= static_cast<size_t>(1024);
    }
    return in;
}

const std::error_category& ibv_wc_error_category();

class ibv_wc_error_category_t : public std::error_category {
  private:
    ibv_wc_error_category_t() {}

  public:
    ~ibv_wc_error_category_t() override{};
    const char* name() const noexcept override { return "ibv_wc"; }
    std::string message(int condition) const override {
        return ibv_wc_status_str(static_cast<ibv_wc_status>(condition));
    }
    friend const std::error_category& ibv_wc_error_category();
};

inline const std::error_category& ibv_wc_error_category() {
    static ibv_wc_error_category_t cat{};
    return cat;
}

#endif /* COMMON_H */
