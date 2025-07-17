#define _GNU_SOURCE
#include "doca_hp_shm.h"

#include <doca_ctx.h>
#include <doca_error.h>
#include <doca_log.h>
#include <doca_mmap.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "time_utils.h"

DOCA_LOG_REGISTER(MY_DOCA::SHM);

/* ------------ 可调宏 ------------ */
#define SHM_NAME \
  "/dev/hugepages/mino_mino_doca_rdv_buf" /* 必在 hugetlbfs 目录 */
#define HUGEPAGE_SZ (2UL * 1024 * 1024)   /* 2 MB */
#define MAX_BLOCKS 10                     /* 最大块数 */
/* -------------------------------- */

/* 内存块结构 */
struct shm_block {
  void *addr;             /* 块的虚拟地址 */
  size_t offset;          /* 块在文件中的偏移 */
  size_t length;          /* 块的长度 */
  struct doca_mmap *mmap; /* 块的DOCA mmap */
  unsigned int used;      /* 使用计数 */
  int valid;              /* 块是否有效 */
  pthread_mutex_t lock;   /* 块级锁 */
};

/* 全局共享内存状态 */
static struct {
  int fd;                              /* 共享内存文件描述符 */
  size_t total_size;                   /* 文件总大小 */
  size_t next_offset;                  /* 下一个可用偏移 */
  struct shm_block blocks[MAX_BLOCKS]; /* 块列表 */
  int initialized;                     /* 初始化标志 */
  pthread_mutex_t global_lock;         /* 全局锁 */
} shm_state = {.fd = -1,
               .total_size = 0,
               .next_offset = 0,
               .initialized = 0,
               .global_lock = PTHREAD_MUTEX_INITIALIZER};

static int _ret_errno(const char *what) {
  DOCA_LOG_ERR("%s failed: %s", what, strerror(errno));
  return -errno;
}

/* 初始化共享内存管理系统 */
static int initialize_shm_system(const char *name) {
  struct stat st;

  if (shm_state.initialized) return 0;

  /* 打开或创建共享内存文件 */
  shm_state.fd = open(name, O_RDWR | O_CREAT, 0666);
  if (shm_state.fd < 0) return _ret_errno("open");

  /* 获取文件大小 */
  if (fstat(shm_state.fd, &st) < 0) {
    int rc = _ret_errno("fstat");
    close(shm_state.fd);
    shm_state.fd = -1;
    return rc;
  }

  shm_state.total_size = st.st_size;
  shm_state.next_offset = 0;

  /* 初始化块列表 */
  for (int i = 0; i < MAX_BLOCKS; i++) {
    shm_state.blocks[i].valid = 0;
    shm_state.blocks[i].used = 0;
    shm_state.blocks[i].addr = NULL;
    shm_state.blocks[i].mmap = NULL;
    shm_state.blocks[i].offset = 0;
    shm_state.blocks[i].length = 0;
    pthread_mutex_init(&shm_state.blocks[i].lock, NULL);
  }

  shm_state.initialized = 1;
  return 0;
}

/* 查找可用块 */
static int find_available_block(size_t required_len) {
  /* 先查找可复用的块 */
  for (int i = 0; i < MAX_BLOCKS; i++) {
    if (shm_state.blocks[i].valid &&
        shm_state.blocks[i].length >= required_len) {
      return i;
    }
  }

  /* 查找空闲槽位以创建新块 */
  for (int i = 0; i < MAX_BLOCKS; i++) {
    if (!shm_state.blocks[i].valid) {
      return i;
    }
  }

  return -1; /* 没有可用块 */
}

/* 创建新的内存块 */
static int create_new_block(int block_idx, size_t required_len,
                            struct doca_dev *dev) {
  struct shm_block *block = &shm_state.blocks[block_idx];
  size_t aligned_len = (required_len + (HUGEPAGE_SZ - 1)) & ~(HUGEPAGE_SZ - 1);
  size_t block_offset = shm_state.next_offset;
  doca_error_t drc;

  /* 确保文件足够大 */
  if (block_offset + aligned_len > shm_state.total_size) {
    /* 扩展文件大小 */
    if (ftruncate(shm_state.fd, block_offset + aligned_len) < 0) {
      return _ret_errno("ftruncate");
    }
    shm_state.total_size = block_offset + aligned_len;
    DOCA_LOG_INFO("Extended shared memory file to %zu bytes",
                  shm_state.total_size);
  }

  /* 映射内存块 */
  void *addr = mmap(NULL, aligned_len, PROT_READ | PROT_WRITE,
                    MAP_SHARED | MAP_HUGETLB, shm_state.fd, block_offset);
  if (addr == MAP_FAILED) {
    return _ret_errno("mmap");
  }

  /* 触实际用到的内存 */
  size_t page_sz = (size_t)sysconf(_SC_PAGESIZE);
  volatile char *p = (volatile char *)addr;
  for (size_t i = 0; i < required_len; i += page_sz) p[i] = 0;

  /* 创建DOCA mmap */
  struct doca_mmap *dm = NULL;
  drc = doca_mmap_create(&dm);
  if (drc != DOCA_SUCCESS) goto doca_fail;

  drc = doca_mmap_set_permissions(dm, DOCA_ACCESS_FLAG_LOCAL_READ_WRITE |
                                          DOCA_ACCESS_FLAG_RDMA_READ |
                                          DOCA_ACCESS_FLAG_RDMA_WRITE);
  if (drc != DOCA_SUCCESS) goto doca_fail;

  drc = doca_mmap_set_memrange(dm, addr, aligned_len);
  if (drc != DOCA_SUCCESS) goto doca_fail;

  drc = doca_mmap_add_dev(dm, dev);
  if (drc != DOCA_SUCCESS) goto doca_fail;

  drc = doca_mmap_enable_thread_safety(dm);
  if (drc != DOCA_SUCCESS) {
    DOCA_LOG_ERR("doca_mmap_enable_thread_safety: %s",
                 doca_error_get_descr(drc));
    goto doca_fail;
  }

  TIMER_START(dm_start);
  drc = doca_mmap_start(dm);
  TIMER_END(dm_start, "doca_mmap_start 耗时: ");
  if (drc != DOCA_SUCCESS) goto doca_fail;

  /* 更新块信息 */
  block->addr = addr;
  block->offset = block_offset;
  block->length = aligned_len;
  block->mmap = dm;
  block->used = 1;
  block->valid = 1;

  /* 更新下一个可用偏移 */
  shm_state.next_offset += aligned_len;

  DOCA_LOG_INFO("Created new block %d: offset=%zu size=%zu", block_idx,
                block_offset, aligned_len);
  return 0;

doca_fail:
  DOCA_LOG_ERR("init DOCA mmap failed: %s", doca_error_get_descr(drc));
  if (dm) doca_mmap_destroy(dm);
  munmap(addr, aligned_len);
  return drc;
}

int my_doca_open_or_create_shm(const char *name, size_t required_len,
                               struct doca_dev *dev,
                               struct my_shm_region *out) {
  int block_idx, rc;

  if (!name || !out || !dev || required_len == 0) return -EINVAL;

  /* 加全局锁 */
  pthread_mutex_lock(&shm_state.global_lock);

  /* 初始化共享内存系统 */
  rc = initialize_shm_system(name);
  if (rc != 0) {
    pthread_mutex_unlock(&shm_state.global_lock);
    return rc;
  }

  /* 查找可用块 */
  block_idx = find_available_block(required_len);
  if (block_idx < 0) {
    pthread_mutex_unlock(&shm_state.global_lock);
    return -ENOMEM;
  }

  /* 获取块锁 */
  struct shm_block *block = &shm_state.blocks[block_idx];
  pthread_mutex_lock(&block->lock);

  /* 释放全局锁 */
  pthread_mutex_unlock(&shm_state.global_lock);

  /* 检查是否已有有效块可复用 */
  if (block->valid) {
    block->used++;
    out->addr = block->addr;
    out->length = block->length;
    out->mmap = block->mmap;
    out->used = block->used;
    out->block_id = block_idx; /* 记录块ID，用于关闭 */
    pthread_mutex_unlock(&block->lock);
    DOCA_LOG_INFO("Reusing block %d, use count: %u", block_idx, block->used);
    return 0;
  }

  /* 创建新块 */
  rc = create_new_block(block_idx, required_len, dev);
  if (rc != 0) {
    pthread_mutex_unlock(&block->lock);
    return rc;
  }

  /* 设置输出参数 */
  out->addr = block->addr;
  out->length = block->length;
  out->mmap = block->mmap;
  out->used = block->used;
  out->block_id = block_idx;

  pthread_mutex_unlock(&block->lock);
  return 0;
}

void my_doca_close_shm(struct my_shm_region *reg) {
  if (!reg || reg->block_id < 0 || reg->block_id >= MAX_BLOCKS) return;

  struct shm_block *block = &shm_state.blocks[reg->block_id];

  pthread_mutex_lock(&block->lock);

  if (!block->valid || block->used == 0) {
    pthread_mutex_unlock(&block->lock);
    return;
  }

  block->used--;

  DOCA_LOG_INFO("减少块 %d 的使用计数至 %u", reg->block_id, block->used);

  // if (block->used == 0) {
  //     /* 最后一个使用者，真正释放块 */
  //     DOCA_LOG_INFO("块 %d 的最后一个使用者已完成，释放资源", reg->block_id);
  //     /* 完全释放块 */
  //     // DOCA_LOG_INFO("释放块 %d 的资源", reg->block_id);

  //     if (block->mmap) {
  //         doca_mmap_stop(block->mmap);
  //         doca_mmap_destroy(block->mmap);
  //     }

  //     if (block->addr) {
  //         munmap(block->addr, block->length);
  //     }

  //     block->addr = NULL;
  //     block->mmap = NULL;
  //     block->valid = 0;
  // }else {
  //     DOCA_LOG_INFO("块 %d 仍有 %u 个使用者，保留资源", reg->block_id,
  //     block->used);
  // }

  // 始终保持块有效
  if (block->used == 0) {
    DOCA_LOG_INFO("块 %d 引用计数为0，但保持资源不释放以供后续复用",
                  reg->block_id);
  } else {
    DOCA_LOG_INFO("块 %d 仍有 %u 个使用者", reg->block_id, block->used);
  }

  pthread_mutex_unlock(&block->lock);

  /* 清理输入结构 */
  reg->addr = NULL;
  reg->length = 0;
  reg->mmap = NULL;
  reg->used = 0;
  reg->block_id = -1;
}

int my_doca_open_or_create_shm_with_block_id(const char *name,
                                             size_t required_len,
                                             struct doca_dev *dev,
                                             struct my_shm_region *out,
                                             int block_id) {
  int rc;

  struct timespec t1, t2, t3, t4, t5, t6;
  double global_lock_time, block_lock_time, check_reuse_time, create_time,
      total_operation_time;

  // 记录函数开始时间
  clock_gettime(CLOCK_MONOTONIC, &t1);

  if (!name || !out || !dev || required_len == 0 || block_id < 0 ||
      block_id >= MAX_BLOCKS)
    return -EINVAL;

  // 尝试获取全局锁之前的时间
  clock_gettime(CLOCK_MONOTONIC, &t2);

  /* 加全局锁 */
  pthread_mutex_lock(&shm_state.global_lock);

  // 成功获取全局锁后的时间
  clock_gettime(CLOCK_MONOTONIC, &t3);

  /* 初始化共享内存系统 */
  rc = initialize_shm_system(name);
  if (rc != 0) {
    pthread_mutex_unlock(&shm_state.global_lock);
    return rc;
  }

  // 准备获取块锁的时间
  clock_gettime(CLOCK_MONOTONIC, &t4);

  /* 获取指定块的锁 */
  struct shm_block *block = &shm_state.blocks[block_id];
  pthread_mutex_lock(&block->lock);

  /* 释放全局锁 */
  pthread_mutex_unlock(&shm_state.global_lock);

  // 获取块锁后的时间
  clock_gettime(CLOCK_MONOTONIC, &t5);

  /* 检查是否已有有效块可复用 */
  if (block->valid) {
    block->used++;
    out->addr = block->addr;
    out->length = block->length;
    out->mmap = block->mmap;
    out->used = block->used;
    out->block_id = block_id;

    // 复用完成的时间
    clock_gettime(CLOCK_MONOTONIC, &t6);

    // 计算各阶段时间
    global_lock_time =
        (t3.tv_sec - t2.tv_sec) + (t3.tv_nsec - t2.tv_nsec) / 1e6;
    block_lock_time = (t5.tv_sec - t4.tv_sec) + (t5.tv_nsec - t4.tv_nsec) / 1e6;
    check_reuse_time =
        (t6.tv_sec - t5.tv_sec) + (t6.tv_nsec - t5.tv_nsec) / 1e6;
    // 实际操作时间（排除了锁等待时间）
    total_operation_time = check_reuse_time;

    DOCA_LOG_INFO(
        "Block %d 复用性能分析: 全局锁等待=%.2f毫秒, 块锁等待=%.2f毫秒, "
        "复用操作=%.2f毫秒, 纯操作时间=%.2f毫秒",
        block_id, global_lock_time, block_lock_time, check_reuse_time,
        total_operation_time);

    pthread_mutex_unlock(&block->lock);
    DOCA_LOG_INFO("Reusing specified block %d, use count: %u", block_id,
                  block->used);
    return 0;
  }

  /* 创建新块 */
  rc = create_new_block(block_id, required_len, dev);
  if (rc != 0) {
    pthread_mutex_unlock(&block->lock);
    return rc;
  }

  /* 设置输出参数 */
  out->addr = block->addr;
  out->length = block->length;
  out->mmap = block->mmap;
  out->used = block->used;
  out->block_id = block_id;

  // 创建完成的时间
  clock_gettime(CLOCK_MONOTONIC, &t6);

  // 计算各阶段时间
  global_lock_time = (t3.tv_sec - t2.tv_sec) + (t3.tv_nsec - t2.tv_nsec) / 1e6;
  block_lock_time = (t5.tv_sec - t4.tv_sec) + (t5.tv_nsec - t4.tv_nsec) / 1e6;
  create_time = (t6.tv_sec - t5.tv_sec) + (t6.tv_nsec - t5.tv_nsec) / 1e6;

  // 实际操作时间（排除了锁等待时间）
  total_operation_time = create_time;

  DOCA_LOG_INFO(
      "Block %d 创建性能分析: 全局锁等待=%.2f毫秒, 块锁等待=%.2f毫秒, "
      "创建操作=%.2f 毫秒, 纯操作时间=%.2f毫秒",
      block_id, global_lock_time, block_lock_time, create_time,
      total_operation_time);

  pthread_mutex_unlock(&block->lock);
  return 0;
}
int my_doca_open_or_create_shm_smart(const char *name, size_t required_len,
                                     struct doca_dev *dev,
                                     struct my_shm_region *out) {
  int rc;
  struct timespec t1, t2, t3, t4, t5, t6;
  double find_block_time, block_op_time;

  // 记录函数开始时间
  clock_gettime(CLOCK_MONOTONIC, &t1);

  if (!name || !out || !dev || required_len == 0) return -EINVAL;

  // 尝试获取全局锁之前的时间
  clock_gettime(CLOCK_MONOTONIC, &t2);

  /* 加全局锁 */
  pthread_mutex_lock(&shm_state.global_lock);

  // 成功获取全局锁后的时间
  clock_gettime(CLOCK_MONOTONIC, &t3);

  /* 初始化共享内存系统 */
  rc = initialize_shm_system(name);
  if (rc != 0) {
    pthread_mutex_unlock(&shm_state.global_lock);
    return rc;
  }

  /* 智能查找合适的块 */
  // TODO: 需要合适的策略选择已有的块，最好是该块大小与模型大小的差量最小
  int best_fit_idx = -1;
  size_t best_fit_size = SIZE_MAX;  // 初始设为最大值
  int free_idx = -1;                // 第一个空闲槽位

  for (int i = 0; i < MAX_BLOCKS; i++) {
    if (shm_state.blocks[i].valid) {
      // 已有块，检查大小是否合适
      if (shm_state.blocks[i].length >= required_len &&
          shm_state.blocks[i].length < best_fit_size) {
        // 找到更好的匹配
        best_fit_idx = i;
        best_fit_size = shm_state.blocks[i].length;
      }
    } else if (free_idx == -1) {
      // 记录第一个空闲槽位
      free_idx = i;
    }
  }

  // 找到块的时间
  clock_gettime(CLOCK_MONOTONIC, &t4);
  find_block_time = (t4.tv_sec - t3.tv_sec) + (t4.tv_nsec - t3.tv_nsec) / 1e6;

  int selected_idx;
  bool is_reusing = false;

  // 决定使用哪个块
  if (best_fit_idx >= 0) {
    // 有合适的现有块可复用
    selected_idx = best_fit_idx;
    is_reusing = true;
    DOCA_LOG_INFO("找到合适的块 %d 复用，大小: %zu (需要: %zu)", selected_idx,
                  shm_state.blocks[selected_idx].length, required_len);
  } else if (free_idx >= 0) {
    // 没有合适的块，但有空闲槽位
    selected_idx = free_idx;
    DOCA_LOG_INFO("没有合适的块可复用，将创建新块 %d，需要大小: %zu",
                  selected_idx, required_len);
  } else {
    // 没有空闲槽位
    pthread_mutex_unlock(&shm_state.global_lock);
    DOCA_LOG_ERR("没有可用的块槽位");
    return -ENOMEM;
  }

  /* 获取选中块的锁 */
  struct shm_block *block = &shm_state.blocks[selected_idx];
  pthread_mutex_lock(&block->lock);

  /* 释放全局锁 */
  pthread_mutex_unlock(&shm_state.global_lock);

  // 获取块锁后的时间
  clock_gettime(CLOCK_MONOTONIC, &t5);

  /* 处理块 */
  if (is_reusing) {
    // 复用现有块
    block->used++;
    out->addr = block->addr;
    out->length = block->length;
    out->mmap = block->mmap;
    out->used = block->used;
    out->block_id = selected_idx;

    // 复用完成的时间
    clock_gettime(CLOCK_MONOTONIC, &t6);
    block_op_time = (t6.tv_sec - t5.tv_sec) + (t6.tv_nsec - t5.tv_nsec) / 1e6;

    DOCA_LOG_INFO(
        "智能复用块 %d，使用计数: %u, 大小: %zu (需要: %zu), "
        "查找耗时: %.2f毫秒, 操作耗时: %.2f毫秒",
        selected_idx, block->used, block->length, required_len, find_block_time,
        block_op_time);
  } else {
    // 创建新块
    rc = create_new_block(selected_idx, required_len, dev);
    if (rc != 0) {
      pthread_mutex_unlock(&block->lock);
      return rc;
    }

    // 设置输出参数
    out->addr = block->addr;
    out->length = block->length;
    out->mmap = block->mmap;
    out->used = block->used;
    out->block_id = selected_idx;

    // 创建完成的时间
    clock_gettime(CLOCK_MONOTONIC, &t6);
    block_op_time = (t6.tv_sec - t5.tv_sec) + (t6.tv_nsec - t5.tv_nsec) / 1e6;

    DOCA_LOG_INFO(
        "智能创建块 %d，大小: %zu (需要: %zu), "
        "查找耗时: %.2f毫秒, 操作耗时: %.2f毫秒",
        selected_idx, block->length, required_len, find_block_time,
        block_op_time);
  }

  pthread_mutex_unlock(&block->lock);
  return 0;
}

void print_block_status(void) {
  pthread_mutex_lock(&shm_state.global_lock);

  DOCA_LOG_INFO("======= 内存块状态 =======");
  for (int i = 0; i < MAX_BLOCKS; i++) {
    if (shm_state.blocks[i].valid) {
      DOCA_LOG_INFO("块 %d: 有效=%d, 使用计数=%u, 大小=%zu, 地址=%p", i,
                    shm_state.blocks[i].valid, shm_state.blocks[i].used,
                    shm_state.blocks[i].length, shm_state.blocks[i].addr);
    }
  }
  DOCA_LOG_INFO("=========================");

  pthread_mutex_unlock(&shm_state.global_lock);
}

void my_doca_cleanup_all_blocks(void) {
  DOCA_LOG_INFO("清理所有共享内存块");

  pthread_mutex_lock(&shm_state.global_lock);

  for (int i = 0; i < MAX_BLOCKS; i++) {
    struct shm_block *block = &shm_state.blocks[i];

    if (block->valid) {
      pthread_mutex_lock(&block->lock);

      DOCA_LOG_INFO("清理块 %d (使用计数: %u)", i, block->used);

      if (block->mmap) {
        doca_mmap_stop(block->mmap);
        doca_mmap_destroy(block->mmap);
      }

      if (block->addr) {
        munmap(block->addr, block->length);
      }

      block->addr = NULL;
      block->mmap = NULL;
      block->valid = 0;
      block->used = 0;

      pthread_mutex_unlock(&block->lock);
    }
  }

  if (shm_state.fd >= 0) {
    close(shm_state.fd);
    shm_state.fd = -1;
  }

  shm_state.initialized = 0;
  shm_state.total_size = 0;
  shm_state.next_offset = 0;

  pthread_mutex_unlock(&shm_state.global_lock);

  DOCA_LOG_INFO("共享内存清理完成");
}