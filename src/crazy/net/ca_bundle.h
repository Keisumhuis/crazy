/**
 * @file ca_bundle.h
 * @author keisum (keisumhuis@gmail.com)
 * @brief 随框架提供的 CA bundle（编译进库，不依赖运行期相对路径）
 * @version 0.1
 * @date 2026-10-10
 *
 * @copyright Copyright (c) 2026
 */
#pragma once

#include <cstddef>
#include <string>

namespace crazy {
	/**
	 * @brief 随框架编译进来的 CA bundle 的 PEM 文本.
	 * @return 未编译进来时返回空字符串.
	 */
	const std::string& bundledCaPem();
	/**
	 * @brief 随框架编译进来的 CA bundle 中的证书数量.
	 * @return 未编译进来时返回 `0`.
	 */
	std::size_t bundledCaCertificateCount();
	/**
	 * @brief 随框架编译进来的 CA bundle 的仓库内来源路径（供日志/诊断使用）.
	 * @return 未编译进来时返回空字符串.
	 */
	const char* bundledCaSource();
}
