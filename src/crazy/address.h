/**
 * @file address.h
 * @version 0.0.1
 * @date 2023-12-07
 * @author Keisum (keisumhuis@gmail.com)
 * @copyright Copyright (c) 2023  Keisum_huis
 * @brief
 */
#ifndef ____CRAZY_ADDRESS_H____
#define ____CRAZY_ADDRESS_H____

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <string.h>

#include <map>
#include <memory>
#include <stdexcept>
#include <string>

#include "endian.h"

namespace crazy {

	static constexpr size_t g_unixPathMaxLen = sizeof(((sockaddr_un*)0)->sun_path) - 1;
	
	class IPAddress;
	class Address {
	public:
		using Ptr = std::shared_ptr<Address>;
		virtual ~Address() {}
		static Address::Ptr Create(const sockaddr* addr, socklen_t addrlen);
		static bool Lookup(std::vector<Address::Ptr>& result, const std::string& host,
            		int32_t family = AF_INET, int32_t type = SOCK_STREAM, int32_t protocol = 0);
		static Address::Ptr LookupAny(const std::string& host,
            		int32_t family = AF_INET, int32_t type = SOCK_STREAM, int32_t protocol = 0);
		static std::shared_ptr<IPAddress> LookupAnyIPAddress(const std::string& host,
            		int32_t family = AF_INET, int32_t type = SOCK_STREAM, int32_t protocol = 0);
		static bool GetInterfaceAddresses(std::map<std::string
                    	,std::pair<Address::Ptr, uint32_t> >& result, int32_t family = AF_INET);
		static bool GetInterfaceAddresses(std::vector<std::pair<Address::Ptr, uint32_t> >&result
                    	,const std::string& iface, int32_t family = AF_INET);
		int32_t GetFamily() const;
		virtual sockaddr* GetAddr() const = 0;
		virtual socklen_t GetAddrLen() const = 0;
		virtual std::string ToString() = 0;
	};

	class IPAddress : public Address {
	public:
		using Ptr = std::shared_ptr<IPAddress>;

		virtual IPAddress::Ptr BroadcastAddress(const uint32_t prefix_len) = 0;
		virtual IPAddress::Ptr NetworkAddress(const uint32_t prefix_len) = 0;
		virtual IPAddress::Ptr SubnetMask(const uint32_t prefix_len) = 0;

		virtual uint32_t GetPort() const = 0;
		virtual void SetPort(const uint32_t port) = 0;
	};

	class IPv4Address final : public IPAddress {
	public:
		using Ptr = std::shared_ptr<IPv4Address>;
		IPv4Address();
		IPv4Address(const sockaddr_in& address);
		IPv4Address(const std::string& address, uint16_t port = 0);
		
		sockaddr* GetAddr() const override;
		socklen_t GetAddrLen() const override;
		
		IPAddress::Ptr BroadcastAddress(const uint32_t prefix_len) override;
		IPAddress::Ptr NetworkAddress(const uint32_t prefix_len) override;
		IPAddress::Ptr SubnetMask(const uint32_t prefix_len) override;

		uint32_t GetPort() const override;
		void SetPort(const uint32_t port) override;
		std::string ToString() override;
	private:
		sockaddr_in m_addr;
	};

	class IPv6Address final : public IPAddress {
	public:
		using Ptr = std::shared_ptr<IPv6Address>;
		IPv6Address();
		IPv6Address(const sockaddr_in6& address);
		IPv6Address(const std::string& address, uint16_t port = 0);
		
		sockaddr* GetAddr() const override;
		socklen_t GetAddrLen() const override;
		
		IPAddress::Ptr BroadcastAddress(const uint32_t prefix_len) override;
		IPAddress::Ptr NetworkAddress(const uint32_t prefix_len) override;
		IPAddress::Ptr SubnetMask(const uint32_t prefix_len) override;

		uint32_t GetPort() const override;
		void SetPort(const uint32_t port) override;
		std::string ToString() override;
	private:
		sockaddr_in6 m_addr;
	};

	class UnixAddress final : public Address {
	public:
		using Ptr = std::shared_ptr<UnixAddress>;
		UnixAddress();
		UnixAddress(const std::string& path);
		sockaddr* GetAddr() const override;
		socklen_t GetAddrLen() const override;
		std::string ToString() override;
	private:
		sockaddr_un m_addr;
	};
}

#endif // ! ____CRAZY_ADDRESS_H____

