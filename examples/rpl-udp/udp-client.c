#include "contiki.h"
#include "net/routing/routing.h"
#include "random.h"
#include "net/netstack.h"
#include "net/ipv6/simple-udp.h"
#include <stdint.h>
#include <inttypes.h>

/* 1. ADDED THIS HEADER */
#include "net/queuebuf.h"

#include "sys/log.h"
#include "sys/node-id.h"
#include "sys/energest.h"
#define LOG_MODULE "CLIENT"
#define LOG_LEVEL LOG_LEVEL_INFO

#define WITH_SERVER_REPLY 1
#define UDP_CLIENT_PORT 8765
#define UDP_SERVER_PORT 5678

#define SEND_INTERVAL (10 * CLOCK_SECOND)

static struct simple_udp_connection udp_conn;
static uint32_t rx_count = 0;

/*---------------------------------------------------------------------------*/
PROCESS(udp_client_process, "UDP client");
AUTOSTART_PROCESSES(&udp_client_process);
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

  LOG_INFO("Received response '%.*s' from ", datalen, (char *)data);
  LOG_INFO_6ADDR(sender_addr);
#if LLSEC802154_CONF_ENABLED
  LOG_INFO_(" LLSEC LV:%d", uipbuf_get_attr(UIPBUF_ATTR_LLSEC_LEVEL));
#endif
  LOG_INFO_("\n");
  rx_count++;
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(udp_client_process, ev, data)
{
  static struct etimer periodic_timer;
  static struct etimer warmup_timer; /* Naya Timer */
  static char str[32];
  uip_ipaddr_t dest_ipaddr;
  static uint32_t tx_count;
  static uint32_t missed_tx_count;

  /* 2. DECLARED ONCE HERE */
  static int peak_buffer = 0;

  PROCESS_BEGIN();

  /* =================================================================== */
  /* 1. NETWORK WARMUP DELAY (300 SECONDS)                               */
  /* =================================================================== */
  etimer_set(&warmup_timer, 300 * CLOCK_SECOND);
  PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&warmup_timer));
  /* =================================================================== */

  /* Initialize UDP connection */
  simple_udp_register(&udp_conn, UDP_CLIENT_PORT, NULL,
                      UDP_SERVER_PORT, udp_rx_callback);

  etimer_set(&periodic_timer, random_rand() % SEND_INTERVAL);

  while (1)
  {
    /* YIELD kisi bhi event (timer ya network packet) ka wait karega */
    PROCESS_YIELD();

    /* 3. CHECK BUFFER AFTER YIELD (when process wakes up) */
    int current = queuebuf_num_elements();
    if (current > peak_buffer)
    {
      peak_buffer = current; /* <-- Yahan Peak calculate/update ho raha hai */
    }

    /* Periodic Timer Check (Normal Client Traffic) */
    if (etimer_expired(&periodic_timer))
    {
      /* 1. YAHAN ADD KAREIN: Har node timer fire hote hi sabse pehle apna buffer print karega */
      LOG_INFO("Node Buffer Status | Peak Buf: %d/%d\n", peak_buffer, QUEUEBUF_CONF_NUM);
      peak_buffer = 0; /* Next cycle ke liye turant reset karein */

      /* 2. Iske baad aapka normal TX ka check aayega */
      if (NETSTACK_ROUTING.node_is_reachable() &&
          NETSTACK_ROUTING.get_root_ipaddr(&dest_ipaddr))
      {
        /* Send to DAG root */
        LOG_INFO("Sending request %" PRIu32 " to ", tx_count);
        LOG_INFO_6ADDR(&dest_ipaddr);
        LOG_INFO_("\n");
        uint32_t tx_time = (uint32_t)clock_time();
        snprintf(str, sizeof(str), "%" PRIu32 " %" PRIu32, tx_count, tx_time);
        simple_udp_sendto(&udp_conn, str, strlen(str), &dest_ipaddr);
        tx_count++;
      }
      else
      {
        LOG_INFO("Not reachable yet\n");
        if (tx_count > 0)
        {
          missed_tx_count++;
        }
      }

      /* Add some jitter and restart periodic timer */
      etimer_set(&periodic_timer, SEND_INTERVAL - CLOCK_SECOND + (random_rand() % (15 * CLOCK_SECOND)));
    }
  }
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/