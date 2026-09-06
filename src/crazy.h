/**
 * @file crazy.h
 * @author kesium (keisumhuis@gmail.com)
 * @brief
 * @version 0.1
 * @date 13
 *
 * @copyright Copyright (c) 2025
 */
#pragma once

#include "crazy/actor_interface.h"
#include "crazy/application.h"
#include "crazy/atomic_lock.h"
#include "crazy/buffer.h"
#include "crazy/byte_order.h"
#include "crazy/cond_mutex.h"
#include "crazy/config.h"
#include "crazy/date_time.h"
#include "crazy/file_lock.h"
#include "crazy/json.h"
#include "crazy/key_value_pair.h"
#include "crazy/logger.h"
#include "crazy/message_base.h"
#include "crazy/mvcc_lock_wapper.h"
#include "crazy/nocopyable.h"
#include "crazy/protocol.h"
#include "crazy/reflection.h"
#include "crazy/singleton.h"
#include "crazy/time_zone.h"
#include "crazy/uri.h"
#include "crazy/utils.h"
#include "crazy/uuid.h"

#include "crazy/mmap/mmap.h"
#include "crazy/mmap/mmap_vector.h"

#include "crazy/encryption/base64.h"
#include "crazy/encryption/md5.h"

#include "crazy/mysql/mysql_connection.h"
#include "crazy/mysql/mysql_connection_pool.h"
#include "crazy/mysql/mysql_native_connection.h"

#include "crazy/http_application.h"

#include "crazy/clickhouse/clickhouse_connection.h"
#include "crazy/clickhouse/clickhouse_connection_pool.h"

#include "crazy/net/acceptor.h"
#include "crazy/net/client_actor.h"
#include "crazy/net/local_socket.h"
#include "crazy/net/selector.h"
#include "crazy/net/service_actor.h"
#include "crazy/net/socket.h"
#include "crazy/net/telnet_service_actor.h"

#include "crazy/net/http/http_header.h"
#include "crazy/net/http/http_multipart.h"
#include "crazy/net/http/http_message_base.h"
#include "crazy/net/http/http_request.h"
#include "crazy/net/http/http_router.h"
#include "crazy/net/http/http_response.h"
#include "crazy/net/http/http_version.h"
#include "crazy/net/http/http11_common.h"
