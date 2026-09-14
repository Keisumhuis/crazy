# crazy 文档目录

[English documentation](README.en.md) | [中文文档索引](doc/README.md)

## 入门

| 文档 | 说明 |
|------|------|
| [功能总览](doc/zh/overview.md) | 项目定位、模块划分和最小运行示例 |
| [编译与构建](doc/zh/build.md) | CMake 选项、跨平台构建脚本和第三方依赖 |
| [接入现有项目](doc/zh/integration.md) | 头文件、CMake 目标、手动链接方式 |
| [测试](doc/zh/testing.md) | 测试构建、单测运行方式和目标分组 |

## 应用与消息

| 文档 | 说明 |
|------|------|
| [Application](doc/zh/application.md) | 进程入口、Actor 注册、路由、线程池和生命周期 |
| [Actor](doc/zh/actor.md) | 独立线程、消息队列、异步任务和事件循环 |
| [MessageBase](doc/zh/message.md) | 请求、应答、路由字段和强类型内部消息 |
| [Daemon](doc/zh/daemon.md) | supervisor/worker 守护进程模式和重启机制 |

## 网络

| 文档 | 说明 |
|------|------|
| [TCP Socket](doc/zh/socket.md) | IPv4 TCP 监听、连接、收发和短写处理 |
| [LocalSocket](doc/zh/local-socket.md) | 同机进程通信和 Application 命令 Socket |
| [Selector 与定时任务](doc/zh/selector.md) | 多路复用、循环定时器和每日任务 |
| [Encoder](doc/zh/encoder.md) | MessageBase 二进制帧编码 |
| [Decoder](doc/zh/decoder.md) | 增量帧解析、长度限制和异常处理 |
| [Session](doc/zh/session.md) | 连接、收发缓冲、消息回调和背压 |
| [ServiceActor](doc/zh/service-actor.md) | TCP 服务端会话、心跳和业务入口 |
| [ClientActor](doc/zh/client-actor.md) | 客户端连接、断线和心跳状态 |
| [TelnetServiceActor](doc/zh/telnet-service.md) | 密码认证和文本命令转发 |
| [HTTP 服务端](doc/zh/http-server.md) | 路由、延迟响应、静态文件和目录列表 |
| [HTTP 客户端](doc/zh/http-client.md) | 同步请求、连接复用和 multipart 上传 |
| [WebSocket](doc/zh/websocket.md) | WebSocket 服务端和阻塞式客户端 |
| [SMTP 客户端](doc/zh/smtp.md) | 明文 SMTP、认证和文本邮件发送 |

## 配置与序列化

| 文档 | 说明 |
|------|------|
| [日志系统](doc/zh/logger.md) | 六级日志、Appender 和自定义格式 |
| [配置管理](doc/zh/config.md) | INI 文件和目录加载及类型化读取 |
| [Base64](doc/zh/base64.md) | 二进制数据的文本编码与解码 |
| [MD5](doc/zh/md5.md) | 兼容性摘要计算和安全边界 |
| [JSON 序列化](doc/zh/json.md) | 基础类型、STL 容器和自定义类型 |
| [REFLECTION](doc/zh/reflection.md) | 自定义类型字段反射和序列化集成 |
| [二进制协议](doc/zh/protocol.md) | 类型标记、手动读写和反射编解码 |

## 数据与存储

| 文档 | 说明 |
|------|------|
| [MySQL](doc/zh/mysql.md) | 连接、查询、事务、预处理语句和连接池 |
| [ClickHouse](doc/zh/clickhouse.md) | 查询、Block 插入和连接池 |
| [Buffer](doc/zh/buffer.md) | 读写游标、动态扩容和复用 |
| [MmapInterface](doc/zh/mmap-interface.md) | 跨平台内存映射文件的打开、扩容和关闭 |
| [MmapVector](doc/zh/mmap-vector.md) | 基于映射文件的持久化连续数组 |

## 并发

| 文档 | 说明 |
|------|------|
| [ThreadPool](doc/zh/thread-pool.md) | 固定线程池、轮询和指定线程投递 |
| [AtomicLock](doc/zh/atomic-lock.md) | 适用于极短临界区的自旋锁 |
| [CondMutex](doc/zh/cond-mutex.md) | 互斥锁、条件等待和唤醒 |
| [FileLock](doc/zh/file-lock.md) | 跨进程文件锁和单实例控制 |
| [MVCCLockWrapper](doc/zh/mvcc.md) | 无锁读取、写事务和版本检测 |

## 基础工具

| 文档 | 说明 |
|------|------|
| [DateTime](doc/zh/date-time.md) | 日期解析、格式化、比较和日历计算 |
| [TimeZone](doc/zh/time-zone.md) | 固定偏移、UTC 和本地时间转换 |
| [URI](doc/zh/uri.md) | URI 解析、修改、比较和相对地址解析 |
| [通用工具](doc/zh/utilities.md) | 时间戳、UUID、字符串、路径、线程和端序 |
| [KeyValuePair](doc/zh/key-value-pair.md) | 动态行数据、类型读取和多种导出格式 |
