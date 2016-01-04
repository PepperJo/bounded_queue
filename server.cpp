#include <iostream>
#include <sstream>
#include <cstdlib>
#include <thread>

#include <boost/program_options.hpp>

#include <rdma/rdma_cma.h>

#include <psl/log.h>
#include <psl/net.h>

#include <common.h>
#include <bounded_queue.h>

constexpr int connection_backlog = 128;

struct DeviceContext {
    ibv_mr& mr;
    ibv_device_attr dev_attr;
};

int main(int argc, char* argv[]) {
    namespace bop = boost::program_options;

    bop::options_description desc("Options");
    // clang-format off
    desc.add_options()
        ("help", "produce this message")
        ("s", bop::value<Bytes>()->required(), "size (K/M/G)")
        ("ip", bop::value<psl::net::in_addr>()->default_value({}),
        "listen only from this ip")
        ("p", bop::value<psl::net::in_port_t>()->default_value(default_port),
        "listen on port")
        ("h", "enbale hugepages (madvise)");
    // clang-format on

    bop::positional_options_description p;
    p.add("ip", 1);
    p.add("p", 1);

    bop::variables_map vm;
    bop::store(
        bop::command_line_parser(argc, argv).options(desc).positional(p).run(),
        vm);

    if (vm.count("help")) {
        std::cout << desc << "\n";
        return 1;
    }
    bop::notify(vm);

    auto size = vm["s"].as<Bytes>();
    LOG_ERR_EXIT(!size.value, EINVAL, std::system_category());

    rdma_cm_id* id;
    LOG_ERR_EXIT(rdma_create_id(nullptr, &id, nullptr, RDMA_PS_TCP), errno,
                 std::system_category());

    psl::net::in_addr ip = vm["ip"].as<psl::net::in_addr>();
    psl::net::in_port_t port = vm["p"].as<psl::net::in_port_t>();
    sockaddr_in addr;
    addr.sin_addr = ip;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    LOG_ERR_EXIT(rdma_bind_addr(id, reinterpret_cast<sockaddr*>(&addr)), errno,
                 std::system_category());

    std::cout << "Server listening on " << ip << ":" << port;
    if (id->verbs) {
        std::cout << " (dev = " << id->verbs->device->dev_name << ")";
    }
    std::cout << '\n';

    LOG_ERR_EXIT(rdma_listen(id, connection_backlog), errno,
                 std::system_category());

    size_t nclients = 0;
    while (true) {
        rdma_cm_id* child_id;
        LOG_ERR_EXIT(rdma_get_request(id, &child_id), errno,
                     std::system_category());

        sockaddr_in* child_addr =
            reinterpret_cast<sockaddr_in*>(rdma_get_peer_addr(child_id));
        sockaddr_in* listen_addr =
            reinterpret_cast<sockaddr_in*>(rdma_get_local_addr(child_id));
        std::cout << "#" << nclients << " " << listen_addr->sin_addr << ":"
                  << ntohs(listen_addr->sin_port) << " <- "
                  << psl::terminal::graphic_format::BOLD << child_addr->sin_addr
                  << ":" << ntohs(child_addr->sin_port)
                  << psl::terminal::graphic_format::RESET << '\n';
        nclients++;

        bounded_queue::Memory mem{size.value};
        if (vm.count("h")) {
#ifdef MADV_HUGEPAGE
            LOG_ERR_EXIT(madvise(mem.get(), mem.raw_size(), MADV_HUGEPAGE),
                    errno, std::system_category());
#else
            LOG_ERR_EXIT("no hugepage support!", EINVAL,
                    std::system_category());
#endif
        }
        memset(mem.get(), 0, mem.size());
        ibv_mr* mr;
        LOG_ERR_EXIT(!(mr = ibv_reg_mr(child_id->pd, mem.get(),
                                        mem.raw_size(),
                                       IBV_ACCESS_LOCAL_WRITE |
                                           IBV_ACCESS_REMOTE_WRITE |
                                           IBV_ACCESS_REMOTE_READ |
                                           IBV_ACCESS_REMOTE_ATOMIC)),
                     errno, std::system_category());

        ibv_device_attr dev_attr;
        LOG_ERR_EXIT(ibv_query_device(child_id->verbs, &dev_attr), errno,
                     std::system_category());

        ibv_cq* cq;
        LOG_ERR_EXIT(
            !(cq = ibv_create_cq(child_id->verbs, 16,
                                 nullptr, nullptr, 0)),
            errno, std::system_category());

        ibv_qp_init_attr qp_init_attr = {};
        qp_init_attr.qp_type = IBV_QPT_RC;
        qp_init_attr.sq_sig_all = 0;
        qp_init_attr.send_cq = cq;
        qp_init_attr.recv_cq = cq;
        qp_init_attr.cap.max_inline_data = 0;
        qp_init_attr.cap.max_recv_wr = 1;
        qp_init_attr.cap.max_send_wr = 16;
        qp_init_attr.cap.max_recv_sge = 1;
        qp_init_attr.cap.max_send_sge = 1;
        LOG_ERR_EXIT(rdma_create_qp(child_id, child_id->pd, &qp_init_attr),
                     errno, std::system_category());

        ClientConnectionData client_data;
        std::cout << "-->" << (uint32_t)child_id->event->param.conn.private_data_len << '\n';
        // std::cout << id->event->param.conn.private_data_len << '\n';
        LOG_ERR_EXIT(child_id->event->param.conn.private_data_len <
                sizeof(client_data), EINVAL, std::system_category());
        client_data = *reinterpret_cast<const ClientConnectionData*>(child_id->event->param.conn.private_data);

        ServerConnectionData conn_data;
        conn_data.address = reinterpret_cast<uint64_t>(mem.get());
        conn_data.size = mem.size();
        conn_data.rkey = mr->rkey;
        rdma_conn_param conn_param = {};
        conn_param.private_data = reinterpret_cast<void*>(&conn_data);
        conn_param.private_data_len = sizeof(conn_data);
        conn_param.responder_resources = dev_attr.max_qp_rd_atom;
        conn_param.initiator_depth = dev_attr.max_qp_rd_atom;
        LOG_ERR_EXIT(rdma_accept(child_id, &conn_param), errno,
                     std::system_category());

        std::thread{[mem = std::move(mem), &child_id, &cq, &client_data]() {
            bounded_queue::Consumer<Sep> c{mem};
            uint64_t old_back = c.back();
            ibv_send_wr wr = {};
            wr.next = NULL;
            ibv_sge sge;
            sge.addr = reinterpret_cast<uint64_t>(&old_back);
            sge.length = sizeof(old_back);
            wr.num_sge = 1;
            wr.wr.rdma.remote_addr = client_data.address;
            wr.wr.rdma.rkey = client_data.rkey;
            wr.opcode = IBV_WR_RDMA_WRITE;
            wr.send_flags = IBV_SEND_INLINE | IBV_SEND_SIGNALED;

            size_t i = 0;
            constexpr size_t batch = 8;
            ibv_wc wc[batch];
            while (true) {
                c.consume();
                if (c.back() - old_back > mem.size()/2) {
                    old_back = c.back();
                    ibv_send_wr* bad_wr;
                    LOG_ERR_EXIT(ibv_post_send(child_id->qp, &wr, &bad_wr),
                            errno, std::system_category());
                }
                if (i++ % batch == 0) {
                    int num_wc;
                    LOG_ERR_EXIT((num_wc = ibv_poll_cq(cq, batch, wc)) < 0,
                            errno, std::system_category());
                    i -= num_wc;
                }
            }
        }
        }.detach();
    }

    return 0;
}
