// DHCP Library v0.3 - April 25, 2009
// Author: Jordan Terrell - blog.jordanterrell.com

#ifndef AutoIP_h
#define AutoIP_h

/* 169.254.0.0 */
#define AUTOIP_NET              0xA9FE0000
/* 169.254.1.0 */
#define AUTOIP_RANGE_START      (AUTOIP_NET | 0x0100)
/* 169.254.254.255 */
#define AUTOIP_RANGE_END        (AUTOIP_NET | 0xFEFF)

/* RFC 3927 Constants */
#define PROBE_WAIT              1   /* second   (initial random delay)                 */
#define PROBE_MIN               1   /* second   (minimum delay till repeated probe)    */
#define PROBE_MAX               2   /* seconds  (maximum delay till repeated probe)    */
#define PROBE_NUM               3   /*          (number of probe packets)              */
#define ANNOUNCE_NUM            2   /*          (number of announcement packets)       */
#define ANNOUNCE_INTERVAL       1   /* seconds  (time between announcement packets)    */
#define ANNOUNCE_WAIT           1   /* seconds  (delay before announcing)              */
#define MAX_CONFLICTS           10  /*          (max conflicts before rate limiting)   */
#define RATE_LIMIT_INTERVAL     60  /* seconds  (delay between successive attempts)    */
#define DEFEND_INTERVAL         10  /* seconds  (min. wait between defensive ARPs)     */

/** AutoIP Timing */
#define AUTOIP_TMR_INTERVAL     100
#define AUTOIP_TICKS_PER_SECOND (1000 / AUTOIP_TMR_INTERVAL)

/* AUTOIP state machine. */
#define AUTOIP_STATE_OFF		0
#define AUTOIP_STATE_PROBING	1
#define AUTOIP_STATE_ANNOUNCING	2
#define AUTOIP_STATE_BOUND		3

/* UDP port number for AUTOIP */
#define	AUTOIP_PORT		219

#define ETH_HWADDR_LEN    6

#define IANA_HWTYPE_ETHERNET	1

#define ETHTYPE_IP		0x0800U
#define ETHTYPE_ARP 	0x0806U
#define ETHTYPE_LLDP	0x88CCU

#ifdef PACK_STRUCT_USE_INCLUDES
#  include "arch/bpstruct.h"
#endif
PACK_STRUCT_BEGIN
/** An Ethernet MAC address */
struct eth_addr {
  PACK_STRUCT_FLD_8(u8_t addr[ETH_HWADDR_LEN]);
} PACK_STRUCT_STRUCT;
PACK_STRUCT_END
#ifdef PACK_STRUCT_USE_INCLUDES
#  include "arch/epstruct.h"
#endif

/**
 * struct ip4_addr_wordaligned is used in the definition of the ARP packet format in
 * order to support compilers that don't have structure packing.
 */
#ifdef PACK_STRUCT_USE_INCLUDES
#  include "arch/bpstruct.h"
#endif
PACK_STRUCT_BEGIN
struct ip4_addr_wordaligned {
  PACK_STRUCT_FIELD(u16_t addrw[2]);
} PACK_STRUCT_STRUCT;
PACK_STRUCT_END
#ifdef PACK_STRUCT_USE_INCLUDES
#  include "arch/epstruct.h"
#endif

#ifdef PACK_STRUCT_USE_INCLUDES
#  include "arch/bpstruct.h"
#endif
PACK_STRUCT_BEGIN
/** the ARP message, see RFC 826 ("Packet format") */
struct etharp_hdr {
  PACK_STRUCT_FIELD(u16_t hwtype);
  PACK_STRUCT_FIELD(u16_t proto);
  PACK_STRUCT_FLD_8(u8_t  hwlen);
  PACK_STRUCT_FLD_8(u8_t  protolen);
  PACK_STRUCT_FIELD(u16_t opcode);
  PACK_STRUCT_FLD_S(struct eth_addr shwaddr);
  PACK_STRUCT_FLD_S(struct ip4_addr_wordaligned sipaddr);
  PACK_STRUCT_FLD_S(struct eth_addr dhwaddr);
  PACK_STRUCT_FLD_S(struct ip4_addr_wordaligned dipaddr);
} PACK_STRUCT_STRUCT;
PACK_STRUCT_END
#ifdef PACK_STRUCT_USE_INCLUDES
#  include "arch/epstruct.h"
#endif

#define SIZEOF_ETHARP_HDR 28

#define ETH_PAD_SIZE	0

#ifdef PACK_STRUCT_USE_INCLUDES
#  include "arch/bpstruct.h"
#endif
PACK_STRUCT_BEGIN
/** Ethernet header */
struct eth_hdr {
#if ETH_PAD_SIZE
  PACK_STRUCT_FLD_8(u8_t padding[ETH_PAD_SIZE]);
#endif
  PACK_STRUCT_FLD_S(struct eth_addr dest);
  PACK_STRUCT_FLD_S(struct eth_addr src);
  PACK_STRUCT_FIELD(u16_t type);
} PACK_STRUCT_STRUCT;
PACK_STRUCT_END
#ifdef PACK_STRUCT_USE_INCLUDES
#  include "arch/epstruct.h"
#endif

#define SIZEOF_ETH_HDR (14 + ETH_PAD_SIZE)

/* ARP message types (opcodes) */
#define ARP_REQUEST	1
#define ARP_REPLY	2

/* ARP request sub types */
#define ARP_PROBE		1
#define ARP_ANNOUNCE		2

#endif
