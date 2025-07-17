#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

/* ===== 修改后的宏 ===== */
#define SHM_NAME "/dev/hugepages/mino_doca_rdv_buf" /* 一定在 hugetlbfs 里 */
#define SHM_SIZE (1UL * 1024 * 1024 * 1024)         /* 1 GiB */
#define INFO_PATH "/tmp/doca_rdv.shm"
/* ======================= */

static void *shm_addr = NULL;
static void cleanup(void) {
  if (shm_addr) munmap(shm_addr, SHM_SIZE);
  unlink(SHM_NAME); /* open() 创建的文件用 unlink */
  unlink(INFO_PATH);
  exit(0);
}
static void handle(int s) {
  (void)s;
  cleanup();
}

static void daemonize(void) {
  if (fork() > 0) exit(0);
  if (setsid() < 0) exit(1);
  if (fork() > 0) exit(0);
  umask(0);
  if (chdir("/") != 0) {
    perror("chdir");
    exit(1);
  }
  close(STDIN_FILENO);
  close(STDOUT_FILENO);
  close(STDERR_FILENO);
  open("/dev/null", O_RDONLY);
  open("/dev/null", O_WRONLY);
  open("/dev/null", O_RDWR);
}

int main(int argc, char *argv[]) {
  if (argc == 2 && strcmp(argv[1], "--cleanup") == 0) cleanup();

  /* --- HugePages 余量检查 --- */
  {
    FILE *f = fopen("/proc/meminfo", "r");
    long free_hp = 0;
    char line[128];
    while (f && fgets(line, sizeof line, f))
      if (sscanf(line, "HugePages_Free: %ld", &free_hp) == 1) break;
    if (f) fclose(f);
    if (free_hp * 2UL * 1024 * 1024 < SHM_SIZE) {
      fprintf(stderr, "Not enough free 2MB HugePages: need %lu, free %ld\n",
              SHM_SIZE / (2UL * 1024 * 1024), free_hp);
      return 1;
    }
  }

  daemonize();
  signal(SIGTERM, handle);
  signal(SIGINT, handle);

  /* --- 用 open() 在 hugetlbfs 创建文件 --- */
  int fd = open(SHM_NAME, O_CREAT | O_RDWR, 0666);
  if (fd < 0) {
    perror("open");
    return 1;
  }
  if (ftruncate(fd, SHM_SIZE) < 0) {
    perror("ftruncate");
    close(fd);
    return 1;
  }

  void *addr = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE,
                    MAP_SHARED | MAP_HUGETLB, /* 关键：用 HugeTLB */
                    fd, 0);
  if (addr == MAP_FAILED) {
    perror("mmap");
    close(fd);
    return 1;
  }
  shm_addr = addr;

  /* 触页（可选，只触前几百 MB 亦可） */
  for (size_t off = 0; off < SHM_SIZE; off += 4096)
    ((volatile char *)shm_addr)[off] = 0;

  /* 写 info 文件：路径 + 大小 */
  FILE *fp = fopen(INFO_PATH, "w");
  if (fp) {
    fprintf(fp, "%s %lu\n", SHM_NAME, SHM_SIZE);
    fclose(fp);
  }

  while (1) sleep(60);
}