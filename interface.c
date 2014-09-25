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

#include "lists.h"
#include "interface.h" 
#include "init_modules.h"

#include "vty.h"

#define DEBUGP	printf

#define BUG_ON(cond) \
	do { if (cond) printf("Error: %s", __FUNC_, __LINE__); \
		abort(); \
	} while (0)


LIST_HEAD(iface_list);

struct iface *if_lookup_by_index(unsigned int index)
{
	struct list_head *ptr;
	
	iterate(ptr, &iface_list) {
		struct iface *ifa = container_of(ptr, struct iface, list);
		
		if (ifa->index == index)
			return ifa;
	}
	return NULL;
}	

struct iface *if_lookup_by_name(const char *name)
{
	struct list_head *ptr;
	
	iterate(ptr, &iface_list) {
		struct iface *ifa = container_of(ptr, struct iface, list);
		
		if (strcmp(ifa->name, name) == 0)
			return ifa;
	}
	return NULL;
}

static inline void print_haddr(char *buf, const uint8_t *addr, int addrlen)
{
	int i = 0;

	for (; i < addrlen; i++) {
		buf += sprintf(buf, "%02x", addr[i]);
		if (i % 2 && i < (addrlen - 1))
			buf += sprintf(buf, ".");
	}
}

void if_print_stats(CL_SOCK *vty, struct iface *ifa)
{
	char haddr[20];

	vty_out(vty, "%s is %s, line protocol is %s\r\n", ifa->name, 
		(ifa->flags & IFF_UP) ? "up" : "down", 
		(ifa->flags & IFF_RUNNING) ? "up" : "down");
	
	print_haddr(haddr, ifa->haddr, ifa->haddr_len);
	vty_out(vty, "  Hardware is %s, address is %s (bia %s)\r\n", 
	       "Ethernet", haddr, haddr);

/*	if (ifa->description)
		vty_out(vty, "  Description: \r\n");
*/
	if (!list_empty(&ifa->in_ifaddr)) {
		struct in_ifaddr *p;
		
		/* Primary addr, always first in the list.
		 */
		p = container_of(ifa->in_ifaddr.next, struct in_ifaddr, list);
		vty_out(vty, "  Internet address is %u.%u.%u.%u/%d\r\n", 
			NIPQUAD(p->ifa_address), p->ifa_prefixlen);
	}	
	vty_out(vty, "  MTU %u bytes\r\n", ifa->mtu);
	
	vty_out(vty, "  Encapsulation %s, loopback %s\r\n", 
		"Ethernet", (ifa->flags & IFF_LOOPBACK) ? "set" : "not set");
	
	vty_out(vty, "  Last clearing of \"show interface\" counters -FALTA-\r\n");

	vty_out(vty, "  Queueing strategy: %s\r\n", ifa->qdisc);  
  
	#define stats(stat) (ifa->stats.stat - ifa->old_stats.stat)
	vty_out(vty, "    %lu packets input, %lu bytes, %lu no buffer\r\n",
		stats(rx_packets),
		stats(rx_bytes),
		stats(rx_dropped));
	
	vty_out(vty, "    Received %lu broadcasts\r\n", stats(multicast)); /* Are the same? */
	
	vty_out(vty, "    %lu input errors, %lu CRC, %lu frame, %lu overrun, %lu ignored\r\n",
		stats(rx_errors),
		stats(rx_crc_errors),
		stats(rx_frame_errors),
		stats(rx_fifo_errors),
		stats(rx_missed_errors));

	vty_out(vty, "    %lu input packets with dribble condition detected\r\n",
		stats(rx_length_errors));
	
	vty_out(vty, "    %lu packets output, %lu bytes, %lu underruns\r\n", 
		stats(tx_packets),
		stats(tx_bytes),
		stats(tx_fifo_errors));
	
	/* Faltan desde aca. 1*/
	
	vty_out(vty, "    %lu output errors, %lu collisions, %lu interface resets\r\n",
		stats(tx_errors),
		stats(collisions),
		stats(tx_carrier_errors));

	vty_out(vty, "    %lu aborted packets, %lu carrier errors\r\n",
		stats(tx_aborted_errors),
		stats(tx_carrier_errors));
		
	vty_out(vty, "    %lu heartbeat failures, %lu window errors\r\n", 
        	stats(tx_heartbeat_errors),
		stats(tx_window_errors));
}


void if_update_interface(struct iface *kiface)
{
	struct iface *iface;
	
	iface = if_lookup_by_index(kiface->index);
	if (!iface)
		return;
	
	strcpy(iface->name, kiface->name);
	strcpy(iface->qdisc, kiface->qdisc);
	
	iface->haddr_len = kiface->haddr_len;
	memcpy(iface->haddr, kiface->haddr, kiface->haddr_len);
	
	iface->flags = kiface->flags;

	iface->mtu	  = kiface->mtu;
	iface->weight	  = kiface->weight;
	iface->txqueuelen = kiface->txqueuelen;
	
	iface->stats = kiface->stats;
}

#define M_ZERO(ptr)	do { memset(ptr, 0,sizeof(typeof(*ptr))); } while(0)

void if_add_interface(struct iface *kiface)
{
	struct iface *iface;
	
	iface = (struct iface *)malloc(sizeof(struct iface));

	*iface = *kiface;
	
	INIT_LIST_HEAD(&iface->in_ifaddr);
	M_ZERO(&iface->old_stats);
	
	list_add_tail(&iface->list, &iface_list);
	DEBUGP("Interface %s added\n", iface->name);
}

void if_add_ifaddr(int index, struct in_ifaddr *kifaddr)
{
	struct in_ifaddr *in_ifaddr;
	struct iface *iface;
	
	iface = if_lookup_by_index(index);
	if (!iface)
		return;
	
	in_ifaddr = (struct in_ifaddr *)malloc(sizeof(struct in_ifaddr));
	
	*in_ifaddr = *kifaddr;
	
	list_add_tail(&in_ifaddr->list, &iface->in_ifaddr);
	
	DEBUGP("ifaddr 	name: %s\n", in_ifaddr->ifa_label);
	DEBUGP("	addr: %u.%u.%u.%u/%d added to %s\n", 
		NIPQUAD(in_ifaddr->ifa_address), 
		in_ifaddr->ifa_prefixlen, iface->name);
}

void if_add_qdisc(int index, struct in_qdisc *kqdisc)
{
	struct in_qdisc *qdisc;
	struct iface *iface;
	
	iface = if_lookup_by_index(index);
	if (!iface)
		return;
	
	DEBUGP("name: %s\n", kqdisc->name);
	DEBUGP("stats:\n");
	DEBUGP("    %llu bytes, %u packets, %u drops, %u overlimits\n",
		kqdisc->stats.bytes,
		kqdisc->stats.packets,
		kqdisc->stats.drops,
		kqdisc->stats.overlimits);
	
	DEBUGP("    %u bps, %u pps, %u qlen, %u backlog\n", 
		kqdisc->stats.bps,
		kqdisc->stats.pps,
		kqdisc->stats.qlen,
		kqdisc->stats.backlog);
}

int if_get_interfaces(void)
{
#ifdef HAVE_RT_NETLINK
	if (rtnl_get_ifaces(if_add_interface) < 0
	    || rtnl_get_ifaddr(if_add_ifaddr) < 0) {
	 	DEBUGP("Error: if_get_interfaces()\n");
		return -1;
	}

	rtnl_get_qdisc(if_add_qdisc);
#endif
	return 0;
}

static inline int show_all_ifaces(CL_SOCK *vty)
{
	struct list_head *ptr;
	
	iterate(ptr, &iface_list) {
		struct iface *ifa = container_of(ptr, struct iface, list);
		
		if_print_stats(vty, ifa);
	}
	return 0;
}

int if_show_interface(CL_SOCK *vty, const char *ifaname)
{
	struct iface *ifa;
	
#ifdef HAVE_RT_NETLINK
	if (rtnl_get_ifaces(if_update_interface) < 0)
	 	return -1;
#endif
	
	if (!ifaname) {
		show_all_ifaces(vty);
		return 0;
	}

	ifa = if_lookup_by_name(ifaname);
	if (!ifa) {
		vty_out(vty, "Invalid Interface\r\n");
		return -1;
	}
	if_print_stats(vty, ifa);
		
	return 0;	
}

int if_clear_counters(void)
{
	struct list_head *ptr;
	
	iterate(ptr, &iface_list) {
		struct iface *ifa = container_of(ptr, struct iface, list);
		
		ifa->old_stats = ifa->stats;
	}
}


/*************************  I N T E R F A C E  C O M M A N D S ************************/
/*

#include "commands.h"
*/
/*
CMD_DEFINE_FN(show_interfaces, "interfaces", 
	"Interface status and configuration", 1, 0)
{
	if (vty->argc != 2)
		vty_out(vty, "CAMBIAR --- PROBLEMAS DE ARGUMENTOS");
		
	automore_on(vty);
	
	if (show_iface(argv[1]) < 0)
		vty_out(vty, "Invalid Interface.");
	else
		show_all_interfaces();
	
	automore_off(vty);
	
	return 0;
}
*/

init_call(if_get_interfaces);
