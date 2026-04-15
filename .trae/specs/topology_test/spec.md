# 拓扑结构多进程测试 - 产品需求文档

## Overview
- **Summary**: 基于eProto协议，创建一个多进程测试环境，用于验证复杂拓扑结构中的设备间通信
- **Purpose**: 测试和验证`combined_topology.md`中定义的拓扑结构，确保设备间能够按照最短路径正确通信
- **Target Users**: 开发人员和测试人员，用于验证eProto协议在复杂网络拓扑中的通信能力

## Goals
- 实现5个设备（A、B、C、D、E）的多进程通信
- 按照拓扑结构中的最短路径进行通信
- 支持命令行发送消息和回复消息
- 验证设备间的通信路径是否符合预期
- 提供清晰的拓扑结构文档

## Non-Goals (Out of Scope)
- 不实现真实的网络协议栈
- 不处理网络故障和恢复
- 不支持动态拓扑变更
- 不实现性能测试

## Background & Context
- 基于现有的`multi_process`示例，使用管道进行进程间通信
- 拓扑结构定义在`combined_topology.md`中，需要按照该结构实现设备间的通信
- 每个设备作为独立进程运行，通过管道与其他设备通信

## Functional Requirements
- **FR-1**: 实现5个设备进程（A、B、C、D、E）
- **FR-2**: 按照拓扑结构建立进程间的管道通信
- **FR-3**: 支持命令行发送消息到指定设备
- **FR-4**: 支持自动回复接收到的消息
- **FR-5**: 实现消息的转发功能，按照最短路径路由
- **FR-6**: 提供拓扑结构文档

## Non-Functional Requirements
- **NFR-1**: 进程启动和通信稳定可靠
- **NFR-2**: 命令行界面友好，操作简单
- **NFR-3**: 通信路径符合拓扑结构定义
- **NFR-4**: 错误处理机制完善

## Constraints
- **Technical**: 使用管道进行进程间通信
- **Dependencies**: 依赖eProto协议库
- **Limitations**: 仅支持本地测试，不支持网络分布式部署

## Assumptions
- 所有设备进程在同一台机器上运行
- 管道通信是可靠的
- eProto协议库已经正确实现

## Acceptance Criteria

### AC-1: 设备进程启动
- **Given**: 所有设备进程启动
- **When**: 执行启动脚本
- **Then**: 5个设备进程成功启动，无错误
- **Verification**: `programmatic`

### AC-2: 拓扑结构文档
- **Given**: 查看拓扑测试目录
- **When**: 查看README.md文件
- **Then**: 文档包含完整的拓扑结构和通信路径
- **Verification**: `human-judgment`

### AC-3: 设备间通信
- **Given**: 设备A运行
- **When**: 发送消息到设备B
- **Then**: 设备B收到消息并回复
- **Verification**: `programmatic`

### AC-4: 消息转发
- **Given**: 设备A运行
- **When**: 发送消息到设备C（通过B转发）
- **Then**: 设备C收到消息并回复，消息正确经过B转发
- **Verification**: `programmatic`

### AC-5: 命令行界面
- **Given**: 设备进程运行
- **When**: 输入命令发送消息
- **Then**: 命令执行成功，消息发送并收到回复
- **Verification**: `human-judgment`

## Open Questions
- [ ] 管道命名和管理方式
- [ ] 设备启动顺序
- [ ] 错误处理机制
- [ ] 测试脚本的实现