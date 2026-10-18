# =============================================================================
# 文档示例编译闸 —— 翻译单元生成器
#
# 由 CMakeLists.txt 在 configure 期以 `cmake -P` 调用（不需要额外工具链依赖）。
# 作用：把 doc/<lang>/*.md 中「带 #include 的 C++ 代码块」抽成**可单独编译**的翻译
#       单元，供 test_doc_examples 的编译闸使用。
#       （REQ-20261009-aggregate-header-doc-sync 的 AC-4 / AC-5 / AC-6 / AC-7）
#
# 入参：
#   DOC_DIR  文档目录（例如 <src>/doc/zh）
#   LANG     zh / en（用于生成唯一函数名与文件名）
#   OUT_DIR  生成目录
#
# 产出：
#   ${OUT_DIR}/doc_<lang>_<page>_<n>.cc   每个代码块一个 TU
#   ${OUT_DIR}/count.txt                  生成的 TU 数量（单个整数）
#   ${OUT_DIR}/pages.txt                  覆盖到的页面名（逗号分隔，供测试断言逐页覆盖）
#
# 抽取与等价 TU 口径（与需求文档《实现说明》里记录的基线一致）：
#   1. 只取 ```cpp / ```c++ / ```cc 围栏块；
#   2. 只取块内**至少有一条 `#include`** 的块（纯片段不计入；见 G1 决策 O4）；
#   3. 块内所有 `#include` 行提到 TU 顶部（位置不影响语义）；
#   4. 其余内容：若该块本身已是「声明单元」（含 `main`、类/结构/枚举/命名空间定义、
#      或顶层函数定义），原样保留；否则包进一个函数体，让「语句序列」式示例也能真编译。
#      这是「为每块构造等价 TU」，不改变示例语义。
# =============================================================================

cmake_minimum_required(VERSION 3.15)

if(NOT DEFINED DOC_DIR OR NOT DEFINED LANG OR NOT DEFINED OUT_DIR)
	message(FATAL_ERROR "gen_doc_examples.cmake 需要 -DDOC_DIR=<dir> -DLANG=<zh|en> -DOUT_DIR=<dir>")
endif()

file(MAKE_DIRECTORY "${OUT_DIR}")

# 单块是否为「声明单元」的判定式（CMake 正则不支持 \s/\w，故全部用显式字符类；
# 避免 `\(` 之类的反斜杠转义，一律写成 [(] [)] [{]）
set(RE_TOKEN "[A-Za-z0-9_:<>,&*]")
set(RE_FUNC "\n[ \t]*${RE_TOKEN}+([ \t]+${RE_TOKEN}+)*[ \t\r\n]+[A-Za-z0-9_:]+[ \t\r\n]*[(][^;]*[)][ \t\r\n]*(const[ \t\r\n]*)?(override|final)?[ \t\r\n]*[{]")
set(RE_DECL "\n[ \t]*(class|struct|union|enum|namespace)[ \t\r\n]")
set(RE_TMPL "\n[ \t]*template[ \t]*<")

file(GLOB DOC_FILES "${DOC_DIR}/*.md")
list(SORT DOC_FILES)

set(UNIT_COUNT 0)
set(PAGES "")

foreach(DOC_FILE ${DOC_FILES})
	get_filename_component(PAGE "${DOC_FILE}" NAME_WE)
	set(PAGE_ID "${PAGE}")
	string(REPLACE "-" "_" PAGE_ID "${PAGE_ID}")
	string(REPLACE "." "_" PAGE_ID "${PAGE_ID}")

	file(READ "${DOC_FILE}" CONTENT)
	string(REPLACE "\r\n" "\n" CONTENT "${CONTENT}")

	set(PAGE_UNITS_BEFORE ${UNIT_COUNT})
	set(BLOCK_INDEX 0)
	set(POS 0)
	set(IN_BLOCK FALSE)
	set(CONTENT_START 0)

	while(TRUE)
		# CMake 的 string(FIND) 不支持起始偏移，故先切出剩余串再相对定位
		string(SUBSTRING "${CONTENT}" ${POS} -1 REMAINDER)
		string(FIND "${REMAINDER}" "```" REL_FENCE)
		if(REL_FENCE EQUAL -1)
			break()
		endif()
		math(EXPR FENCE_POS "${POS} + ${REL_FENCE}")

		if(IN_BLOCK)
			# 闭合围栏：截出块内容
			math(EXPR BODY_LEN "${FENCE_POS} - ${CONTENT_START}")
			if(BODY_LEN LESS 0)
				set(BODY_LEN 0)
			endif()
			string(SUBSTRING "${CONTENT}" ${CONTENT_START} ${BODY_LEN} BODY)
			set(IN_BLOCK FALSE)
			math(EXPR POS "${FENCE_POS} + 3")

			# --- 只处理带 #include 的块 ---
			string(FIND "${BODY}" "#include" INCLUDE_POS)
			if(INCLUDE_POS EQUAL -1)
				continue()
			endif()

			# --- 拆出 include 行与其余内容 ---
			set(INCLUDES "")
			set(REST "")
			set(REMAIN "${BODY}")
			while(TRUE)
				string(FIND "${REMAIN}" "\n" LINE_END)
				if(LINE_END EQUAL -1)
					set(LINE "${REMAIN}")
					set(REMAIN "")
				else()
					string(SUBSTRING "${REMAIN}" 0 ${LINE_END} LINE)
					math(EXPR NEXT_POS "${LINE_END} + 1")
					string(SUBSTRING "${REMAIN}" ${NEXT_POS} -1 REMAIN)
				endif()
				string(STRIP "${LINE}" LINE_STRIPPED)
				if(LINE_STRIPPED MATCHES "^#include")
					set(INCLUDES "${INCLUDES}${LINE}\n")
				else()
					set(REST "${REST}${LINE}\n")
				endif()
				if(REMAIN STREQUAL "")
					break()
				endif()
			endwhile()

			# --- 判定是否需要包一层函数体 ---
			set(PROBE "\n${REST}")
			string(REGEX MATCH "${RE_FUNC}" MATCH_FUNC "${PROBE}")
			string(REGEX MATCH "${RE_DECL}" MATCH_DECL "${PROBE}")
			string(REGEX MATCH "${RE_TMPL}" MATCH_TMPL "${PROBE}")
			set(AS_IS FALSE)
			if(MATCH_FUNC OR MATCH_DECL OR MATCH_TMPL)
				set(AS_IS TRUE)
			endif()

			math(EXPR BLOCK_INDEX "${BLOCK_INDEX} + 1")
			math(EXPR UNIT_COUNT "${UNIT_COUNT} + 1")

			# 块起始行号（用于回溯定位）
			string(SUBSTRING "${CONTENT}" 0 ${CONTENT_START} PREFIX)
			string(REGEX REPLACE "[^\n]" "" NEWLINES "${PREFIX}")
			string(LENGTH "${NEWLINES}" LINE_NO)
			math(EXPR LINE_NO "${LINE_NO} + 1")

			set(OUT_FILE "${OUT_DIR}/doc_${LANG}_${PAGE_ID}_${BLOCK_INDEX}.cc")
			set(HEADER "// 自动生成，请勿手工修改。\n// 来源：doc/${LANG}/${PAGE}.md 第 ${LINE_NO} 行起的 C++ 代码块。\n// 由 tests/gen_doc_examples.cmake 抽取；口径见该脚本头部注释。\n")
			if(AS_IS)
				set(TU "${HEADER}${INCLUDES}\n${REST}")
			else()
				set(TU "${HEADER}${INCLUDES}\nvoid crazy_doc_example_${LANG}_${PAGE_ID}_${BLOCK_INDEX}() {\n${REST}}\n")
			endif()
			file(WRITE "${OUT_FILE}" "${TU}")
		else()
			# 起始围栏：读语言标签（从围栏后到本行行尾）
			math(EXPR REL_TAG_START "${REL_FENCE} + 3")
			string(SUBSTRING "${REMAINDER}" ${REL_TAG_START} -1 TAG_TAIL)
			string(FIND "${TAG_TAIL}" "\n" REL_TAG_END)
			if(REL_TAG_END EQUAL -1)
				break()
			endif()
			string(SUBSTRING "${TAG_TAIL}" 0 ${REL_TAG_END} TAG)
			string(STRIP "${TAG}" TAG)
			math(EXPR TAG_END "${FENCE_POS} + 3 + ${REL_TAG_END}")
			if(TAG STREQUAL "cpp" OR TAG STREQUAL "c++" OR TAG STREQUAL "cc")
				set(IN_BLOCK TRUE)
				math(EXPR CONTENT_START "${TAG_END} + 1")
			endif()
			math(EXPR POS "${TAG_END} + 1")
		endif()
	endwhile()

	# 只有真的产出过翻译单元的页面才算「被覆盖」
	if(UNIT_COUNT GREATER PAGE_UNITS_BEFORE)
		list(APPEND PAGES "${PAGE}")
	endif()
endforeach()

file(WRITE "${OUT_DIR}/count.txt" "${UNIT_COUNT}\n")
list(REMOVE_DUPLICATES PAGES)
list(SORT PAGES)
string(REPLACE ";" "," PAGES_JOINED "${PAGES}")
file(WRITE "${OUT_DIR}/pages.txt" "${PAGES_JOINED}\n")
message(STATUS "doc/${LANG}: generated ${UNIT_COUNT} example translation units into ${OUT_DIR}")
