#include "ws_session.h"

namespace crazy::http {

static ConfigValue<uint32_t>::Ptr g_websocket_message_max_size
    = Config::Lookup("websocket.message.max_size"
            ,(uint32_t) 1024 * 1024 * 32, "websocket message max size");

WSSession::WSSession(Socket::Ptr sock) 
    : HttpSession (sock) {
}
Request::Ptr WSSession::HandleShake() {
    auto req = RecvRequest();
    do {
        if (!req) {
            CRAZY_ERROR(CRAZY_ROOT_LOGGER()) << "invalid http request";
            break;
        }
        if (strcasecmp(req->GetHeader("Upgrade").c_str(), "websocket")) {
            CRAZY_ERROR(CRAZY_ROOT_LOGGER()) << "http header upgrade != websocket";
            break;
        }
        if (strcasecmp(req->GetHeader("Connection").c_str(), "Upgrade")) {
            CRAZY_ERROR(CRAZY_ROOT_LOGGER()) << "http header connection != upgrade";
            break;
        }
        if (req->GetHeaderAs<int>("Sec-webSocket-Version") != 13) {
            CRAZY_ERROR(CRAZY_ROOT_LOGGER()) << "http header Sec-webSocket-Version != 13";
            break;
        }
        auto key = req->GetHeader("Sec-WebSocket-Key");
        if (key.empty()) {
            CRAZY_ERROR(CRAZY_ROOT_LOGGER()) << "http header ec-WebSocket-Key = nil";
            break;
        }
        std::string v = key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
        v = base64encode(sha1sum(v));
        req->SetWebsocket(true);

        auto res = req->GetResponse();
        res->SetStatus(HttpStatus::SWITCHING_PROTOCOLS);
        res->SetWebsocket(true);
        res->SetReason("Web Socket Protocol Handshake");
        res->SetHeader("Upgrade", "websocket");
        res->SetHeader("Connection", "Upgrade");
        res->SetHeader("Sec-WebSocket-Accept", v);
        SendResponse(res);
        return req;
    } while (false);
    return nullptr;
}
WSFrameMessage::Ptr WSSession::RecvMessage() {
    return WSSessionUtil::WSRecvMessage(this, false);
}
int32_t WSSession::SendMessage(WSFrameMessage::Ptr msg, bool fin) {
    return WSSessionUtil::WSSendMessage(this, msg, false, fin);
}
int32_t WSSession::SendMessage(const std::string& msg, WSOpcode opcode, bool fin) {
    return WSSessionUtil::WSSendMessage(this, WSFrameMessage::Ptr(new WSFrameMessage(opcode, msg)), false, fin);
}
int32_t WSSession::Ping() {
    return WSSessionUtil::WSPing(this);
}
int32_t WSSession::Pong() {
    return WSSessionUtil::WSPong(this);
}
WSFrameMessage::Ptr WSSessionUtil::WSRecvMessage(Stream* stream, bool client) {
    WSOpcode opcode = WSOpcode::CONTINUE;
    std::string data;
    int cur_len = 0;
    do {
        WSFrameHead ws_head;
        if(stream->ReadFixSize(&ws_head, sizeof(ws_head)) <= 0) {
            break;
        }
        if(ws_head.opcode == WSOpcode::PING) {
            if(WSPong(stream) <= 0) {
                break;
            }
        } else if(ws_head.opcode == WSOpcode::PONG) {
        } else if(ws_head.opcode == WSOpcode::CONTINUE
                || ws_head.opcode == WSOpcode::TEXT_FRAME
                || ws_head.opcode == WSOpcode::BIN_FRAME) {
            if(!client && !ws_head.mask) {
                break;
            }
            uint64_t length = 0;
            if(ws_head.payload == 126) {
                uint16_t len = 0;
                if(stream->ReadFixSize(&len, sizeof(len)) <= 0) {
                    break;
                }
                length = ByteSwapOnLittleEndian(len);
            } else if(ws_head.payload == 127) {
                uint64_t len = 0;
                if(stream->ReadFixSize(&len, sizeof(len)) <= 0) {
                    break;
                }
                length = ByteSwapOnLittleEndian(len);
            } else {
                length = ws_head.payload;
            }

            if((cur_len + length) >= g_websocket_message_max_size->GetValue()) {
                break;
            }

            char mask[4] = {0};
            if(ws_head.mask) {
                if(stream->ReadFixSize(mask, sizeof(mask)) <= 0) {
                    break;
                }
            }
            data.resize(cur_len + length);
            if(stream->ReadFixSize(&data[cur_len], length) <= 0) {
                break;
            }
            if(ws_head.mask) {
                for(int i = 0; i < (int)length; ++i) {
                    data[cur_len + i] ^= mask[i % 4];
                }
            }
            cur_len += length;

            if(!opcode && ws_head.opcode != WSOpcode::CONTINUE) {
                opcode = ws_head.opcode;
            }

            if(ws_head.fin) {
                return WSFrameMessage::Ptr(new WSFrameMessage(opcode, std::move(data)));
            }
        } 
    } while(true);
    return nullptr;
}
int32_t WSSessionUtil::WSSendMessage(Stream* stream, WSFrameMessage::Ptr msg, bool client, bool fin) {
    do {
        WSFrameHead ws_head;
        memset(&ws_head, 0, sizeof(ws_head));
        ws_head.fin = fin;
        ws_head.opcode = msg->GetOpcode();
        ws_head.mask = client;
        uint64_t size = msg->GetData().size();
        if(size < 126) {
            ws_head.payload = size;
        } else if(size < 65536) {
            ws_head.payload = 126;
        } else {
            ws_head.payload = 127;
        }
        
        if(stream->WriteFixSize(&ws_head, sizeof(ws_head)) <= 0) {
            break;
        }
        if(ws_head.payload == 126) {
            uint16_t len = size;
            len = ByteSwapOnLittleEndian(len);
            if(stream->WriteFixSize(&len, sizeof(len)) <= 0) {
                break;
            }
        } else if(ws_head.payload == 127) {
            uint64_t len = ByteSwapOnLittleEndian(size);
            if(stream->WriteFixSize(&len, sizeof(len)) <= 0) {
                break;
            }
        }
        if(client) {
            char mask[4];
            uint32_t rand_value = rand();
            memcpy(mask, &rand_value, sizeof(mask));
            std::string data = msg->GetData();
            for(size_t i = 0; i < data.size(); ++i) {
                data[i] ^= mask[i % 4];
            }

            if(stream->WriteFixSize(mask, sizeof(mask)) <= 0) {
                break;
            }
        }
        if(stream->WriteFixSize(msg->GetData().c_str(), size) <= 0) {
            break;
        }
        return size + sizeof(ws_head);
    } while(0);
    stream->Close();
    return -1;
}
int32_t WSSessionUtil::WSPing(Stream* stream) {
    WSFrameHead ws_head;
    memset(&ws_head, 0, sizeof(ws_head));
    ws_head.fin = 1;
    ws_head.opcode = WSOpcode::PING;
    int32_t v = stream->WriteFixSize(&ws_head, sizeof(ws_head));
    if(v <= 0) {
        stream->Close();
    }
    return v;
}
int32_t WSSessionUtil::WSPong(Stream* stream) {
    WSFrameHead ws_head;
    memset(&ws_head, 0, sizeof(ws_head));
    ws_head.fin = 1;
    ws_head.opcode = WSOpcode::PONG;
    int32_t v = stream->WriteFixSize(&ws_head, sizeof(ws_head));
    if(v <= 0) {
        stream->Close();
    }
    return v;
}

}