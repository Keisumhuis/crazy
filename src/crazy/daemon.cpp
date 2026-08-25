#include "crazy/daemon.h"

#include <signal.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <errno.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#include <algorithm>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>

#include "crazy/file_lock.h"
#include "crazy/logger.h"
#include "crazy/utils.h"

namespace crazy {
	namespace {
		//! 守护进程内部参数, 不对外暴露.
		const char* kDaemonRoleArg = "--crazy-daemon-role";
		const char* kDaemonRoleSupervisor = "supervisor";
		const char* kDaemonRoleWorker = "worker";

		//! 1号守护进程停止标记.
		volatile sig_atomic_t g_daemonStopping = 0;

		/**
		 * @brief 子进程句柄.
		 */
		struct ChildProcess {
#ifdef _WIN32
			HANDLE process = nullptr;
#else
			pid_t pid = -1;
#endif
		};

		/**
		 * @brief 解析守护进程角色.
		 */
		DaemonRole ParseDaemonRole(const std::string& role) {
			if (role == kDaemonRoleSupervisor) {
				return DaemonRole::supervisor;
			}
			if (role == kDaemonRoleWorker) {
				return DaemonRole::worker;
			}
			return DaemonRole::none;
		}

		/**
		 * @brief 生成子进程参数.
		 */
		std::vector<std::string> BuildChildArguments(const std::string& role, const std::vector<std::string>& baseArgs) {
			std::vector<std::string> args = baseArgs;
			if (args.empty()) {
				args.push_back(PathUtil::GetExecutablePath());
			}
			else {
				args[0] = PathUtil::GetExecutablePath();
			}
			args.insert(args.begin() + 1, std::string(kDaemonRoleArg) + "=" + role);
			return args;
		}

#ifdef _WIN32
		/**
		 * @brief Windows命令行参数转义.
		 */
		std::string QuoteWindowsArgument(const std::string& arg) {
			if (arg.empty()) {
				return "\"\"";
			}

			bool needQuote = false;
			for (char ch : arg) {
				if (ch == ' ' || ch == '\t' || ch == '"') {
					needQuote = true;
					break;
				}
			}
			if (!needQuote) {
				return arg;
			}

			std::string result = "\"";
			size_t backslashes = 0;
			for (char ch : arg) {
				if (ch == '\\') {
					++backslashes;
					continue;
				}
				if (ch == '"') {
					result.append(backslashes * 2 + 1, '\\');
					result.push_back(ch);
					backslashes = 0;
					continue;
				}
				result.append(backslashes, '\\');
				backslashes = 0;
				result.push_back(ch);
			}
			result.append(backslashes * 2, '\\');
			result.push_back('"');
			return result;
		}

		/**
		 * @brief 生成Windows命令行.
		 */
		std::string BuildWindowsCommandLine(const std::vector<std::string>& args) {
			std::string commandLine;
			for (const auto& arg : args) {
				if (!commandLine.empty()) {
					commandLine.push_back(' ');
				}
				commandLine += QuoteWindowsArgument(arg);
			}
			return commandLine;
		}

		/**
		 * @brief Windows控制台信号处理.
		 */
		BOOL WINAPI DaemonConsoleHandler(DWORD) {
			g_daemonStopping = 1;
			return TRUE;
		}
#else
		/**
		 * @brief 重定向标准输入输出到空设备.
		 */
		void RedirectStandardIoToNull() {
			int fd = open("/dev/null", O_RDWR);
			if (fd < 0) {
				return;
			}
			dup2(fd, STDIN_FILENO);
			dup2(fd, STDOUT_FILENO);
			dup2(fd, STDERR_FILENO);
			if (fd > STDERR_FILENO) {
				close(fd);
			}
		}

		/**
		 * @brief Linux信号处理.
		 */
		void DaemonSignalHandler(int) {
			g_daemonStopping = 1;
		}
#endif

		/**
		 * @brief 启动子进程.
		 */
		bool StartProcess(const std::vector<std::string>& args, bool detached, ChildProcess* child) {
			if (args.empty()) {
				return false;
			}
#ifdef _WIN32
			STARTUPINFOA startupInfo;
			PROCESS_INFORMATION processInfo;
			ZeroMemory(&startupInfo, sizeof(startupInfo));
			ZeroMemory(&processInfo, sizeof(processInfo));
			startupInfo.cb = sizeof(startupInfo);

			std::string commandLine = BuildWindowsCommandLine(args);
			DWORD creationFlags = CREATE_NEW_PROCESS_GROUP;
			if (detached) {
				creationFlags |= DETACHED_PROCESS;
			}

			BOOL ok = CreateProcessA(
				args[0].c_str(),
				commandLine.empty() ? nullptr : &commandLine[0],
				nullptr,
				nullptr,
				FALSE,
				creationFlags,
				nullptr,
				nullptr,
				&startupInfo,
				&processInfo);

			if (!ok) {
				return false;
			}

			CloseHandle(processInfo.hThread);
			if (child) {
				child->process = processInfo.hProcess;
			}
			else {
				CloseHandle(processInfo.hProcess);
			}
			return true;
#else
			pid_t pid = fork();
			if (pid < 0) {
				return false;
			}
			if (pid == 0) {
				if (detached) {
					if (setsid() < 0) {
						_exit(1);
					}
					RedirectStandardIoToNull();
				}

				std::vector<char*> argv;
				argv.reserve(args.size() + 1);
				for (const auto& arg : args) {
					argv.push_back(const_cast<char*>(arg.c_str()));
				}
				argv.push_back(nullptr);
				execv(args[0].c_str(), argv.data());
				_exit(127);
			}

			if (child) {
				child->pid = pid;
			}
			return true;
#endif
		}

		/**
		 * @brief 等待子进程退出.
		 */
		int WaitProcess(ChildProcess& child) {
#ifdef _WIN32
			while (!g_daemonStopping) {
				DWORD waitResult = WaitForSingleObject(child.process, 1000);
				if (waitResult == WAIT_OBJECT_0) {
					DWORD exitCode = 1;
					GetExitCodeProcess(child.process, &exitCode);
					CloseHandle(child.process);
					child.process = nullptr;
					return static_cast<int>(exitCode);
				}
				if (waitResult != WAIT_TIMEOUT) {
					CloseHandle(child.process);
					child.process = nullptr;
					return 1;
				}
			}

			TerminateProcess(child.process, 0);
			WaitForSingleObject(child.process, INFINITE);
			CloseHandle(child.process);
			child.process = nullptr;
			return 0;
#else
			int status = 0;
			while (true) {
				pid_t rt = waitpid(child.pid, &status, 0);
				if (rt == child.pid) {
					if (WIFEXITED(status)) {
						return WEXITSTATUS(status);
					}
					if (WIFSIGNALED(status)) {
						return 128 + WTERMSIG(status);
					}
					return 1;
				}
				if (rt < 0 && errno == EINTR) {
					if (g_daemonStopping) {
						kill(child.pid, SIGTERM);
					}
					continue;
				}
				return 1;
			}
#endif
		}

		/**
		 * @brief 安装守护进程信号处理.
		 */
		void InstallSignalHandlers() {
			g_daemonStopping = 0;
#ifdef _WIN32
			SetConsoleCtrlHandler(DaemonConsoleHandler, TRUE);
#else
			signal(SIGTERM, DaemonSignalHandler);
			signal(SIGINT, DaemonSignalHandler);
			signal(SIGHUP, SIG_IGN);
#endif
		}

		/**
		 * @brief 等待下次重启.
		 */
		bool SleepBeforeRestart(int seconds) {
			for (int i = 0; i < seconds && !g_daemonStopping; ++i) {
				std::this_thread::sleep_for(std::chrono::seconds(1));
			}
			return !g_daemonStopping;
		}
	}

	DaemonArguments Daemon::ParseArguments(int32_t argc, char** argv) {
		DaemonArguments result;
		if (argc > 0 && argv && argv[0]) {
			result.applicationArgs.push_back(argv[0]);
			result.childArgs.push_back(argv[0]);
		}

		for (int32_t i = 1; i < argc; ++i) {
			std::string arg = argv[i] ? argv[i] : "";
			if (arg == "-d" || arg == "--daemon") {
				result.enabled = true;
				result.applicationArgs.push_back(arg);
				continue;
			}

			if (arg == kDaemonRoleArg) {
				if (i + 1 < argc) {
					result.role = ParseDaemonRole(argv[++i] ? argv[i] : "");
				}
				continue;
			}

			const std::string rolePrefix = std::string(kDaemonRoleArg) + "=";
			if (arg.compare(0, rolePrefix.size(), rolePrefix) == 0) {
				result.role = ParseDaemonRole(arg.substr(rolePrefix.size()));
				continue;
			}

			result.applicationArgs.push_back(arg);
			result.childArgs.push_back(arg);
		}

		if (result.applicationArgs.empty()) {
			result.applicationArgs.push_back(PathUtil::GetExecutablePath());
		}
		if (result.childArgs.empty()) {
			result.childArgs.push_back(PathUtil::GetExecutablePath());
		}

		return result;
	}

	bool Daemon::StartSupervisor(const std::vector<std::string>& serviceArgs) {
		auto supervisorArgs = BuildChildArguments(kDaemonRoleSupervisor, serviceArgs);
		return StartProcess(supervisorArgs, true, nullptr);
	}

	int Daemon::RunSupervisor(const std::vector<std::string>& serviceArgs) {
		InstallSignalHandlers();

		FileLock daemonLock(PathUtil::GetExecutableName() + ".daemon.lock");
		if (!daemonLock.lock()) {
			const char* duplicateStartTips = "the daemon supervisor has been launched. please do not start it again";
			std::cerr << duplicateStartTips << std::endl;
			CRAZY_SYSTEM_ERROR() << duplicateStartTips;
			return 0;
		}

		int restartDelay = 1;
		while (!g_daemonStopping) {
			auto workerArgs = BuildChildArguments(kDaemonRoleWorker, serviceArgs);
			ChildProcess worker;
			auto startTime = std::chrono::steady_clock::now();
			if (!StartProcess(workerArgs, false, &worker)) {
				CRAZY_SYSTEM_ERROR() << "start daemon worker failed";
				return 1;
			}

			int exitCode = WaitProcess(worker);
			if (g_daemonStopping || exitCode == 0) {
				return 0;
			}

			CRAZY_SYSTEM_ERROR() << "daemon worker exited abnormally, exit code = " << exitCode;
			auto aliveSeconds = std::chrono::duration_cast<std::chrono::seconds>(
				std::chrono::steady_clock::now() - startTime).count();
			if (aliveSeconds >= 60) {
				restartDelay = 1;
			}

			if (!SleepBeforeRestart(restartDelay)) {
				break;
			}
			restartDelay = restartDelay * 2 > 30 ? 30 : restartDelay * 2;
		}

		return 0;
	}
}
