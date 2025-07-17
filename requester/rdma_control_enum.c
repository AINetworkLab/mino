// #include "rdma_control_enum.h"
// #include <string.h>
// #include <stdio.h>
// #include <unistd.h>
// #include <fcntl.h>
// #include <errno.h>
// #include <stdlib.h>
// #include <arpa/inet.h>
// #include <sys/socket.h>
// #include <netinet/in.h>
// #include <sys/epoll.h>

// static void set_nonblocking(int fd) {
//     int flags = fcntl(fd, F_GETFL, 0);
//     fcntl(fd, F_SETFL, flags | O_NONBLOCK);
// }

// // 初始化时，client和server都设置epoll
// int init_control_client(struct control_channel *ctrl, const char *server_ip, uint16_t port) {
//     ctrl->sockfd = socket(AF_INET, SOCK_STREAM, 0);
//     if (ctrl->sockfd < 0) return -1;

//     struct sockaddr_in serv_addr = {0};
//     serv_addr.sin_family = AF_INET;
//     serv_addr.sin_port = htons(port);
//     inet_pton(AF_INET, server_ip, &serv_addr.sin_addr);

//     if (connect(ctrl->sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
//         perror("connect");
//         return -1;
//     }

//     // Client也设置epoll来监听这个连接
//     ctrl->epoll_fd = epoll_create1(0);
//     if (ctrl->epoll_fd < 0) return -1;

//     set_nonblocking(ctrl->sockfd);
    
//     struct epoll_event ev = {0};
//     ev.events = EPOLLIN;
//     ev.data.fd = ctrl->sockfd;
//     epoll_ctl(ctrl->epoll_fd, EPOLL_CTL_ADD, ctrl->sockfd, &ev);

//     ctrl->is_server = false;
//     return 0;
// }

// int init_control_server(struct control_channel *ctrl, uint16_t port) {
//     ctrl->listen_fd = socket(AF_INET, SOCK_STREAM, 0);
//     if (ctrl->listen_fd < 0) return -1;

//     int opt = 1;
//     setsockopt(ctrl->listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

//     struct sockaddr_in addr = {0};
//     addr.sin_family = AF_INET;
//     addr.sin_addr.s_addr = INADDR_ANY;
//     addr.sin_port = htons(port);

//     if (bind(ctrl->listen_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
//         perror("bind");
//         return -1;
//     }

//     listen(ctrl->listen_fd, 128);
//     set_nonblocking(ctrl->listen_fd);

//     ctrl->epoll_fd = epoll_create1(0);
//     if (ctrl->epoll_fd < 0) return -1;

//     struct epoll_event ev = {0};
//     ev.events = EPOLLIN;
//     ev.data.fd = ctrl->listen_fd;
//     epoll_ctl(ctrl->epoll_fd, EPOLL_CTL_ADD, ctrl->listen_fd, &ev);

//     ctrl->is_server = true;
//     return 0;
// }

// int accept_new_client(struct control_channel *ctrl) {
//     struct sockaddr_in client_addr;
//     socklen_t len = sizeof(client_addr);
//     int client_fd = accept(ctrl->listen_fd, (struct sockaddr*)&client_addr, &len);
//     if (client_fd < 0) return -1;

//     set_nonblocking(client_fd);

//     struct epoll_event ev = {0};
//     ev.events = EPOLLIN;
//     ev.data.fd = client_fd;
//     epoll_ctl(ctrl->epoll_fd, EPOLL_CTL_ADD, client_fd, &ev);

//     return client_fd;
// }

// int send_flag(int sockfd, ctrl_flag_t flag) {
//     uint8_t val = (uint8_t)flag;
//     return write(sockfd, &val, sizeof(uint8_t));
// }

// int recv_flag(int sockfd, ctrl_flag_t *flag) {
//     uint8_t buf = 0;
//     int ret = read(sockfd, &buf, sizeof(uint8_t));
//     if (ret > 0)
//         *flag = (ctrl_flag_t)buf;
//     return ret;
// }

// // int wait_flag(struct control_channel *ctrl, ctrl_flag_t expected_flag) {
// //     ctrl_flag_t flag;

// //     if (!ctrl->is_server) {
// //         // 客户端直接读
// //         while (1) {
// //             int ret = recv_flag(ctrl->sockfd, &flag);
// //             if (ret <= 0) {
// //                 perror("recv_flag (client)");
// //                 continue;
// //             }
// //             if (flag == expected_flag)
// //                 return ctrl->sockfd;
// //         }
// //     } else {
// //         // 服务端 epoll 等待事件
// //         struct epoll_event event;
// //         while (1) {
// //             int n = epoll_wait(ctrl->epoll_fd, &event, 1, -1);
// //             if (n <= 0) continue;

// //             int fd = event.data.fd;

// //             if (fd == ctrl->listen_fd) {
// //                 int client_fd = accept_new_client(ctrl);
// //                 if (client_fd >= 0)
// //                     continue;
// //             } else {
// //                 int ret = recv_flag(fd, &flag);
// //                 if (ret <= 0) {
// //                     perror("recv_flag (server)");
// //                     continue;
// //                 }
// //                 if (flag == expected_flag)
// //                     return fd;
// //             }
// //         }
// //     }
// // }

// // 统一的wait_flag函数
// int wait_flag(struct control_channel *ctrl, ctrl_flag_t expected_flag) {
//     ctrl_flag_t flag;
//     struct epoll_event event;

//     while (1) {
//         int n = epoll_wait(ctrl->epoll_fd, &event, 1, -1);
//         if (n <= 0) {
//             if (errno == EINTR) continue;
//             perror("epoll_wait");
//             return -1;
//         }

//         int fd = event.data.fd;

//         if (ctrl->is_server && fd == ctrl->listen_fd) {
//             // 服务端处理新连接
//             int client_fd = accept_new_client(ctrl);
//             if (client_fd < 0) {
//                 perror("accept_new_client failed");
//             }
//             continue;
//         } else {
//             // 处理数据（客户端的sockfd或服务端的client_fd）
//             int ret = recv_flag(fd, &flag);
//             if (ret < 0) {
//                 if (errno == EAGAIN || errno == EWOULDBLOCK) {
//                     continue; // 非阻塞，暂时无数据
//                 }
//                 // 真正的错误
//                 perror("recv_flag");
//                 if (ctrl->is_server) {
//                     // 服务端清理断开的客户端连接
//                     epoll_ctl(ctrl->epoll_fd, EPOLL_CTL_DEL, fd, NULL);
//                     close(fd);
//                 }
//                 continue;
//             } else if (ret == 0) {
//                 // 连接关闭
//                 if (ctrl->is_server) {
//                     epoll_ctl(ctrl->epoll_fd, EPOLL_CTL_DEL, fd, NULL);
//                     close(fd);
//                 }
//                 continue;
//             } else if (flag == expected_flag) {
//                 return fd;
//             }
//             // 收到了数据，但不是期望的flag，继续等待
//         }
//     }
// }

// void close_control_channel(struct control_channel *ctrl) {
//     if (ctrl->is_server) {
//         if (ctrl->listen_fd > 0)
//             close(ctrl->listen_fd);
//         if (ctrl->epoll_fd > 0)
//             close(ctrl->epoll_fd);
//     } else {
//         if (ctrl->sockfd > 0)
//             close(ctrl->sockfd);
//     }
// }

#include "rdma_control_enum.h"
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/epoll.h>

// static void optimize_socket(int fd) {
//     int flags = fcntl(fd, F_GETFL, 0);
//     fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    
//     // 关键：禁用Nagle算法
//     int nodelay = 1;
//     setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));
    
//     // 立即发送小数据
//     int lowat = 1;
//     setsockopt(fd, SOL_SOCKET, SO_SNDLOWAT, &lowat, sizeof(lowat));
// }

static void optimize_socket(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    
    // 禁用Nagle算法
    int nodelay = 1;
    if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay)) < 0) {
        perror("TCP_NODELAY failed");
    }
    
    // 验证设置是否生效
    int check_nodelay = 0;
    socklen_t len = sizeof(check_nodelay);
    getsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &check_nodelay, &len);
    printf("TCP_NODELAY is %s\n", check_nodelay ? "enabled" : "disabled");
    
    // 立即发送小数据
    int lowat = 1;
    setsockopt(fd, SOL_SOCKET, SO_SNDLOWAT, &lowat, sizeof(lowat));
}

int init_control_client(struct control_channel *ctrl, const char *server_ip, uint16_t port) {
    ctrl->sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (ctrl->sockfd < 0) return -1;

    struct sockaddr_in serv_addr = {0};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    inet_pton(AF_INET, server_ip, &serv_addr.sin_addr);

    if (connect(ctrl->sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("connect");
        return -1;
    }

    // 简化：一对一场景，只需要一个epoll监听一个fd
    ctrl->epoll_fd = epoll_create1(0);
    if (ctrl->epoll_fd < 0) return -1;

    optimize_socket(ctrl->sockfd);
    
    struct epoll_event ev = {0};
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = ctrl->sockfd;
    epoll_ctl(ctrl->epoll_fd, EPOLL_CTL_ADD, ctrl->sockfd, &ev);

    ctrl->is_server = false;
    return 0;
}

int init_control_server(struct control_channel *ctrl, uint16_t port) {
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) return -1;

    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(listen_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(listen_fd);
        return -1;
    }

    listen(listen_fd, 1);  // 只需要1个连接
    printf("Server waiting for client connection on port %d...\n", port);
    
    // 等待唯一的客户端连接
    struct sockaddr_in client_addr;
    socklen_t len = sizeof(client_addr);
    ctrl->sockfd = accept(listen_fd, (struct sockaddr*)&client_addr, &len);
    if (ctrl->sockfd < 0) {
        perror("accept");
        close(listen_fd);
        return -1;
    }
    
    // 一对一场景：连接建立后立即关闭监听socket
    close(listen_fd);
    
    printf("Client connected from %s:%d\n", 
           inet_ntoa(client_addr.sin_addr), 
           ntohs(client_addr.sin_port));

    // 简化：只监听这一个连接
    ctrl->epoll_fd = epoll_create1(0);
    if (ctrl->epoll_fd < 0) return -1;

    optimize_socket(ctrl->sockfd);
    
    struct epoll_event ev = {0};
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = ctrl->sockfd;
    epoll_ctl(ctrl->epoll_fd, EPOLL_CTL_ADD, ctrl->sockfd, &ev);

    ctrl->is_server = true;
    return 0;
}

// int send_flag(struct control_channel *ctrl, ctrl_flag_t flag) {
//     uint8_t val = (uint8_t)flag;
//     return write(ctrl->sockfd, &val, sizeof(uint8_t));
// }

int send_flag(struct control_channel *ctrl, ctrl_flag_t flag) {
    uint8_t val = (uint8_t)flag;
    
    // 使用send而不是write，添加立即发送标志
    ssize_t ret = send(ctrl->sockfd, &val, sizeof(uint8_t), MSG_DONTWAIT);
    
    return ret;
}

int recv_flag(int sockfd, ctrl_flag_t *flag) {
    uint8_t buf = 0;
    int ret = read(sockfd, &buf, sizeof(uint8_t));
    if (ret > 0)
        *flag = (ctrl_flag_t)buf;
    return ret;
}

// 简化的wait_flag：一对一场景，返回0表示成功，-1表示失败
int wait_flag(struct control_channel *ctrl, ctrl_flag_t expected_flag) {
    ctrl_flag_t flag;
    struct epoll_event event;

    while (1) {
        int n = epoll_wait(ctrl->epoll_fd, &event, 1, 10);
        if (n < 0) {
            if (errno == EINTR) continue;
            perror("epoll_wait");
            return -1;
        }
        if (n == 0) continue; // 超时，继续等待
        // 一对一场景：event.data.fd必然是ctrl->sockfd
        while (1) {
            int ret = recv_flag(ctrl->sockfd, &flag);
            if (ret < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    break; // 没有更多数据
                }
                perror("recv_flag");
                return -1;
            } else if (ret == 0) {
                // 连接关闭
                printf("Connection closed by peer\n");
                return -1;
            } else if (flag == expected_flag) {
                return 0; // 成功找到期望的flag
            }
            // 收到其他flag，继续读取
        }
    }
}

void close_control_channel(struct control_channel *ctrl) {
    if (ctrl->sockfd > 0) {
        close(ctrl->sockfd);
    }
    if (ctrl->epoll_fd > 0) {
        close(ctrl->epoll_fd);
    }
}