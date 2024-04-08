# 协程与协程调度器（coroutine & scheduler）
[toc]

## 协程框架调研
### libgo
- 魅族
- windows、linux
[https://github.com/yyzybb537/libgo](https://github.com/yyzybb537/libgo)
### libco
- 腾讯
- linux
[https://github.com/Tencent/libco](https://github.com/Tencent/libco)
### mordor/sylar
- mozy
- windows、linux、mac
[https://github.com/mozy/mordor](https://github.com/mozy/mordor)
### coroutine-cloudwu
- 风云
- linux
[https://github.com/cloudwu/coroutine](https://github.com/cloudwu/coroutine)
### C++20
- 
[https://isocpp.org/](https://isocpp.org/)

## 协程实现方式
### ucountext_t (coroutine-cloudwu、mordor/crazy)
### fiber (mordor)
### setjmp/longjmp (mordor)
### boost.fcontext (libgo、mordor)
### 汇编 (libco)

## 协程
### 有栈协程
### 无栈协程
### 对称协程
### 非对称协程

## 协程调度器
协程调度器，一个进程中根据CPU最大支持线程数开启对应的线程（16超线程-16调度器线程，8超线程-8调度器线程），
### GMP 模型：
![img](https://upload-images.jianshu.io/upload_images/14151453-3c06a96e56ff490b.png?imageMogr2/auto-orient/strip|imageView2/2/w/1200/format/webp)
	G (Goroutine)：Goroutine 是 Go 语言实现的轻量级线程，它是用户空间中的并发执行实体。每个 Goroutine 包含一个栈、一组寄存器以及待执行的函数。在Go程序中，创建新Goroutine的开销非常小，且可以动态调整栈大小。
	M (Machine): M 表示操作系统的线程，也就是内核线程。每一个 M 都与一个操作系统线程相对应，并在其上执行 Go 的代码。M 负责执行 Goroutine，但并不直接控制哪些 Goroutine 应该运行；它需要通过绑定到 P 来获取可运行的 Goroutine。
	P (Processor)：P 可以理解为逻辑处理器或调度器上下文。系统中存在多个 P，每个 P 都有自己的本地任务队列，存储着等待运行的 Goroutine。M 必须持有至少一个 P 才能执行 Goroutine，当 M 没有可运行的 Goroutine 时，会尝试从其关联的 P 的本地队列或其他 P 的全局队列中窃取任务来执行。
#### GMP 调度器的工作流程大致如下：
- 当程序启动时，Go 运行时创建一些初始的 M 和 P。
- 用户创建新的 Goroutine，这些 Goroutine 被放入某个 P 的本地任务队列。
- 每个 M 在执行完当前 Goroutine 后，会从关联的 P 的队列中取出下一个 Goroutine 继续执行。
- 如果 P 的本地队列为空，则 M 尝试从其他 P 的队列中窃取任务（任务窃取机制），或者将自己置于空闲列表并让出 CPU 时间片给其他活跃的 M。
- 当有 Goroutine 阻塞时（例如进行 I/O 操作），对应的 M 会选择释放当前 P 并阻塞，同时运行时可能会创建新的 M 或者唤醒已存在的空闲 M 来服务于其他的 P，确保多核CPU得到充分利用。
