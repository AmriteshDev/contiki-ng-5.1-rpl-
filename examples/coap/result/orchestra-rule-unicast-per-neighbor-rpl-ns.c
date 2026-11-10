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
/**
 * \file
 *         Orchestra: a slotframe dedicated to unicast data transmission. Designed primarily
 *         for RPL non-storing mode but would work with any mode-of-operation.
 *         Uses uIPv6 DS6 neighhbor tables to obtain knowledge of the children.
 *         Only supports the receiver based mode, and works as follows:
 *           Nodes listen at a timeslot defined as hash(MAC) % ORCHESTRA_SB_UNICAST_PERIOD
 *           Nodes transmit at: for any neighbor, hash(nbr.MAC) % ORCHESTRA_SB_UNICAST_PERIOD
 *
 * \author Simon Duquennoy <simon.duquennoy@inria.fr>
 *         Atis Elsts <atis.elsts@edi.lv>
 */

#include "contiki.h"
#include "lib/list.h"
#include "orchestra.h"
#include "net/ipv6/uip-ds6-route.h"
#include "net/packetbuf.h"

#include "net/ipv6/uip-ds6-nbr.h"

#include "sys/log.h"

#define LOG_MODULE "OrchUC"
#define LOG_LEVEL LOG_LEVEL_INFO

static uint16_t slotframe_handle = 0;
static struct tsch_slotframe *sf_unicast;

/*---------------------------------------------------------------------------*/
static uint16_t
get_node_timeslot(const linkaddr_t *addr)
{
  if (addr != NULL && ORCHESTRA_UNICAST_PERIOD > 0)
  {
    return ORCHESTRA_LINKADDR_HASH(addr) % ORCHESTRA_UNICAST_PERIOD;
  }
  else
  {
    return 0xffff;
  }
}
/*---------------------------------------------------------------------------*/
static uint16_t
get_node_channel_offset(const linkaddr_t *addr)
{
  if (addr != NULL && ORCHESTRA_UNICAST_MAX_CHANNEL_OFFSET >= ORCHESTRA_UNICAST_MIN_CHANNEL_OFFSET)
  {
    return ORCHESTRA_LINKADDR_HASH(addr) % (ORCHESTRA_UNICAST_MAX_CHANNEL_OFFSET - ORCHESTRA_UNICAST_MIN_CHANNEL_OFFSET + 1) + ORCHESTRA_UNICAST_MIN_CHANNEL_OFFSET;
  }
  else
  {
    return 0xffff;
  }
}
/*---------------------------------------------------------------------------*/
/* A slot is shared by every neighbor that hashes to that timeslot. */
static int
address_matches(const linkaddr_t *candidate, const linkaddr_t *target,
                int match_timeslot)
{
  if (candidate == NULL || target == NULL ||
      linkaddr_cmp(candidate, &linkaddr_null) ||
      linkaddr_cmp(target, &linkaddr_null))
  {
    return 0;
  }
  return match_timeslot
             ? get_node_timeslot(candidate) == get_node_timeslot(target)
             : linkaddr_cmp(candidate, target);
}

/* DS6 callbacks run after the link-layer entry is added or removed, and
 * TSCH updates its current time source before invoking its callback. */
static int
address_is_in_use(const linkaddr_t *addr, int match_timeslot)
{
  const struct tsch_neighbor *time_source;
  const linkaddr_t *time_source_addr;
  uip_ds6_nbr_t *nbr;

  if (addr == NULL || linkaddr_cmp(addr, &linkaddr_null))
  {
    return 0;
  }

  time_source = tsch_queue_get_time_source();
  time_source_addr = time_source != NULL
                         ? tsch_queue_get_nbr_address(time_source)
                         : NULL;
  if (address_matches(time_source_addr, addr, match_timeslot))
  {
    return 1;
  }

  for (nbr = uip_ds6_nbr_head(); nbr != NULL; nbr = uip_ds6_nbr_next(nbr))
  {
    const linkaddr_t *nbr_addr = (const linkaddr_t *)uip_ds6_nbr_get_ll(nbr);
    if (address_matches(nbr_addr, addr, match_timeslot))
    {
      return 1;
    }
  }
  return 0;
}

/* Reconcile only this rule's TX links. Never remove an RX link, even if it
 * shares both timeslot and channel offset with a TX link. */
static void
reconcile_uc_link(const linkaddr_t *addr)
{
  struct tsch_link *link;
  struct tsch_link *next;
  uint16_t timeslot;
  int slot_needed;
  int tx_link_found = 0;

  if (sf_unicast == NULL || addr == NULL ||
      linkaddr_cmp(addr, &linkaddr_null))
  {
    return;
  }
  timeslot = get_node_timeslot(addr);
  if (timeslot == 0xffff)
  {
    return;
  }
  if (tsch_is_locked())
  {
    LOG_WARN("Cannot reconcile slot %u while TSCH is locked\n", timeslot);
    return;
  }
  slot_needed = address_is_in_use(addr, 1);

  for (link = list_head(sf_unicast->links_list); link != NULL; link = next)
  {
    next = list_item_next(link);
    if (link->timeslot == timeslot && link->channel_offset == 0 &&
        link->link_type == LINK_TYPE_NORMAL &&
        link->link_options == (LINK_OPTION_SHARED | LINK_OPTION_TX) &&
        linkaddr_cmp(&link->addr, &tsch_broadcast_address))
    {
      if (slot_needed)
      {
        tx_link_found = 1;
      }
      else
      {
        if (!tsch_schedule_remove_link(sf_unicast, link))
        {
          LOG_ERR("Cannot remove TX link at slot %u\n", timeslot);
        }
      }
    }
  }

  if (slot_needed && !tx_link_found)
  {
    /* select_packet() supplies the destination's channel offset. */
    if (tsch_schedule_add_link(sf_unicast, LINK_OPTION_SHARED | LINK_OPTION_TX,
                               LINK_TYPE_NORMAL, &tsch_broadcast_address,
                               timeslot, 0, 0) == NULL)
    {
      LOG_ERR("Cannot add TX link at slot %u\n", timeslot);
    }
  }
}

static void
neighbor_updated(const linkaddr_t *linkaddr, uint8_t is_added)
{
  if (linkaddr == NULL || linkaddr_cmp(linkaddr, &linkaddr_null))
  {
    return;
  }
  reconcile_uc_link(linkaddr);
  /* Avoid discarding packets if this LL address is still in DS6 (including
   * another IPv6 address for the same LL address) or is the time source. */
  if (!is_added && !address_is_in_use(linkaddr, 0))
  {
    tsch_queue_free_packets_to(linkaddr);
  }
}
/*---------------------------------------------------------------------------*/
static int
select_packet(uint16_t *slotframe, uint16_t *timeslot, uint16_t *channel_offset)
{
  /* Select data packets we have a unicast link to */
  const linkaddr_t *dest = packetbuf_addr(PACKETBUF_ADDR_RECEIVER);
  if (packetbuf_attr(PACKETBUF_ATTR_FRAME_TYPE) == FRAME802154_DATAFRAME && !orchestra_is_root_schedule_active(dest) && !linkaddr_cmp(dest, &linkaddr_null))
  {
    if (slotframe != NULL)
    {
      *slotframe = slotframe_handle;
    }
    if (timeslot != NULL)
    {
      *timeslot = get_node_timeslot(dest);
    }
    /* set per-packet channel offset */
    if (channel_offset != NULL)
    {
      *channel_offset = get_node_channel_offset(dest);
    }
    return 1;
  }
  return 0;
}
/*---------------------------------------------------------------------------*/
static void
new_time_source(const struct tsch_neighbor *old,
                const struct tsch_neighbor *new)
{
  if (new != old)
  {
    const linkaddr_t *old_addr = old != NULL
                                     ? tsch_queue_get_nbr_address(old)
                                     : NULL;
    const linkaddr_t *new_addr = new != NULL
                                     ? tsch_queue_get_nbr_address(new)
                                     : NULL;

    if (new_addr != NULL && !linkaddr_cmp(new_addr, &linkaddr_null))
    {
      linkaddr_copy(&orchestra_parent_linkaddr, new_addr);
    }
    else
    {
      linkaddr_copy(&orchestra_parent_linkaddr, &linkaddr_null);
    }
    reconcile_uc_link(old_addr);
    reconcile_uc_link(new_addr);
    if (old_addr != NULL && !address_is_in_use(old_addr, 0))
    {
      tsch_queue_free_packets_to(old_addr);
    }
  }
}
/*---------------------------------------------------------------------------*/
static void
init(uint16_t sf_handle)
{
  uint16_t rx_timeslot;
  linkaddr_t *local_addr = &linkaddr_node_addr;

  slotframe_handle = sf_handle;
  /* Slotframe for unicast transmissions */
  sf_unicast = tsch_schedule_add_slotframe(slotframe_handle, ORCHESTRA_UNICAST_PERIOD);
  rx_timeslot = get_node_timeslot(local_addr);
  /* Add a Rx link at our own timeslot. */
  tsch_schedule_add_link(sf_unicast,
                         LINK_OPTION_RX,
                         LINK_TYPE_NORMAL, &tsch_broadcast_address,
                         rx_timeslot, get_node_channel_offset(local_addr), 1);
}
/*---------------------------------------------------------------------------*/
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