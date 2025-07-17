#ifndef MY_DOCA_SHM_H
#define MY_DOCA_SHM_H

#include <stddef.h>
#include <doca_mmap.h>
#include <doca_dev.h>

/* 共享内存区域描述符 */
struct my_shm_region {
    void *addr;              /* 内存区域起始地址 */
    size_t length;           /* 内存区域长度 */
    struct doca_mmap *mmap;  /* DOCA 内存映射对象 */
    unsigned int used;       /* 使用计数 */
    int block_id;            /* 块ID，用于关闭操作 */
};

/**
 * 打开或创建共享内存区域
 *
 * @param name         共享内存文件路径，必须在 hugetlbfs 挂载点
 * @param required_len 所需内存大小（字节）
 * @param dev          DOCA设备对象
 * @param out          输出：共享内存区域描述符
 *
 * @return 0成功，负数表示错误码
 */
int my_doca_open_or_create_shm(const char *name,
                               size_t required_len,
                               struct doca_dev *dev,
                               struct my_shm_region *out);

/**
 * 打开或创建共享内存区域，使用指定的块ID
 *
 * @param name         共享内存文件路径，必须在 hugetlbfs 挂载点
 * @param required_len 所需内存大小（字节）
 * @param dev          DOCA设备对象
 * @param out          输出：共享内存区域描述符
 * @param block_id     指定要使用的块ID
 *
 * @return 0成功，负数表示错误码
 */
int my_doca_open_or_create_shm_with_block_id(const char *name,
                                             size_t required_len,
                                             struct doca_dev *dev,
                                             struct my_shm_region *out,
                                             int block_id);

/**
 * 关闭共享内存区域
 *
 * @param reg 共享内存区域描述符
 */
void my_doca_close_shm(struct my_shm_region *reg);

int my_doca_open_or_create_shm_smart(const char *name,
                                     size_t required_len,
                                     struct doca_dev *dev,
                                     struct my_shm_region *out);


void my_doca_cleanup_all_blocks(void);

void print_block_status(void);

#endif /* MY_DOCA_SHM_H */