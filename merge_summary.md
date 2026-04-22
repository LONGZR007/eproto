此次合并主要添加了转发回调机制，允许在数据包转发过程中进行加密/解密和数据转换，同时更新了相关API和示例代码。这些变更增强了eProto协议的灵活性和安全性，使其能够更好地适应复杂的嵌入式通信场景。
| 文件 | 变更 |
|------|---------|
| README.md | - 添加了转发回调机制的功能介绍<br>- 更新了eproto_add_bus API的使用示例，添加了forward_callback参数<br>- 添加了转发回调机制文档的链接 |
| inc/eproto.h | - 移除了旧的总线管理结构体定义<br>- 添加了转发后处理回调函数类型eproto_forward_post_func_t<br>- 添加了转发回调函数类型eproto_forward_callback_t<br>- 更新了总线管理结构体，添加了forward_callback字段<br>- 更新了eproto_add_bus函数声明，添加了forward_callback参数 |
| src/eproto.c | - 初始化总线管理结构体时设置forward_callback为NULL<br>- 更新了eproto_add_bus函数实现，添加了forward_callback参数的处理<br>- 在eproto_forward_frame函数中实现了转发回调机制，支持在转发前处理数据<br>- 添加了后处理回调的调用逻辑 |
| examples/simple_test/simple_test.c | - 更新了两个设备的eproto_add_bus调用，添加了NULL作为forward_callback参数 |
| examples/thread_example/device1.c | - 更新了eproto_add_bus调用，添加了NULL作为forward_callback参数 |
| examples/thread_example/device2.c | - 更新了eproto_add_bus调用，添加了NULL作为forward_callback参数 |
| examples/thread_example/device3.c | - 更新了eproto_add_bus调用，添加了NULL作为forward_callback参数 |
| examples/serial_three_devices/device_macro.c | - 更新了eproto_add_bus调用，添加了NULL作为forward_callback参数 |
| examples/topology_test/process_a.c | - 更新了eproto_add_bus调用，添加了NULL作为forward_callback参数 |
| examples/topology_test/process_b.c | - 更新了eproto_add_bus调用，添加了NULL作为forward_callback参数 |
| examples/topology_test/process_c.c | - 更新了eproto_add_bus调用，添加了NULL作为forward_callback参数 |
| examples/topology_test/process_d.c | - 更新了eproto_add_bus调用，添加了NULL作为forward_callback参数 |
| examples/topology_test/process_e.c | - 更新了eproto_add_bus调用，添加了NULL作为forward_callback参数 |
| docs/FORWARD_CALLBACK_IMPLEMENTATION.md | - 新增了转发回调机制的详细实现文档 |
| examples/forward_callback_test/Makefile | - 新增了转发回调测试的Makefile |
| examples/forward_callback_test/common.c | - 新增了转发回调测试的公共代码 |
| examples/forward_callback_test/common.h | - 新增了转发回调测试的公共头文件 |
| examples/forward_callback_test/device_a.c | - 新增了转发回调测试的设备A代码 |
| examples/forward_callback_test/device_b.c | - 新增了转发回调测试的设备B代码 |
| examples/forward_callback_test/device_c.c | - 新增了转发回调测试的设备C代码 |
| examples/forward_callback_test/eproto_config.h | - 新增了转发回调测试的配置文件 |
| examples/forward_callback_test/fixed_block_allocator.c | - 新增了固定块分配器实现 |
| examples/forward_callback_test/fixed_block_allocator.h | - 新增了固定块分配器头文件 |
| examples/forward_callback_test/main.c | - 新增了转发回调测试的主程序 |
| examples/forward_callback_test/run_with_timeout.py | - 新增了转发回调测试的超时运行脚本 |