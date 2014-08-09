/*-
 * Copyright (c) 2012, 2013 Gabor Pali
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/libkern.h>
#include <sys/lock.h>
#include <sys/mutex.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/if_arp.h>
#include <net/ethernet.h>

#include "caml/mlvalues.h"
#include "caml/memory.h"
#include "caml/alloc.h"
#include "caml/fail.h"
#include "caml/bigarray.h"

static const u_char lladdr_all[ETHER_ADDR_LEN] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

struct mbuf_entry {
	LIST_ENTRY(mbuf_entry)	me_next;
	struct mbuf		*me_m;
};

struct plugged_if {
	TAILQ_ENTRY(plugged_if)	pi_next;
	struct ifnet		*pi_ifp;
	u_short	pi_index;
	u_short	pi_llindex;
	int	pi_flags;
	u_char	pi_lladdr[ETHER_ADDR_LEN];	/* Real MAC address */
	u_char	pi_lladdr_v[ETHER_ADDR_LEN];	/* Virtual MAC address */
#ifdef NETIF_DEBUG
	int	pi_rx_qlen;
#endif
	char	pi_xname[IFNAMSIZ];
	struct  mtx			pi_rx_lock;
	LIST_HEAD(, mbuf_entry)		pi_rx_head;
};

TAILQ_HEAD(plugged_ifhead, plugged_if) pihead =
    TAILQ_HEAD_INITIALIZER(pihead);

static int plugged;


/* Currently only Ethernet interfaces are returned. */
CAMLprim value caml_get_vifs(value v_unit);
CAMLprim value caml_plug_vif(value id, value index, value mac);
CAMLprim value caml_unplug_vif(value id);
CAMLprim value caml_get_mbufs(value id);
CAMLprim value caml_get_next_mbuf(value id);
CAMLprim value caml_put_mbufs(value id, value bufs);

/* netgraph(3) node hooks stolen from ng_ether(4) */
extern void (*ng_ether_input_p)(struct ifnet *ifp, struct mbuf **mp);
extern void (*ng_ether_input_orphan_p)(struct ifnet *ifp, struct mbuf *m);
extern int  (*ng_ether_output_p)(struct ifnet *ifp, struct mbuf **mp);
extern void (*ng_ether_attach_p)(struct ifnet *ifp);
extern void (*ng_ether_detach_p)(struct ifnet *ifp);

void netif_init(void);
void netif_deinit(void);

static void netif_ether_input(struct ifnet *ifp, struct mbuf **mp);
static int  netif_ether_output(struct ifnet *ifp, struct mbuf **mp);
static void netif_ether_input_orphan(struct ifnet *ifp, struct mbuf *m);
static void netif_ether_attach(struct ifnet *ifp);
static void netif_ether_detach(struct ifnet *ifp);

static void (*prev_ng_ether_input_p)(struct ifnet *ifp, struct mbuf **mp);
static void (*prev_ng_ether_input_orphan_p)(struct ifnet *ifp, struct mbuf *m);
static int  (*prev_ng_ether_output_p)(struct ifnet *ifp, struct mbuf **mp);
static void (*prev_ng_ether_attach_p)(struct ifnet *ifp);
static void (*prev_ng_ether_detach_p)(struct ifnet *ifp);


static struct plugged_if *
find_pi_by_index(u_short val)
{
	struct plugged_if *pip;

	TAILQ_FOREACH(pip, &pihead, pi_next)
		if (pip->pi_index == val)
			return pip;
	return NULL;
}

static struct plugged_if *
find_pi_by_llindex(u_short val)
{
	struct plugged_if *pip;

	TAILQ_FOREACH(pip, &pihead, pi_next)
		if (pip->pi_llindex == val)
			return pip;

	return NULL;
}

static struct plugged_if *
find_pi_by_name(const char *val)
{
	struct plugged_if *pip;

	TAILQ_FOREACH(pip, &pihead, pi_next)
		if (strncmp(pip->pi_xname, val, IFNAMSIZ) == 0)
			return pip;
	return NULL;
}

CAMLprim value
caml_get_vifs(value v_unit)
{
	CAMLparam1(v_unit);
	CAMLlocal2(result, r);
	struct ifnet *ifp;
	struct ifaddr *ifa;
	struct sockaddr_dl *sdl;

	result = Val_emptylist;
	IFNET_RLOCK_NOSLEEP();
	TAILQ_FOREACH(ifp, &V_ifnet, if_link) {
		IF_ADDR_RLOCK(ifp);
		TAILQ_FOREACH(ifa, &ifp->if_addrhead, ifa_link) {
			sdl = (struct sockaddr_dl *) ifa->ifa_addr;
			if (sdl != NULL && sdl->sdl_family == AF_LINK &&
			    sdl->sdl_type == IFT_ETHER) {
				/* We have a MAC address, add the interface. */
				r = caml_alloc(2, 0);
				Store_field(r, 0,
				    caml_copy_string(ifp->if_xname));
				Store_field(r, 1, result);
				result = r;
				break;
			}
		}
		IF_ADDR_RUNLOCK(ifp);
	}
	IFNET_RUNLOCK_NOSLEEP();
	CAMLreturn(result);
}

CAMLprim value
caml_plug_vif(value id, value index, value mac)
{
	CAMLparam3(id, index, mac);
	struct ifnet *ifp;
	struct sockaddr_dl *sdl;
	struct plugged_if *pip;
	char*	lladdr;
	int found;
#ifdef NETIF_DEBUG
	u_char	*p1, *p2;
#endif

	pip = __calloc(1, sizeof(struct plugged_if));

	if (pip == NULL)
		caml_failwith("No memory for plugging a new interface");

	found = 0;
	IFNET_WLOCK();
	TAILQ_FOREACH(ifp, &V_ifnet, if_link) {
		if (strncmp(ifp->if_xname, String_val(id), IFNAMSIZ) != 0)
			continue;

		/* Add a fake NetGraph node, if needed. */
		if (IFP2AC(ifp)->ac_netgraph == NULL)
			IFP2AC(ifp)->ac_netgraph = (void *) 1;
		/* Ensure that the MTU is enough for Mirage. */
		ifp->if_mtu   = max(1514, ifp->if_mtu);
		pip->pi_ifp   = ifp;
		pip->pi_flags = ifp->if_flags;
		bcopy(ifp->if_xname, pip->pi_xname, IFNAMSIZ);
		pip->pi_llindex = ifp->if_index;

		IF_ADDR_RLOCK(ifp);
		sdl = (struct sockaddr_dl *) ifp->if_addr->ifa_addr;
		if (sdl != NULL && sdl->sdl_family == AF_LINK &&
		    sdl->sdl_type == IFT_ETHER)
			bcopy(LLADDR(sdl), pip->pi_lladdr, ETHER_ADDR_LEN);
		IF_ADDR_RUNLOCK(ifp);

		found = 1;
		break;
	}
	IFNET_WUNLOCK();

	if (!found) {
		__free(pip);
		caml_failwith("Invalid interface");
	}

	pip->pi_index = Int_val(index);

	lladdr = String_val(mac);
	bcopy(lladdr, pip->pi_lladdr_v, ETHER_ADDR_LEN);

	mtx_init(&pip->pi_rx_lock, "plugged_if_rx", NULL, MTX_DEF);
	LIST_INIT(&pip->pi_rx_head);

	if (plugged == 0)
		TAILQ_INIT(&pihead);

	TAILQ_INSERT_TAIL(&pihead, pip, pi_next);
	plugged++;

#ifdef NETIF_DEBUG
	p1 = pip->pi_lladdr;
	p2 = pip->pi_lladdr_v;
	printf("caml_plug_vif: ifname=[%s] MAC=("
	    "real=%02x:%02x:%02x:%02x:%02x:%02x, "
	    "virtual=%02x:%02x:%02x:%02x:%02x:%02x)\n",
	    pip->pi_xname, p1[0], p1[1], p1[2], p1[3], p1[4], p1[5],
	    p2[0], p2[1], p2[2], p2[3], p2[4], p2[5]);
#endif

	CAMLreturn(Val_bool(pip->pi_flags & IFF_UP));
}

CAMLprim value
caml_unplug_vif(value id)
{
	CAMLparam1(id);
	struct plugged_if *pip;
	struct ifnet *ifp;
	struct mbuf_entry *e1;
	struct mbuf_entry *e2;

	pip = find_pi_by_name(String_val(id));
	if (pip == NULL)
		CAMLreturn(Val_unit);

	IFNET_WLOCK();
	TAILQ_FOREACH(ifp, &V_ifnet, if_link) {
		if (strncmp(ifp->if_xname, String_val(id), IFNAMSIZ) == 0) {
			/* Remove the fake NetGraph node, if there was any. */
			if (IFP2AC(ifp)->ac_netgraph == (void *) 1)
				IFP2AC(ifp)->ac_netgraph = (void *) 0;
			break;
		}
	}
	IFNET_WUNLOCK();

#ifdef NETIF_DEBUG
	printf("caml_unplug_vif: ifname=[%s]\n", pip->pi_xname);
#endif

	TAILQ_REMOVE(&pihead, pip, pi_next);

	e1 = LIST_FIRST(&pip->pi_rx_head);
	while (e1 != NULL) {
		e2 = LIST_NEXT(e1, me_next);
		m_freem(e1->me_m);
		__free(e1);
		e1 = e2;
	}
	LIST_INIT(&pip->pi_rx_head);
	mtx_destroy(&pip->pi_rx_lock);

	__free(pip);
	plugged--;

	CAMLreturn(Val_unit);
}

/* Listening to incoming Ethernet frames. */
void
netif_ether_input(struct ifnet *ifp, struct mbuf **mp)
{
	struct plugged_if *pip;
	struct mbuf_entry *e;
	struct ether_header *eh;
	char mine, bcast;
#ifdef NETIF_DEBUG
	int i;
#endif

#ifdef NETIF_DEBUG
	printf("New incoming frame on if=[%s]!\n", ifp->if_xname);
#endif

	if (plugged == 0) {
#ifdef NETIF_DEBUG
		printf("No plugged interface, skipping.\n");
#endif
		goto end;
	}

	pip = find_pi_by_llindex(ifp->if_index);

	if (pip == NULL) {
#ifdef NETIF_DEBUG
		printf("No interface found for index: %d\n", ifp->if_index);
#endif
		goto end;
	}

	eh = mtod(*mp, struct ether_header *);
	mine  = bcmp(eh->ether_dhost, pip->pi_lladdr_v, ETHER_ADDR_LEN) == 0;
	bcast = bcmp(eh->ether_dhost, lladdr_all, ETHER_ADDR_LEN) == 0;

#ifdef NETIF_DEBUG
	printf("Destination: %02x:%02x:%02x:%02x:%02x:%02x (%04x), %s.\n",
	    eh->ether_dhost[0], eh->ether_dhost[1], eh->ether_dhost[2],
	    eh->ether_dhost[3], eh->ether_dhost[4], eh->ether_dhost[5],
	    ntohs(eh->ether_type), (mine || bcast) ? "intercepting" : "skipping");
#endif

	/* Let the frame escape if it is neither ours nor broadcast. */
	if (!mine && !bcast)
		goto end;

	e = (struct mbuf_entry *) __malloc(sizeof(struct mbuf_entry));

	/* Out of memory, cannot do much. */
	if (e == NULL)
		goto end;

	e->me_m = bcast ? m_copypacket(*mp, M_DONTWAIT) : *mp;
	mtx_lock(&pip->pi_rx_lock);
	LIST_INSERT_HEAD(&pip->pi_rx_head, e, me_next);
#ifdef NETIF_DEBUG
	i = (++pip->pi_rx_qlen);
#endif
	mtx_unlock(&pip->pi_rx_lock);
#ifdef NETIF_DEBUG
	printf("[%s]: %d frames are queued.\n", pip->pi_xname, i);
#endif

	if (!bcast)
		*mp = NULL;

end:
	if (prev_ng_ether_input_p != NULL && (*mp) != NULL)
		(*prev_ng_ether_input_p)(ifp, mp);
}

CAMLprim value
caml_get_mbufs(value id)
{
	CAMLparam1(id);
	CAMLlocal3(result, t, r);
	struct plugged_if *pip;
	struct mbuf_entry *e1;
	struct mbuf_entry *e2;
	struct mbuf *m;
	struct mbuf *n;
#ifdef NETIF_DEBUG
	int num_pages;
#endif

#ifdef NETIF_DEBUG
	printf("caml_get_mbufs(): invoked\n");
#endif

	result = Val_emptylist;

	if (plugged == 0)
		CAMLreturn(result);

	pip = find_pi_by_index(Int_val(id));

#ifdef NETIF_DEBUG
	printf("caml_get_mbufs(): pip=%p\n", pip);
	num_pages = 0;
#endif

	if (pip == NULL)
		CAMLreturn(result);

	mtx_lock(&pip->pi_rx_lock);
	e1 = LIST_FIRST(&pip->pi_rx_head);
	while (e1 != NULL) {
		for (m = e1->me_m; m != NULL; m = m->m_nextpkt) {
			for (n = m; n != NULL; n = n->m_next) {
				t = caml_alloc(3, 0);
				Store_field(t, 0,
				    caml_ba_alloc_dims(CAML_BA_UINT8
				    | CAML_BA_C_LAYOUT | CAML_BA_FBSD_MBUF, 1,
				    (void *) n, (long) n->m_len));
				Store_field(t, 1, Val_int(0));
				Store_field(t, 2, Val_int(n->m_len));
				r = caml_alloc(2, 0);
				Store_field(r, 0, t);
				Store_field(r, 1, result);
				result = r;
#ifdef NETIF_DEBUG
				num_pages++;
				pip->pi_rx_qlen--;
#endif
			}
		}
		e2 = LIST_NEXT(e1, me_next);
		__free(e1);
		e1 = e2;
	}
	LIST_INIT(&pip->pi_rx_head);
	mtx_unlock(&pip->pi_rx_lock);

#ifdef NETIF_DEBUG
	printf("caml_get_mbufs(): shipped %d pages.\n", num_pages);
#endif

	CAMLreturn(result);
}

CAMLprim value
caml_get_next_mbuf(value id)
{
	CAMLparam1(id);
	CAMLlocal2(result, v);
	struct plugged_if *pip;
	struct mbuf_entry *ep;
	struct mbuf *m;
	long len;

	pip = find_pi_by_index(Int_val(id));

	if (pip == NULL)
		caml_failwith("No interface");

#ifdef NETIF_DEBUG
	printf("caml_get_next_mbufs(): [%s]\n", pip->pi_xname);
#endif

	mtx_lock(&pip->pi_rx_lock);
	ep = LIST_FIRST(&pip->pi_rx_head);
	mtx_unlock(&pip->pi_rx_lock);

	/* No frame today. */
	if (ep == NULL)
		CAMLreturn(Val_none);

	m = ep->me_m;

	mtx_lock(&pip->pi_rx_lock);
	if (m->m_nextpkt == NULL) {
		LIST_REMOVE(ep, me_next);
#ifdef NETIF_DEBUG
		pip->pi_rx_qlen--;
#endif
	}
	else
		ep->me_m = m->m_nextpkt;
	mtx_unlock(&pip->pi_rx_lock);

	if (m->m_nextpkt == NULL)
		__free(ep);

	/* Flatten packet if it is multi-part. */
	if (m->m_next != NULL) {
		len = m->m_pkthdr.len;
		v = caml_ba_alloc_dims(CAML_BA_UINT8 | CAML_BA_C_LAYOUT, 1,
		    NULL, len);
		m_copydata(m, 0, len, Caml_ba_array_val(v)->data);
		m_freem(m);
	}
	else {
		len = m->m_len;
		v = caml_ba_alloc_dims(CAML_BA_UINT8 | CAML_BA_C_LAYOUT |
		    CAML_BA_FBSD_MBUF, 1, (void *) m, len);
	}

	result = caml_alloc(3, 0);
	Store_field(result, 0, v);
	Store_field(result, 1, Val_int(0));
	Store_field(result, 2, Val_int(len));

#ifdef NETIF_DEBUG
	printf("Frame extracted of size %ld (data=%p).\n", len,
	    Caml_ba_array_val(v)->data);
#endif

	CAMLreturn(Val_some(result));
}

int
netif_ether_output(struct ifnet *ifp, struct mbuf **mp)
{
	int error;

#ifdef NETIF_DEBUG
	printf("New outgoing frame on if=[%s], feeding back.\n", ifp->if_xname);
#endif

	if (plugged > 0)
		netif_ether_input(ifp, mp);

	if (prev_ng_ether_output_p != NULL && (*mp) != NULL)
		if ((error = (*prev_ng_ether_output_p)(ifp, mp)) != 0)
			return error;

	return 0;
}

static void
netif_ether_input_orphan(struct ifnet *ifp, struct mbuf *m)
{
	if (prev_ng_ether_input_orphan_p != NULL)
		(*prev_ng_ether_input_orphan_p)(ifp, m);
}

static void
netif_ether_attach(struct ifnet *ifp)
{
	if (prev_ng_ether_attach_p != NULL)
		(*prev_ng_ether_attach_p)(ifp);
}

static void
netif_ether_detach(struct ifnet *ifp)
{
	if (prev_ng_ether_attach_p != NULL)
		(*prev_ng_ether_detach_p)(ifp);
}

static int
netif_mbuf_free(struct mbuf *__nothing, void *p1, void *p2)
{
	struct caml_ba_proxy *proxy = (struct caml_ba_proxy *) p1;

#ifdef NETIF_DEBUG
	printf("netif_mbuf_free: %p, %p\n", p1, p2);
#endif

	if (--(proxy->refcount) > 0)
        return (EXT_FREE_OK);

	switch (proxy->type) {
		case CAML_FREEBSD_IOPAGE:
			__contigfree(p2, proxy->size);
			break;

		case CAML_FREEBSD_MBUF:
			__free(p2);
			break;

		default:
			printf("Unknown Bigarray metadata type: %02x\n",
			    proxy->type);
			break;
	}

	__free(proxy);

    return (EXT_FREE_OK);
}

static struct mbuf *
netif_map_to_mbuf(struct caml_ba_array *b, int v_off, int *v_len)
{
	struct mbuf **mp;
	struct mbuf *m;
	struct mbuf *frag;
	struct caml_ba_proxy *proxy;
	void *data;
	size_t frag_len;
	char *p;

	proxy = b->proxy;
	data = (char *) b->data + v_off;
	frag_len = min(proxy->size - v_off, *v_len);
	*v_len = frag_len;

	mp = &frag;
	p = data;

	while (frag_len > 0) {
		MGET(m, M_DONTWAIT, MT_DATA);

		if (m == NULL) {
			m_freem(frag);
			return NULL;
		}

		m->m_flags       |= M_EXT;
		m->m_ext.ext_type = EXT_EXTREF;
		m->m_ext.ext_buf  = (void *) p;
		m->m_ext.ext_free = netif_mbuf_free;
		m->m_ext.ext_arg1 = proxy;
		m->m_ext.ext_arg2 = data;
		m->m_ext.ref_cnt  = &proxy->refcount;
		m->m_len          = min(MCLBYTES, frag_len);
		m->m_data         = m->m_ext.ext_buf;
		*(m->m_ext.ref_cnt) += 1;

		frag_len -= m->m_len;
		p += m->m_len;
		*mp = m;
		mp = &(m->m_next);
	}

	return frag;
}

CAMLprim value
caml_put_mbufs(value id, value bufs)
{
	CAMLparam2(id, bufs);
	CAMLlocal2(v, t);
	struct plugged_if *pip;
	struct mbuf **mp;
	struct mbuf *frag;
	struct mbuf *pkt;
	struct caml_ba_array *b;
	struct ifnet *ifp;
	size_t pkt_len;
	int v_off, v_len;
	char bcast, real;
	struct ether_header *eh;

	if ((bufs == Val_emptylist) || (plugged == 0))
		CAMLreturn(Val_unit);

	pip = find_pi_by_index(Int_val(id));
	if (pip == NULL)
		CAMLreturn(Val_unit);

	ifp = pip->pi_ifp;
	pkt_len = 0;
	mp = &pkt;

	while (bufs != Val_emptylist) {
		t = Field(bufs, 0);
		v = Field(t, 0);
		v_off = Int_val(Field(t, 1));
		v_len = Int_val(Field(t, 2));
		b = Caml_ba_array_val(v);
		frag = netif_map_to_mbuf(b, v_off, &v_len);
		if (frag == NULL)
			caml_failwith("No memory for mapping to mbuf");
		*mp = frag;
		mp = &(frag->m_next);
		pkt_len += v_len;
		bufs = Field(bufs, 1);
	}

	pkt->m_flags       |= M_PKTHDR;
	pkt->m_pkthdr.len   = pkt_len;
	pkt->m_pkthdr.rcvif = ifp;
	SLIST_INIT(&pkt->m_pkthdr.tags);

	if (pkt->m_pkthdr.len > pip->pi_ifp->if_mtu)
		printf("%s: Packet is greater (%d) than the MTU (%ld)\n",
		    pip->pi_xname, pkt->m_pkthdr.len, pip->pi_ifp->if_mtu);

	eh = mtod(pkt, struct ether_header *);
	real  = bcmp(eh->ether_dhost, pip->pi_lladdr, ETHER_ADDR_LEN) == 0;
	bcast = bcmp(eh->ether_dhost, lladdr_all, ETHER_ADDR_LEN) == 0;

#ifdef NETIF_DEBUG
	printf("Sending to: %02x:%02x:%02x:%02x:%02x:%02x (%04x), %s%s"
	    " size=%d.\n",
	    eh->ether_dhost[0], eh->ether_dhost[1], eh->ether_dhost[2],
	    eh->ether_dhost[3], eh->ether_dhost[4], eh->ether_dhost[5],
	    ntohs(eh->ether_type),
	    (real || bcast)  ? "[if_input]"  : "",
	    (!real || bcast) ? "[if_output]" : "",
	    (int) pkt_len);
#endif

	/* Sending to the real Ethernet address. */
	if (real || bcast)
		(ifp->if_input)(ifp,
		    bcast ? m_copypacket(pkt, M_DONTWAIT) : pkt);

	if (!real || bcast)
		(ifp->if_transmit)(ifp, pkt);

	CAMLreturn(Val_unit);
}

void
netif_init(void)
{
	prev_ng_ether_input_p = ng_ether_input_p;
	ng_ether_input_p = netif_ether_input;

	prev_ng_ether_input_orphan_p = ng_ether_input_orphan_p;
	ng_ether_input_orphan_p = netif_ether_input_orphan;

	prev_ng_ether_output_p = ng_ether_output_p;
	ng_ether_output_p = netif_ether_output;

	prev_ng_ether_attach_p = ng_ether_attach_p;
	ng_ether_attach_p = netif_ether_attach;

	prev_ng_ether_detach_p = ng_ether_detach_p;
	ng_ether_detach_p = netif_ether_detach;
}

void
netif_deinit(void)
{
	struct plugged_if *p1, *p2;
	struct ifnet *ifp;
	struct mbuf_entry *e1, *e2;

	ng_ether_input_p        = prev_ng_ether_input_p;
	ng_ether_input_orphan_p = prev_ng_ether_input_orphan_p;
	ng_ether_output_p       = prev_ng_ether_output_p;
	ng_ether_attach_p       = prev_ng_ether_attach_p;
	ng_ether_detach_p       = prev_ng_ether_detach_p;

	p1 = TAILQ_FIRST(&pihead);
	while (p1 != NULL) {
		p2  = TAILQ_NEXT(p1, pi_next);
		ifp = p1->pi_ifp;
		IFNET_WLOCK();
		if (IFP2AC(ifp)->ac_netgraph == (void *) 1)
			IFP2AC(ifp)->ac_netgraph = (void *) 0;
		IFNET_WUNLOCK();
		mtx_lock(&p1->pi_rx_lock);
		e1 = LIST_FIRST(&p1->pi_rx_head);
		while (e1 != NULL) {
			e2 = LIST_NEXT(e1, me_next);
			m_freem(e1->me_m);
			__free(e1);
			e1 = e2;
		}
#ifdef NETIF_DEBUG
		p1->pi_rx_qlen = 0;
#endif
		LIST_INIT(&p1->pi_rx_head);
		mtx_unlock(&p1->pi_rx_lock);
		__free(p1);
		plugged--;
		p1 = p2;
	}
	TAILQ_INIT(&pihead);
}
