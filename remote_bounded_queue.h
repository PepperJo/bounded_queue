#ifndef REMOTE_BOUNDED_QUEUE_H
#define REMOTE_BOUNDED_QUEUE_H

#include <bounded_queue.h>

#include <cstdint>

namespace bounded_queue {

using Separator = Sep<uint32_t>;

class Message {
  private:
    Element<Separator> e_;
    Message(Element<Separator> e) : e_{e} {}

  public:
    template <class T = void> T* data() const {
        return reinterpret_cast<T*>(e_.data());
    }

    size_t size() const { return e_.size(); }

    operator bool() const { return e_; }

    friend class Sender;
    friend class Receiver;
};

using CompletionFunction = std::function<void(const Message& msg)>;
class Sender {
  protected:
    Producer<Separator> producer_;

    Sender(std::shared_ptr<Memory> mem) : producer_{mem} {}

  public:
    virtual ~Sender() = 0;
    virtual Message alloc(size_t size) = 0;
    virtual std::system_error send(Message msg, CompletionFunction comp) = 0;
};

class Receiver {
  protected:
    Consumer<Separator> consumer_;

    Receiver(std::shared_ptr<Memory> mem) : consumer_{mem} {}

  public:
    virtual ~Receiver() = 0;
    virtual Message alloc(size_t size) = 0;
    virtual std::system_error receive(Message msg, CompletionFunction comp) = 0;
};

class Session {
  public:
    virtual ~Session() = 0;
    virtual Receiver createReceiver(std::shared_ptr<Memory> mem) = 0;
    virtual Sender createSender(std::shared_ptr<Memory> mem) = 0;
};
};

#endif /* REMOTE_BOUNDED_QUEUE_H */
