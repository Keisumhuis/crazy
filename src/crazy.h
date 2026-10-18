/**
 * @file crazy.h
 * @author kesium (keisumhuis@gmail.com)
 * @brief crazy 聚合头：按目录分组收录全部自研头
 * @version 0.1
 * @date 13
 *
 * @copyright Copyright (c) 2025
 */
#pragma once

// 未收录 crazy/rapidjson/**（vendored，经 crazy/json.h 暴露）；wepoll.h 只能在 Windows 上用。

// crazy/
#include "crazy/actor_interface.h"
#include "crazy/application.h"
#include "crazy/ascii_logo.h"
#include "crazy/atomic_lock.h"
#include "crazy/buffer.h"
#include "crazy/byte_order.h"
#include "crazy/command_line.h"
#include "crazy/common.h"
#include "crazy/cond_mutex.h"
#include "crazy/config.h"
#include "crazy/daemon.h"
#include "crazy/date_time.h"
#include "crazy/file_lock.h"
#include "crazy/http_application.h"
#include "crazy/json.h"
#include "crazy/key_value_pair.h"
#include "crazy/logger.h"
#include "crazy/message_base.h"
#include "crazy/mvcc_lock_wrapper.h"
#include "crazy/nocopyable.h"
#include "crazy/protocol.h"
#include "crazy/reflection.h"
#include "crazy/singleton.h"
#include "crazy/thread_pool.h"
#include "crazy/time_zone.h"
#include "crazy/uri.h"
#include "crazy/utils.h"
#include "crazy/uuid.h"
#include "crazy/version.h"

// crazy/clickhouse/
#include "crazy/clickhouse/clickhouse_connection.h"
#include "crazy/clickhouse/clickhouse_connection_pool.h"

// crazy/encryption/
#include "crazy/encryption/base64.h"
#include "crazy/encryption/md5.h"

// crazy/mmap/
#include "crazy/mmap/mmap.h"
#include "crazy/mmap/mmap_vector.h"

// crazy/mysql/
#include "crazy/mysql/mysql_connection.h"
#include "crazy/mysql/mysql_connection_pool.h"
#include "crazy/mysql/mysql_native_connection.h"

// crazy/net/
#include "crazy/net/acceptor.h"
#include "crazy/net/ca_bundle.h"
#include "crazy/net/client_actor.h"
#include "crazy/net/connection.h"
#include "crazy/net/decoder.h"
#include "crazy/net/encoder.h"
#include "crazy/net/local_socket.h"
#include "crazy/net/selector.h"
#include "crazy/net/service_actor.h"
#include "crazy/net/session.h"
#include "crazy/net/socket.h"
#include "crazy/net/socket_interface.h"
#include "crazy/net/telnet_service_actor.h"
#include "crazy/net/tls.h"
#ifdef _WIN32
#include "crazy/net/wepoll.h"
#endif

// crazy/net/http/
#include "crazy/net/http/http_client.h"
#include "crazy/net/http/http_header.h"
#include "crazy/net/http/http_message_base.h"
#include "crazy/net/http/http_multipart.h"
#include "crazy/net/http/http_parser.h"
#include "crazy/net/http/http_request.h"
#include "crazy/net/http/http_response.h"
#include "crazy/net/http/http_router.h"
#include "crazy/net/http/http_server.h"
#include "crazy/net/http/http_session.h"
#include "crazy/net/http/http_version.h"
#include "crazy/net/http/http11_common.h"
#include "crazy/net/http/multipart_parser.h"
#include "crazy/net/http/websocket_client.h"
#include "crazy/net/http/websocket_parser.h"

// crazy/net/smtp/
#include "crazy/net/smtp/smtp_client.h"
