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

#include <doca_argp.h>
#include <doca_log.h>
#include <stdlib.h>

#include "rdma_common.h"
#include "time_utils.h"

#define DEFAULT_STRING "Hi DOCA RDMA!"
#define DEFAULT_RDMA_CM_PORT (13579)
#define DEFAULT_LOCAL_CONNECTION_DESC_PATH "/tmp/local_connection_desc_path.txt"
/* Default path to save the remote connection descriptor that should be passed
 * from the other side */
#define DEFAULT_REMOTE_CONNECTION_DESC_PATH \
  "/tmp/remote_connection_desc_path.txt"
/* Default path to read/save the remote mmap connection descriptor that should
 * be passed to the other side */
#define DEFAULT_REMOTE_RESOURCE_CONNECTION_DESC_PATH \
  "/tmp/remote_resource_desc_path.txt"

DOCA_LOG_REGISTER(RDMA_WRITE_RESPONDER::MAIN);

#ifdef __cplusplus
extern "C" {
#endif

__attribute__((visibility("default"))) int rdma_write_responder_my();

#ifdef __cplusplus
}
#endif

/* Sample's Logic */
doca_error_t rdma_write_responder(struct rdma_config *cfg, char *py_buf,
                                  size_t *actual_size);

doca_error_t set_default_config_value_my(struct rdma_config *cfg) {
  if (cfg == NULL) return DOCA_ERROR_INVALID_VALUE;

  /* Set the default configuration values (Example values) */
  // DEFAULT_STRING "Hi DOCA RDMA!"
  strcpy(cfg->send_string, DEFAULT_STRING);
  strcpy(cfg->read_string, DEFAULT_STRING);
  strcpy(cfg->write_string, DEFAULT_STRING);
  // DEFAULT_LOCAL_CONNECTION_DESC_PATH "/tmp/local_connection_desc_path.txt"
  strcpy(cfg->local_connection_desc_path, DEFAULT_LOCAL_CONNECTION_DESC_PATH);
  // DEFAULT_REMOTE_CONNECTION_DESC_PATH "/tmp/remote_connection_desc_path.txt"
  strcpy(cfg->remote_connection_desc_path, DEFAULT_REMOTE_CONNECTION_DESC_PATH);
  // DEFAULT_REMOTE_RESOURCE_CONNECTION_DESC_PATH
  // "/tmp/remote_resource_desc_path.txt"
  strcpy(cfg->remote_resource_desc_path,
         DEFAULT_REMOTE_RESOURCE_CONNECTION_DESC_PATH);
  cfg->is_gid_index_set = false;
  cfg->num_connections = 1;
  cfg->transport_type = DOCA_RDMA_TRANSPORT_TYPE_RC;

  /* Only related rdma cm */
  cfg->use_rdma_cm = true;
  // DEFAULT_RDMA_CM_PORT (13579)
  // cfg->cm_port = DEFAULT_RDMA_CM_PORT;
  cfg->cm_addr_type = DOCA_RDMA_ADDR_TYPE_IPv4;
  memset(cfg->cm_addr, 0, SERVER_ADDR_LEN);
  // 设置远程服务器地址
  // strcpy(cfg->cm_addr, "192.168.200.1");
  // strcpy(cfg->cm_addr, "192.168.200.2");
  // 设置设备名称
  strcpy(cfg->device_name, "mlx5_0");

  return DOCA_SUCCESS;
}

/*
 * Sample main function
 *
 * @argc [in]: command line arguments size
 * @argv [in]: array of command line arguments
 * @return: EXIT_SUCCESS on success and EXIT_FAILURE otherwise
 */
int rdma_write_responder_my(char *py_buf, size_t *actual_size, int cm_port,
                            int tcp_port, size_t mem_len) {
  struct rdma_config cfg;
  doca_error_t result;
  struct doca_log_backend *sdk_log;
  int exit_status = EXIT_FAILURE;
  cfg.cm_port = cm_port;
  cfg.tcp_port = tcp_port;
  cfg.mem_len = mem_len;

  TIMER_START(p1);
  /* Set the default configuration values (Example values) */
  result = set_default_config_value_my(&cfg);
  if (result != DOCA_SUCCESS) goto sample_exit;

  /* Register a logger backend */
  result = doca_log_backend_create_standard();
  if (result != DOCA_SUCCESS) goto sample_exit;

  /* Register a logger backend for internal SDK errors and warnings */
  result = doca_log_backend_create_with_file_sdk(stderr, &sdk_log);
  if (result != DOCA_SUCCESS) goto sample_exit;
  result = doca_log_backend_set_sdk_level(sdk_log, DOCA_LOG_LEVEL_TRACE);
  if (result != DOCA_SUCCESS) goto sample_exit;

  DOCA_LOG_INFO("Starting the sample");

  TIMER_END(p1, "初始化 rdma ");

  /* Start sample */
  result = rdma_write_responder(&cfg, py_buf, actual_size);
  if (result != DOCA_SUCCESS) {
    DOCA_LOG_ERR("rdma_write_responder() failed: %s",
                 doca_error_get_descr(result));
    goto argp_cleanup;
  }

  exit_status = EXIT_SUCCESS;

argp_cleanup:
  doca_argp_destroy();

sample_exit:
  if (exit_status == EXIT_SUCCESS)
    DOCA_LOG_INFO("Sample finished successfully");
  else
    DOCA_LOG_INFO("Sample finished with errors");
  return exit_status;
}
#include <openssl/md5.h>
int main() {
  const size_t BUF_SIZE = 2048ULL * 1024 * 1024;  // 2GB
  const size_t mem_len = 400 * 1024 * 1024;       // 300MB
  const int cm_port = 13840;
  const int tcp_port = 18803;

  // 分配内存缓冲区
  char *py_buf = (char *)malloc(BUF_SIZE);
  printf("缓冲区大小%ld", BUF_SIZE);
  if (py_buf == NULL) {
    fprintf(stderr, "内存分配失败\n");
    return 1;
  }

  // 初始化实际大小变量
  size_t actual_size = 0;

  // 调用RDMA函数
  int ret =
      rdma_write_responder_my(py_buf, &actual_size, cm_port, tcp_port, mem_len);

  // 处理返回结果
  if (ret == 0) {
    // 计算MD5哈希
    unsigned char digest[MD5_DIGEST_LENGTH];
    MD5((unsigned char *)py_buf, actual_size, digest);

    // 转换为十六进制字符串
    char md5_str[33];
    for (int i = 0; i < 16; i++) {
      sprintf(&md5_str[i * 2], "%02x", (unsigned int)digest[i]);
    }

    printf("[Responder:%d] 接收到 %zu 字节, MD5: %s\n", cm_port, actual_size,
           md5_str);
  } else {
    printf("[Responder:%d] 出错，返回值: %d\n", cm_port, ret);
  }

  // 释放内存
  free(py_buf);

  return ret;
}
