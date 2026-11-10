#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "contiki.h"
#include "contiki-net.h"

#include "coap-engine.h"
#include "coap-blocking-api.h"
#include "sys/node-id.h"

#include "net/netstack.h"
#include "net/mac/tsch/tsch.h"
#include "net/routing/routing.h"
// #include "net/mac/tsch/tsch-queue.h"
// #include "net/queuebuf.h"

#include "lib/random.h"

#define LOG_MODULE "CLIENT"
#define LOG_LEVEL LOG_LEVEL_INFO

#define SERVER_EP "coap://[fd00::201:1:1:1]"

#define TOGGLE_INTERVAL (60 * CLOCK_SECOND)

/*---------------------------------------------------------------------------*/
PROCESS(er_example_client, "Erbium Example Client");
AUTOSTART_PROCESSES(&er_example_client);

static struct etimer periodic_timer;
static struct etimer warmup_timer;

static uint32_t total_sent = 0;
static uint32_t total_received = 0;
static clock_time_t send_time;

void client_chunk_handler(coap_message_t *response)
{
  if (response == NULL)
  {
    printf("Request Timed Out (No ACK received)\n");
    printf("[METRIC] TIMEOUT : node=%u | Sent: %u | Acked: %u\n", node_id, total_sent, total_received);
    return;
  }
  /* 1. Update received count */
  total_received++;

  /* 2. Calculate Delay (RTT) */
  clock_time_t delay_ticks = clock_time() - send_time;
  uint32_t delay_ms = (uint32_t)(((uint64_t)delay_ticks * 1000) / CLOCK_SECOND);

  const uint8_t *chunk;
  int len = coap_get_payload(response, &chunk);

  printf("Server Reply: %.*s\n", len, (char *)chunk);

  /* 3. Sirf RTT aur Raw Counts print karein */
  printf("[METRIC] SUCCESS: node=%u | RTT: %u ms | Sent: %u | Acked: %u\n",
         node_id, delay_ms, total_sent, total_received);
}

PROCESS_THREAD(er_example_client, ev, data)
{
  static coap_endpoint_t server_ep;
  static coap_message_t request[1];

  PROCESS_BEGIN();

  /* =================================================================== */
  /* 1. NETWORK WARMUP DELAY (300 SECONDS)                               */
  /* =================================================================== */

  etimer_set(&warmup_timer, 600 * CLOCK_SECOND);
  PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&warmup_timer));
  /* =================================================================== */
  /* =================================================================== */

  LOG_INFO("Starting 6TiSCH CoAP Client\n");

  /* Parse server endpoint */
  coap_endpoint_parse(SERVER_EP, strlen(SERVER_EP), &server_ep);

  /* Start periodic timer */

  // etimer_set(&periodic_timer, random_rand() % TOGGLE_INTERVAL);
  /* Spread initial requests randomly over the first 60 seconds */

  // etimer_set(&periodic_timer, random_rand() % TOGGLE_INTERVAL);

  /* REPLACED: Added full random exponential jitter to shape traffic footprints */
  uint32_t random_jitter = random_rand() % (60 * CLOCK_SECOND);
  etimer_set(&periodic_timer, TOGGLE_INTERVAL + random_jitter);

  while (1)
  {

    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&periodic_timer));

    /* Check network connectivity (6TiSCH routing) */
    if (tsch_is_associated && NETSTACK_ROUTING.node_is_reachable())
    {

      // printf("\n--- Sending CON POST Request to Server ---\n");

      /* Initialize CoAP message as Confirmable POST */
      coap_init_message(request, COAP_TYPE_CON, COAP_POST, 0);

      /* Set URI path */
      coap_set_header_uri_path(request, "test/hello");

      /* Add Hello payload */
      const char msg[] = "Hello Server!";
      coap_set_payload(request, (uint8_t *)msg, strlen(msg));

      // /* NOW measure buffer — closer to actual send time */
      // int tsch_global = tsch_queue_global_packet_count();
      // int global_max = TSCH_QUEUE_NUM_PER_NEIGHBOR * TSCH_QUEUE_MAX_NEIGHBOR_QUEUES;

      // struct tsch_neighbor *parent = tsch_queue_get_time_source();
      // int parent_queue = 0;
      // int parent_max = TSCH_QUEUE_NUM_PER_NEIGHBOR;

      // if (parent == NULL)
      // {
      //   printf("[WARN] Node %u: No TSCH time source — parent queue invalid\n", node_id);
      // }
      // else
      // {
      //   parent_queue = tsch_queue_nbr_packet_count(parent);
      // }

      // printf("[STATS] Node %u: Global-Queue=%d/%d pkts | Parent-Queue=%d/%d pkts | Sent: %u | Acked: %u\n",
      //        node_id, tsch_global, global_max,
      //        parent_queue, parent_max,
      //        total_sent, total_received);

      total_sent++;
      send_time = clock_time();

      /* Send CoAP request and wait for ACK/Response */
      COAP_BLOCKING_REQUEST(&server_ep, request, client_chunk_handler);

      printf("Request Finished \n");
    }
    else
    {
      printf(" Network not reachable yet\n");
    }

    /* Restart timer */

    etimer_set(&periodic_timer, TOGGLE_INTERVAL - CLOCK_SECOND + (random_rand() % (30 * CLOCK_SECOND)));
  }

  PROCESS_END();
}