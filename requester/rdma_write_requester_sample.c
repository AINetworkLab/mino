#include <doca_buf.h>
#include <doca_buf_inventory.h>
#include <doca_ctx.h>
#include <doca_error.h>
#include <doca_log.h>

#include "rdma_common_pool_th.h"
// 时间工具头文件
#include <unistd.h>

#include "doca_hp_shm.h"
#include "mmap_pool.h"
#include "rdma_control_enum.h"
#include "time_utils.h"

// #include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

DOCA_LOG_REGISTER(RDMA_WRITE_REQUESTER::SAMPLE);

/*
 * Write the connection details for the responder to read,
 * and read the connection details and the remote mmap string of the responder
 * In DC transport mode it is only needed to read the remote connection details
 *
 * @cfg [in]: Configuration parameters
 * @resources [in/out]: RDMA resources
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
static doca_error_t write_read_connection(struct rdma_config *cfg,
                                          struct rdma_resources *resources) {
  doca_error_t result = DOCA_SUCCESS;

  if (cfg->transport_type == DOCA_RDMA_TRANSPORT_TYPE_RC) {
    /* Write the RDMA connection details */
    result = write_file(cfg->local_connection_desc_path,
                        (char *)resources->rdma_conn_descriptor,
                        resources->rdma_conn_descriptor_size);
    if (result != DOCA_SUCCESS) {
      DOCA_LOG_ERR("Failed to write the RDMA connection details: %s",
                   doca_error_get_descr(result));
      return result;
    }

    DOCA_LOG_INFO("You can now copy %s to the responder",
                  cfg->local_connection_desc_path);
  }

  DOCA_LOG_INFO(
      "Please copy %s and %s from the responder and then press enter after "
      "pressing enter in the responder side",
      cfg->remote_connection_desc_path, cfg->remote_resource_desc_path);

  /* Wait for enter */
  wait_for_enter();

  /* Read the remote RDMA connection details */
  result = read_file(cfg->remote_connection_desc_path,
                     (char **)&resources->remote_rdma_conn_descriptor,
                     &resources->remote_rdma_conn_descriptor_size);
  if (result != DOCA_SUCCESS) {
    DOCA_LOG_ERR("Failed to read the remote RDMA connection details: %s",
                 doca_error_get_descr(result));
    return result;
  }

  /* Read the remote mmap connection details */
  result = read_file(cfg->remote_resource_desc_path,
                     (char **)&resources->remote_mmap_descriptor,
                     &resources->remote_mmap_descriptor_size);
  if (result != DOCA_SUCCESS) {
    DOCA_LOG_ERR("Failed to read the remote RDMA mmap connection details: %s",
                 doca_error_get_descr(result));
    return result;
  }

  return result;
}

/*
 * RDMA write task completed callback
 *
 * @rdma_write_task [in]: Completed task
 * @task_user_data [in]: doca_data from the task
 * @ctx_user_data [in]: doca_data from the context
 */
static void rdma_write_completed_callback(
    struct doca_rdma_task_write *rdma_write_task,
    union doca_data task_user_data, union doca_data ctx_user_data) {
  struct rdma_resources *resources = (struct rdma_resources *)ctx_user_data.ptr;
  // doca_error_t *first_encountered_error = (doca_error_t *)task_user_data.ptr;
  // doca_error_t result = DOCA_SUCCESS, tmp_result;

  DOCA_LOG_INFO("RDMA write task was done Successfully");
  // // DOCA_LOG_INFO("Written to responder \"%s\"",
  // resources->cfg->write_string);

  // // 从 rdma_write_task 获取对应的 buf
  // const struct doca_buf *src_buf =
  // doca_rdma_task_write_get_src_buf(rdma_write_task); const struct doca_buf
  // *dst_buf = doca_rdma_task_write_get_dst_buf(rdma_write_task);
  // // 强制转换去掉 const，然后再传给 doca_buf_dec_refcount
  // struct doca_buf *non_const_src_buf = (struct doca_buf *)src_buf;
  // struct doca_buf *non_const_dst_buf = (struct doca_buf *)dst_buf;

  // size_t src_len, dst_len;
  // doca_buf_get_len(non_const_src_buf, &src_len);
  // doca_buf_get_len(non_const_dst_buf, &dst_len);

  // DOCA_LOG_INFO("回调处理任务: src_len=%zu, dst_len=%zu", src_len, dst_len);

  // void *range;
  // size_t len;
  // result = doca_mmap_get_memrange(resources->remote_mmap, &range, &len);
  // DOCA_LOG_INFO("回调时remote_mmap状态: range=%p, len=%zu", range, len);

  // doca_task_free(doca_rdma_task_write_as_task(rdma_write_task));

  // tmp_result = doca_buf_dec_refcount(non_const_dst_buf, NULL);

  // if (tmp_result != DOCA_SUCCESS) {
  // DOCA_LOG_ERR("Failed to decrease dst_buf count: %s",
  // doca_error_get_descr(tmp_result)); DOCA_ERROR_PROPAGATE(result,
  // tmp_result);
  // }
  // tmp_result = doca_buf_dec_refcount(non_const_src_buf, NULL);
  // if (tmp_result != DOCA_SUCCESS) {
  // DOCA_LOG_ERR("Failed to decrease src_buf count: %s",
  // doca_error_get_descr(tmp_result)); DOCA_ERROR_PROPAGATE(result,
  // tmp_result);
  // }

  // /* Update that an error was encountered, if any */
  // DOCA_ERROR_PROPAGATE(*first_encountered_error, tmp_result);

  // ws 报错的源头1
  // ，由于每次只提交一个任务，因此执行完之后，这里会执行--导致任务数为
  // 0.从而关闭 ctx
  resources->num_remaining_tasks--;
  DOCA_LOG_INFO("剩余的任务数量：%zu", resources->num_remaining_tasks);
  // TODO: 何时关闭连接和上下文的逻辑
  /* Stop context once all tasks are completed */
  if (resources->num_remaining_tasks == 0 && resources->close_ctx == true) {
    if (resources->cfg->use_rdma_cm == true)
      (void)rdma_cm_disconnect(resources);
    (void)doca_ctx_stop(resources->rdma_ctx);
    DOCA_LOG_INFO("关闭 ctx");
  }
}

// static void rdma_write_completed_callback(struct doca_rdma_task_write
// *rdma_write_task, 					  union doca_data
// task_user_data, 					  union doca_data
// ctx_user_data)
// {
// 	struct rdma_resources *resources = (struct rdma_resources
// *)ctx_user_data.ptr; 	doca_error_t *first_encountered_error =
// (doca_error_t
// *)task_user_data.ptr;
// 	// doca_error_t result = DOCA_SUCCESS, tmp_result;
// 	doca_error_t tmp_result = DOCA_SUCCESS;

// 	DOCA_LOG_INFO("RDMA write task was done Successfully");
// 	// DOCA_LOG_INFO("Written to responder \"%s\"",
// resources->cfg->write_string);

// 	doca_task_free(doca_rdma_task_write_as_task(rdma_write_task));
// 	DOCA_LOG_INFO("Task frees Successfully");
// 	DOCA_LOG_INFO("剩余的任务数量：%zu", resources->num_remaining_tasks);

// 	// tmp_result = doca_buf_dec_refcount(resources->dst_buf, NULL);
// 	// if (tmp_result != DOCA_SUCCESS) {
// 	// 	DOCA_LOG_ERR("Failed to decrease dst_buf count: %s",
// doca_error_get_descr(tmp_result));
// 	// 	DOCA_ERROR_PROPAGATE(result, tmp_result);
// 	// }
// 	// tmp_result = doca_buf_dec_refcount(resources->src_buf, NULL);
// 	// if (tmp_result != DOCA_SUCCESS) {
// 	// 	DOCA_LOG_ERR("Failed to decrease src_buf count: %s",
// doca_error_get_descr(tmp_result));
// 	// 	DOCA_ERROR_PROPAGATE(result, tmp_result);
// 	// }

// 	DOCA_LOG_INFO("src_buf 和 dst_buf 的引用计数减少成功");
// 	/* Update that an error was encountered, if any */
// 	DOCA_ERROR_PROPAGATE(*first_encountered_error, tmp_result);

// 	resources->num_remaining_tasks--;
// 	/* Stop context once all tasks are completed */
// 	if (resources->num_remaining_tasks == 0) {
// 		if (resources->cfg->use_rdma_cm == true){
// 			DOCA_LOG_INFO("rdma_cm_disconnect 的报错");
// 			(void)rdma_cm_disconnect(resources);
// 		}
// 		DOCA_LOG_INFO("doca_ctx_stop 的报错");
// 		(void)doca_ctx_stop(resources->rdma_ctx);
// 	}
// }

/*
 * RDMA write task error callback
 *
 * @rdma_write_task [in]: failed task
 * @task_user_data [in]: doca_data from the task
 * @ctx_user_data [in]: doca_data from the context
 */
static void rdma_write_error_callback(
    struct doca_rdma_task_write *rdma_write_task,
    union doca_data task_user_data, union doca_data ctx_user_data) {
  struct rdma_resources *resources = (struct rdma_resources *)ctx_user_data.ptr;
  struct doca_task *task = doca_rdma_task_write_as_task(rdma_write_task);
  doca_error_t *first_encountered_error = (doca_error_t *)task_user_data.ptr;
  doca_error_t result;

  /* Update that an error was encountered */
  result = doca_task_get_status(task);
  DOCA_ERROR_PROPAGATE(*first_encountered_error, result);
  DOCA_LOG_ERR("RDMA write task failed: %s", doca_error_get_descr(result));

  result = doca_buf_dec_refcount(resources->dst_buf, NULL);
  if (result != DOCA_SUCCESS)
    DOCA_LOG_ERR("Failed to decrease dst_buf count: %s",
                 doca_error_get_descr(result));
  result = doca_buf_dec_refcount(resources->src_buf, NULL);
  if (result != DOCA_SUCCESS)
    DOCA_LOG_ERR("Failed to decrease src_buf count: %s",
                 doca_error_get_descr(result));
  doca_task_free(task);

  resources->num_remaining_tasks--;
  /* Stop context once all tasks are completed */
  if (resources->num_remaining_tasks == 0) {
    if (resources->cfg->use_rdma_cm == true)
      (void)rdma_cm_disconnect(resources);
    (void)doca_ctx_stop(resources->rdma_ctx);
    DOCA_LOG_INFO("关闭 ctx");
  }
}

/*
 * Export and receive connection details, and connect to the remote RDMA
 *
 * @resources [in]: RDMA resources
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
static doca_error_t rdma_write_requester_export_and_connect(
    struct rdma_resources *resources) {
  doca_error_t result;

  if (resources->cfg->use_rdma_cm == true) return rdma_cm_connect(resources);

  /* Export RDMA connection details */
  result = doca_rdma_export(resources->rdma, &(resources->rdma_conn_descriptor),
                            &(resources->rdma_conn_descriptor_size),
                            &(resources->connections[0]));
  if (result != DOCA_SUCCESS) {
    DOCA_LOG_ERR("Failed to export RDMA: %s", doca_error_get_descr(result));
    return result;
  }

  /* write and read connection details to the responder */
  result = write_read_connection(resources->cfg, resources);
  if (result != DOCA_SUCCESS) {
    DOCA_LOG_ERR(
        "Failed to write and read connection details from responder: %s",
        doca_error_get_descr(result));
    return result;
  }

  /* Connect RDMA */
  result = doca_rdma_connect(
      resources->rdma, resources->remote_rdma_conn_descriptor,
      resources->remote_rdma_conn_descriptor_size, resources->connections[0]);
  if (result != DOCA_SUCCESS)
    DOCA_LOG_ERR(
        "Failed to connect the requester's RDMA to the responder's RDMA: %s",
        doca_error_get_descr(result));

  return result;
}

/*
 * Prepare and submit RDMA write task
 *
 * @resources [in]: RDMA resources
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
//  ws  准备和提交 rdma 写任务实现
#define NUM_TASKS (2)
// #define MEM_RANGE_LEN (104857600)
// #define MAX_BUFF_SIZE (134217728)
// #define MAX_BUFF_SIZE (256)

static doca_error_t rdma_write_prepare_and_submit_task(
    struct rdma_resources *resources) {
  struct doca_rdma_task_write *rdma_write_task = NULL;
  union doca_data task_user_data = {0};
  char *remote_mmap_range;
  size_t remote_mmap_range_len;
  void *src_buf_data;
  doca_error_t result, tmp_result;

  struct doca_buf *src_buf = NULL;
  struct doca_buf *dst_buf = NULL;

  char *data_ptr = resources->cfg->data_ptr;
  size_t data_len = resources->cfg->data_len;

  /* Create remote mmap */
  result = doca_mmap_create_from_export(NULL, resources->remote_mmap_descriptor,
                                        resources->remote_mmap_descriptor_size,
                                        resources->doca_device,
                                        &(resources->remote_mmap));
  if (result != DOCA_SUCCESS) {
    DOCA_LOG_ERR("Failed to create mmap from export: %s",
                 doca_error_get_descr(result));
    return result;
  }

  /* Get the remote mmap memory range */
  result = doca_mmap_get_memrange(resources->remote_mmap,
                                  (void **)&remote_mmap_range,
                                  &remote_mmap_range_len);
  if (result != DOCA_SUCCESS) {
    DOCA_LOG_ERR("Failed to get DOCA memory map range: %s",
                 doca_error_get_descr(result));
    return result;
  }

  DOCA_LOG_INFO("文件大小为：%zu", data_len);

  DOCA_LOG_INFO("remote_mmap_range 的地址为：%p", remote_mmap_range);
  DOCA_LOG_INFO("remote_mmap_range_len 的大小为：%zu", remote_mmap_range_len);

  // 4. 为每个分块创建 src_buf/dst_buf 并提交 RDMA 写任务
  // 分块
  bool flag = 1;
  size_t head_len = sizeof(flag) + sizeof(data_len);
  size_t total_len = head_len + data_len;
  char *combined_task = malloc(total_len);
  if (combined_task == NULL) {
    DOCA_LOG_ERR("Failed to allocate memory for combined task");
    return result;
  }

  memcpy(combined_task, &flag, sizeof(flag));
  memcpy(combined_task + sizeof(flag), &data_len, sizeof(data_len));
  memcpy(combined_task + head_len, data_ptr, data_len);

  DOCA_LOG_INFO("本次传输的总数据块大小为：%zu", total_len);
  DOCA_LOG_INFO("有效数据块大小为：%zu", data_len);

  DOCA_LOG_INFO("buf_inventory 的地址为：%p", resources->buf_inventory);
  DOCA_LOG_INFO("mmap 的地址为：%p", resources->mmap);

  TIMER_START(mem_write);
  /* Add src buffer to DOCA buffer inventory from the remote mmap */
  result = doca_buf_inventory_buf_get_by_data(
      resources->buf_inventory, resources->mmap, resources->mmap_memrange,
      total_len, &src_buf);
  if (result != DOCA_SUCCESS) {
    DOCA_LOG_ERR("Failed to allocate DOCA buffer to DOCA buffer inventory: %s",
                 doca_error_get_descr(result));
    return result;
  }

  /* Set data of src buffer to be the string we want to write */
  result = doca_buf_get_data(src_buf, &src_buf_data);
  if (result != DOCA_SUCCESS) {
    DOCA_LOG_ERR("Failed to get source buffer data: %s",
                 doca_error_get_descr(result));
    goto destroy_src_buf;
  }

  memcpy(src_buf_data, combined_task, total_len);

  /* Add dst buffer to DOCA buffer inventory */
  result = doca_buf_inventory_buf_get_by_addr(
      resources->buf_inventory, resources->remote_mmap, remote_mmap_range,
      total_len, &dst_buf);
  if (result != DOCA_SUCCESS) {
    DOCA_LOG_ERR("Failed to allocate DOCA buffer to DOCA buffer inventory: %s",
                 doca_error_get_descr(result));
    goto destroy_src_buf;
  }

  /* Include first_encountered_error in user data of task to be used in the
   * callbacks */
  task_user_data.ptr = &(resources->first_encountered_error);

  /* Allocate and construct RDMA write task */
  result = doca_rdma_task_write_allocate_init(
      resources->rdma, resources->connections[0], src_buf, dst_buf,
      task_user_data, &rdma_write_task);
  if (result != DOCA_SUCCESS) {
    DOCA_LOG_ERR("Failed to allocate RDMA write task: %s",
                 doca_error_get_descr(result));
    goto destroy_dst_buf;
  }

  // DOCA_LOG_INFO("dst_buf 地址: %p", dst_buf);
  // DOCA_LOG_INFO("src_buf 地址: %p", src_buf);
  // void *src_data = NULL, *dst_data = NULL;
  // doca_buf_get_data(src_buf, &src_data);
  // doca_buf_get_data(dst_buf, &dst_data);

  // DOCA_LOG_INFO("src_data 指针 = %p", src_data);
  // DOCA_LOG_INFO("dst_data 指针 = %p", dst_data);

  DOCA_LOG_INFO("mmap_memrange 的地址为：%p", resources->mmap_memrange);

  if (resources->mmap_memrange == NULL) {
    DOCA_LOG_ERR("mmap_memrange 是 NULL！");
    return DOCA_ERROR_INVALID_VALUE;
  }

  /* Submit RDMA write task */
  DOCA_LOG_INFO("Submitting RDMA write task that writes to the responder");
  resources->num_remaining_tasks++;
  result = doca_task_submit(doca_rdma_task_write_as_task(rdma_write_task));
  if (result != DOCA_SUCCESS) {
    DOCA_LOG_ERR("Failed to submit RDMA write task: %s",
                 doca_error_get_descr(result));
    goto free_task;
  }

  TIMER_END(mem_write, "内存写入耗时(不含写入完成返回信号时间): ");

  // send_flag(resources->ctrl->sockfd, CTRL_WRITE_FINISH);
  send_flag(resources->ctrl, CTRL_WRITE_FINISH);
  DOCA_LOG_INFO("写完成发送，准备等待 ACK");
  wait_flag(resources->ctrl, CTRL_ACK);
  DOCA_LOG_INFO("收到接收方回复的 ACK");
  // send_flag(resources->ctrl->sockfd, CTRL_WRITE_FINISH);
  send_flag(resources->ctrl, CTRL_WRITE_FINISH);

  free(combined_task);

  return result;

free_task:
  doca_task_free(doca_rdma_task_write_as_task(rdma_write_task));
destroy_dst_buf:
  tmp_result = doca_buf_dec_refcount(dst_buf, NULL);
  if (tmp_result != DOCA_SUCCESS) {
    DOCA_LOG_ERR("Failed to decrease dst_buf count: %s",
                 doca_error_get_descr(tmp_result));
    DOCA_ERROR_PROPAGATE(result, tmp_result);
  }
destroy_src_buf:
  tmp_result = doca_buf_dec_refcount(src_buf, NULL);
  if (tmp_result != DOCA_SUCCESS) {
    DOCA_LOG_ERR("Failed to decrease src_buf count: %s",
                 doca_error_get_descr(tmp_result));
    DOCA_ERROR_PROPAGATE(result, tmp_result);
  }
  return result;
}

// 构造数据包：header + data
typedef struct {
  size_t chunk_idx;
  size_t total_chunks;
  size_t chunk_data_size;
  size_t total_data_size;  // 添加这个字段
  bool is_last_chunk;
} chunk_header_t;

static doca_error_t send_single_chunk(struct rdma_resources *resources,
                                      char *chunk_data, size_t chunk_data_size,
                                      size_t chunk_idx, size_t total_chunks,
                                      size_t total_data_size,
                                      bool is_last_chunk,
                                      char *remote_mmap_range) {
  struct doca_buf *src_buf = NULL;
  struct doca_buf *dst_buf = NULL;
  struct doca_rdma_task_write *rdma_write_task = NULL;
  void *src_buf_data;
  union doca_data task_user_data = {0};
  doca_error_t result, tmp_result;

  chunk_header_t header = {.chunk_idx = chunk_idx,
                           .total_chunks = total_chunks,
                           .chunk_data_size = chunk_data_size,
                           .total_data_size = total_data_size,
                           .is_last_chunk = is_last_chunk};

  if (is_last_chunk) {
    resources->close_ctx = true;
  }

  TIMER_START(bef);

  size_t total_chunk_size = sizeof(header) + chunk_data_size;

  DOCA_LOG_INFO("进入到了send_single_chunk，发送第 %zu 块", chunk_idx);

  // 每次都重新分配 src_buf
  result = doca_buf_inventory_buf_get_by_data(
      resources->buf_inventory, resources->mmap, resources->mmap_memrange,
      resources->cfg->mem_len, &src_buf);
  if (result != DOCA_SUCCESS) {
    DOCA_LOG_ERR("Failed to allocate src buffer for chunk %zu: %s", chunk_idx,
                 doca_error_get_descr(result));
    return result;
  }

  result = doca_buf_get_data(src_buf, &src_buf_data);
  if (result != DOCA_SUCCESS) {
    DOCA_LOG_ERR("Failed to get src buffer data for chunk %zu: %s", chunk_idx,
                 doca_error_get_descr(result));
    goto destroy_src_buf;
  }

  // 每次都重新分配 dst_buf
  result = doca_buf_inventory_buf_get_by_addr(
      resources->buf_inventory, resources->remote_mmap, remote_mmap_range,
      resources->cfg->mem_len, &dst_buf);
  if (result != DOCA_SUCCESS) {
    DOCA_LOG_ERR("Failed to allocate dst buffer for chunk %zu: %s", chunk_idx,
                 doca_error_get_descr(result));
    goto destroy_src_buf;
  }

  // 复制数据到缓冲区
  memcpy(src_buf_data, &header, sizeof(header));
  memcpy((char *)src_buf_data + sizeof(header), chunk_data, chunk_data_size);

  // 创建 RDMA 写任务
  task_user_data.ptr = &(resources->first_encountered_error);
  result = doca_rdma_task_write_allocate_init(
      resources->rdma, resources->connections[0], src_buf, dst_buf,
      task_user_data, &rdma_write_task);
  if (result != DOCA_SUCCESS) {
    DOCA_LOG_ERR("Failed to allocate RDMA write task for chunk %zu: %s",
                 chunk_idx, doca_error_get_descr(result));
    goto destroy_dst_buf;
  }

  DOCA_LOG_INFO("提交第 %zu 块 RDMA 写任务 (大小: %zu)", chunk_idx,
                total_chunk_size);
  resources->num_remaining_tasks++;
  result = doca_task_submit(doca_rdma_task_write_as_task(rdma_write_task));
  if (result != DOCA_SUCCESS) {
    DOCA_LOG_ERR("Failed to submit RDMA write task for chunk %zu: %s",
                 chunk_idx, doca_error_get_descr(result));
    goto free_task;
  }

  TIMER_END(bef, "块发送前耗时 ");

  TIMER_START(chunk_write);
  // 等待任务完成
  while (resources->num_remaining_tasks > 0) {
    if (doca_pe_progress(resources->pe) == 0) {
      nanosleep((const struct timespec[]){{0, 1000L}}, NULL);
    }
  }
  TIMER_END(chunk_write, "写入耗时: ");
  // 清理当前块的资源
  doca_task_free(doca_rdma_task_write_as_task(rdma_write_task));

  tmp_result = doca_buf_dec_refcount(dst_buf, NULL);
  if (tmp_result != DOCA_SUCCESS) {
    DOCA_LOG_ERR("Failed to decrease dst_buf count: %s",
                 doca_error_get_descr(tmp_result));
  }

  tmp_result = doca_buf_dec_refcount(src_buf, NULL);
  if (tmp_result != DOCA_SUCCESS) {
    DOCA_LOG_ERR("Failed to decrease src_buf count: %s",
                 doca_error_get_descr(tmp_result));
  }

  return DOCA_SUCCESS;

free_task:
  doca_task_free(doca_rdma_task_write_as_task(rdma_write_task));
destroy_dst_buf:
  tmp_result = doca_buf_dec_refcount(dst_buf, NULL);
  if (tmp_result != DOCA_SUCCESS) {
    DOCA_LOG_ERR("Failed to decrease dst_buf count: %s",
                 doca_error_get_descr(tmp_result));
    DOCA_ERROR_PROPAGATE(result, tmp_result);
  }
destroy_src_buf:
  tmp_result = doca_buf_dec_refcount(src_buf, NULL);
  if (tmp_result != DOCA_SUCCESS) {
    DOCA_LOG_ERR("Failed to decrease src_buf count: %s",
                 doca_error_get_descr(tmp_result));
    DOCA_ERROR_PROPAGATE(result, tmp_result);
  }
  return result;
}

// 修改后的主传输函数
static doca_error_t rdma_write_prepare_and_submit_task_chunked(
    struct rdma_resources *resources) {
  TIMER_START(all);
  char *remote_mmap_range;
  size_t remote_mmap_range_len;
  doca_error_t result;

  // 保存原始指针，避免修改 resources->cfg
  char *data_ptr = resources->cfg->data_ptr;
  size_t data_len = resources->cfg->data_len;

  // // 为了测试分块传输，人为放大数据大小
  // size_t test_multiplier = 30;
  // size_t enlarged_data_len = original_data_len * test_multiplier;

  // char *enlarged_data = malloc(enlarged_data_len);
  // if (enlarged_data == NULL) {
  //     DOCA_LOG_ERR("Failed to allocate enlarged test data");
  //     return DOCA_ERROR_NO_MEMORY;
  // }

  // TIMER_START(large);
  // for (size_t i = 0; i < test_multiplier; i++) {
  //     memcpy(enlarged_data + i * original_data_len, original_data_ptr,
  //     original_data_len);
  // }
  // TIMER_END(large, "放大数据耗时: ");
  // char* data_ptr = enlarged_data;
  // size_t data_len = enlarged_data_len;

  TIMER_START(remo);
  // 创建远程 mmap
  result = doca_mmap_create_from_export(NULL, resources->remote_mmap_descriptor,
                                        resources->remote_mmap_descriptor_size,
                                        resources->doca_device,
                                        &(resources->remote_mmap));
  if (result != DOCA_SUCCESS) {
    DOCA_LOG_ERR("Failed to create mmap from export: %s",
                 doca_error_get_descr(result));
    // free(enlarged_data);
    return result;
  }

  result = doca_mmap_get_memrange(resources->remote_mmap,
                                  (void **)&remote_mmap_range,
                                  &remote_mmap_range_len);
  if (result != DOCA_SUCCESS) {
    DOCA_LOG_ERR("Failed to get DOCA memory map range: %s",
                 doca_error_get_descr(result));
    // free(enlarged_data);
    return result;
  }
  TIMER_END(remo, "获取远程 mmap 耗时: ");

  // 计算分块参数
  size_t header_len = sizeof(chunk_header_t);
  size_t max_data_per_chunk = resources->cfg->mem_len - header_len;
  size_t total_chunks =
      (data_len + max_data_per_chunk - 1) / max_data_per_chunk;

  DOCA_LOG_INFO("总文件大小: %zu, 远程缓冲区大小: %zu", data_len,
                remote_mmap_range_len);
  DOCA_LOG_INFO("将分为 %zu 块传输，每块最大数据: %zu", total_chunks,
                max_data_per_chunk);

  size_t bytes_sent = 0;

  TIMER_END(all, "传输第一块前耗时");
  // 逐块传输，每次重新分配和释放缓冲区
  for (size_t chunk_idx = 0; chunk_idx < total_chunks; chunk_idx++) {
    size_t remaining_bytes = data_len - bytes_sent;
    size_t current_chunk_data_size = (remaining_bytes > max_data_per_chunk)
                                         ? max_data_per_chunk
                                         : remaining_bytes;

    bool is_last_chunk = (chunk_idx == total_chunks - 1);

    // 发送当前块
    result = send_single_chunk(resources, data_ptr + bytes_sent,
                               current_chunk_data_size, chunk_idx, total_chunks,
                               data_len, is_last_chunk, remote_mmap_range);

    if (result != DOCA_SUCCESS) {
      DOCA_LOG_ERR("发送第 %zu 块失败", chunk_idx);
      //   free(enlarged_data);
      return result;
    }

    // 发送写完成信号并等待 ACK
    // send_flag(resources->ctrl->sockfd, CTRL_WRITE_FINISH);
    send_flag(resources->ctrl, CTRL_WRITE_FINISH);
    DOCA_LOG_INFO("第 %zu 块写完成，等待接收方 ACK", chunk_idx);
    TIMER_START(waitf);
    wait_flag(resources->ctrl, CTRL_ACK);
    TIMER_END(waitf, "等待 块ACK 耗时");
    DOCA_LOG_INFO("收到第 %zu 块的 ACK，可以继续", chunk_idx);

    bytes_sent += current_chunk_data_size;
    DOCA_LOG_INFO("已发送 %zu/%zu 块, 累计: %zu/%zu 字节", chunk_idx + 1,
                  total_chunks, bytes_sent, data_len);
  }

  DOCA_LOG_INFO("所有 %zu 块传输完成", total_chunks);
  //   free(enlarged_data);
  return DOCA_SUCCESS;
}

// static doca_error_t send_single_chunk(struct rdma_resources *resources,
// 									 struct
// doca_buf *src_buf,
//                                      struct doca_buf *dst_buf,
//                                      void *src_buf_data,
//                                      char *chunk_data,
//                                      size_t chunk_data_size,
//                                      size_t chunk_idx,
//                                      size_t total_chunks,
// 									 size_t
// total_data_size,
//                                      bool is_last_chunk,
//                                      char *remote_mmap_range,
//                                      size_t remote_mmap_range_len)
// {
//     struct doca_rdma_task_write *rdma_write_task = NULL;
//     union doca_data task_user_data = {0};
//     // struct doca_buf *src_buf = NULL;
//     // struct doca_buf *dst_buf = NULL;
//     // void *src_buf_data;
//     doca_error_t result, tmp_result;

//     chunk_header_t header = {
//     .chunk_idx = chunk_idx,
//     .total_chunks = total_chunks,
//     .chunk_data_size = chunk_data_size,
//     .total_data_size = total_data_size,  // 添加总数据大小
//     .is_last_chunk = is_last_chunk
// 	};

//     size_t total_chunk_size = sizeof(header) + chunk_data_size;
//     // char *combined_chunk = malloc(total_chunk_size);
//     // if (combined_chunk == NULL) {
//     //     DOCA_LOG_ERR("Failed to allocate memory for chunk %zu",
//     chunk_idx);
//     //     return DOCA_ERROR_NO_MEMORY;
//     // }

//     // memcpy(combined_chunk, &header, sizeof(header));
//     // memcpy(combined_chunk + sizeof(header), chunk_data, chunk_data_size);

//     TIMER_START(chunk_write);

// 	DOCA_LOG_INFO("进入到了send_single_chunk");

// 	memcpy(src_buf_data, &header, sizeof(header));
//     memcpy((char*)src_buf_data + sizeof(header), chunk_data,
//     chunk_data_size);

//     // // 创建源缓冲区
//     // result = doca_buf_inventory_buf_get_by_data(resources->buf_inventory,
//     //                         resources->mmap,
//     //                         resources->mmap_memrange,
//     //                         resources->cfg->mem_len,
//     //                         &src_buf);
//     // if (result != DOCA_SUCCESS) {
//     //     DOCA_LOG_ERR("Failed to allocate src buffer for chunk %zu: %s",
//     //                 chunk_idx, doca_error_get_descr(result));
//     //     goto cleanup_malloc;
//     // }

//     // result = doca_buf_get_data(src_buf, &src_buf_data);
//     // if (result != DOCA_SUCCESS) {
//     //     DOCA_LOG_ERR("Failed to get src buffer data for chunk %zu: %s",
//     //                 chunk_idx, doca_error_get_descr(result));
//     //     goto destroy_src_buf;
//     // }

//     // memcpy(src_buf_data, combined_chunk, total_chunk_size);

//     // // 创建目标缓冲区
//     // result = doca_buf_inventory_buf_get_by_addr(resources->buf_inventory,
//     //                         resources->remote_mmap,
//     //                         remote_mmap_range,
//     //                         resources->cfg->mem_len,
//     //                         &dst_buf);
//     // if (result != DOCA_SUCCESS) {
//     //     DOCA_LOG_ERR("Failed to allocate dst buffer for chunk %zu: %s",
//     //                 chunk_idx, doca_error_get_descr(result));
//     //     goto destroy_src_buf;
//     // }

//     // 创建并提交 RDMA 写任务
//     task_user_data.ptr = &(resources->first_encountered_error);
//     result = doca_rdma_task_write_allocate_init(resources->rdma,
//                             resources->connections[0],
//                             src_buf,
//                             dst_buf,
//                             task_user_data,
//                             &rdma_write_task);
//     if (result != DOCA_SUCCESS) {
//         DOCA_LOG_ERR("Failed to allocate RDMA write task for chunk %zu: %s",
//                     chunk_idx, doca_error_get_descr(result));
//         goto destroy_dst_buf;
//     }

//     DOCA_LOG_INFO("提交第 %zu 块 RDMA 写任务 (大小: %zu)", chunk_idx,
//     total_chunk_size); resources->num_remaining_tasks++; result =
//     doca_task_submit(doca_rdma_task_write_as_task(rdma_write_task)); if
//     (result != DOCA_SUCCESS) {
//         DOCA_LOG_ERR("Failed to submit RDMA write task for chunk %zu: %s",
//                     chunk_idx, doca_error_get_descr(result));
//         goto free_task;
//     }

//     TIMER_END(chunk_write, "写入耗时: ");

//     // 发送写完成信号并等待 ACK
//     // send_flag(resources->ctrl->sockfd, CTRL_WRITE_FINISH);
//     // DOCA_LOG_INFO("第 %zu 块写完成，等待接收方 ACK", chunk_idx);
//     // wait_flag(resources->ctrl, CTRL_ACK);
//     // DOCA_LOG_INFO("收到第 %zu 块的 ACK，可以继续", chunk_idx);

//     // free(combined_chunk);

//     return result;

// free_task:
//     doca_task_free(doca_rdma_task_write_as_task(rdma_write_task));
// destroy_dst_buf:
//     tmp_result = doca_buf_dec_refcount(dst_buf, NULL);
//     if (tmp_result != DOCA_SUCCESS) {
//         DOCA_LOG_ERR("Failed to decrease dst_buf count: %s",
//         doca_error_get_descr(tmp_result)); DOCA_ERROR_PROPAGATE(result,
//         tmp_result);
//     }
// // destroy_src_buf:
// //     tmp_result = doca_buf_dec_refcount(src_buf, NULL);
// //     if (tmp_result != DOCA_SUCCESS) {
// //         DOCA_LOG_ERR("Failed to decrease src_buf count: %s",
// doca_error_get_descr(tmp_result));
// //         DOCA_ERROR_PROPAGATE(result, tmp_result);
// //     }
// // cleanup_malloc:
//     // free(combined_chunk);
//     return result;
// }

// // ws 自定义 chunked 发送函数
// static doca_error_t rdma_write_prepare_and_submit_task_chunked(struct
// rdma_resources *resources)
// {
//     char *remote_mmap_range;
//     size_t remote_mmap_range_len;
//     doca_error_t result, tmp_result;
// 	struct doca_buf *src_buf = NULL;
//     struct doca_buf *dst_buf = NULL;
//     void *src_buf_data;

//     // char* data_ptr = resources->cfg->data_ptr;
//     // size_t data_len = resources->cfg->data_len;

// 	// 保存原始指针，避免修改 resources->cfg
//     char* original_data_ptr = resources->cfg->data_ptr;
//     size_t original_data_len = resources->cfg->data_len;

// 	// 为了测试分块传输，人为放大数据大小
//    // 放大数据
//     size_t test_multiplier = 8;
//     size_t enlarged_data_len = original_data_len * test_multiplier;

//     char *enlarged_data = malloc(enlarged_data_len);
//     if (enlarged_data == NULL) {
//         DOCA_LOG_ERR("Failed to allocate enlarged test data");
//         return DOCA_ERROR_NO_MEMORY;
//     }

//     for (size_t i = 0; i < test_multiplier; i++) {
//         memcpy(enlarged_data + i * original_data_len, original_data_ptr,
//         original_data_len);
//     }

//     // 使用局部变量，不修改 resources->cfg
//     char* data_ptr = enlarged_data;
//     size_t data_len = enlarged_data_len;

//     // 创建远程 mmap (保持原有逻辑)
//     result = doca_mmap_create_from_export(NULL,
//                         resources->remote_mmap_descriptor,
//                         resources->remote_mmap_descriptor_size,
//                         resources->doca_device,
//                         &(resources->remote_mmap));
//     if (result != DOCA_SUCCESS) {
//         DOCA_LOG_ERR("Failed to create mmap from export: %s",
//         doca_error_get_descr(result)); return result;
//     }

//     result = doca_mmap_get_memrange(resources->remote_mmap, (void
//     **)&remote_mmap_range, &remote_mmap_range_len); if (result !=
//     DOCA_SUCCESS) {
//         DOCA_LOG_ERR("Failed to get DOCA memory map range: %s",
//         doca_error_get_descr(result)); return result;
//     }

// 	// ========== 一次性创建 src_buf 和 dst_buf ==========
//     result = doca_buf_inventory_buf_get_by_data(resources->buf_inventory,
//                             resources->mmap,
//                             resources->mmap_memrange,
//                             resources->cfg->mem_len,
//                             &src_buf);
//     if (result != DOCA_SUCCESS) {
//         DOCA_LOG_ERR("Failed to allocate src buffer: %s",
//         doca_error_get_descr(result)); goto destroy_src_buf;
//     }

//     result = doca_buf_get_data(src_buf, &src_buf_data);
//     if (result != DOCA_SUCCESS) {
//         DOCA_LOG_ERR("Failed to get src buffer data: %s",
//         doca_error_get_descr(result)); goto destroy_src_buf;
//     }

//     result = doca_buf_inventory_buf_get_by_addr(resources->buf_inventory,
//                             resources->remote_mmap,
//                             remote_mmap_range,
//                             resources->cfg->mem_len,
//                             &dst_buf);
//     if (result != DOCA_SUCCESS) {
//         DOCA_LOG_ERR("Failed to allocate dst buffer: %s",
//         doca_error_get_descr(result)); goto destroy_dst_buf;
//     }

//     DOCA_LOG_INFO("总文件大小: %zu, 远程缓冲区大小: %zu", data_len,
//     remote_mmap_range_len);

//     // 计算分块参数
//     size_t header_len = sizeof(chunk_header_t);
//     size_t max_data_per_chunk = resources->cfg->mem_len - header_len;
//     size_t total_chunks = (data_len + max_data_per_chunk - 1) /
//     max_data_per_chunk; size_t bytes_sent = 0;

//     DOCA_LOG_INFO("将分为 %zu 块传输，每块最大数据: %zu", total_chunks,
//     max_data_per_chunk);

//     // 逐块传输
//     for (size_t chunk_idx = 0; chunk_idx < total_chunks; chunk_idx++) {
//         size_t remaining_bytes = data_len - bytes_sent;
//         size_t current_chunk_data_size = (remaining_bytes >
//         max_data_per_chunk) ?
//                                         max_data_per_chunk : remaining_bytes;

//         bool is_last_chunk = (chunk_idx == total_chunks - 1);

//         result = send_single_chunk(resources,
// 								 src_buf,
// 								 dst_buf,
// 								 src_buf_data,
//                                  data_ptr + bytes_sent,
//                                  current_chunk_data_size,
//                                  chunk_idx,
//                                  total_chunks,
// 								 data_len,
//                                  is_last_chunk,
//                                  remote_mmap_range,
//                                  remote_mmap_range_len);

//         if (result != DOCA_SUCCESS) {
//             DOCA_LOG_ERR("发送第 %zu 块失败", chunk_idx);
//             return result;
//         }

// 		// 发送写完成信号并等待 ACK
// 		send_flag(resources->ctrl->sockfd, CTRL_WRITE_FINISH);
// 		DOCA_LOG_INFO("第 %zu 块写完成，等待接收方 ACK", chunk_idx);
// 		wait_flag(resources->ctrl, CTRL_ACK);
// 		DOCA_LOG_INFO("收到第 %zu 块的 ACK，可以继续", chunk_idx);

//         bytes_sent += current_chunk_data_size;
//         DOCA_LOG_INFO("已发送 %zu/%zu 块, 累计: %zu/%zu 字节",
//                      chunk_idx + 1, total_chunks, bytes_sent, data_len);
//     }

//     DOCA_LOG_INFO("所有 %zu 块传输完成", total_chunks);

// 	// 最后释放内存
//     free(enlarged_data);
//     return DOCA_SUCCESS;
// destroy_dst_buf:
//     tmp_result = doca_buf_dec_refcount(dst_buf, NULL);
//     if (tmp_result != DOCA_SUCCESS) {
//         DOCA_LOG_ERR("Failed to decrease dst_buf count: %s",
//         doca_error_get_descr(tmp_result)); DOCA_ERROR_PROPAGATE(result,
//         tmp_result);
//     }
// destroy_src_buf:
//     tmp_result = doca_buf_dec_refcount(src_buf, NULL);
//     if (tmp_result != DOCA_SUCCESS) {
//         DOCA_LOG_ERR("Failed to decrease src_buf count: %s",
//         doca_error_get_descr(tmp_result)); DOCA_ERROR_PROPAGATE(result,
//         tmp_result);
//     }
// 	return result;
// }

/*
 * RDMA write requester state change callback
 * This function represents the state machine for this RDMA program
 *
 * @user_data [in]: doca_data from the context
 * @ctx [in]: DOCA context
 * @prev_state [in]: Previous DOCA context state
 * @next_state [in]: Next DOCA context state
 */
static void rdma_write_requester_state_change_callback(
    const union doca_data user_data, struct doca_ctx *ctx,
    enum doca_ctx_states prev_state, enum doca_ctx_states next_state) {
  struct rdma_resources *resources = (struct rdma_resources *)user_data.ptr;
  struct rdma_config *cfg = resources->cfg;
  doca_error_t result = DOCA_SUCCESS;
  (void)prev_state;
  (void)ctx;

  switch (next_state) {
    case DOCA_CTX_STATE_STARTING:
      DOCA_LOG_INFO("RDMA context entered starting state");
      break;
    case DOCA_CTX_STATE_RUNNING:
      DOCA_LOG_INFO("RDMA context is running");

      result = rdma_write_requester_export_and_connect(resources);
      if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERR("rdma_write_requester_export_and_connect() failed: %s",
                     doca_error_get_descr(result));
        break;
      } else
        DOCA_LOG_INFO("RDMA context finished initialization");

      if (cfg->use_rdma_cm == true) break;

      result = rdma_write_prepare_and_submit_task(resources);
      if (result != DOCA_SUCCESS)
        DOCA_LOG_ERR("rdma_write_prepare_and_submit_task() failed: %s",
                     doca_error_get_descr(result));
      break;
    case DOCA_CTX_STATE_STOPPING:
      /**
       * doca_ctx_stop() has been called.
       * In this sample, this happens either due to a failure encountered, in
       * which case doca_pe_progress() will cause any inflight task to be
       * flushed, or due to the successful compilation of the sample flow. In
       * both cases, in this sample, doca_pe_progress() will eventually
       * transition the context to idle state.
       */
      DOCA_LOG_INFO(
          "RDMA context entered into stopping state. Any inflight tasks will "
          "be flushed");
      break;
    case DOCA_CTX_STATE_IDLE:
      DOCA_LOG_INFO("RDMA context has been stopped");

      /* We can stop progressing the PE */
      resources->run_pe_progress = false;
      break;
    default:
      break;
  }

  /* If something failed - update that an error was encountered and stop the ctx
   */
  if (result != DOCA_SUCCESS) {
    DOCA_ERROR_PROPAGATE(resources->first_encountered_error, result);
    (void)doca_ctx_stop(ctx);
  }
}

/*
 * Requester side of the RDMA write
 *
 * @cfg [in]: Configuration parameters
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
// ws  主要的实现传输函数
doca_error_t rdma_write_requester(struct rdma_config *cfg) {
  struct rdma_resources resources = {0};
  union doca_data ctx_user_data = {0};
  // const uint32_t mmap_permissions = DOCA_ACCESS_FLAG_LOCAL_READ_WRITE;
  const uint32_t mmap_permissions =
      DOCA_ACCESS_FLAG_LOCAL_READ_WRITE |  // 必须：本地能读写内存
      DOCA_ACCESS_FLAG_RDMA_WRITE;         // ✅
                                    // 必须：允许这块内存被远端（另一个进程）写
  const uint32_t rdma_permissions =
      DOCA_ACCESS_FLAG_LOCAL_READ_WRITE | DOCA_ACCESS_FLAG_RDMA_WRITE;
  struct timespec ts = {
      .tv_sec = 0,
      .tv_nsec = SLEEP_IN_NANOS,
  };
  doca_error_t result, tmp_result;

  // 初始化控制通道 enum
  resources.ctrl = malloc(sizeof(struct control_channel));
  if (resources.ctrl == NULL) {
    DOCA_LOG_ERR("Failed to allocate memory for control channel");
    return 1;
  }
  if (init_control_client(resources.ctrl, "192.168.200.2", cfg->tcp_port) !=
      0) {
    DOCA_LOG_ERR("Failed to connect to control channel\n");
    return 1;
  }

  /* Allocating resources */
  // ws allocate_rdma_resources 定义在 rdma_common.c 中
  TIMER_START(shm);
  result = allocate_rdma_resources(cfg, mmap_permissions, rdma_permissions,
                                   doca_rdma_cap_task_write_is_supported,
                                   &resources);
  if (result != DOCA_SUCCESS) {
    DOCA_LOG_ERR("Failed to allocate RDMA Resources: %s",
                 doca_error_get_descr(result));
    return result;
  }
  TIMER_END(shm, "分配资源耗时: ");

  DOCA_LOG_INFO("resources->rdma = %p", resources.rdma);

  // Enable the task  启用任务 设置
  result = doca_rdma_task_write_set_conf(
      resources.rdma, rdma_write_completed_callback, rdma_write_error_callback,
      NUM_RDMA_TASKS);

  // 该值 NUM_RDMA_TASKS 是个定值，在运行的时候。应该是设置的最大任务数量。
  // DOCA_LOG_INFO("NUM_RDMA_TASKS: %u",NUM_RDMA_TASKS);

  if (result != DOCA_SUCCESS) {
    DOCA_LOG_ERR("Unable to set configurations for RDMA write task: %s",
                 doca_error_get_descr(result));
    goto destroy_resources;
  }

  result = doca_ctx_set_state_changed_cb(
      resources.rdma_ctx, rdma_write_requester_state_change_callback);
  if (result != DOCA_SUCCESS) {
    DOCA_LOG_ERR("Unable to set state change callback for RDMA context: %s",
                 doca_error_get_descr(result));
    goto destroy_resources;
  }

  /* Create DOCA buffer inventory */

  result = doca_buf_inventory_create(INVENTORY_NUM_INITIAL_ELEMENTS,
                                     &resources.buf_inventory);
  if (result != DOCA_SUCCESS) {
    DOCA_LOG_ERR("Failed to create DOCA buffer inventory: %s",
                 doca_error_get_descr(result));
    goto destroy_resources;
  }

  /* Start DOCA buffer inventory */

  result = doca_buf_inventory_start(resources.buf_inventory);

  if (result != DOCA_SUCCESS) {
    DOCA_LOG_ERR("Failed to start DOCA buffer inventory: %s",
                 doca_error_get_descr(result));
    goto destroy_buf_inventory;
  }

  if (cfg->use_rdma_cm == true) {
    resources.is_requester = true;
    resources.require_remote_mmap = true;
    // resources.task_fn = rdma_write_prepare_and_submit_task;
    // 设置为分块发送函数逻辑
    resources.task_fn = rdma_write_prepare_and_submit_task_chunked;
    result = config_rdma_cm_callback_and_negotiation_task(
        &resources,
        /* need_send_mmap_info */ false,
        /* need_recv_mmap_info */ true);
    if (result != DOCA_SUCCESS) {
      DOCA_LOG_ERR(
          "Failed to config RDMA CM callbacks and negotiation functions: %s",
          doca_error_get_descr(result));
      goto destroy_buf_inventory;
    }
  }

  /* Include the program's resources in user data of context to be used in
   * callbacks */
  ctx_user_data.ptr = &(resources);
  result = doca_ctx_set_user_data(resources.rdma_ctx, ctx_user_data);
  if (result != DOCA_SUCCESS) {
    DOCA_LOG_ERR("Failed to set context user data: %s",
                 doca_error_get_descr(result));
    goto destroy_resources;
  }

  /* Start RDMA context */
  result = doca_ctx_start(resources.rdma_ctx);
  if (result != DOCA_SUCCESS) {
    DOCA_LOG_ERR("Failed to start RDMA context: %s",
                 doca_error_get_descr(result));
    goto stop_buf_inventory;
  }

  DOCA_LOG_INFO("在while (resources.run_pe_progress) 之前都是成功执行的");

  /*
   * Run the progress engine which will run the state machine defined in
   * rdma_write_requester_state_change_callback() When the context moves to
   * idle, the context change callback call will signal to stop running the
   * progress engine.
   */
  while (resources.run_pe_progress) {
    if (doca_pe_progress(resources.pe) == 0) nanosleep(&ts, &ts);
  }

  /* Assign the result we update in the callbacks */
  result = resources.first_encountered_error;

stop_buf_inventory:
  tmp_result = doca_buf_inventory_stop(resources.buf_inventory);
  if (tmp_result != DOCA_SUCCESS) {
    DOCA_LOG_ERR("Failed to stop DOCA buffer inventory: %s",
                 doca_error_get_descr(tmp_result));
    DOCA_ERROR_PROPAGATE(result, tmp_result);
  }
destroy_buf_inventory:
  tmp_result = doca_buf_inventory_destroy(resources.buf_inventory);
  if (tmp_result != DOCA_SUCCESS) {
    DOCA_LOG_ERR("Failed to destroy DOCA buffer inventory: %s",
                 doca_error_get_descr(tmp_result));
    DOCA_ERROR_PROPAGATE(result, tmp_result);
  }
destroy_resources:
  my_doca_close_shm(resources.reg);
  close_control_channel(resources.ctrl);
  free(resources.ctrl);
  DOCA_LOG_INFO("执行关闭资源");
  // tmp_result = destroy_rdma_resources(&resources, cfg);
  // if (tmp_result != DOCA_SUCCESS) {
  // 	DOCA_LOG_ERR("Failed to destroy DOCA RDMA resources: %s",
  // doca_error_get_descr(tmp_result)); 	DOCA_ERROR_PROPAGATE(result,
  // tmp_result);
  // }

  return result;
}
