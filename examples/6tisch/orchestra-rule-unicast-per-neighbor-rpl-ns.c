/*
 * Copyright (c) 2016, Inria.
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
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */
/*
 * Orchestra unicast rule for RPL non-storing mode, receiver-based: a node listens in the slot
 * (last link-address byte) mod ORCHESTRA_UNICAST_PERIOD and sends to a neighbour in that neighbour's slot.
 *
 * Modified copy of os/services/orchestra/orchestra-rule-unicast-per-neighbor-rpl-ns.c of Contiki-NG
 * (commit 6a35472; authors of the original: Simon Duquennoy, Atis Elsts). The core file is not edited; project-conf.h
 * selects this rule. Differences from the original:
 *
 * 1. Reference count of the shared TX cell. All neighbours that hash to one slot share one TX cell. The original
 *    removes that cell whenever any neighbour (or the old time source) leaves, even if another current neighbour
 *    still hashes to the same slot. A lost downlink acknowledgement of a witness report was traced to this: a relay
 *    lost its cell when its parent changed while a child in the same slot still needed it. Here the cell is removed
 *    only when no remaining neighbour, and not the current time source, hashes to the slot ([CADET-ORCH-REFCOUNT]).
 * 2. A data packet whose slot has no TX cell is not pinned to that slot: the next rule, the default common shared
 *    cell, carries it ([CADET-ORCH-FALLBACK]). The original always pins the packet.
 * 3. The receive slot of the node is logged at boot ([CADET-ORCH-SLOT]).
 */
#include "contiki.h"
#include "orchestra.h"
#include "net/ipv6/uip-ds6-route.h"
#include "net/ipv6/uip-ds6-nbr.h"
#include "net/mac/tsch/tsch.h"
#include "net/packetbuf.h"

#define LOG_MODULE "ORCH-TEST"
#define LOG_LEVEL LOG_LEVEL_NONE
#include "sys/log.h"

static uint16_t slotframe_handle = 0;
static struct tsch_slotframe *sf_unicast;
static uint32_t refcount_kept;
static uint32_t refcount_removed;

static uint16_t
get_node_timeslot(const linkaddr_t *addr)
{
  if (addr != NULL && ORCHESTRA_UNICAST_PERIOD > 0)
  {
    return ORCHESTRA_LINKADDR_HASH(addr) % ORCHESTRA_UNICAST_PERIOD;
  }
  return 0xffff;
}

static uint16_t
get_node_channel_offset(const linkaddr_t *addr)
{
  if (addr != NULL && ORCHESTRA_UNICAST_MAX_CHANNEL_OFFSET >= ORCHESTRA_UNICAST_MIN_CHANNEL_OFFSET)
  {
    return ORCHESTRA_LINKADDR_HASH(addr) % (ORCHESTRA_UNICAST_MAX_CHANNEL_OFFSET - ORCHESTRA_UNICAST_MIN_CHANNEL_OFFSET + 1) + ORCHESTRA_UNICAST_MIN_CHANNEL_OFFSET;
  }
  return 0xffff;
}

// static void
// add_uc_link(const linkaddr_t *linkaddr)
// {
//   if (linkaddr != NULL && !linkaddr_cmp(linkaddr, &linkaddr_null))
//   {
//     tsch_schedule_add_link(sf_unicast,
//                            LINK_OPTION_SHARED | LINK_OPTION_TX,
//                            LINK_TYPE_NORMAL, &tsch_broadcast_address,
//                            get_node_timeslot(linkaddr), 0, 0);
//   }
// }

static void
add_uc_link(const linkaddr_t *linkaddr)
{
  uint16_t timeslot;

  if (sf_unicast == NULL ||
      linkaddr == NULL ||
      linkaddr_cmp(linkaddr, &linkaddr_null))
  {
    return;
  }

  timeslot = get_node_timeslot(linkaddr);

  if (timeslot == 0xffff)
  {
    return;
  }

  // if (tsch_schedule_get_link_by_timeslot(sf_unicast, timeslot) != NULL)
  // {
  //   return;
  // }

  // tsch_schedule_add_link(sf_unicast,
  //                        LINK_OPTION_SHARED | LINK_OPTION_TX,
  //                        LINK_TYPE_NORMAL,
  //                        &tsch_broadcast_address,
  //                        timeslot, 0, 0);

  struct tsch_link *existing;

  existing = tsch_schedule_get_link_by_timeslot(sf_unicast, timeslot);

  if (existing != NULL)
  {
    LOG_INFO("[ORCH-ADD] addr_last=%u slot=%u action=SKIP_EXISTING tx=%u rx=%u\n",
             (unsigned)linkaddr->u8[LINKADDR_SIZE - 1],
             (unsigned)timeslot,
             (unsigned)((existing->link_options & LINK_OPTION_TX) != 0),
             (unsigned)((existing->link_options & LINK_OPTION_RX) != 0));
    return;
  }

  LOG_INFO("[ORCH-ADD] addr_last=%u slot=%u action=ADD\n",
           (unsigned)linkaddr->u8[LINKADDR_SIZE - 1],
           (unsigned)timeslot);

  tsch_schedule_add_link(sf_unicast,
                         LINK_OPTION_SHARED | LINK_OPTION_TX,
                         LINK_TYPE_NORMAL,
                         &tsch_broadcast_address,
                         timeslot, 0, 0);
}

/* Last address byte of a remaining holder of this slot, 0 if there is none. The departing address itself is
 * excluded (it may still be in the DS6 table when the old parent is replaced). */
static int
slot_in_use(uint16_t timeslot, const linkaddr_t *departing)
{
  uip_ds6_nbr_t *nbr;
  struct tsch_neighbor *ts;
  const linkaddr_t *ts_addr;

  // if (timeslot == get_node_timeslot(&linkaddr_node_addr))
  // {
  //   return 1;
  // }

  ts = tsch_queue_get_time_source();
  ts_addr = (ts != NULL) ? tsch_queue_get_nbr_address(ts) : NULL;
  if (ts_addr != NULL &&
      !linkaddr_cmp(ts_addr, &linkaddr_null) &&
      !linkaddr_cmp(ts_addr, departing) &&
      get_node_timeslot(ts_addr) == timeslot)
  {
    LOG_INFO("[ORCH-HOLDER] slot=%u action=KEEP reason=TIME_SOURCE holder=%u\n",
             (unsigned)timeslot,
             (unsigned)ts_addr->u8[LINKADDR_SIZE - 1]);
    return 1;
  }

  for (nbr = uip_ds6_nbr_head(); nbr != NULL; nbr = uip_ds6_nbr_next(nbr))
  {
    const linkaddr_t *ll = (const linkaddr_t *)uip_ds6_nbr_get_ll(nbr);
    if (ll != NULL &&
        !linkaddr_cmp(ll, &linkaddr_null) &&
        !linkaddr_cmp(ll, departing) &&
        get_node_timeslot(ll) == timeslot)
    {
      LOG_INFO("[ORCH-HOLDER] slot=%u action=KEEP reason=NEIGHBOR holder=%u\n",
               (unsigned)timeslot,
               (unsigned)ll->u8[LINKADDR_SIZE - 1]);
      return 1;
    }
  }
  return 0;
}
static void
remove_uc_link(const linkaddr_t *linkaddr)
{
  uint16_t timeslot;
  int in_use;
  // if (linkaddr == NULL || linkaddr_cmp(linkaddr, &linkaddr_null))
  // {
  //   return;
  // }
  // timeslot = get_node_timeslot(linkaddr);

  if (sf_unicast == NULL ||
      linkaddr == NULL ||
      linkaddr_cmp(linkaddr, &linkaddr_null))
  {
    return;
  }
  timeslot = get_node_timeslot(linkaddr);
  if (timeslot == 0xffff)
  {
    return;
  }

  in_use = slot_in_use(timeslot, linkaddr);
  if (!in_use)
  {

    // tsch_schedule_remove_link_by_offsets(sf_unicast, timeslot, 0);
    // refcount_removed++;
    //// new start
    // struct tsch_link *l = tsch_schedule_get_link_by_timeslot(sf_unicast, timeslot);
    // if (l != NULL && (l->link_options & LINK_OPTION_TX) &&
    //     !(l->link_options & LINK_OPTION_RX))
    // {
    //   tsch_schedule_remove_link(sf_unicast, l);
    //   refcount_removed++;
    // }

    struct tsch_link *l =
        tsch_schedule_get_link_by_timeslot(sf_unicast, timeslot);

    if (l != NULL &&
        (l->link_options & LINK_OPTION_TX) &&
        !(l->link_options & LINK_OPTION_RX))
    {

      tsch_schedule_remove_link(sf_unicast, l);
      refcount_removed++;

      LOG_INFO("[ORCH-REMOVE] departing=%u slot=%u action=REMOVED\n",
               (unsigned)linkaddr->u8[LINKADDR_SIZE - 1],
               (unsigned)timeslot);
    }
    else if (l != NULL)
    {

      LOG_INFO("[ORCH-REMOVE] departing=%u slot=%u action=SKIP_LINK tx=%u rx=%u\n",
               (unsigned)linkaddr->u8[LINKADDR_SIZE - 1],
               (unsigned)timeslot,
               (unsigned)((l->link_options & LINK_OPTION_TX) != 0),
               (unsigned)((l->link_options & LINK_OPTION_RX) != 0));
    }
    else
    {

      LOG_INFO("[ORCH-REMOVE] departing=%u slot=%u action=NO_LINK\n",
               (unsigned)linkaddr->u8[LINKADDR_SIZE - 1],
               (unsigned)timeslot);
    }

    //// new end
  }
  else
  {
    refcount_kept++;
    LOG_INFO("[ORCH-REMOVE] departing=%u slot=%u action=KEEP\n",
             (unsigned)linkaddr->u8[LINKADDR_SIZE - 1],
             (unsigned)timeslot);
  }
  // LOG_WARN("[CADET-ORCH-REFCOUNT] departing=%u slot=%u holder=%u action=%s kept=%lu removed=%lu\n",
  //          (unsigned)linkaddr->u8[LINKADDR_SIZE - 1], (unsigned)timeslot,
  //          (unsigned)holder, holder == 0 ? "REMOVED" : "KEPT",
  //          (unsigned long)refcount_kept, (unsigned long)refcount_removed);
  /* As in the original: queued packets to the departing neighbour are freed. */
  tsch_queue_free_packets_to(linkaddr);
}

static void
neighbor_updated(const linkaddr_t *linkaddr, uint8_t is_added)
{
  if (is_added)
  {
    add_uc_link(linkaddr);
  }
  else
  {
    remove_uc_link(linkaddr);
  }
}

static int
select_packet(uint16_t *slotframe, uint16_t *timeslot, uint16_t *channel_offset)
{
  const linkaddr_t *dest = packetbuf_addr(PACKETBUF_ADDR_RECEIVER);
  if (packetbuf_attr(PACKETBUF_ATTR_FRAME_TYPE) == FRAME802154_DATAFRAME && !orchestra_is_root_schedule_active(dest) && !linkaddr_cmp(dest, &linkaddr_null))
  {
    /* A packet pinned to a slot in which this node has no TX cell can never be sent and would hold a queue buffer
     * for good. Returning 0 lets the next rule, the default common shared cell, carry it, as for any traffic without
     * a dedicated cell. */
    struct tsch_link *l = sf_unicast != NULL
                              ? tsch_schedule_get_link_by_timeslot(sf_unicast, get_node_timeslot(dest))
                              : NULL;
    if (l == NULL || !(l->link_options & LINK_OPTION_TX))
    {

      static uint32_t no_cell_fallbacks;
      no_cell_fallbacks++;

      LOG_INFO("[ORCH-FALLBACK] dest_last=%u slot=%u reason=%s action=RETURN_0 count=%lu\n",
               (unsigned)dest->u8[LINKADDR_SIZE - 1],
               (unsigned)get_node_timeslot(dest),
               l == NULL ? "NO_LINK" : "NO_TX",
               (unsigned long)no_cell_fallbacks);

      return 0;
    }
    if (slotframe != NULL)
    {
      *slotframe = slotframe_handle;
    }
    if (timeslot != NULL)
    {
      *timeslot = get_node_timeslot(dest);
    }
    if (channel_offset != NULL)
    {
      *channel_offset = get_node_channel_offset(dest);
    }
    return 1;
  }
  return 0;
}

static void
new_time_source(const struct tsch_neighbor *old, const struct tsch_neighbor *new)
{
  if (new != old)
  {
    const linkaddr_t *old_addr = tsch_queue_get_nbr_address(old);
    const linkaddr_t *new_addr = tsch_queue_get_nbr_address(new);
    remove_uc_link(old_addr);
    add_uc_link(new_addr);
  }
}

static void
init(uint16_t sf_handle)
{
  linkaddr_t *local_addr = &linkaddr_node_addr;
  slotframe_handle = sf_handle;
  sf_unicast = tsch_schedule_add_slotframe(slotframe_handle, ORCHESTRA_UNICAST_PERIOD);
  tsch_schedule_add_link(sf_unicast, LINK_OPTION_RX, LINK_TYPE_NORMAL,
                         &tsch_broadcast_address, get_node_timeslot(local_addr),
                         get_node_channel_offset(local_addr), 1);
  LOG_INFO("[ORCH-INIT] local_last=%u rx_slot=%u period=%u sf=%s\n",
           (unsigned)local_addr->u8[LINKADDR_SIZE - 1],
           (unsigned)get_node_timeslot(local_addr),
           (unsigned)ORCHESTRA_UNICAST_PERIOD,
           sf_unicast != NULL ? "OK" : "NULL");
}

// struct orchestra_rule cadet_unicast_per_neighbor_rpl_ns_refcount = {
//     init,
//     new_time_source,
//     select_packet,
//     NULL,
//     NULL,
//     neighbor_updated,
//     NULL,
//     "cadet unicast per neighbor non-storing refcount",
//     ORCHESTRA_UNICAST_PERIOD,
// };

struct orchestra_rule unicast_per_neighbor_rpl_ns = {
    init,
    new_time_source,
    select_packet,
    NULL,
    NULL,
    neighbor_updated,
    NULL,
    "unicast per neighbor non-storing",
    ORCHESTRA_UNICAST_PERIOD,
};