# dpu_rdma_debug

```plaintext
./requester
├── daemon_hugepages.c # First, allocate 1GB of hugepages memory.
├── debug.gdb #gdb configuration file
├── doca_hp_shm.c # Manage reuse or create a new mmap.
├── doca_hp_shm.h
├── meson.build
├── rdma_common_pool_th.c # Correspond to rdma_common.c in the sample code.
├── rdma_common_pool_th.h
├── rdma_control_enum.c # Replaces the wait_for_enter() function with TCP-based management logic.
├── rdma_control_enum.h
├── rdma_write_requester_my.c # Correspond to rdma_write_requester_main.c in the sample code.
├── rdma_write_requester_sample.c # Correspond to rdma_write_requester_sample.c in the sample code.
├── rebulid.sh # Clean up the previous build results, reinitialize the Meson build environment, and compile the project.
└── time_utils.h
```

```plaintext
./responder
├── meson.build
├── rdma_common.c
├── rdma_common.h
├── rdma_control_enum.c
├── rdma_control_enum.h
├── rdma_write_responder_my.c # Correspond to rdma_write_responder_main.c in the sample code.
├── rdma_write_responder_sample.c  # Correspond to rdma_write_responder_sample.c in the sample code.
├── rebuild.sh # Clean up the previous build results, reinitialize the Meson build environment, and compile the project.
├── responder_all.sh # Entry points for executing responder1 to responder4 separately.
└── time_utils.h
```
