#pragma once
#include <ifaddrs.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netdb.h>
#include <net/if.h> 
#include <netinet/in.h>
#include <unistd.h>
#include <linux/if_link.h>
#include <linux/rtnetlink.h>
#include <string>
#include <cinttypes>
#include <unordered_map>
#include <cstring>
#include <memory>
#include <list>

class cnetifaces {
public:
	class iface {
	public:
		std::string name;
#pragma pack(push,1)
		struct {
			uint32_t up : 1;
			uint32_t broadcast : 1;
			uint32_t debug : 1;
			uint32_t loopback : 1;
			uint32_t pointopoint : 1;
			uint32_t notrailers : 1;
			uint32_t running : 1;
			uint32_t noarp : 1;
			uint32_t promisc : 1;
			uint32_t allmulti : 1;
			uint32_t master : 1;
			uint32_t slave : 1;
			uint32_t multicast : 1;
			uint32_t portsel : 1;
			uint32_t automedia : 1;
			uint32_t dynamic : 1;
			uint32_t : 16;
		} flags;
#pragma pack(pop)
		size_t		mtu;
		size_t		index;
		struct {
			uint32_t	address;
			uint32_t	netmask;
			inline bool empty() const { return address == 0; }
		} ip4;
		struct {
			uint8_t		address[16];
			uint8_t		netmask[16];
			inline bool empty() const { auto&& a64 = reinterpret_cast<const uint64_t*>(address); return (a64[0] + a64[1]) == 0; }
		} ip6;
		struct {
			uint8_t		address[6];
			inline bool empty() const { auto&& a16 = reinterpret_cast<const uint16_t*>(address); return (a16[0] + a16[1] + a16[2]) == 0; }
		} mac;
		iface(const char* iface_name) : name(iface_name), flags({ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 }), mtu(0), index(0),
			ip4({ 0,0 }), ip6({ {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0} ,{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0} }),
			mac({ 0,0,0,0,0,0 }) {
			;
		}
	};
	struct stat {
		std::string name;
		uint32_t	index;
		struct {
			uint64_t	rx_packets;		/* total packets received	*/
			uint64_t	tx_packets;		/* total packets transmitted	*/
			uint64_t	rx_bytes;		/* total bytes received 	*/
			uint64_t	tx_bytes;		/* total bytes transmitted	*/
			uint64_t	rx_errors;		/* bad packets received		*/
			uint64_t	tx_errors;		/* packet transmit problems	*/
			uint64_t	rx_dropped;		/* no space in linux buffers	*/
			uint64_t	tx_dropped;		/* no space available in linux	*/
			uint64_t	multicast;		/* multicast packets received	*/
			uint64_t	collisions;
		} counter;
	};
public:
	class list {
	private:
		std::unordered_map<std::string, cnetifaces::iface> ifaces_list;
		static std::unordered_map<std::string, cnetifaces::iface> update() noexcept {
			std::unordered_map<std::string, cnetifaces::iface> list;
			struct ifaddrs* ifaddr;
			if (getifaddrs(&ifaddr) != -1) {
				for (struct ifaddrs* ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {

					if (ifa->ifa_name) {
						auto&& it = list.emplace(ifa->ifa_name, cnetifaces::iface(ifa->ifa_name));

						if (ifa->ifa_addr) {
							if (ifa->ifa_addr->sa_family == AF_INET) {
								it.first->second.ip4.address = ntohl(reinterpret_cast<struct sockaddr_in*>(ifa->ifa_addr)->sin_addr.s_addr);
								if (ifa->ifa_netmask) {
									it.first->second.ip4.netmask = ntohl(reinterpret_cast<struct sockaddr_in*>(ifa->ifa_netmask)->sin_addr.s_addr);
								}
							}
							else if (ifa->ifa_addr->sa_family == AF_INET6) {
								memcpy(it.first->second.ip6.address, reinterpret_cast<struct sockaddr_in6*>(ifa->ifa_addr)->sin6_addr.__in6_u.__u6_addr8, sizeof(it.first->second.ip6.address));
								if (ifa->ifa_netmask) {
									memcpy(it.first->second.ip6.netmask, reinterpret_cast<struct sockaddr_in6*>(ifa->ifa_netmask)->sin6_addr.__in6_u.__u6_addr8, sizeof(it.first->second.ip6.netmask));
								}
							}
							/*
							else if (ifa->ifa_addr->sa_family == AF_PACKET && ifa->ifa_data != nullptr) {
								struct rtnl_link_stats *stats = (struct rtnl_link_stats *)ifa->ifa_data;
							}
							*/
							*(uint32_t*)(&it.first->second.flags) = (uint32_t)ifa->ifa_flags;
						}

						{
							auto sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
							if (sock > 0) {
								struct ifreq ifr;
								strcpy(ifr.ifr_name, ifa->ifa_name);
								if (it.first->second.mac.empty() && ioctl(sock, SIOCGIFHWADDR, &ifr) == 0) {
									memcpy(it.first->second.mac.address, ifr.ifr_addr.sa_data, sizeof(it.first->second.mac.address));
								}
								if (!it.first->second.mtu && ioctl(sock, SIOCGIFMTU, &ifr) == 0) {
									it.first->second.mtu = ifr.ifr_mtu;
								}
								if (ioctl(sock, SIOCGIFINDEX, &ifr) == 0) {
									it.first->second.index = ifr.ifr_ifindex;
								}
								::close(sock);
							}
						}
					}

				}
				freeifaddrs(ifaddr);
			}
			return list;
		}
	public:
		using iterator = std::unordered_map<std::string, iface>::const_iterator;
		list() : ifaces_list(std::move(update())) { ; }
		inline iterator begin() const { return ifaces_list.cbegin(); }
		inline iterator end() const { return ifaces_list.cend(); }
		inline iterator at(const std::string& ifname) const noexcept { return ifaces_list.find(ifname); }
	};

	class stats {
	private:
		std::unordered_map<std::string, cnetifaces::stat>	stats_list;
		using iterator = std::unordered_map < std::string, cnetifaces::stat>::const_iterator;
		static std::unordered_map<std::string, cnetifaces::stat> update() noexcept {
			int	stats_netlink = socket(AF_NETLINK, SOCK_DGRAM | SOCK_NONBLOCK, NETLINK_ROUTE);
			std::unordered_map<std::string, cnetifaces::stat> info_stats;
			if (stats_netlink > 0) {

				struct {
					struct nlmsghdr n;
					struct ifinfomsg r;
				} req;
				memset(&req, 0, sizeof(req));
				req.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifaddrmsg));
				req.n.nlmsg_type = RTM_GETLINK;
				req.n.nlmsg_flags = NLM_F_REQUEST | NLM_F_ROOT;

				auto status = send(stats_netlink, &req, req.n.nlmsg_len, 0);

				if (status > 0) {

					std::unordered_map<size_t, cnetifaces::stat> list_info;

					ssize_t len = status = 0, offset = 0;
					std::list<std::pair<std::unique_ptr<uint8_t[]>, ssize_t>> buffers;
					buffers.emplace_back(std::make_pair(std::unique_ptr<uint8_t[]>(new uint8_t[4096]), 4096));
					do {
						len += status;
						buffers.back().second = status = recv(stats_netlink, buffers.back().first.get(), buffers.back().second, 0);
						if (status > 0) {
							buffers.emplace_back(std::make_pair(std::unique_ptr<uint8_t[]>(new uint8_t[4096]), 4096));
							continue;
						}
					} while (status > 0);
					if (len) {
						std::unique_ptr<uint8_t[]> buffer(new uint8_t[len]);
						for (auto&& b_it = buffers.begin(); b_it != buffers.end() && b_it->second > 0; offset += b_it->second, b_it++) {
							memcpy(buffer.get() + offset, b_it->first.get(), b_it->second);
						}
						offset = len;
						for (struct nlmsghdr* it = (struct nlmsghdr*)buffer.get(); NLMSG_OK(it, offset) && offset > (ssize_t)sizeof(struct nlmsghdr);
							offset -= NLMSG_ALIGN(it->nlmsg_len), it = (struct nlmsghdr*)((char*)it + NLMSG_ALIGN(it->nlmsg_len))) {
							auto rtmp = (struct ifinfomsg*)NLMSG_DATA(it);
							auto rtatp = (struct rtattr*)IFLA_RTA(rtmp);
							ssize_t rtattrlen = IFLA_PAYLOAD(it);

							if (rtmp->ifi_index == 0) { break; }

							for (; RTA_OK(rtatp, rtattrlen); rtatp = RTA_NEXT(rtatp, rtattrlen)) {
								if (rtatp->rta_type == IFLA_IFNAME) {
									auto&& if_if = list_info.emplace(rtmp->ifi_index, cnetifaces::stat());
									if_if.first->second.name = (char*)RTA_DATA(rtatp);
									if_if.first->second.index = rtmp->ifi_index;
								}
								else if (rtatp->rta_type == IFLA_STATS64) {
									struct rtnl_link_stats64* link_stats = (struct rtnl_link_stats64*)RTA_DATA(rtatp);
									auto&& if_if = list_info.emplace(rtmp->ifi_index, cnetifaces::stat());
									if_if.first->second.index = rtmp->ifi_index;
									if_if.first->second.counter.rx_packets = link_stats->rx_packets;
									if_if.first->second.counter.tx_packets = link_stats->tx_packets;
									if_if.first->second.counter.rx_bytes = link_stats->rx_bytes;
									if_if.first->second.counter.tx_bytes = link_stats->tx_bytes;
									if_if.first->second.counter.rx_errors = link_stats->rx_errors;
									if_if.first->second.counter.tx_errors = link_stats->tx_errors;
									if_if.first->second.counter.rx_dropped = link_stats->rx_dropped;
									if_if.first->second.counter.tx_dropped = link_stats->tx_dropped;
									if_if.first->second.counter.multicast = link_stats->multicast;
									if_if.first->second.counter.collisions = link_stats->collisions;
								}
							}
						}
						for (auto&& it : list_info) {
							info_stats.emplace(it.second.name, it.second);
						}
					}
					return info_stats;
				}
				::close(stats_netlink);
			}
			return info_stats;
		}
	public:
		stats() noexcept : stats_list(std::move(update())) { ; }
		inline iterator begin() const { return stats_list.cbegin(); }
		inline iterator end() const { return stats_list.cend(); }
		inline iterator at(const std::string& ifname) const noexcept { return stats_list.find(ifname); }
	};
};