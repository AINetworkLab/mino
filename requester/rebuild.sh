#!/bin/bash
rm -rf build 
meson setup build 
meson compile -C build

# rm /home/mino/load/samples/doca_rdma/doca_mmap_desc_*
# > /home/mino/load/samples/doca_rdma/doca_mmap_pool.txt