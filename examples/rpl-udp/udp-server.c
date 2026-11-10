#include "contiki.h"
#include "net/routing/routing.h"
#include "net/netstack.h"
#include "net/ipv6/simple-udp.h"
#include <inttypes.h>
#include <stdio.h> /* Required for sscanf */

#include "net/queuebuf.h"
#include "sys/log.h"
#include "sys/node-id.h"
#include "sys/energest.h"

#define LOG_MODULE "SERVER"
#define LOG_LEVEL LOG_LEVEL_INFO

#define WITH_SERVER_REPLY 1
#define UDP_CLIENT_PORT 8765
#define UDP_SERVER_PORT 5678

static struct simple_udp_connection udp_conn;

/* 1. Global counter for PDR computation */
static uint32_t server_rx_count = 0;

PROCESS(udp_server_process, "UDP server");
AUTOSTART_PROCESSES(&udp_server_process);
/*---------------------------------------------------------------------------*/
static void
udp_rx_callback(struct simple_udp_connection *c,
                const uip_ipaddr_t *sender_addr,
                uint16_t sender_port,
                const uip_ipaddr_t *receiver_addr,
                uint16_t receiver_port,
                const uint8_t *data,
                uint16_t datalen)
{
  /* Capture exact arrival time */
  uint32_t rx_time = (uint32_t)clock_time();

  uint32_t seqno = 0;
  uint32_t tx_time = 0;

  /* Increment Total Packets Received for PDR */
  server_rx_count++;

  /* Extract sequence number and transmit time from payload */
  if (sscanf((char *)data, "%" PRIu32 " %" PRIu32, &seqno, &tx_time) == 2)
  {
    /* Final Delay formula: (Arrival Time - Send Time) converted to milliseconds */
    /* Calculate End-to-End Delay */
    uint32_t delay_ticks = rx_time - tx_time;
    uint32_t delay_ms = (delay_ticks * 1000) / CLOCK_SECOND;

    LOG_INFO("RX from ");
    LOG_INFO_6ADDR(sender_addr);
    LOG_INFO_(" | Seq: %" PRIu32 " | Delay: %" PRIu32 " ms\n", seqno, delay_ms);
  }
  else
  {
    LOG_INFO("Received unformatted data: '%.*s'\n", datalen, (char *)data);
  }

#if WITH_SERVER_REPLY
  /* simple_udp_sendto(&udp_conn, data, datalen, sender_addr); */
#endif
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(udp_server_process, ev, data)
{
  static struct etimer log_timer;
  static int peak_buffer = 0;

  PROCESS_BEGIN();

  NETSTACK_ROUTING.root_start();

  simple_udp_register(&udp_conn, UDP_SERVER_PORT, NULL,
                      UDP_CLIENT_PORT, udp_rx_callback);

  /* Set 10-second timer to print overall stats */
  etimer_set(&log_timer, 10 * CLOCK_SECOND);

  while (1)
  {
    PROCESS_YIELD();

    int current = queuebuf_num_elements();
    if (current > peak_buffer)
    {
      peak_buffer = current;
    }

    if (etimer_expired(&log_timer))
    {
      /* Print total received (for PDR) and buffer status */
      LOG_INFO("Server Status | Total RX: %" PRIu32 " | Peak Buf: %d/%d\n",
               server_rx_count, peak_buffer, QUEUEBUF_CONF_NUM);

      peak_buffer = 0;
      etimer_reset(&log_timer);
    }
  }

  PROCESS_END();
}