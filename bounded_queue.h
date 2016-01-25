#ifndef BOUNDED_QUEUE_H
#define BOUNDED_QUEUE_H

#include <system_error>
#include <memory>
#include <type_traits>
#include <limits>

namespace bounded_queue {

template <class T> class Sep {
  private:
    static_assert(std::is_integral<T>::value, "not an integral");
    T value_;
    static constexpr int FOOTER =
        1 << (sizeof(T) * std::numeric_limits<unsigned char>::digits - 1);

  public:
    void header(T s) volatile { value_ = s; }
    void footer() volatile { value_ = FOOTER; }

    bool is_footer() const volatile { return value_ == FOOTER; }

    T size() const volatile { return value_ & ~FOOTER; }

    bool valid() const volatile {
        /* footer = size == 0 & FOOTER, header = !FOOTER & size != 0*/
        return is_footer() || (!(value_ & FOOTER) && size() != 0);
    }
};

using Index = size_t;
class Memory {
  private:
    size_t size_;
    void* mem_;

  public:
    Memory(size_t size);
    ~Memory();

    void* at(Index idx) {
        auto raw = reinterpret_cast<char*>(mem_);
        return raw + (idx % size());
    }

    void* raw() const { return mem_; }

    size_t raw_size() const { return size_ * 2; }

    size_t size() const { return size_; }
};

template <class Separator> class Element {
  private:
    Separator* sep_;
    const Index idx_;
    Element(Separator* sep, Index idx) : sep_{sep}, idx_{idx} {
        assert(sep == nullptr || !sep->is_footer());
    }

  public:
    template <class T = void> T* data() const {
        return reinterpret_cast<T*>(sep_ + 1);
    }

    size_t size() const { return sep_->size(); }

    operator bool() const { return sep_ != nullptr; }

    void* get() const { return reinterpret_cast<void*>(sep_); }

    size_t raw_size() const {
        return /*header*/ sizeof(*sep_) + sep_->size() +
               /*footer*/ sizeof(*sep_);
    }

    Index idx() const { return idx_; }

    template <class> friend class Producer;
    template <class> friend class Consumer;
};

template <class Separator> class Producer {
  private:
    std::shared_ptr<Memory> mem_;
    Index front_;
    size_t left(Index back) const {
        if (front_ < back) {
            return back - front_;
        }
        return mem_->size() - (front_ - back);
    }

  public:
    Producer(std::shared_ptr<Memory> mem) : mem_{mem}, front_{0} {}

    Element<Separator> produce(size_t size, Index back) {
        const size_t hdr_data_size = sizeof(Separator) + size;
        const size_t element_size = hdr_data_size + sizeof(Separator);
        if (element_size > left(back)) {
            return {nullptr, 0};
        }
        /*      front_
         *         |
         * ------------------------
         *   |H|+++|F|
         * ------------------------
         */
        auto hdr = reinterpret_cast<Separator*>(mem_->at(front_));
        hdr->header(size);
        /*      front_
         *         |
         * ------------------------
         *   |H|+++|H|
         * ------------------------
         */
        auto old_front = front_;
        front_ += hdr_data_size;
        reinterpret_cast<Separator*>(mem_->at(front_))->footer();
        /*              front_
         *                |
         * ------------------------
         *   |H|+++|H|++++|F|
         * ------------------------
         */
        return {hdr, old_front};
    }
};

template <class Separator> class Consumer {
  private:
    std::shared_ptr<Memory> mem_;
    Index back_;

  public:
    Consumer(std::shared_ptr<Memory> mem) : mem_{mem}, back_{0} {}

    const Element<Separator> consume() {
        auto sep = reinterpret_cast<Separator*>(mem_->at(back_));
        if (!sep->valid() || sep->is_footer()) {
            /* back_
             *   |
             * ------------------------
             *   |F|
             * ------------------------
             */
            return {nullptr, 0};
        }
        /* back_
         *   |
         * ------------------------
         *   |H|+++|F|
         * ------------------------
         */
        Index new_back = back_ + sizeof(Separator) + sep->size();
        /*  back_  new_back
         *   |     |
         * ------------------------
         *   |H|+++|?|
         * ------------------------
         *
         *  we need to check if there is a valid footer
         *  or header
         */
        if (!reinterpret_cast<Separator*>(mem_->at(new_back))->valid()) {
            return {nullptr, 0};
        }
        /*        back_
         *         |
         * ------------------------
         *   |H|+++|H/F|
         * ------------------------
         */
        auto old_back = back_;
        back_ = new_back;
        return {sep, old_back};
    }

    Index back() const { return back_; }
};
}

#endif /* BOUNDED_QUEUE_H */
