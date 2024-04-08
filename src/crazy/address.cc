#include "address.h"
#include "socket.h"
#include <sstream>

namespace crazy {

template <typename type>
static type CreateMask(uint32_t bits) {
	if (bits > sizeof(type) * 8) {
		throw std::logic_error("bits > sizeof(type) * 8");
	}
	return (1 << (sizeof(type) * 8 -  bits)) - 1;
}
template<class type>
static uint32_t CountBytes(type value) {
    	uint32_t result = 0;
    	for(; value; ++result) {
        	value &= value - 1;
    	}
   	return result;
}
Address::Ptr Address::Create(const sockaddr* addr, socklen_t addrlen) {
	if (!addr) {
		return nullptr;
	}

	Address::Ptr result;
	switch (addr->sa_family) {
		case AF_INET:
			result.reset(new IPv4Address(*(const sockaddr_in*)addr));		
			break;
		case AF_INET6:
			result.reset(new IPv6Address(*(const sockaddr_in6*)addr));		
			break;
		default: 
			throw std::logic_error("unknow address");
	}
	return result;
}
bool Address::Lookup(std::vector<Address::Ptr>& result, const std::string& host,
            		int32_t family, int32_t type, int32_t protocol) {
	addrinfo hints, *results, *next;
	hints.ai_flags = 0;
    	hints.ai_family = family;
    	hints.ai_socktype = type;
    	hints.ai_protocol = protocol;
    	hints.ai_addrlen = 0;
    	hints.ai_canonname = nullptr;
    	hints.ai_addr = nullptr;
    	hints.ai_next = nullptr;
	
	std::string node;
	const char* service = nullptr;
	
	if (!host.empty() && host[0] == '[') {
		const char* endipv6 = (const char*)memchr(host.c_str() + 1, ']', host.size() - 1);
		if (endipv6) {
			if (*(endipv6 + 1) == ':') {
				service = endipv6 + 2;
			}
			node = host.substr(1, endipv6 - host.c_str() - 1);
		}
	}

	if (node.empty()) {
		service = (const char*)memchr(host.c_str(), ':', host.size());
        	if(service) {
            		if(!memchr(service + 1, ':', host.c_str() + host.size() - service - 1)) {
                		node = host.substr(0, service - host.c_str());
               	 		++service;
			}
            	}
        }
	
	if (node.empty()) {
        	node = host;
    	}
    	auto error = getaddrinfo(node.c_str(), service, &hints, &results);
    	if (error) {
		CRAZY_ERROR(CRAZY_ROOT_LOGGER()) << "Address::Lookup getaddress(" << host << ", "
            		<< family << ", " << type << ") err=" << error << " errstr="
            		<< gai_strerror(error);
    		return false;
	}

	next = results;
    	while (next) {
        	result.push_back(Create(next->ai_addr, (socklen_t)next->ai_addrlen));
        	next = next->ai_next;
    	}

    	freeaddrinfo(results);
    	return !result.empty();
}
Address::Ptr Address::LookupAny(const std::string& host,
            		int32_t family, int32_t type, int32_t protocol) {
	std::vector<Address::Ptr> result;
    	if (Lookup(result, host, family, type, protocol)) {
        	return result[0];
    	}
    	return nullptr;
}
std::shared_ptr<IPAddress> Address::LookupAnyIPAddress(const std::string& host,
            		int32_t family, int32_t type, int32_t protocol) {
	std::vector<Address::Ptr> result;
    	if (Lookup(result, host, family, type, protocol)) {
        	for (auto& i : result) {
            		IPAddress::Ptr v = std::dynamic_pointer_cast<IPAddress>(i);
            		if (v) {
                		return v;
            		}
        	}
    	}
    	return nullptr;
}
bool Address::GetInterfaceAddresses(std::map<std::string
                    	,std::pair<Address::Ptr, uint32_t> >& result, int32_t family) {
	ifaddrs *next, *results;
    	if (getifaddrs(&results) != 0) {
        	CRAZY_ERROR(CRAZY_ROOT_LOGGER()) << "Address::GetInterfaceAddresses getifaddrs "
            		" err=" << errno << " errstr=" << strerror(errno);
        	return false;
    	}

    	try {
        	for (next = results; next; next = next->ifa_next) {
            		Address::Ptr addr;
            		uint32_t prefix_len = ~0u;
            		if (family != AF_UNSPEC && family != next->ifa_addr->sa_family) {
                		continue;
            		}
            		switch (next->ifa_addr->sa_family) {
                	case AF_INET: {
                        	addr = Create(next->ifa_addr, sizeof(sockaddr_in));
                        	uint32_t netmask = ((sockaddr_in*)next->ifa_netmask)->sin_addr.s_addr;
                        	prefix_len = CountBytes(netmask);
                    	}
               		break;
                	case AF_INET6: {
                        	addr = Create(next->ifa_addr, sizeof(sockaddr_in6));
                        	in6_addr& netmask = ((sockaddr_in6*)next->ifa_netmask)->sin6_addr;
                        	prefix_len = 0;
                        	for (int i = 0; i < 16; ++i) {
                            		prefix_len += CountBytes(netmask.s6_addr[i]);
                        	}
                    	}
                    	break;
                		default:break;
            		}

            		if (addr) {
                		result.insert(std::make_pair(next->ifa_name,
                        		std::make_pair(addr, prefix_len)));
            		}
        	}
    	} catch (...) {
        	CRAZY_ERROR(CRAZY_ROOT_LOGGER()) << "Address::GetInterfaceAddresses exception";
        	freeifaddrs(results);
        	return false;
    	}	
    	freeifaddrs(results);
    	return !result.empty();
}
bool Address::GetInterfaceAddresses(std::vector<std::pair<Address::Ptr, uint32_t> >&result
                    	,const std::string& iface, int32_t family) {
	if (iface.empty() || iface == "*") {
        	if (family == AF_INET || family == AF_UNSPEC) {
            		result.push_back(std::make_pair(Address::Ptr(new IPv4Address()), 0));
        	}
        	if (family == AF_INET6 || family == AF_UNSPEC) {
            		result.push_back(std::make_pair(Address::Ptr(new IPv6Address()), 0));
        	}
        	return true;
    	}

    	std::map<std::string, std::pair<Address::Ptr, uint32_t> > results;

    	if (!GetInterfaceAddresses(results, family)) {
        	return false;
    	}

    	auto its = results.equal_range(iface);
    	for (; its.first != its.second; ++its.first) {
        	result.push_back(its.first->second);
    	}
    	return !result.empty();
}
int32_t Address::GetFamily() const {
	return GetAddr()->sa_family;
}
IPv4Address::IPv4Address() {
}
IPv4Address::IPv4Address(const sockaddr_in& address) {
	m_addr = m_addr;
}
IPv4Address::IPv4Address(const std::string& address, uint16_t port) {
	memset(&m_addr, 0, sizeof(m_addr));
	m_addr.sin_family = AF_INET;
	m_addr.sin_port = ByteSwapOnLittleEndian(port);
	inet_pton(AF_INET, address.c_str(), &m_addr.sin_addr);
}
sockaddr* IPv4Address::GetAddr() const {
	return (sockaddr*)&m_addr;
}
socklen_t IPv4Address::GetAddrLen() const {
	return sizeof(m_addr);
}	
IPAddress::Ptr IPv4Address::BroadcastAddress(const uint32_t prefix_len) {
	if (prefix_len > 32) {
		return nullptr;
	}
	sockaddr_in baddr(m_addr);
	baddr.sin_addr.s_addr |= ByteSwapOnLittleEndian(CreateMask<uint32_t>(prefix_len));
	return IPv4Address::Ptr(new IPv4Address(baddr));
}
IPAddress::Ptr IPv4Address::NetworkAddress(const uint32_t prefix_len) {
	if (prefix_len > 32) {
		return nullptr;
	}
	sockaddr_in baddr(m_addr);
	baddr.sin_addr.s_addr &= ByteSwapOnLittleEndian(CreateMask<uint32_t>(prefix_len));
	return IPv4Address::Ptr(new IPv4Address(baddr));
}
IPAddress::Ptr IPv4Address::SubnetMask(const uint32_t prefix_len) {
	sockaddr_in subnet;
	memset(&subnet, 0, sizeof(subnet));
	subnet.sin_family = AF_INET;
	subnet.sin_addr.s_addr = ~ByteSwapOnLittleEndian(CreateMask<uint32_t>(prefix_len));
	return IPv4Address::Ptr(new IPv4Address(subnet));
}
uint32_t IPv4Address::GetPort() const {
	return ByteSwapOnBigEndian(m_addr.sin_port);
}
void IPv4Address::SetPort(const uint32_t port) {
	m_addr.sin_port = ByteSwapOnLittleEndian(port);
}
std::string IPv4Address::ToString() {
	std::stringstream ss;
	uint32_t addr = ByteSwapOnLittleEndian(m_addr.sin_addr.s_addr);
    	ss << ((addr >> 24) & 0xff) << "."
       		<< ((addr >> 16) & 0xff) << "."
       		<< ((addr >> 8) & 0xff) << "."
       		<< (addr & 0xff);
    	ss << ":" << ByteSwapOnLittleEndian(m_addr.sin_port);
	return ss.str();
}
IPv6Address::IPv6Address() {
}
IPv6Address::IPv6Address(const sockaddr_in6& address) {
	m_addr = address;
}
IPv6Address::IPv6Address(const std::string& address, uint16_t port) {
	memset(&m_addr, 0, sizeof(m_addr));
	m_addr.sin6_family = AF_INET;
	m_addr.sin6_port = ByteSwapOnLittleEndian(port);
	inet_pton(AF_INET, address.c_str(), &m_addr);
}
sockaddr* IPv6Address::GetAddr() const {
	return (sockaddr*)&m_addr;
}
socklen_t IPv6Address::GetAddrLen() const {
	return sizeof(m_addr);
}
IPAddress::Ptr IPv6Address::BroadcastAddress(const uint32_t prefix_len) {
	sockaddr_in6 baddr(m_addr);
	baddr.sin6_addr.s6_addr[prefix_len / 8] |= CreateMask<uint8_t>(prefix_len % 8);
	for (size_t i = prefix_len / 8 + 1; i < 16; ++i) {
		baddr.sin6_addr.s6_addr[i] = 0xFF;	
	}
	return IPv6Address::Ptr(new IPv6Address(baddr));
}
IPAddress::Ptr IPv6Address::NetworkAddress(const uint32_t prefix_len) {
	sockaddr_in6 baddr(m_addr);
	baddr.sin6_addr.s6_addr[prefix_len / 8] &= CreateMask<uint8_t>(prefix_len % 8);
	for (size_t i = prefix_len / 8 + 1; i < 16; ++i) {
		baddr.sin6_addr.s6_addr[i] = 0x00;	
	}
	return IPv6Address::Ptr(new IPv6Address(baddr));
}
IPAddress::Ptr IPv6Address::SubnetMask(const uint32_t prefix_len) {
	sockaddr_in6 subnet;
	memset(&subnet, 0, sizeof(subnet));
	subnet.sin6_family = AF_INET6;
	subnet.sin6_addr.s6_addr[prefix_len / 8] = ~CreateMask<uint8_t>(prefix_len % 8);
	for (size_t i = 0; i < prefix_len / 8; ++i) {
		subnet.sin6_addr.s6_addr[i] = 0xFF;
	}
	return IPv6Address::Ptr(new IPv6Address(subnet));
}
uint32_t IPv6Address::GetPort() const {
	return ByteSwapOnBigEndian(m_addr.sin6_port);
}
void IPv6Address::SetPort(const uint32_t port) {
	m_addr.sin6_port = ByteSwapOnLittleEndian(port);
}
std::string IPv6Address::ToString() {
	std::stringstream ss;
	ss << "[";
    	uint16_t* addr = (uint16_t*)m_addr.sin6_addr.s6_addr;
    	bool used_zeros = false;
    	for(size_t i = 0; i < 8; ++i) {
        	if(addr[i] == 0 && !used_zeros) {
            		continue;
        	}
        	if(i && addr[i - 1] == 0 && !used_zeros) {
           		 ss << ":";
            		used_zeros = true;
        	}
        	if(i) {
            		ss << ":";
        	}
        	ss << std::hex << (int)ByteSwapOnLittleEndian(addr[i]) << std::dec;
    	}

    	if(!used_zeros && addr[7] == 0) {
        	ss << "::";
    	}

    	ss << "]:" << ByteSwapOnLittleEndian(m_addr.sin6_port);
    	return ss.str();
}
UnixAddress::UnixAddress() {
}
UnixAddress::UnixAddress(const std::string& path) {
	memset(&m_addr, 0, sizeof(m_addr));
	m_addr.sun_family = AF_UNIX;
	if (path.empty()) {
		throw std::logic_error("unix sockaddr path is empty");
	}
	strncpy(m_addr.sun_path, path.c_str()
			, path.size() > g_unixPathMaxLen ? g_unixPathMaxLen : path.size());
}
sockaddr* UnixAddress::GetAddr() const {
	return (sockaddr*)&m_addr;
}
socklen_t UnixAddress::GetAddrLen() const {
	return sizeof(m_addr);
}
std::string UnixAddress::ToString() {
	std::stringstream ss;
	ss << m_addr.sun_path;
    	return ss.str();
}

}
