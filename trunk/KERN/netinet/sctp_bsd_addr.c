/*-
 * Copyright (c) 2001-2007, by Cisco Systems, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions are met:
 * 
 * a) Redistributions of source code must retain the above copyright notice, 
 *   this list of conditions and the following disclaimer.
 *
 * b) Redistributions in binary form must reproduce the above copyright 
 *    notice, this list of conditions and the following disclaimer in 
 *   the documentation and/or other materials provided with the distribution.
 *
 * c) Neither the name of Cisco Systems, Inc. nor the names of its 
 *    contributors may be used to endorse or promote products derived 
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF 
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

/* $KAME: sctp_output.c,v 1.46 2005/03/06 16:04:17 itojun Exp $	 */

#ifdef __FreeBSD__
#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/netinet/sctp_bsd_addr.c,v 1.7 2007/05/08 17:01:10 rrs Exp $");
#endif

#include <netinet/sctp_os.h>
#include <netinet/sctp_var.h>
#include <netinet/sctp_pcb.h>
#include <netinet/sctp_header.h>
#include <netinet/sctputil.h>
#include <netinet/sctp_output.h>
#include <netinet/sctp_bsd_addr.h>
#include <netinet/sctp_uio.h>
#include <netinet/sctputil.h>
#include <netinet/sctp_timer.h>
#include <netinet/sctp_asconf.h>
#include <netinet/sctp_sysctl.h>
#include <netinet/sctp_indata.h>
#include <sys/unistd.h>


#if defined(SCTP_USE_THREAD_BASED_ITERATOR)
void
sctp_wakeup_iterator(void)
{
	wakeup(&sctppcbinfo.iterator_running);
}

static void
sctp_iterator_thread(void *v)
{
	SCTP_IPI_ITERATOR_WQ_LOCK();
	sctppcbinfo.iterator_running = 0;
	while (1) {
		msleep(&sctppcbinfo.iterator_running,
#if defined(__FreeBSD__)
		       &sctppcbinfo.ipi_iterator_wq_mtx,
#elif defined(__APPLE__)
		       sctppcbinfo.ipi_iterator_wq_mtx,
#endif
	 	       0, "waiting_for_work", 0);
		sctp_iterator_worker();
	}
}

void
sctp_startup_iterator(void)
{
#if defined(__FreeBSD__)
	int ret;
	ret = kthread_create(sctp_iterator_thread,
			     (void *)NULL , 
			     &sctppcbinfo.thread_proc,
			     RFPROC, 
			     SCTP_KTHREAD_PAGES, 
			     SCTP_KTRHEAD_NAME);
#elif defined(__APPLE__)
	sctppcbinfo.thread_proc = IOCreateThread(sctp_iterator_thread,
						 (void *)NULL);
#endif
}
#endif


void
sctp_gather_internal_ifa_flags(struct sctp_ifa *ifa)
{
	struct in6_ifaddr *ifa6;
	ifa6 = (struct in6_ifaddr *)ifa->ifa;
	ifa->flags = ifa6->ia6_flags;
	if (!ip6_use_deprecated) {
		if (ifa->flags &
		    IN6_IFF_DEPRECATED) {
			ifa->localifa_flags |= SCTP_ADDR_IFA_UNUSEABLE;
		} else {
			ifa->localifa_flags &= ~SCTP_ADDR_IFA_UNUSEABLE;
		}
	} else {
		ifa->localifa_flags &= ~SCTP_ADDR_IFA_UNUSEABLE;
	}
	if (ifa->flags &
	    (IN6_IFF_DETACHED |
	     IN6_IFF_ANYCAST |
	     IN6_IFF_NOTREADY)) {
		ifa->localifa_flags |= SCTP_ADDR_IFA_UNUSEABLE;
	} else {
		ifa->localifa_flags &= ~SCTP_ADDR_IFA_UNUSEABLE;
	}
}



static uint32_t
sctp_is_desired_interface_type(struct ifaddr *ifa)
{
        int result;
 
        /* check the interface type to see if it's one we care about */
        switch (ifa->ifa_ifp->if_type) {
        case IFT_ETHER:
        case IFT_ISO88023:
	case IFT_ISO88024:
        case IFT_ISO88025:
	case IFT_ISO88026:
        case IFT_STARLAN:
        case IFT_P10:
        case IFT_P80:
        case IFT_HY:
        case IFT_FDDI:
        case IFT_XETHER:
	case IFT_ISDNBASIC:
	case IFT_ISDNPRIMARY:
	case IFT_PTPSERIAL:
	case IFT_PPP:
	case IFT_LOOP:
	case IFT_SLIP:
#if !defined(__APPLE__)
	case IFT_IP:
	case IFT_IPOVERCDLC:
	case IFT_IPOVERCLAW:
	case IFT_VIRTUALIPADDRESS:
#endif
                result = 1;
                break;
        default:
                result = 0;
        }

        return (result);
}

static void
sctp_init_ifns_for_vrf(int vrfid)
{
	/* Here we must apply ANY locks needed by the
	 * IFN we access and also make sure we lock
	 * any IFA that exists as we float through the
	 * list of IFA's
	 */
	struct ifnet *ifn;
	struct ifaddr *ifa;
	struct in6_ifaddr *ifa6;
	struct sctp_ifa *sctp_ifa;
	uint32_t ifa_flags;

	TAILQ_FOREACH(ifn, &ifnet, if_list) {
		TAILQ_FOREACH(ifa, &ifn->if_addrlist, ifa_list) {
			if(ifa->ifa_addr == NULL) {
				continue;
			}
			if ((ifa->ifa_addr->sa_family != AF_INET) &&
			    (ifa->ifa_addr->sa_family != AF_INET6)
				) {
				/* non inet/inet6 skip */
				continue;
			}
			if(ifa->ifa_addr->sa_family == AF_INET6) {
				ifa6 = (struct in6_ifaddr *)ifa;
				ifa_flags = ifa6->ia6_flags;
				if (IN6_IS_ADDR_UNSPECIFIED(&((struct sockaddr_in6 *)ifa->ifa_addr)->sin6_addr)) {
					/* skip unspecifed addresses */
					continue;
				}

			} else if (ifa->ifa_addr->sa_family == AF_INET) {
				if (((struct sockaddr_in *)ifa->ifa_addr)->sin_addr.s_addr == 0) {
					continue;
				}
			}

			if (sctp_is_desired_interface_type(ifa) == 0) {
				/* non desired type */
				continue;
			}

			if((ifa->ifa_addr->sa_family == AF_INET6) ||
			   (ifa->ifa_addr->sa_family == AF_INET)) {
				if (ifa->ifa_addr->sa_family == AF_INET6) {
					ifa6 = (struct in6_ifaddr *)ifa;
					ifa_flags = ifa6->ia6_flags;
				} else {
					ifa_flags = 0;
				}
				sctp_ifa = sctp_add_addr_to_vrf(vrfid, 
								(void *)ifn,
								ifn->if_index, 
								ifn->if_type,
#ifdef __APPLE__
								ifn->if_name,
#else
								ifn->if_xname,
#endif
								(void *)ifa,
								ifa->ifa_addr,
								ifa_flags, 0
					);
				if(sctp_ifa) {
					sctp_ifa->localifa_flags &= ~SCTP_ADDR_DEFER_USE;
				} 
			}
		}
	}
}


void 
sctp_init_vrf_list(int vrfid)
{
	if(vrfid > SCTP_MAX_VRF_ID)
		/* can't do that */
		return;

	/* Don't care about return here */
	(void)sctp_allocate_vrf(vrfid);

	/* Now we need to build all the ifn's 
	 * for this vrf and there addresses
	 */
	sctp_init_ifns_for_vrf(vrfid); 
}

static uint8_t first_time=0;


void
sctp_addr_change(struct ifaddr *ifa, int cmd)
{
	struct sctp_ifa *ifap=NULL;
	uint32_t ifa_flags=0;
	struct in6_ifaddr *ifa6;
	/* BSD only has one VRF, if this changes
	 * we will need to hook in the right 
	 * things here to get the id to pass to
	 * the address managment routine.
	 */
	if (first_time == 0) {
		/* Special test to see if my ::1 will showup with this */
		first_time = 1;
		sctp_init_ifns_for_vrf(SCTP_DEFAULT_VRFID);
	}
	if ((cmd != RTM_ADD) && (cmd != RTM_DELETE)) {
		/* don't know what to do with this */
		return;
	}

	if(ifa->ifa_addr == NULL) {
		return;
	}
	if ((ifa->ifa_addr->sa_family != AF_INET) &&
	    (ifa->ifa_addr->sa_family != AF_INET6)
		) {
		/* non inet/inet6 skip */
		return;
	}
	if(ifa->ifa_addr->sa_family == AF_INET6) {
		ifa6 = (struct in6_ifaddr *)ifa;
		ifa_flags = ifa6->ia6_flags;
		if (IN6_IS_ADDR_UNSPECIFIED(&((struct sockaddr_in6 *)ifa->ifa_addr)->sin6_addr)) {
			/* skip unspecifed addresses */
			return;
		}

	} else if (ifa->ifa_addr->sa_family == AF_INET) {
		if (((struct sockaddr_in *)ifa->ifa_addr)->sin_addr.s_addr == 0) {
			return;
		}
	}

	if (sctp_is_desired_interface_type(ifa) == 0) {
		/* non desired type */
		return;
	}
	if(cmd == RTM_ADD) {
		ifap = sctp_add_addr_to_vrf(SCTP_DEFAULT_VRFID, (void *)ifa->ifa_ifp,
					    ifa->ifa_ifp->if_index, ifa->ifa_ifp->if_type,
#ifdef __APPLE__
		                            ifa->ifa_ifp->if_name,
#else
		                            ifa->ifa_ifp->if_xname,
#endif
					    (void *)ifa, ifa->ifa_addr, ifa_flags, 1);
		
	} else if (cmd == RTM_DELETE) {

		sctp_del_addr_from_vrf(SCTP_DEFAULT_VRFID, ifa->ifa_addr, ifa->ifa_ifp->if_index);
		/* We don't bump refcount here so when it completes
		 * the final delete will happen.
		 */
 	}
}

struct mbuf *
sctp_get_mbuf_for_msg(unsigned int space_needed, int want_header, 
		      int how, int allonebuf, int type)
{
	struct mbuf *m = NULL;
#if defined(__FreeBSD__) && __FreeBSD_version > 602000
	m =  m_getm2(NULL, space_needed, how, type, want_header ? M_PKTHDR : 0);
	if(m == NULL) {
		/* bad, no memory */
		return(m);
	}
	if (allonebuf) {
		int siz;
		if(SCTP_BUF_IS_EXTENDED(m)) {
			siz = SCTP_BUF_EXTEND_SIZE(m);
		} else {
			if(want_header)
				siz = MHLEN;
			else
				siz = MLEN;
		}
		if (siz < space_needed) {
			m_freem(m);
			return (NULL);
		}
	}
	if(SCTP_BUF_NEXT(m)) {
		sctp_m_freem( SCTP_BUF_NEXT(m));
		SCTP_BUF_NEXT(m) = NULL;
	}
#ifdef SCTP_MBUF_LOGGING
	if(SCTP_BUF_IS_EXTENDED(m)) {
		sctp_log_mb(m, SCTP_MBUF_IALLOC);
	}
#endif
#else
#if defined(__FreeBSD__) && __FreeBSD_version >= 601000
	int aloc_size;
	int index=0;
#endif
	int mbuf_threshold;
	if (want_header) {
		MGETHDR(m, how, type);
	} else {
		MGET(m, how, type);
	}
	if (m == NULL) {
		return (NULL);
	}
	if(allonebuf == 0)
		mbuf_threshold = sctp_mbuf_threshold_count;
	else
		mbuf_threshold = 1;


	if (space_needed > (((mbuf_threshold - 1) * MLEN) + MHLEN)) {
#if defined(__FreeBSD__) && __FreeBSD_version >= 601000
	try_again:
		index = 4;
		if(space_needed <= MCLBYTES){ 
			aloc_size = MCLBYTES;
		} else {
			aloc_size = MJUMPAGESIZE;
			index = 5;
		}
		m_cljget(m, how, aloc_size);
		if (m == NULL) {
			return (NULL);
		}
		if (SCTP_BUF_IS_EXTENDED(m) == 0) {
			if((aloc_size != MCLBYTES) &&
			   (allonebuf == 0)){
				aloc_size -= 10;
				goto try_again;
			}
			sctp_m_freem(m);
			return (NULL);
		} 
#else
		MCLGET(m, how);
		if (m == NULL) {
			return (NULL);
		}
		if (SCTP_BUF_IS_EXTENDED(m) == 0) {
			sctp_m_freem(m);
			return (NULL);
		}
#endif
	}
	SCTP_BUF_LEN(m) = 0;
	SCTP_BUF_NEXT(m) = SCTP_BUF_NEXT_PKT(m) = NULL;
#ifdef SCTP_MBUF_LOGGING
	if(SCTP_BUF_IS_EXTENDED(m)) {
		sctp_log_mb(m, SCTP_MBUF_IALLOC);
	}
#endif
#endif
	return (m);
}


#ifdef SCTP_PACKET_LOGGING

int packet_log_pos=0;
int packet_log_start=0;
int packet_log_end=0;
int packet_log_old_end=SCTP_PACKET_LOG_SIZE;
int packet_log_wrapped = 0;
uint8_t packet_log_buffer[SCTP_PACKET_LOG_SIZE];


void
sctp_packet_log(struct mbuf *m, int length)
{
	int *lenat, needed, thisone;
	void *copyto;
	uint32_t *tick_tock;
	int total_len, spare;
	total_len = SCTP_SIZE32((length + (2 * sizeof(int))));
	/* Log a packet to the buffer. */
	if (total_len> SCTP_PACKET_LOG_SIZE) {
		/* Can't log this packet I have not a buffer big enough */
		return;
	}
	printf("Put in %d bytes -pkt log\n", total_len);
	if((SCTP_PACKET_LOG_SIZE - packet_log_end) <= total_len) {
		/* it won't fit on the end. 
		 * We must go back to the beginning.
		 * To do this we go back and cahnge packet_log_start.
		 */
		printf("Won't fit in end:%d to eob\n", packet_log_end);
		lenat = (int *)packet_log_buffer;
		if(packet_log_start > packet_log_end) {
			/* calculate the head room */
			spare = (int)((caddr_t)&packet_log_buffer[packet_log_start] -
				      (caddr_t)&packet_log_buffer);
		} else {
			spare = 0;
		}
		printf("spare is %d\n", spare);
		needed = total_len - spare;
		packet_log_wrapped = 1;
		packet_log_old_end = packet_log_end;
		/* Now update the start */
		while (needed > 0) {
			thisone = (*(int *)(&packet_log_buffer[packet_log_start]));
			needed -= thisone;
			/* move to next one */
			printf("Move start up from %d by %d\n",
			       packet_log_start, thisone);
			packet_log_start += thisone;
			if(packet_log_start >= packet_log_old_end) {
				/* the front one is the start now */
				printf("at old end start at beginning now\n");
				packet_log_start = 0;
				break;
			}
		} 
	} else {
		printf("append at the end %d\n", packet_log_end);
		lenat = (int *)&packet_log_buffer[packet_log_end];
		if (packet_log_start > packet_log_end) {
			printf("Start:%d is larger\n", packet_log_start);
			if ((packet_log_end + total_len) > packet_log_start) {
				needed = total_len - ((packet_log_start - packet_log_end));
				printf("Need to move forward start by at least %d bytes\n", needed);
				while (needed > 0) {
					thisone = (*(int *)(&packet_log_buffer[packet_log_start]));
					needed -= thisone;
					/* move to next one */
					printf("Start moves up from %d by %d 2\n", packet_log_start, thisone);
					packet_log_start += thisone;
					if(packet_log_wrapped && (packet_log_start >= packet_log_old_end) ){
						packet_log_start = 0;
						printf("wrapped to top\n");
						break;
					}
				}
			}
		}
	}
	copyto = (void *)lenat;

	packet_log_end = (((caddr_t)copyto + total_len) - (caddr_t)packet_log_buffer);
	lenat = (int *)copyto;
	*lenat = total_len;
	lenat++;
	tick_tock = (uint32_t *)lenat;
	*tick_tock = sctp_get_tick_count();
	tick_tock++;
	copyto = (void *)tick_tock;
	m_copydata(m, 0, length,(caddr_t)copyto);
}


int
sctp_copy_out_packet_log(uint8_t *target , int length)
{
	/* We wind through the packet log starting at
	 * start copying up to length bytes out.
	 * We return the number of bytes copied.
	 */
	int tocopy, this_copy, copied=0;
	void *at;
	tocopy = length;
	if(packet_log_start == packet_log_end) {
		/* no data */
		return (0);
	}
	if (packet_log_start > packet_log_end) {
		/* we have a wrapped buffer, we
		 * must copy from start to the old end.
		 * Then copy from the top of the buffer
		 * to the end.
		 */
		at = (void *)&packet_log_buffer[packet_log_start];
		this_copy = min(tocopy, (packet_log_old_end - packet_log_start));
		memcpy(target, at, this_copy);
		tocopy -= this_copy;
		copied += this_copy;
		if(tocopy == 0)
			return (copied);
		this_copy = min(tocopy, packet_log_end);
		at = (void *)&packet_log_buffer;
		memcpy(&target[copied], at, this_copy);
		copied += this_copy;
		return(copied);
	} else {
		/* we have one contiguous buffer */
		at = (void *)&packet_log_buffer;
		this_copy = min(length, packet_log_end);
		memcpy(target, at, this_copy);
		return (this_copy);
	}
}

#endif
