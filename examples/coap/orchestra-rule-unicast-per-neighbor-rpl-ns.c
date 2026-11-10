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
// static void
// add_uc_link(const linkaddr_t *linkaddr)
// {
//   if (linkaddr != NULL)
//   {
//     uint16_t timeslot = get_node_timeslot(linkaddr);

//     /* Add a Tx link to the neighbor; do not replace any existing links
//      * at that cell. The channel offset here does not matter:
//      * select_packet() always sets the right channel offset per packet. */
//     tsch_schedule_add_link(sf_unicast,
//                            LINK_OPTION_SHARED | LINK_OPTION_TX,
//                            LINK_TYPE_NORMAL, &tsch_broadcast_address,
//                            timeslot, 0, 0);
//   }
// }

static void
add_uc_link(const linkaddr_t *linkaddr)
{
  uint16_t timeslot;
  struct tsch_link *l;

  /* Check NULL pointer and null address */
  if (linkaddr == NULL ||
      linkaddr_cmp(linkaddr, &linkaddr_null))
  {
    return;
  }

  if (sf_unicast == NULL)
  {
    return;
  }

  timeslot = get_node_timeslot(linkaddr);

  if (timeslot == 0xffff)
  {
    return;
  }

  /*
   * Scan the complete unicast slotframe.
   * Check whether the required Orchestra TX link
   * already exists at this timeslot.
   */
  if (!tsch_is_locked())
  {
    for (l = list_head(sf_unicast->links_list);
         l != NULL;
         l = list_item_next(l))
    {

      if (l->timeslot == timeslot &&
          l->channel_offset == 0 &&
          l->link_options == (LINK_OPTION_SHARED | LINK_OPTION_TX) &&
          l->link_type == LINK_TYPE_NORMAL &&
          linkaddr_cmp(&l->addr, &tsch_broadcast_address))
      {

        /* Required TX link already exists */
        return;
      }
    }
  }
  else
  {
    return;
  }

  /* No matching TX link found — add one */
  tsch_schedule_add_link(sf_unicast,
                         LINK_OPTION_SHARED | LINK_OPTION_TX,
                         LINK_TYPE_NORMAL, &tsch_broadcast_address,
                         timeslot, 0, 0);
}

/*---------------------------------------------------------------------------*/
// static void
// remove_uc_link(const linkaddr_t *linkaddr)
// {
//   if (linkaddr != NULL)
//   {
//     uint16_t timeslot = get_node_timeslot(linkaddr);
//     tsch_schedule_remove_link_by_offsets(sf_unicast, timeslot, 0);
//     tsch_queue_free_packets_to(linkaddr);
//   }
// }

static void
remove_uc_link(const linkaddr_t *linkaddr)
{
  uint16_t timeslot;

  /* Check both NULL pointer and null address */
  if (linkaddr == NULL ||
      linkaddr_cmp(linkaddr, &linkaddr_null))
  {
    return;
  }

  timeslot = get_node_timeslot(linkaddr);

  LOG_INFO("REMOVE_REQUEST ts=%u addr=", timeslot);
  LOG_INFO_LLADDR(linkaddr);
  LOG_INFO_("\n");

  /* Keep existing queue-flush behaviour */
  tsch_queue_free_packets_to(linkaddr);

  /* Guard 1: Never delete our own RX slot */
  if (timeslot == get_node_timeslot(&linkaddr_node_addr))
  {

    LOG_INFO("KEEP: own RX slot ts=%u addr=", timeslot);
    LOG_INFO_LLADDR(linkaddr);
    LOG_INFO_("\n");

    return;
  }

  /* Guard 2: Never delete current parent's slot */
  if (!linkaddr_cmp(&orchestra_parent_linkaddr, &linkaddr_null) &&
      timeslot == get_node_timeslot(&orchestra_parent_linkaddr))
  {

    LOG_INFO("KEEP: current parent slot ts=%u addr=", timeslot);
    LOG_INFO_LLADDR(linkaddr);
    LOG_INFO_("\n");
    return;
  }

  /* Guard 3: Check remaining DS6 neighbors */
  {
    uip_ds6_nbr_t *nbr;

    for (nbr = uip_ds6_nbr_head();
         nbr != NULL;
         nbr = uip_ds6_nbr_next(nbr))
    {

      const linkaddr_t *nbr_lladdr =
          (const linkaddr_t *)uip_ds6_nbr_get_ll(nbr);

      if (nbr_lladdr != NULL &&
          !linkaddr_cmp(nbr_lladdr, &linkaddr_null) &&
          !linkaddr_cmp(nbr_lladdr, linkaddr) &&
          get_node_timeslot(nbr_lladdr) == timeslot)
      {
        LOG_INFO("KEEP: shared slot ts=%u other neighbor=", timeslot);
        LOG_INFO_LLADDR(nbr_lladdr);
        LOG_INFO_("\n");
        return;
      }
    }
  }

  LOG_INFO("DELETE: all matching links ts=%u removed neighbor=", timeslot);
  LOG_INFO_LLADDR(linkaddr);
  LOG_INFO_("\n");

  /* Remove ALL matching entries, including duplicates */
  tsch_schedule_remove_link_by_offsets(sf_unicast, timeslot, 0);
}

// /*---------------------------------------------------------------------------*/
static void
neighbor_updated(const linkaddr_t *linkaddr, uint8_t is_added)
{
  if (is_added)
  {
    add_uc_link(linkaddr);
  }
  else
  {
    if (linkaddr != NULL)
    {
      LOG_INFO("REMOVE_SOURCE: ds6_neighbor_removed addr=");
      LOG_INFO_LLADDR(linkaddr);
      LOG_INFO_("\n");
    }
    remove_uc_link(linkaddr);
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
// static void
// new_time_source(const struct tsch_neighbor *old, const struct tsch_neighbor *new)
// {
//   if (new != old)
//   {
//     const linkaddr_t *old_addr = tsch_queue_get_nbr_address(old);
//     const linkaddr_t *new_addr = tsch_queue_get_nbr_address(new);
//     // remove_uc_link(old_addr);

//     if (uip_ds6_nbr_ll_lookup((const uip_lladdr_t *)old_addr) == NULL)
//     {
//       LOG_INFO("OLD_TS_NOT_IN_DS6: removing old link\n");
//       remove_uc_link(old_addr);
//     }
//     else
//     {
//       LOG_INFO("OLD_TS_STILL_IN_DS6: keeping old link\n");
//     }
//     add_uc_link(new_addr);
//   }
// }

static void
new_time_source(const struct tsch_neighbor *old,
                const struct tsch_neighbor *new)
{
  if (new != old)
  {
    const linkaddr_t *old_addr =
        old != NULL ? tsch_queue_get_nbr_address(old) : NULL;

    const linkaddr_t *new_addr =
        new != NULL ? tsch_queue_get_nbr_address(new) : NULL;

    /* Update parent BEFORE removing old parent's link */
    if (new_addr != NULL &&
        !linkaddr_cmp(new_addr, &linkaddr_null))
    {
      linkaddr_copy(&orchestra_parent_linkaddr, new_addr);
    }
    else
    {
      linkaddr_copy(&orchestra_parent_linkaddr, &linkaddr_null);
    }
    if (old_addr != NULL &&
        !linkaddr_cmp(old_addr, &linkaddr_null))
    {
      LOG_INFO("REMOVE_SOURCE: time_source_change old=");
      LOG_INFO_LLADDR(old_addr);
      LOG_INFO_("\n");
    }
    remove_uc_link(old_addr);

    if (new_addr != NULL &&
        !linkaddr_cmp(new_addr, &linkaddr_null))
    {
      add_uc_link(new_addr);
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
