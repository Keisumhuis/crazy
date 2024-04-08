/**
 * @file tcp_server.h
 * @version 0.0.1
 * @date 2023-12-07
 * @author Keisum (keisumhuis@gmail.com)
 * @copyright Copyright (c) 2023  Keisum_huis
 * @brief
 */
#ifndef ____CRAZY_TCP_SERVER_H____
#define ____CRAZY_TCP_SERVER_H____

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "address.h"
#include "logger.h"
#include "scheduler.h"
#include "selector.h"
#include "socket.h"

namespace crazy {

	class TcpServer : public std::enable_shared_from_this<TcpServer> {
	public:
		using Ptr = std::shared_ptr<TcpServer>;
		TcpServer();
		virtual ~TcpServer();

		virtual bool Bind(Address::Ptr addr, bool ssl = false);
		virtual void Bind(const std::vector<Address::Ptr>& addrs
				, std::vector<Address::Ptr>& fails
				, bool ssl = false);
		bool LoadCertificates(const std::string& cert_file, const std::string& key_file);
		virtual bool Start();
		virtual bool Stop();
		bool IsStoppoing() const;
		std::set<Socket::Ptr> GetSockets() const;
		std::string GetName() const;
		void SetName(const std::string& name);
		virtual void OnClient(Socket::Ptr sock);
		virtual void OnAccept(Socket::Ptr sock);
	private:
		std::set<Socket::Ptr> m_socks;
		bool m_ssl = false;
		bool m_stopping = true;
		std::string m_name;
	};

}

#endif // ! ____CRAZY_TCP_SERVER_H____
