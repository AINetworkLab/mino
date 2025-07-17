#include <doca_argp.h>
#include <doca_log.h>
#include <stdlib.h>

#include "rdma_common_pool_th.h"

// 时间工具头文件
#include "doca_hp_shm.h"
#include "mmap_pool.h"
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

DOCA_LOG_REGISTER(RDMA_WRITE_REQUESTER::MY);

#ifdef __cplusplus
extern "C" {
#endif

__attribute__((visibility("default"))) void my_exported_function();

#ifdef __cplusplus
}
#endif

/* Sample's Logic */
doca_error_t rdma_write_requester(struct rdma_config *cfg);

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
  // memset(cfg->cm_addr, 0, SERVER_ADDR_LEN);
  // 设置远程服务器地址
  strcpy(cfg->cm_addr, "192.168.200.2");
  // 设置设备名称
  strcpy(cfg->device_name, "mlx5_1");

  return DOCA_SUCCESS;
}

/*
 * Sample main function
 *
 * @argc [in]: command line arguments size
 * @argv [in]: array of command line arguments
 * @return: EXIT_SUCCESS on success and EXIT_FAILURE otherwise
 */

// int rdma_init(struct rdma_config *cfg)
// {
// 	// static int already_initialized = 0;
//     // static pthread_mutex_t init_lock = PTHREAD_MUTEX_INITIALIZER;

// 	// pthread_mutex_lock(&init_lock);
//     // if (already_initialized) {
//     //     pthread_mutex_unlock(&init_lock);
//     //     return EXIT_SUCCESS;
//     // }

// 	doca_error_t result;
// 	struct doca_log_backend *sdk_log;
// 	int exit_status = EXIT_FAILURE;

// 	/* Set the default configuration values (Example values) */
// 	// set_default_config_value() 的定义在 rdma_common.c 中
// 	result = set_default_config_value_my(cfg);
// 	if (result != DOCA_SUCCESS){
// 		goto sample_exit;
// 	}

// 	/* Register a logger backend */
// 	result = doca_log_backend_create_standard();
// 	if (result != DOCA_SUCCESS){
// 		goto sample_exit;
// 	}

// 	/* Register a logger backend for internal SDK errors and warnings */
// 	result = doca_log_backend_create_with_file_sdk(stderr, &sdk_log);
// 	if (result != DOCA_SUCCESS){
// 		goto sample_exit;
// 	}
// 	result = doca_log_backend_set_sdk_level(sdk_log,
// DOCA_LOG_LEVEL_WARNING); 	if (result != DOCA_SUCCESS){ 		goto
// sample_exit;
// 	}

// 	DOCA_LOG_INFO("Starting the sample");
// 	exit_status = EXIT_SUCCESS;

// sample_exit:
// 	// pthread_mutex_unlock(&init_lock);
// 	if (exit_status == EXIT_SUCCESS)
// 		DOCA_LOG_INFO("Inite finished successfully");
// 	else
// 		DOCA_LOG_INFO("Inite finished with errors");
// 	return exit_status;
// }

int rdma_init(struct rdma_config *cfg) {
  static int already_initialized = 0;
  static pthread_mutex_t init_lock = PTHREAD_MUTEX_INITIALIZER;

  pthread_mutex_lock(&init_lock);

  doca_error_t result;
  struct doca_log_backend *sdk_log;
  int exit_status = EXIT_FAILURE;

  result = set_default_config_value_my(cfg);
  if (result != DOCA_SUCCESS) {
    pthread_mutex_unlock(&init_lock);
    goto sample_exit;
  }

  // 只有第一个线程执行日志初始化
  if (!already_initialized) {
    /* Register a logger backend */
    result = doca_log_backend_create_standard();
    if (result != DOCA_SUCCESS) {
      pthread_mutex_unlock(&init_lock);
      goto sample_exit;
    }

    /* Register a logger backend for internal SDK errors and warnings */
    result = doca_log_backend_create_with_file_sdk(stderr, &sdk_log);
    if (result != DOCA_SUCCESS) {
      pthread_mutex_unlock(&init_lock);
      goto sample_exit;
    }
    result = doca_log_backend_set_sdk_level(sdk_log, DOCA_LOG_LEVEL_TRACE);
    if (result != DOCA_SUCCESS) {
      pthread_mutex_unlock(&init_lock);
      goto sample_exit;
    }

    already_initialized = 1;
    DOCA_LOG_INFO("Logger initialized successfully");
  }

  pthread_mutex_unlock(&init_lock);

  DOCA_LOG_INFO("Starting the sample");
  exit_status = EXIT_SUCCESS;

sample_exit:
  if (exit_status == EXIT_SUCCESS)
    DOCA_LOG_INFO("Init finished successfully");
  else
    DOCA_LOG_INFO("Init finished with errors");
  return exit_status;
}

// 调用的函数
int rdma_write(const char *data_ptr, size_t data_size, uint16_t cm_port,
               uint16_t tcp_port) {
  // 结构体 rdma_config 定义在 rdma_common.h 中
  struct rdma_config cfg;
  int exit_status = EXIT_FAILURE;
  doca_error_t result;
  cfg.cm_port = cm_port;
  cfg.tcp_port = tcp_port;

  result = rdma_init(&cfg);

  cfg.data_ptr = (char *)data_ptr;  // 强转去掉 const
  cfg.data_len = data_size;

  /* Start sample */
  // ws 写请求的发送  rdma_write_requester 定义在 sample.c 中
  result = rdma_write_requester(&cfg);

  if (result != DOCA_SUCCESS) {
    DOCA_LOG_ERR("rdma_write_requester() failed: %s",
                 doca_error_get_descr(result));
    goto argp_cleanup;
  }

  exit_status = EXIT_SUCCESS;

  if (exit_status == EXIT_SUCCESS)
    DOCA_LOG_INFO("Sample finished successfully");
  else
    DOCA_LOG_INFO("Sample finished with errors");
  return exit_status;

argp_cleanup:
  return 1;
}

// 读取整个文件内容到内存，返回指针和大小
// 返回0表示成功，非0表示失败
int my_read_file(const char *filepath, char **data_ptr, size_t *data_size) {
  FILE *fp = fopen(filepath, "rb");
  if (!fp) {
    perror("Failed to open file");
    return 1;
  }

  fseek(fp, 0, SEEK_END);
  size_t filesize = ftell(fp);
  rewind(fp);

  char *buffer = (char *)malloc(filesize);
  if (!buffer) {
    perror("Failed to allocate memory");
    fclose(fp);
    return 1;
  }

  size_t read_size = fread(buffer, 1, filesize, fp);
  fclose(fp);

  if (read_size != filesize) {
    perror("Failed to read entire file");
    free(buffer);
    return 1;
  }

  *data_ptr = buffer;
  *data_size = filesize;
  return EXIT_SUCCESS;
}

int thread_fun(const char *filepath, int cm_port, uint16_t tcp_port,
               int thread_id, size_t mem_len) {
  char *data = NULL;
  size_t size = 0;

  int ret = my_read_file(filepath, &data, &size);
  if (ret != 0) {
    DOCA_LOG_INFO("read_file failed with code %d\n", ret);
    return 1;
  }

  // 创建配置并设置线程ID
  struct rdma_config cfg;
  rdma_init(&cfg);
  cfg.cm_port = cm_port;
  cfg.tcp_port = tcp_port;
  cfg.data_ptr = data;
  cfg.data_len = size;
  cfg.thread_id = thread_id;  // 设置线程ID
  cfg.mem_len = mem_len;

  // 调用RDMA写请求
  TIMER_START(task_finish);
  doca_error_t result = rdma_write_requester(&cfg);
  TIMER_END(task_finish, "写操作总耗时: ");

  free(data);

  if (result == EXIT_SUCCESS)
    DOCA_LOG_INFO("Sample finished successfully");
  else
    DOCA_LOG_INFO("Sample finished with errors");

  return (result == DOCA_SUCCESS) ? EXIT_SUCCESS : EXIT_FAILURE;
}

// 参数结构体
typedef struct {
  char filepath[256];
  int cm_port;
  uint16_t tcp_port;
  int thread_id;
  size_t mem_len;
} thread_args_t;

// 线程入口
void *thread_entry(void *arg) {
  thread_args_t *args = (thread_args_t *)arg;

  int ret = thread_fun(args->filepath, args->cm_port, args->tcp_port,
                       args->thread_id, args->mem_len);
  if (ret != 0) {
    DOCA_LOG_INFO("thread_fun failed with code %d\n", ret);
  }

  free(args);
  return NULL;
}

int main() {
  // 4组不同的参数
  // const char *files[4] = {
  //     "/home/mino/load/models/hub/checkpoints/mobilenet_v2-7ebf99e0.pth",
  //     "/home/mino/load/models/hub/checkpoints/resnet18-f37072fd.pth",
  //     "/home/mino/load/models/hub/checkpoints/mobilenet_v2-7ebf99e0.pth",
  //     "/home/mino/load/models/hub/checkpoints/resnet18-f37072fd.pth"
  // };
  // const char *files[4] = {
  //     "/home/mino/load/models/hub/checkpoints/resnet18-f37072fd.pth",
  //     "/home/mino/load/models/hub/checkpoints/resnet50-11ad3fa6.pth",
  //     "/home/mino/load/models/hub/checkpoints/resnet18-f37072fd.pth",
  //     "/home/mino/load/models/hub/checkpoints/resnet50-11ad3fa6.pth"
  // };
  const char *files[4] = {
      "/home/mino/load/models/hub/checkpoints/resnet18-f37072fd.pth",
      "/home/mino/load/models/hub/checkpoints/resnet18-f37072fd.pth",
      "/home/mino/load/models/hub/checkpoints/resnet18-f37072fd.pth",
      "/home/mino/load/models/hub/checkpoints/resnet18-f37072fd.pth"};
  // const char *files[4] = {
  //     "/home/mino/load/models/hub/checkpoints/resnet18-f37072fd.pth",
  //     "/home/mino/load/models/hub/checkpoints/resnet50-11ad3fa6.pth",
  //     "/home/mino/load/models/hub/checkpoints/densenet121-a639ec97.pth",
  //     "/home/mino/load/models/hub/checkpoints/densenet201-c1103571.pth"
  // };
  // uint16_t cm_ports[4] = {13580, 13600, 13620, 13640};
  int cm_ports[4] = {13780, 13800, 13820, 13840};  //+200
  // uint16_t tcp_ports[4] = {18600, 18601, 18602, 18603};
  uint16_t tcp_ports[4] = {18700, 18701, 18702, 18803};  //+100
  size_t mem_lens[4] = {50 * 1024 * 1024, 100 * 1024 * 1024, 50 * 1024 * 1024,
                        100 * 1024 * 1024};

  // // 并行执行
  // pthread_t tids[4];
  // for (int i = 0; i < 4; i++) {
  //     thread_args_t *args = malloc(sizeof(thread_args_t));
  //     if (!args) {
  //         perror("malloc failed");
  //         return EXIT_FAILURE;
  //     }

  //     strncpy(args->filepath, files[i], sizeof(args->filepath));
  //     args->filepath[sizeof(args->filepath)-1] = '\0';
  //     args->cm_port = cm_ports[i];
  //     args->tcp_port = tcp_ports[i];
  //     args->thread_id = i;
  //     args->mem_len = mem_lens[i];

  //     int err = pthread_create(&tids[i], NULL, thread_entry, args);
  //     if (err != 0) {
  //         fprintf(stderr, "pthread_create failed: %d\n", err);
  //         free(args);
  //         return EXIT_FAILURE;
  //     }
  // }

  // // 等待所有线程结束
  // for (int i = 0; i < 4; i++) {
  //     pthread_join(tids[i], NULL);
  // }

  //   pthread_t tids[4];
  //   for (int i = 0; i < 4; i++) {
  //     thread_args_t *args = malloc(sizeof(thread_args_t));
  //     if (!args) {
  //       perror("malloc failed");
  //       return EXIT_FAILURE;
  //     }

  //     strncpy(args->filepath, files[i], sizeof(args->filepath));
  //     args->filepath[sizeof(args->filepath) - 1] = '\0';
  //     args->cm_port = cm_ports[i];
  //     args->tcp_port = tcp_ports[i];
  //     args->thread_id = i;
  //     args->mem_len = mem_lens[i];
  //     int err = pthread_create(&tids[i], NULL, thread_entry, args);
  //     if (err != 0) {
  //       fprintf(stderr, "pthread_create failed: %d\n", err);
  //       free(args);
  //       return EXIT_FAILURE;
  //     }
  //     pthread_join(tids[i], NULL);  // 立即等待这个线程完成
  //   }

  // // 串行执行每个任务
  // for (int i = 0; i < 4; i++) {
  //   DOCA_LOG_INFO("=== 开始执行第 %d 个任务 ===", i);

  //   // 打印当前块状态
  //   print_block_status();

  //   // 准备参数
  //   thread_args_t args;
  //   strncpy(args.filepath, files[i], sizeof(args.filepath));
  //   args.filepath[sizeof(args.filepath) - 1] = '\0';
  //   args.cm_port = cm_ports[i];
  //   args.tcp_port = tcp_ports[i];
  //   args.thread_id = i;
  //   args.mem_len = mem_lens[i];

  //   // 直接调用线程函数，而不是创建新线程
  //   thread_fun(args.filepath, args.cm_port, args.tcp_port, args.thread_id,
  //              args.mem_len);

  //   DOCA_LOG_INFO("=== 第 %d 个任务执行完成 ===", i);

  //   // 打印任务完成后的块状态
  //   print_block_status();
  // }

  // DOCA_LOG_INFO("所有任务执行完毕，开始清理资源");
  // my_doca_cleanup_all_blocks();

  // // 设置并行执行
  // pthread_t tids[4];
  // int i;

  // for (i = 0; i < 4; i++) {
  //     thread_args_t *args = malloc(sizeof(thread_args_t));
  //     if (!args) {
  //         perror("malloc failed");
  //         return EXIT_FAILURE;
  //     }

  //     strncpy(args->filepath, files[i], sizeof(args->filepath));
  //     args->filepath[sizeof(args->filepath)-1] = '\0';
  //     args->cm_port = cm_ports[i];
  //     args->tcp_port = tcp_ports[i];
  //     args->thread_id = i;  // 设置线程ID)
  // 	args->mem_len = mem_lens[i]; // 设置内存长度

  //     int err = pthread_create(&tids[i], NULL, thread_entry, args);
  //     if (err != 0) {
  //         fprintf(stderr, "pthread_create failed: %d\n", err);
  //         free(args);
  //         return EXIT_FAILURE;
  //     }
  // }

  // // 等待所有线程结束
  // for (i = 0; i < 4; i++) {
  //     pthread_join(tids[i], NULL);
  // }

  // // 串行执行每个线程的工作
  // for (i = 0; i < 4; i++) {
  //     DOCA_LOG_INFO("开始执行第 %d 个任务", i);

  //     // 准备参数
  //     thread_args_t args;
  //     strncpy(args.filepath, files[i], sizeof(args.filepath));
  //     args.filepath[sizeof(args.filepath)-1] = '\0';
  //     args.cm_port = cm_ports[i];
  //     args.tcp_port = tcp_ports[i];
  //     args.thread_id = i;
  //     args.mem_len = mem_lens[i];

  //     // 直接调用线程函数，而不是创建新线程
  //     thread_fun(args.filepath, args.cm_port, args.tcp_port, args.thread_id,
  //     args.mem_len);

  //     DOCA_LOG_INFO("第 %d 个任务执行完成", i);

  //     // 可选：任务之间添加一些延迟，确保系统状态完全清理
  //     // sleep(1);
  // }

  pthread_t tids[4];
  for (int i = 0; i < 4; i++) {
    DOCA_LOG_INFO("=== 开始执行第 %d 个任务 ===", i);

    print_block_status();

    thread_args_t *args = malloc(sizeof(thread_args_t));
    if (!args) {
      perror("malloc failed");
      return EXIT_FAILURE;
    }

    strncpy(args->filepath, files[i], sizeof(args->filepath));
    args->filepath[sizeof(args->filepath) - 1] = '\0';
    args->cm_port = cm_ports[i];
    args->tcp_port = tcp_ports[i];
    args->thread_id = i;
    args->mem_len = mem_lens[i];

    int err = pthread_create(&tids[i], NULL, thread_entry, args);
    if (err != 0) {
      fprintf(stderr, "pthread_create failed: %d\n", err);
      free(args);
      return EXIT_FAILURE;
    }
    pthread_join(tids[i], NULL);  // 立即等待这个线程完成
  }
  DOCA_LOG_INFO("所有任务执行完毕，开始清理资源");
  my_doca_cleanup_all_blocks();
  return EXIT_SUCCESS;
}