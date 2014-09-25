/**	
 *	Copyright (C) 2005 Christian Hentschel.
 *
 *	This file is part of Open_cli.
 *
 *	Open_cli is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	Open_cli is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with Open_cli; if not, write to the Free Software
 *	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *	Christian Hentschel
 *	chentschel@arnet.com.ar
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <asm/types.h>
#include <sys/socket.h>

#include "lists.h"
#include "tcp_socket.h"
#include "interface.h"

#include <linux/rtnetlink.h>

#define M_ZERO(ptr)	do { memset(ptr, 0,sizeof(typeof(*ptr))); } while(0)

#define DEBUGP	printf


/**
 *  Bind netlink socket to local pid, and connect to kernel
 *	address so we can use sk_send() and sk_recv()
 */
static int set_nlkernel(int sfd, unsigned int nl_groups)
{
	struct sockaddr_nl addr;
	
	M_ZERO(&addr);
	
	addr.nl_family = AF_NETLINK;
	addr.nl_groups = nl_groups;
	addr.nl_pid = getpid();
	
	if (bind(sfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		perror("bind()");
		return -1;
	}

	addr.nl_groups = 0;
	addr.nl_pid = 0;
	
	if (connect(sfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		perror("connect()");
		return -1;
	}
	return 0;
}

static int sk_netlink(unsigned int nl_groups)
{
	int nfd;
	
	nfd = sk_socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
	if (nfd < 0)
		return -1;
	
	if (set_nlkernel(nfd, nl_groups) < 0 || sk_set_nblock(nfd) < 0) {
		close(nfd);
		return -1;	
	}
	return nfd;
}

static int nl_generic_req(int sfd, int type)
{	
	struct {
		struct nlmsghdr nh;
    		struct rtgenmsg r;
	} req = { { 0 }, { 0 } };

	req.nh.nlmsg_len = NLMSG_LENGTH(sizeof(struct rtgenmsg));
	req.nh.nlmsg_flags = NLM_F_ROOT | NLM_F_MATCH | NLM_F_REQUEST;
	req.nh.nlmsg_type = type;
	req.nh.nlmsg_pid = 0;

	req.r.rtgen_family = AF_UNSPEC;
 
	return sk_send(sfd, &req, sizeof(req), 0);
}

static int sk_netlink_err(struct nlmsghdr *h)
{
	struct nlmsgerr *err = (struct nlmsgerr *)NLMSG_DATA(h);
	
	/* If err == 0, it's ACK */
	if (err->error == 0) {
		DEBUGP("ACK: type=%u, seq=%u, pid=%d\n", 
			err->msg.nlmsg_type, 
			err->msg.nlmsg_seq, 
			err->msg.nlmsg_pid);
		
		/* return 0 if !multipart */	
		return (!(h->nlmsg_flags & NLM_F_MULTI)) ? 0 : 1;
	}
	if (h->nlmsg_len < NLMSG_LENGTH(sizeof(struct nlmsgerr)))
		DEBUGP("Error: message truncated\n");
	else {
		DEBUGP("Error: %s, type=%u, seq=%u, pid=%d\n", 
			strerror(-err->error), 
			err->msg.nlmsg_type, 
			err->msg.nlmsg_seq, 
			err->msg.nlmsg_pid);
	}
	return -1;	
}

static int nl_response_hld(int sfd,
			   int (*handler)(struct nlmsghdr *, void *), 
			   void *jarg)
{	
	struct nlmsghdr *h;

#define BUFFLEN	4096
	char buff[BUFFLEN];
	size_t bcount;

	int ret = 0;

	bcount = sk_recv(sfd, buff, BUFFLEN, 0);
	if (bcount <= 0)
		return -1;

	h = (struct nlmsghdr *)buff;
	if (h->nlmsg_flags & MSG_TRUNC)
		return -1;
	
	for (; NLMSG_OK(h, bcount); h = NLMSG_NEXT(h, bcount)) {

		if (h->nlmsg_type == NLMSG_DONE)
			return ret;

		if (h->nlmsg_type == NLMSG_ERROR) {		
			ret = sk_netlink_err(h);
			if (ret <= 0)
				return ret;
			continue;
		}
		DEBUGP("%s: type %u, seq=%u, pid=%d\n", __func__, 
				h->nlmsg_type, h->nlmsg_seq, h->nlmsg_pid);

		ret = (*handler)(h, jarg);
		if (ret < 0) {
			DEBUGP("Error: handling msg.\n");
			return -1;
		}
	}
	return ret;
}

static void netlink_parse_rtattr(struct rtattr *tb[], int maxattr, 
				 struct rtattr *rta, int len)
{
	memset(tb, 0, sizeof(struct rtattr*)*maxattr);
	
	while (RTA_OK(rta, len)) {
		if (rta->rta_type <= maxattr)
			tb[rta->rta_type] = rta;
		rta = RTA_NEXT(rta, len);
	}
}

static int rtnl_iface_get(struct nlmsghdr *h,
			  int (*if_callback)(struct iface *))
{
	struct rtattr *tb[IFLA_MAX + 1];
	struct ifinfomsg *ifi;
	int len;

	struct iface iface;

	if (h->nlmsg_type != RTM_NEWLINK)
		return 0;

	ifi = NLMSG_DATA(h);
	
	len = h->nlmsg_len - NLMSG_LENGTH(sizeof(struct ifinfomsg));
	if (len < 0)
		return -1;
		
	netlink_parse_rtattr(tb, IFLA_MAX, IFLA_RTA(ifi), len);

	if (!tb[IFLA_IFNAME] || !tb[IFLA_ADDRESS]) {
		DEBUGP("Error: getting interface info");
		return -1;
	}

	iface.haddr_len = RTA_PAYLOAD(tb[IFLA_ADDRESS]);
	
	if (iface.haddr_len < MAX_ADDR_LEN) {
		memcpy(iface.haddr, RTA_DATA(tb[IFLA_ADDRESS]),
		       iface.haddr_len);
	} else {
		DEBUGP("Warning: Hardware address is too large: %d", 
			iface.haddr_len);
		return -1;
	}

	strcpy(iface.name, (char *)RTA_DATA(tb[IFLA_IFNAME]));
	strcpy(iface.qdisc, (char *)RTA_DATA(tb[IFLA_QDISC]));

	iface.index = ifi->ifi_index;
	iface.type  = ifi->ifi_type;
	iface.flags = ifi->ifi_flags & 0x0000fffff;

	iface.mtu 	 = (tb[IFLA_MTU]) ? *(int *)RTA_DATA(tb[IFLA_MTU]) : 0;
	iface.weight	 = (tb[IFLA_WEIGHT]) ? *(int *)RTA_DATA(tb[IFLA_WEIGHT]) : 0;
	iface.txqueuelen = *(int *)RTA_DATA(tb[IFLA_TXQLEN]);
	
	iface.stats = *(struct net_device_stats *)RTA_DATA(tb[IFLA_STATS]);

	return if_callback(&iface);
}


#define NIPQUAD(addr) \
        ((unsigned char *)&addr)[0], \
        ((unsigned char *)&addr)[1], \
        ((unsigned char *)&addr)[2], \
        ((unsigned char *)&addr)[3]

static int rtnl_ifaddr_get(struct nlmsghdr *h, 
			   int (*if_callback)(int, struct in_ifaddr *))
{
	struct rtattr *tb[IFA_MAX + 1];
	struct ifaddrmsg *ifaddr;
	int len;
	
	struct in_ifaddr in_ifaddr;
	
	if (h->nlmsg_type != RTM_NEWADDR)
		return 0;
	
	ifaddr = NLMSG_DATA(h);
	
	len = h->nlmsg_len - NLMSG_LENGTH(sizeof(struct ifaddrmsg));
	if (len < 0)
		return -1;

	netlink_parse_rtattr(tb, IFA_MAX, IFA_RTA(ifaddr), len);
	
	strcpy(in_ifaddr.ifa_label, (char *)RTA_DATA(tb[IFA_LABEL]));
	
	in_ifaddr.ifa_address	= *(uint32_t *)RTA_DATA(tb[IFA_ADDRESS]);
	in_ifaddr.ifa_local	= *(uint32_t *)RTA_DATA(tb[IFA_LOCAL]);
	
	if (tb[IFA_BROADCAST]) 
		in_ifaddr.ifa_broadcast	= *(uint32_t *)RTA_DATA(tb[IFA_BROADCAST]);
	
	if (tb[IFA_ANYCAST])
		in_ifaddr.ifa_anycast = *(uint32_t *)RTA_DATA(tb[IFA_ANYCAST]);

	in_ifaddr.ifa_family	= ifaddr->ifa_family;
	in_ifaddr.ifa_prefixlen = ifaddr->ifa_prefixlen;
	
	in_ifaddr.ifa_flags 	= ifaddr->ifa_flags;
	in_ifaddr.ifa_scope	= ifaddr->ifa_scope;
	
	return if_callback(ifaddr->ifa_index, &in_ifaddr);
}

static int rtnl_qdisc_get(struct nlmsghdr *h, 
			   int (*if_callback)(int, struct in_ifaddr *))
{
	struct rtattr *tb[IFA_MAX + 1];
	struct tcmsg *tc;
	int len;

	struct in_qdisc	qdisc;

	if (h->nlmsg_type != RTM_NEWQDISC)
		return 0;
	
	tc = NLMSG_DATA(h);
	
	len = h->nlmsg_len - NLMSG_LENGTH(sizeof(struct tcmsg));
	if (len < 0)
		return -1;

	netlink_parse_rtattr(tb, TCA_MAX, TCA_RTA(tc), len);
	
	strcpy(qdisc.name, (char *)RTA_DATA(tb[TCA_KIND]));
	qdisc.stats = *(struct tc_stats *)RTA_DATA(tb[TCA_STATS]);

	return if_callback(tc->tcm_ifindex, &qdisc);
}

int rtnl_generic_req(int nlreq, 
		     void (*rn_callback)(void *), 
		     void (*if_callback)(void *))
{
	uint32_t groups = 0;
	int nlfd, ret;

	nlfd = sk_netlink(groups);
	if (nlfd < 0)
		return -1;
	
	if (nl_generic_req(nlfd, nlreq) < 0)
		return -1;

	ret = nl_response_hld(nlfd, rn_callback, if_callback);
	
	close(nlfd);
		
	return ret;
}

int rtnl_get_ifaces(int (*if_callback)(void *))
{
	return rtnl_generic_req(RTM_GETLINK, rtnl_iface_get, if_callback);
}

int rtnl_get_ifaddr(int (*if_callback)(void *))
{
	return rtnl_generic_req(RTM_GETADDR, rtnl_ifaddr_get, if_callback);
}

int rtnl_get_qdisc(int (*if_callback)(void *))
{
	return rtnl_generic_req(RTM_GETQDISC, rtnl_qdisc_get, if_callback);
}

/*
struct rtnfo {
	int nlfd;
	int (*if_callback)(void *);
};
*/
