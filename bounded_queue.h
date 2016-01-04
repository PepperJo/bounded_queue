#ifndef BOUNDED_QUEUE_H
#define BOUNDED_QUEUE_H

#include <system_error>
#include <memory>

namespace bounded_queue {

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

        void* raw() const {
            return mem_;
        }

        size_t raw_size() const {
            return size_ * 2;
        }

        size_t size() const {
            return size_;
        }
};

template<class Separator>
class Element {
    private:
        Separator* sep_;
        Element(Separator* sep) : sep_{sep} {
            assert(sep == nullptr || !sep->is_footer);
        }
    public:
        template<class T = void>
        T* data() const {
            // assert(size() <= sizeof(T));
            return reinterpret_cast<T*>(sep_ + 1);
        }

        size_t size() const {
            return sep_->size;
        }

        operator bool() const {
            return sep_ != nullptr;
        }

        void* get() const {
            return reinterpret_cast<void*>(sep_);
        }
        size_t raw_size() const {
            return /*header*/sizeof(*sep_) + sep_->size +
                /*footer*/sizeof(*sep_);
        }

        template<class>
        friend class Producer;
        template<class>
        friend class Consumer;
};

template<class Separator>
class Producer {
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
                return {nullptr};
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
            front_ += hdr_data_size;
            reinterpret_cast<Separator*>(mem_->at(front_))->footer();
            /*              front_
             *                |
             * ------------------------
             *   |H|+++|H|++++|F|
             * ------------------------
             */
            return {hdr};
        }
};

template<class Separator>
class Consumer {
    private:
        std::shared_ptr<Memory> mem_;
        Index back_;
    public:
        Consumer(std::shared_ptr<Memory> mem) : mem_{mem}, back_{0} {}

        const Element<Separator> consume() {
            auto sep = reinterpret_cast<Separator*>(mem_->at(back_));
            if (!sep->valid() || sep->is_footer) {
                /* back_
                 *   |
                 * ------------------------
                 *   |F|
                 * ------------------------
                 */
                return {nullptr};
            }
            /* back_
             *   |
             * ------------------------
             *   |H|+++|F|
             * ------------------------
             */
            Index new_back = back_ + sizeof(Separator) + sep->size;
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
                return {nullptr};
            }
            /*        back_
             *         |
             * ------------------------
             *   |H|+++|H/F|
             * ------------------------
             */
            back_ = new_back;
            return {sep};
        }

        Index back() const {
            return back_;
        }
};

}

#endif /* BOUNDED_QUEUE_H */
