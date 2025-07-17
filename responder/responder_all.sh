#!/bin/bash


> responder_log.txt
# 启动4个程序，后台运行并合并日志
./_doca_rdma_write_responder1 >> responder_log.txt 2>&1 &
./_doca_rdma_write_responder2_100M >> responder_log.txt 2>&1 &
./_doca_rdma_write_responder3 >> responder_log.txt 2>&1 &
./_doca_rdma_write_responder4_100M >> responder_log.txt 2>&1 &

# 提示启动完成
echo "4个 responder 已启动，日志输出到 responder_log.txt"