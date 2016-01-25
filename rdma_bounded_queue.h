#ifndef RDMA_BOUNDED_QUEUE_H
#define RDMA_BOUNDED_QUEUE_H

#include <remote_bounded_queue.h>

#include <rdma/rdma_cma.h>

namespace bounded_queue {
class RdmaSession : public Session {
    public:
        RdmaSession(rdma_cm_id* id);
        void disconnect();
};

class RdmaReceiver : public Receiver {};

class RdmaSender : public Sender {}
}

#endif /* RDMA_BOUNDED_QUEUE_H */
