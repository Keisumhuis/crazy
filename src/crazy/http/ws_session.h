/**
 * @file ws_session.h
 * @version 0.0.1
 * @date 2023-12-07
 * @author Keisum (keisumhuis@gmail.com)
 * @copyright Copyright (c) 2023  Keisum_huis
 * @brief
 * 0               1               2               3
 * 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-------+-+-------------+-------------------------------+
 * |F|R|R|R| opcode|M| Payload len |    Extended payload length    |
 * |I|S|S|S|  (4)  |A|     (7)     |             (16/64)           |
 * |N|V|V|V|       |S|             |   (if payload len==126/127)   |
 * | |1|2|3|       |K|             |                               |
 * +-+-+-+-+-------+-+-------------+ - - - - - - - - - - - - - - - +
 * |     Extended payload length continued, if payload len == 127  |
 * + - - - - - - - - - - - - - - - +-------------------------------+
 * |                               |Masking-key, if MASK set to 1  |
 * +-------------------------------+-------------------------------+
 * | Masking-key (continued)       |          Payload Data         |
 * +-------------------------------- - - - - - - - - - - - - - - - +
 * :                     Payload Data continued ...                :
 * + - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - +
 * |                     Payload Data continued ...                |
 * +---------------------------------------------------------------+
 * opcode:
 * *  %x0 denotes a continuation frame
 * *  %x1 denotes a text frame
 * *  %x2 denotes a binary frame
 * *  %x3-7 are reserved for further non-control frames
 * *  %x8 denotes a connection close
 * *  %x9 denotes a ping
 * *  %xA denotes a pong
 * *  %xB-F are reserved for further control frames
 * Payload length:  7 bits, 7+16 bits, or 7+64 bits
 * Masking-key:  0 or 4 bytes
 */
#ifndef ____CRAZY_HTTP_WS_SESSION_H____
#define ____CRAZY_HTTP_WS_SESSION_H____

#include <memory>

#include "crazy/streams/socket_stream.h"
#include "crazy/util/hash_util.h"
#include "http_session.h"

namespace crazy::http {
#pragma pack(1)
    enum WSOpcode : int32_t {
            CONTINUE = 0,
            TEXT_FRAME = 1,
            BIN_FRAME = 2,
            CLOSE = 8,
            PING = 0x9,
            PONG = 0xA
    };

    struct WSFrameHead {
        WSOpcode opcode: 4;
        bool rsv3: 1;
        bool rsv2: 1;
        bool rsv1: 1;
        bool fin: 1;
        uint32_t payload: 7;
        bool mask: 1;
    };
#pragma pack()

    class WSFrameMessage {
    public:
        using Ptr = std::shared_ptr<WSFrameMessage>;
        WSFrameMessage(WSOpcode opcode = WSOpcode::CONTINUE, const std::string& data = "")
            : m_opcode (opcode) , m_data (data) {}
        WSOpcode GetOpcode() const { return m_opcode; }
        void SetOpcode(WSOpcode opcode) { m_opcode = opcode; } 
        const std::string& GetData() const { return m_data; }
        void SetData(const std::string& data) { m_data = data; }
    private:
        WSOpcode m_opcode;
        std::string m_data;
    };

    class WSSession : public HttpSession {
    public:
        using Ptr = std::shared_ptr<WSSession>;
        WSSession(Socket::Ptr sock);
        Request::Ptr HandleShake();
        WSFrameMessage::Ptr RecvMessage();
        int32_t SendMessage(WSFrameMessage::Ptr msg, bool fin = true);
        int32_t SendMessage(const std::string& msg, WSOpcode opcode = WSOpcode::TEXT_FRAME, bool fin = true);
        int32_t Ping();
        int32_t Pong();
    };

    class WSSessionUtil {
    public:
        static WSFrameMessage::Ptr WSRecvMessage(Stream* stream, bool client);
        static int32_t WSSendMessage(Stream* stream, WSFrameMessage::Ptr msg, bool client, bool fin);
        static int32_t WSPing(Stream* stream);
        static int32_t WSPong(Stream* stream);
    };
}

#endif // ! ____CRAZY_HTTP_WS_SESSION_H____