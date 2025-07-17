// #ifndef RDMA_CONTROL_ENUM_H
// #define RDMA_CONTROL_ENUML_H

// #include <stdbool.h>
// #include <stdint.h>

// #define MAX_EVENTS 64

// // 控制信号枚举
// typedef enum ctrl_flag {
//     CTRL_RECV_READY     = 1,
//     CTRL_WRITE_READY    = 2,
//     CTRL_WRITE_FINISH   = 3,
//     CTRL_DISCONNECT     = 4,
//     CTRL_ERROR          = 5,
//     CTRL_ACK            = 100
// } ctrl_flag_t;

// // 控制通道结构体（客户端与服务端共用）
// struct control_channel {
//     int sockfd;       // 客户端使用的 socket fd
//     int listen_fd;    // 服务端监听用
//     int epoll_fd;     // 服务端 epoll fd
//     bool is_server;   // 是否是服务端
// };

// // 初始化为客户端
// int init_control_client(struct control_channel *ctrl, const char *server_ip, uint16_t port);

// // 初始化为服务端
// int init_control_server(struct control_channel *ctrl, uint16_t port);

// // 服务端接收新连接
// int accept_new_client(struct control_channel *ctrl);

// // 发送 flag
// int send_flag(int sockfd, ctrl_flag_t flag);

// // 等待某个 flag（客户端 or 服务端，统一入口）
// int wait_flag(struct control_channel *ctrl, ctrl_flag_t expected_flag);

// // 关闭
// void close_control_channel(struct control_channel *ctrl);

// #endif // RDMA_CONTROL_CHANNEL_H

#ifndef RDMA_CONTROL_ENUM_H
#define RDMA_CONTROL_ENUM_H

#include <stdint.h>
#include <stdbool.h>

// 控制信号枚举
typedef enum ctrl_flag {
    CTRL_RECV_READY     = 1,
    CTRL_WRITE_READY    = 2,
    CTRL_WRITE_FINISH   = 3,
    CTRL_DISCONNECT     = 4,
    CTRL_ERROR          = 5,
    CTRL_ACK            = 100
} ctrl_flag_t;

// 控制通道结构体 - 简化版本，适用于一对一场景
struct control_channel {
    int sockfd;          // 通信socket
    int epoll_fd;        // epoll文件描述符
    bool is_server;      // 是否为服务端
};

// 函数声明
/**
 * 初始化客户端控制通道
 * @param ctrl 控制通道结构体指针
 * @param server_ip 服务器IP地址
 * @param port 服务器端口
 * @return 成功返回0，失败返回-1
 */
int init_control_client(struct control_channel *ctrl, const char *server_ip, uint16_t port);

/**
 * 初始化服务端控制通道
 * @param ctrl 控制通道结构体指针  
 * @param port 监听端口
 * @return 成功返回0，失败返回-1
 */
int init_control_server(struct control_channel *ctrl, uint16_t port);

/**
 * 发送控制标志
 * @param ctrl 控制通道结构体指针
 * @param flag 要发送的控制标志
 * @return 成功返回发送字节数，失败返回-1
 */
int send_flag(struct control_channel *ctrl, ctrl_flag_t flag);

/**
 * 等待特定的控制标志
 * @param ctrl 控制通道结构体指针
 * @param expected_flag 期望接收的控制标志
 * @return 成功返回0，失败返回-1
 */
int wait_flag(struct control_channel *ctrl, ctrl_flag_t expected_flag);

/**
 * 关闭控制通道
 * @param ctrl 控制通道结构体指针
 */
void close_control_channel(struct control_channel *ctrl);

#endif // RDMA_CONTROL_ENUM_H