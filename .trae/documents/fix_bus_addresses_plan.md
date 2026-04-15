# 修正总线地址配置计划

## 问题分析
之前的总线地址配置有误，需要按照以下方式重新配置：

## 总线拓扑结构
- **总线1（地址0x01）**：设备1拥有
- **总线2（地址0x02）**：设备2拥有
- **总线3（地址0x03）**：设备2拥有  
- **总线4（地址0x04）**：设备3拥有

## 正确的配置方式

### eproto_add_bus 参数
添加的是**自己的总线地址**（main里面定义的设备地址）

### eproto_add_destination_device 参数
添加的是**对方的总线地址**

## 需要修改的文件

1. **device1.c**
   - eproto_add_bus使用地址0x01（设备1自己的总线地址）
   - eproto_add_destination_device添加0x02（设备2的总线2）和0x04（设备3的总线4）
   - device1_receive_thread中eproto_receive_byte使用0x01

2. **device2.c**
   - 第一条总线：eproto_add_bus使用0x02（设备2自己的总线2）
   - 第一条总线：eproto_add_destination_device添加0x01（设备1的总线1）
   - 第二条总线：eproto_add_bus使用0x03（设备2自己的总线3）
   - 第二条总线：eproto_add_destination_device添加0x04（设备3的总线4）
   - device2_receive_thread中：
     - 第一条总线使用0x02
     - 第二条总线使用0x03

3. **device3.c**
   - eproto_add_bus使用0x04（设备3自己的总线地址）
   - eproto_add_destination_device添加0x03（设备2的总线3）
   - device3_receive_thread中eproto_receive_byte使用0x04

## 修改步骤
1. 先修改device1.c的总线配置
2. 再修改device2.c的两条总线配置
3. 最后修改device3.c的总线配置
4. 编译并测试

## 注意事项
- 确保所有总线发送和接收函数使用正确的缓冲区
- 确保eproto_receive_byte使用正确的总线地址
