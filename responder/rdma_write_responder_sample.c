/*
 * Copyright (c) 2023 NVIDIA CORPORATION AND AFFILIATES.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *     * Neither the name of the NVIDIA CORPORATION nor the names of its
 * contributors may be used to endorse or promote products derived from this
 * software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL NVIDIA CORPORATION BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TOR
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <doca_buf.h>
#include <doca_buf_inventory.h>
#include <doca_ctx.h>
#include <doca_error.h>
#include <doca_log.h>

#include "rdma_common.h"
#include "rdma_control_enum.h"
#include "time_utils.h"

#define MAX_BUFF_SIZE (256) /* Maximum DOCA buffer size */

DOCA_LOG_REGISTER(RDMA_WRITE_RESPONDER::SAMPLE);

/*
 * Write the connection details and the mmap details for the requester to read,
 * and read the connection details of the requester
 * In DC transport mode it is only needed to read the remote connection details
 *
 * @cfg [in]: Configuration parameters
 * @resources [in/out]: RDMA resources
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
static doca_error_t write_read_connection(struct rdma_config *cfg,
                                          struct rdma_resources *resources) {
  doca_error_t result = DOCA_SUCCESS;

  /* Write the RDMA connection details */
  result = write_file(cfg->local_connection_desc_path,
                      (char *)resources->rdma_conn_descriptor,
                      resources->rdma_conn_descriptor_size);
  if (result != DOCA_SUCCESS) {
    DOCA_LOG_ERR("Failed to write the RDMA connection details: %s",
                 doca_error_get_descr(result));
    return result;
  }

  /* Write the mmap connection details */
  result = write_file(cfg->remote_resource_desc_path,
                      (char *)resources->mmap_descriptor,
                      resources->mmap_descriptor_size);
  if (result != DOCA_SUCCESS) {
    DOCA_LOG_ERR("Failed to write the RDMA mmap details: %s",
                 doca_error_get_descr(result));
    return result;
  }

  DOCA_LOG_INFO("You can now copy %s and %s to the requester",
                cfg->local_connection_desc_path,
                cfg->remote_resource_desc_path);

  if (cfg->transport_type == DOCA_RDMA_TRANSPORT_TYPE_DC) {
    return result;
  }
  DOCA_LOG_INFO("Please copy %s from the requester and then press enter",
                cfg->remote_connection_desc_path);

  /* Wait for enter */
  wait_for_enter();

  /* Read the remote RDMA connection details */
  result = read_file(cfg->remote_connection_desc_path,
                     (char **)&resources->remote_rdma_conn_descriptor,
                     &resources->remote_rdma_conn_descriptor_size);
  if (result != DOCA_SUCCESS)
    DOCA_LOG_ERR("Failed to read the remote RDMA connection details: %s",
                 doca_error_get_descr(result));

  return result;
}

/*
 * Export and receive connection details, and connect to the remote RDMA
 *
 * @resources [in]: RDMA resources
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
static doca_error_t rdma_write_responder_export_and_connect(
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

  /* Export RDMA mmap */
  result = doca_mmap_export_rdma(resources->mmap, resources->doca_device,
                                 (const void **)&(resources->mmap_descriptor),
                                 &(resources->mmap_descriptor_size));
  if (result != DOCA_SUCCESS) {
    DOCA_LOG_ERR("Failed to export DOCA mmap for RDMA: %s",
                 doca_error_get_descr(result));
    return result;
  }

  /* write and read connection details from the requester */
  result = write_read_connection(resources->cfg, resources);
  if (result != DOCA_SUCCESS)
    DOCA_LOG_ERR(
        "Failed to write and read connection details from the requester: %s",
        doca_error_get_descr(result));

  if (resources->cfg->transport_type == DOCA_RDMA_TRANSPORT_TYPE_DC) {
    return result;
  }
  /* Connect RDMA */
  result = doca_rdma_connect(
      resources->rdma, resources->remote_rdma_conn_descriptor,
      resources->remote_rdma_conn_descriptor_size, resources->connections[0]);
  if (result != DOCA_SUCCESS)
    DOCA_LOG_ERR(
        "Failed to connect the responder's RDMA to the requester's RDMA: %s",
        doca_error_get_descr(result));

  return result;
}

/*
 * Responder wait for requester to finish
 *
 * @resources [in]: RDMA resources
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
// responder 等待 requester 写入的实现
static doca_error_t responder_wait_for_requester_finish(
    struct rdma_resources *resources) {
  DOCA_LOG_INFO("111 ==== mmap_memrange 的地址为：%p",
                resources->mmap_memrange);
  doca_error_t result = DOCA_SUCCESS;

  // /* Wait for enter which means that the requester has finished writing */
  DOCA_LOG_INFO("Wait till the requester has finished writing and press enter");
  // wait_for_enter();
  TIMER_START(finish);
  // int client_fd = wait_flag(resources->ctrl, CTRL_WRITE_FINISH);
  wait_flag(resources->ctrl, CTRL_WRITE_FINISH);
  TIMER_END(finish, "等待发送方发送写任务完成通知耗时");

  /* Step 1: 读取元数据 (flag + data_len) */
  bool flag = 0;
  size_t data_len = 0;
  size_t head_len = sizeof(flag) + sizeof(data_len);

  memcpy(&flag, resources->mmap_memrange, sizeof(flag));
  memcpy(&data_len, resources->mmap_memrange + sizeof(flag), sizeof(data_len));

  DOCA_LOG_INFO("接收到 flag = %d", flag);
  DOCA_LOG_INFO("接收到 data_len = %zu", data_len);

  // send_flag(client_fd, CTRL_ACK);
  send_flag(resources->ctrl, CTRL_ACK);
  DOCA_LOG_INFO(" 已发送 ACK");
  wait_flag(resources->ctrl, CTRL_WRITE_FINISH);

  // TODO: 暂时先不返回 python，将接收到数据追加写入文件

  // 该实现为提取数据为返回 py 做准备
  resources->cfg->result_buf = (char *)malloc(data_len);
  if (resources->cfg->result_buf == NULL) {
    return -1;
  }
  memcpy(resources->cfg->result_buf, resources->mmap_memrange + head_len,
         data_len);
  resources->cfg->result_size = data_len;

  DOCA_LOG_INFO("mmap_memrange 的地址为：%p", resources->mmap_memrange);

  // length_check_error:
  if (resources->cfg->use_rdma_cm == true) {
    result = rdma_cm_disconnect(resources);
    if (result != DOCA_SUCCESS) {
      DOCA_LOG_ERR("Failed to disconnect RDMA connection: %s",
                   doca_error_get_descr(result));
    }
  }

  (void)doca_ctx_stop(resources->rdma_ctx);

  return result;
}

typedef struct {
  size_t chunk_idx;
  size_t total_chunks;
  size_t chunk_data_size;
  size_t total_data_size;  // 添加这个字段
  bool is_last_chunk;
} chunk_header_t;

// ws 自定义 chunked 接受函数
static doca_error_t responder_wait_for_requester_finish_chunked(
    struct rdma_resources *resources) {
  DOCA_LOG_INFO("开始接收分块数据");
  doca_error_t result = DOCA_SUCCESS;

  char *assembled_data = NULL;
  size_t assembled_size = 0;
  size_t expected_chunks = 0;
  size_t received_chunks = 0;
  size_t expected_total_size = 0;

  TIMER_START(finish);

  while (true) {
    DOCA_LOG_INFO("等待第 %zu 块数据", received_chunks + 1);
    TIMER_START(wait);
    // int client_fd = wait_flag(resources->ctrl, CTRL_WRITE_FINISH);
    wait_flag(resources->ctrl, CTRL_WRITE_FINISH);
    TIMER_END(wait, "等待 块数据 耗时");
    DOCA_LOG_INFO("收到写完成信号");

    TIMER_START(get);
    // 读取块头部信息
    chunk_header_t header;
    memcpy(&header, resources->mmap_memrange, sizeof(header));

    DOCA_LOG_INFO("接收到块 %zu/%zu，数据大小: %zu，总数据: %zu，是否最后: %d",
                  header.chunk_idx + 1, header.total_chunks,
                  header.chunk_data_size, header.total_data_size,
                  header.is_last_chunk);

    // 第一次接收时记录预期信息
    if (received_chunks == 0) {
      expected_chunks = header.total_chunks;
      expected_total_size = header.total_data_size;
      DOCA_LOG_INFO("预计接收 %zu 块，总大小 %zu 字节", expected_chunks,
                    expected_total_size);
    }

    // 动态扩展缓冲区
    assembled_data =
        realloc(assembled_data, assembled_size + header.chunk_data_size);
    if (assembled_data == NULL) {
      DOCA_LOG_ERR("Failed to reallocate memory for chunk %zu",
                   received_chunks + 1);
      // send_flag(client_fd, CTRL_ACK);
      send_flag(resources->ctrl, CTRL_ACK);
      return 1;
    }

    // 复制当前块的数据（跳过头部）
    memcpy(assembled_data + assembled_size,
           resources->mmap_memrange + sizeof(chunk_header_t),
           header.chunk_data_size);
    assembled_size += header.chunk_data_size;
    received_chunks++;
    TIMER_END(get, "读取 块数据 耗时");
    DOCA_LOG_INFO("已接收 %zu/%zu 块，累计数据: %zu 字节", received_chunks,
                  expected_chunks, assembled_size);

    // 发送 ACK
    // send_flag(client_fd, CTRL_ACK);
    send_flag(resources->ctrl, CTRL_ACK);
    DOCA_LOG_INFO("已发送第 %zu 块的 ACK", received_chunks);

    // 如果是最后一块，退出循环
    if (header.is_last_chunk) {
      DOCA_LOG_INFO("接收完成，总共 %zu 块，%zu 字节", received_chunks,
                    assembled_size);
      break;
    }
  }

  TIMER_END(finish, "分块接收总耗时");

  // 验证数据大小是否一致
  if (assembled_size == expected_total_size) {
    DOCA_LOG_INFO("✓ 数据接收完整: %zu 字节", assembled_size);
  } else {
    DOCA_LOG_ERR("✗ 数据大小不匹配: 接收 %zu，预期 %zu", assembled_size,
                 expected_total_size);
  }

  // 设置结果
  resources->cfg->result_buf = assembled_data;
  resources->cfg->result_size = assembled_size;

  DOCA_LOG_INFO("最终接收数据大小: %zu", assembled_size);

  // 断开连接
  if (resources->cfg->use_rdma_cm == true) {
    result = rdma_cm_disconnect(resources);
    if (result != DOCA_SUCCESS) {
      DOCA_LOG_ERR("Failed to disconnect RDMA connection: %s",
                   doca_error_get_descr(result));
    }
  }

  (void)doca_ctx_stop(resources->rdma_ctx);

  return result;
}

/*
 * RDMA write responder state change callback
 * This function represents the state machine for this RDMA program
 *
 * @user_data [in]: doca_data from the context
 * @ctx [in]: DOCA context
 * @prev_state [in]: Previous DOCA context state
 * @next_state [in]: Next DOCA context state
 */
static void rdma_write_responder_state_change_callback(
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

      result = rdma_write_responder_export_and_connect(resources);
      if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERR("rdma_write_responder_export_and_connect() failed: %s",
                     doca_error_get_descr(result));
        break;
      } else
        DOCA_LOG_INFO("RDMA context finished initialization");

      if (cfg->use_rdma_cm == true) break;

      result = responder_wait_for_requester_finish(resources);
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
 * Responder side of the RDMA write
 *
 * @cfg [in]: Configuration parameters
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
doca_error_t rdma_write_responder(struct rdma_config *cfg, char *py_buf,
                                  size_t *actual_size) {
  struct rdma_resources resources = {0};
  union doca_data ctx_user_data = {0};
  const uint32_t mmap_permissions =
      DOCA_ACCESS_FLAG_LOCAL_READ_WRITE | DOCA_ACCESS_FLAG_RDMA_WRITE;
  const uint32_t rdma_permissions =
      DOCA_ACCESS_FLAG_RDMA_WRITE | DOCA_ACCESS_FLAG_LOCAL_READ_WRITE;
  doca_error_t result, tmp_result;
  struct timespec ts = {
      .tv_sec = 0,
      .tv_nsec = SLEEP_IN_NANOS,
  };

  // 初始化控制通道
  resources.ctrl = malloc(sizeof(struct control_channel));
  if (resources.ctrl == NULL) {
    DOCA_LOG_ERR("Failed to allocate memory for control channel");
    return 1;
  }
  if (init_control_server(resources.ctrl, cfg->tcp_port) != 0) {
    DOCA_LOG_ERR("Responder failed to setup control socket\n");
    return 1;
  }

  /* Allocating resources */
  result = allocate_rdma_resources(cfg, mmap_permissions, rdma_permissions,
                                   NULL, &resources);
  if (result != DOCA_SUCCESS) {
    DOCA_LOG_ERR("Failed to allocate RDMA Resources: %s",
                 doca_error_get_descr(result));
    return result;
  }

  result = doca_ctx_set_state_changed_cb(
      resources.rdma_ctx, rdma_write_responder_state_change_callback);
  if (result != DOCA_SUCCESS) {
    DOCA_LOG_ERR("Unable to set state change callback for RDMA context: %s",
                 doca_error_get_descr(result));
    goto destroy_resources;
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

  if (cfg->use_rdma_cm == true) {
    resources.is_requester = false;
    resources.require_remote_mmap = true;
    // resources.task_fn = responder_wait_for_requester_finish;
    // 设置为分块接受的逻辑
    resources.task_fn = responder_wait_for_requester_finish_chunked;
    result = config_rdma_cm_callback_and_negotiation_task(
        &resources,
        /* need_send_mmap_info */ true,
        /* need_recv_mmap_info */ false);
    if (result != DOCA_SUCCESS) {
      DOCA_LOG_ERR(
          "Failed to config RDMA CM callbacks and negotiation functions: %s",
          doca_error_get_descr(result));
      goto destroy_resources;
    }
  }

  /* Start RDMA context */
  result = doca_ctx_start(resources.rdma_ctx);
  if (result != DOCA_SUCCESS) {
    DOCA_LOG_ERR("Failed to start RDMA context: %s",
                 doca_error_get_descr(result));
    goto destroy_resources;
  }

  DOCA_LOG_INFO("remote_mmap_descriptor= %p remote_mmap=%p",
                resources.remote_mmap_descriptor, resources.remote_mmap);

  /*
   * Run the progress engine which will run the state machine defined in
   * rdma_write_responder_state_change_callback() When the requester finishes
   * writing, the user will signal to stop running the progress engine.
   */
  // 这里其实在调用 方法 rdma_write_responder_state_change_callback()
  while (resources.run_pe_progress) {
    if (doca_pe_progress(resources.pe) == 0) nanosleep(&ts, &ts);
  }

  memcpy(py_buf, resources.cfg->result_buf, resources.cfg->result_size);
  *actual_size = resources.cfg->result_size;

  free(resources.cfg->result_buf);
  resources.cfg->result_buf = NULL;
  resources.cfg->result_size = 0;

  /* Assign the result we update in the callbacks */
  result = resources.first_encountered_error;

  close_control_channel(resources.ctrl);
  free(resources.ctrl);

destroy_resources:
  if (resources.buf_inventory != NULL) {
    tmp_result = doca_buf_inventory_stop(resources.buf_inventory);
    if (tmp_result != DOCA_SUCCESS) {
      DOCA_LOG_ERR("Failed to stop DOCA buffer inventory: %s",
                   doca_error_get_descr(tmp_result));
      DOCA_ERROR_PROPAGATE(result, tmp_result);
    }
    tmp_result = doca_buf_inventory_destroy(resources.buf_inventory);
    if (tmp_result != DOCA_SUCCESS) {
      DOCA_LOG_ERR("Failed to destroy DOCA buffer inventory: %s",
                   doca_error_get_descr(tmp_result));
      DOCA_ERROR_PROPAGATE(result, tmp_result);
    }
  }
  tmp_result = destroy_rdma_resources(&resources, cfg);
  if (tmp_result != DOCA_SUCCESS) {
    DOCA_LOG_ERR("Failed to destroy DOCA RDMA resources: %s",
                 doca_error_get_descr(tmp_result));
    DOCA_ERROR_PROPAGATE(result, tmp_result);
  }
  return result;
}
