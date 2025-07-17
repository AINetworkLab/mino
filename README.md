# dpu_rdma_debug

```plaintext
./requester
├── daemon_hugepages.c # First, allocate 1GB of hugepages memory.
├── doca_hp_sender.py # The entry point of the .so file used by the requester to invoke RDMA.
├── doca_hp_shm.c # Manage reuse or create a new mmap.
├── doca_hp_shm.h
├── meson.build
├── rdma_common_pool.c # Correspond to rdma_common.c in the sample code.
├── rdma_common_pool.h
├── rdma_control_enum.c # Replaces the wait_for_enter() function with TCP-based management logic.
├── rdma_control_enum.h
├── rdma_write_requester_my.c # Correspond to rdma_write_requester_main.c in the sample code.
├── rdma_write_requester_sample.c # Correspond to rdma_write_requester_sample.c in the sample code.
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
├── responder_1.py
├── responder_2.py
├── responder_3.py
├── responder_4.py
├── responder_all.sh # Entry points for executing responder_1.py to responder_4.py separately.
└── time_utils.h
```
