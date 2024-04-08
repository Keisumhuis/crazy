/**
 * @file crazy.h
 * @version 0.0.1
 * @date 2023-12-07
 * @author Keisum (keisumhuis@gmail.com)
 * @copyright Copyright (c) 2023  Keisum_huis
 * @brief
 */
#ifndef ____CRAZY_CRAZY_H____
#define ____CRAZY_CRAZY_H____

#include "crazy/address.h"
#include "crazy/ascii_logo.h"
#include "crazy/allocator.h"
#include "crazy/assert.h"
#include "crazy/byte_array.h"
#include "crazy/endian.h"
#include "crazy/config.h"
#include "crazy/coroutine.h"
#include "crazy/format.h"
#include "crazy/hook.h"
#include "crazy/lexicl_cast.h"
#include "crazy/logger.h"
#include "crazy/message.h"
#include "crazy/mutex.h"
#include "crazy/processer.h"
#include "crazy/rapidjson_reflection.h"
#include "crazy/reflection.h"
#include "crazy/selector.h"
#include "crazy/scheduler.h"
#include "crazy/socket.h"
#include "crazy/tcp_server.h"
#include "crazy/thread.h"
#include "crazy/timer.h"
#include "crazy/uri.h"
#include "crazy/util.h"

#include "crazy/http/http11.h"
#include "crazy/http/http11_common.h"
#include "crazy/http/http11_parser.h"
#include "crazy/http/httpclient_parser.h"
#include "crazy/http/request.h"
#include "crazy/http/response.h"

#include "crazy/util/crypto_util.h"
#include "crazy/util/hash_util.h"

#endif // ! ____CRAZY_CRAZY_H____
