/* #include <cstring> */
#include <iostream>

#include <errno.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <linux/if_packet.h>
#include <net/ethernet.h>

#include <linux/if_tun.h>
#include <linux/filter.h>
#include <linux/if_ether.h>

#include "net.h"
#include "logger.h"
#include "control.h"

#define FRER_PACKET_BUF_LEN	9216
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

extern Control control;

Net::Net()
{

}

Net::~Net()
{
}

/*
 * tcpdump -d \
 * '   (ether[12:2] == 0x8100 and ether[12 + 4 :2] == 0xF1C1) '\
 * 'or (ether[12:2] == 0xF1C1) '
 *
 * (000) ldh      [12]
 * (001) jeq      #0x8100          jt 2    jf 4
 * (002) ldh      [16]
 * (003) jeq      #0xf1c1          jt 5    jf 6
 * (004) jeq      #0xf1c1          jt 5    jf 6
 * (005) ret      #262144
 * (006) ret      #0
 */
static struct sock_filter raw_filter_vlan_norm[] = {
	{ 0x28, 0, 0, 0x0000000c },
	{ 0x15, 0, 2, 0x00008100 },
	{ 0x28, 0, 0, 0x00000010 },
	{ 0x15, 1, 2, 0x0000f1c1 },
	{ 0x15, 0, 1, 0x0000f1c1 },
	{ 0x6, 0, 0, 0x00040000 },
	{ 0x6, 0, 0, 0x00000000 },
};

static int raw_configure(int fd, int index)
{
	struct sock_fprog prg;

	prg.len = ARRAY_SIZE(raw_filter_vlan_norm);
	prg.filter = raw_filter_vlan_norm;

	if (setsockopt(fd, SOL_SOCKET, SO_ATTACH_FILTER, &prg, sizeof(prg))) {
		PLOGE << "setsockopt SO_ATTACH_FILTER failed: " << strerror(errno);
		return -1;
	}
	return 0;
}

static int sk_interface_index(int fd, const char *ifname)
{
	struct ifreq ifreq;
	int err;

	memset(&ifreq, 0, sizeof(ifreq));
	strncpy(ifreq.ifr_name, ifname, sizeof(ifreq.ifr_name) - 1);
	err = ioctl(fd, SIOCGIFINDEX, &ifreq);
	if (err < 0) {
		PLOGE << "ioctl SIOCGIFINDEX failed: " << strerror(errno);
		return err;
	}
	return ifreq.ifr_ifindex;
}

int Net::OpenSocket(const char *ifname)
{
	struct sockaddr_ll addr;
	int fd, index;

	fd = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
	if (fd < 0) {
		PLOGE << "socket failed: " << strerror(errno);
		goto no_socket;
	}

	index = sk_interface_index(fd, ifname);
	if (index < 0)
		goto no_option;

	memset(&addr, 0, sizeof(addr));
	addr.sll_ifindex = index;
	addr.sll_family = AF_PACKET;
	addr.sll_protocol = htons(ETH_P_ALL);
	if (bind(fd, (struct sockaddr *) &addr, sizeof(addr))) {
		PLOGE << "socket failed: " << strerror(errno);
		goto no_option;
	}

	if (setsockopt(fd, SOL_SOCKET, SO_BINDTODEVICE, ifname, strlen(ifname))) {
		PLOGE << "setsockopt SO_BINDTODEVICE failed: " << strerror(errno);
		goto no_option;
	}

	if (raw_configure(fd, index))
		goto no_option;

	return fd;

no_option:
	close(fd);
no_socket:
	return -1;
}

int Net::NetInit(std::string dev, std::string macaddr)
{
	rawsock_ = OpenSocket(dev.c_str());
	if (rawsock_ == -1)
		return false;

	PLOGD << "create raw socket " << rawsock_;

	return true;
}

void Net::TxFromFrerToLocal(char *buf, int len)
{
    /* struct FrerHeader *frer_hdr = (struct FrerHeader *)buf; */

    size_t hl = sizeof(struct FrerHeader);

    PLOGD << "write tapsock " << len << " bytes";

	if (write(control.tapsock_, buf + hl, len - hl) < 0) {
        PLOGW << "Write etherflame to local network failed";
	}
}

void Net::ProcessFrer(void)
{
    int len;
    char buf[FRER_PACKET_BUF_LEN];
    struct sockaddr_storage sa_str;
    socklen_t s_t = sizeof(sa_str);

    memset(buf, 0, sizeof(buf));

    while (1) {
        memset(&sa_str, 0, sizeof(sa_str));
        if ((len = recvfrom(rawsock_, buf, sizeof(buf), 0, (struct sockaddr *)&sa_str, &s_t)) < 0) 
            continue;

        PLOGD << "recvfrom rawsock " << len << " bytes";
        TxFromFrerToLocal(buf, len);
    }

    return;
}

