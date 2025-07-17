set pagination off
set confirm off

set breakpoint pending on
break rdma_ack_cm_event
set style enabled off

commands
  bt
  # 打印第一个参数（rdma_cm_event指针）
  p/x $rdi
  # 查看内存内容
  x/4gx $rdi
  # 解析事件结构体
  p *(struct rdma_cm_event*)$rdi
  continue
end

run