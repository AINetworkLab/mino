# dpu_rdma_debug
```plaintext
./logs
├── 50_100M  # The result of the program running after modifying in rdma_write_requester_my.c[344] : size_t mem_lens[4] = {50 * 1024 * 1024, 100 * 1024 * 1024, 50 * 1024 * 1024, 100 * 1024 * 1024};
│   ├── debug  #Logs obtained using the "script -c "gdb -x debug.gdb --args ./_doca_rdma_write_requester" all.log" command
│   │   ├──all.log # requester log
│   │   ├──dbg.pcap # tcpdump form requester 
│   │   └──responder_log.txt # responder log
│   └── run  #Logs obtained using the "./_doca_rdma_write_requester > run_all.log 2>&1" command
│       ├──all.log # requester log
│       ├──dbg.pcap # tcpdump form requester 
│       └──responder_log.txt # responder log
├── 50_400M # The result of the program running after modifying in rdma_write_requester_my.c[344] : size_t mem_lens[4] = {50 * 1024 * 1024, 400 * 1024 * 1024, 50 * 1024 * 1024, 400 * 1024 * 1024};
│   ├── debug  #Logs obtained using the "script -c "gdb -x debug.gdb --args ./_doca_rdma_write_requester" all.log" command
│   │   ├──all.log # requester log
│   │   ├──dbg.pcap # tcpdump form requester 
│   │   └──responder_log.txt # responder log
│   └── run  #Logs obtained using the "./_doca_rdma_write_requester > run_all.log 2>&1" command
│       ├──all.log # requester log
│       ├──dbg.pcap # tcpdump form requester 
│       └──responder_log.txt # responder log
└── debug.gdb # gdb configuration file, compatible with x86
```


```plaintext
./requester
├── daemon_hugepages.c # First, allocate 1GB of hugepages memory.
├── debug.gdb # gdb configuration file, compatible with x86
├── doca_hp_shm.c # Manage reuse or create a new mmap.
├── doca_hp_shm.h
├── meson.build
├── rdma_common_pool_th.c # Correspond to rdma_common.c in the sample code.
├── rdma_common_pool_th.h
├── rdma_control_enum.c # Replaces the wait_for_enter() function with TCP-based management logic.
├── rdma_control_enum.h
├── rdma_write_requester_my.c # Correspond to rdma_write_requester_main.c in the sample code.
├── rdma_write_requester_sample.c # Correspond to rdma_write_requester_sample.c in the sample code.
├── rebuild.sh # Clean up the previous build results, reinitialize the Meson build environment, and compile the project.
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
├── rebuild.sh # Clean up the previous build results, reinitialize the Meson build environment, and compile the project.  Modify in rdma_write_responder_my.c '[157] const size_t mem_len = 400 * 1024 * 1024;[158] const int cm_port = 13840;[159] const int tcp_port = 18803;' to build responder1 to responder4
├── responder_all.sh # Entry points for executing responder1 to responder4 separately.
└── time_utils.h
```
