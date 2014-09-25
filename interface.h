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

#ifndef _INTERFACE_H_
#define _INTERFACE_H_

#include <net/route.h>
#include <linux/netdevice.h>
#include <linux/pkt_sched.h>

struct iface {
	struct list_head 	list;
	struct list_head	in_ifaddr;
	struct list_head	in_qdisc;
	
	char		name[IFNAMSIZ]; 
	char		qdisc[50];
	
	uint8_t		haddr[MAX_ADDR_LEN];
	int		haddr_len;
	
	uint32_t	index;
	uint32_t	flags;
	uint32_t	type;
	uint32_t	txqueuelen;
	
	uint32_t	mtu;
	uint32_t	weight; /* metric? */
	
	struct net_device_stats stats;
	struct net_device_stats old_stats;
};

struct in_ifaddr {
	struct list_head	list;
	
	char		ifa_label[IFNAMSIZ];
	uint32_t	ifa_family;	
	
	uint32_t	ifa_local;
	uint32_t	ifa_address;
	uint32_t	ifa_mask;
	uint32_t	ifa_broadcast;
	uint32_t	ifa_anycast;
	unsigned char	ifa_scope;
	unsigned char	ifa_flags;
	unsigned char	ifa_prefixlen;
};

struct in_qdisc {
	struct list_head	list;
	
	char		name[20];
	struct tc_stats stats;
};

#define NIPQUAD(addr) \
        ((unsigned char *)&addr)[0], \
        ((unsigned char *)&addr)[1], \
        ((unsigned char *)&addr)[2], \
        ((unsigned char *)&addr)[3]

void add_iface(struct iface *ifinfo);

#endif /* _INTERFACE_H_ */
