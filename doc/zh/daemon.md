# Daemon

[返回文档目录](../README.md)

`crazy::Daemon` 提供 supervisor/worker 守护进程框架。`Application::exec()` 已经自动解析 `-d`，业务程序通常不需要手工拼接参数。

## 以守护进程运行

```cpp
#include "crazy.h"

int main(int argc, char** argv) {
    crazy::Application app(argc, argv);
    app.registerActor<crazy::ServiceActor>("service");
    app.exec();
}
```

```bash
./your_app -d
```

父进程会启动并监控 worker。worker 异常退出后，supervisor 会重新拉起；应用重启通过退出码 `1` 传递给 supervisor。

## 手工解析参数

```cpp
auto args = crazy::Daemon::ParseArguments(argc, argv);

if (args.enabled && args.role == crazy::DaemonRole::supervisor) {
    return crazy::Daemon::RunSupervisor(args.childArgs);
}

// worker 或普通模式继续初始化业务。
```

| 角色 | 含义 |
|------|------|
| `none` | 普通前台进程 |
| `supervisor` | 只负责监控和拉起 worker |
| `worker` | 执行业务逻辑的进程 |

守护模式会通过文件锁阻止同名 supervisor 或应用重复启动。

## 启动流程

执行 `./your_app -d` 后，流程如下：

1. 初次进程解析 `-d`，判断需要启动 supervisor。
2. 框架尝试获取 `<程序名>.daemon.lock` 和 `<程序名>.lock`。
3. 获取成功后，`Daemon::StartSupervisor()` 启动 supervisor 进程。
4. supervisor 以 worker 角色启动业务进程，并持续等待其退出。
5. worker 正常停止时 supervisor 结束；异常退出时 supervisor 重新拉起。
6. `restartService()` 让 worker 以退出码 1 结束，从而触发重新拉起。

## 停止与重启

```bash
./your_app -s application@shutdown
./your_app -s application@restart
```

如果命令行不可用，可以向 worker 发送系统终止信号。supervisor 和 worker 的生命周期由部署方式决定，Windows 服务、systemd 或容器编排通常有自己的重启策略，不应在同一进程上叠加多层守护机制。

## 锁文件

重复启动时会出现明确的日志：

```text
the daemon supervisor has been launched. please do not start it again
the application has been launched. please do not start it again
```

锁文件由进程持有，异常退出后操作系统会释放句柄。若文件残留但锁已释放，下一次启动仍可获得锁；不要在程序运行时手工删除锁文件。

## 手工监督循环示例

只有需要自定义启动参数处理时才应绕过 Application：

```cpp
int main(int argc, char** argv) {
    auto args = crazy::Daemon::ParseArguments(argc, argv);

    if (args.enabled &&
        args.role == crazy::DaemonRole::supervisor) {
        return crazy::Daemon::RunSupervisor(args.childArgs);
    }

    if (args.enabled &&
        args.role == crazy::DaemonRole::worker) {
        CRAZY_SYSTEM_INFO() << "running worker";
    }

    return 0;
}
```

`childArgs` 已移除守护进程内部参数，适合直接用于重启子进程。`applicationArgs` 保留业务参数，适合交给应用自己的解析器。

## 部署建议

- 前台运行和守护模式不要同时启用外部进程管理器。
- 日志应落到稳定目录，便于排查 worker 重启原因。
- worker 退出码应区分正常停止和异常退出。
- 启动脚本应等待配置文件、数据目录和端口准备完成。
- 多实例部署应让每个实例拥有独立的可执行文件名、锁文件和命令 Socket。

相关文档：[Application](application.md)、[配置管理](config.md)、[日志系统](logger.md)。
