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
#include "lib/random.h"
#include "sys/log.h"
// #include "net/mac/tsch/tsch-queue.h"
// #include "net/queuebuf.h"

#define LOG_MODULE "COMPROMISE NODE"
#define LOG_LEVEL LOG_LEVEL_INFO

#define SERVER_EP "coap://[fd00::201:1:1:1]"

#define TOGGLE_INTERVAL (60 * CLOCK_SECOND)

#if ATTACK_MODE == 1
extern volatile uint8_t cc_response_arrived_zero;
extern volatile uint32_t cc_received_watermark;
extern void zero_counter_attack(void);
extern void attacker_accept_resync(uint32_t watermark);
#define CC_RESPONSE_TIMEOUT (5 * CLOCK_SECOND)
#elif ATTACK_MODE == 2
extern volatile uint8_t cc_response_arrived;
extern void cc_request_attack(void);
#define CC_RESPONSE_TIMEOUT (5 * CLOCK_SECOND)
#endif

PROCESS(er_example_attacker, "Normal CoAP Client Process");

#if ATTACK_MODE != 0
PROCESS(attack_process, "Compromised RPL Attack Process");
AUTOSTART_PROCESSES(&er_example_attacker, &attack_process);
#else
AUTOSTART_PROCESSES(&er_example_attacker);
#endif

/* Keep static timer order identical to coap-example-client.c */
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

/*---------------------------------------------------------------------------*/
/* PROCESS 1: Normal CoAP Client Operation                                   */
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(er_example_attacker, ev, data)
{
  static coap_endpoint_t server_ep;
  static coap_message_t request[1];

  PROCESS_BEGIN();

  /* 1. NETWORK WARMUP DELAY (300 SECONDS) */
  etimer_set(&warmup_timer, 600 * CLOCK_SECOND);
  PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&warmup_timer));

  LOG_INFO("Starting CoAP Client-Compromised\n");

  /* Parse server endpoint */
  coap_endpoint_parse(SERVER_EP, strlen(SERVER_EP), &server_ep);

  /* Start periodic timer for normal traffic */

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

      LOG_INFO("\nSending CON POST Request to Server\n");

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

      printf(" Request Finished \n");
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

#if ATTACK_MODE != 0
/*---------------------------------------------------------------------------*/
/* PROCESS 2: Compromised Node Behavior (attack_process)                     */
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(attack_process, ev, data)
{

  static struct etimer attack_warmup_timer;
  static struct etimer first_attack_timer;
  static struct etimer retry_timer;
  static uint8_t first_attack_fired = 0;
  static uint8_t retry_active = 0;
  static struct etimer cc_timeout_timer;
  static uint8_t waiting_for_cc = 0;

  PROCESS_BEGIN();

  /* Wait for the same 300s warmup period before starting attack countdown */
  etimer_set(&attack_warmup_timer, 600 * CLOCK_SECOND);
  PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&attack_warmup_timer));

  LOG_INFO("Warmup complete. Scheduling first attack in 60 seconds.\n");
  etimer_set(&first_attack_timer, 60 * CLOCK_SECOND);

  while (1)
  {
    PROCESS_YIELD();

    /* 1. Handle High-Priority Poll from rpl-icmp6.c (cc_input) */
    if (ev == PROCESS_EVENT_POLL)
    {
#if ATTACK_MODE == 1
      if (cc_response_arrived_zero == 1)
      {
        cc_response_arrived_zero = 0;
        waiting_for_cc = 0;             /* ← response aaya, timeout cancel */
        etimer_stop(&cc_timeout_timer); /* ← timeout timer band karo */

        uint16_t prob = random_rand() % 100;
        if (prob < 90)
        {
          LOG_INFO("Probability HIT (%u < 80)! Rejecting Sync and firing next Zero Counter Attack!\n", prob);
          zero_counter_attack();
          /* ← naya attack fire kiya, timeout phir se shuru */
          waiting_for_cc = 1;
          etimer_set(&cc_timeout_timer, CC_RESPONSE_TIMEOUT);
        }
        else
        {
          LOG_INFO("Probability MISS (%u >= 80). Accepting resync and pausing attack for 5s.\n", prob);
          attacker_accept_resync(cc_received_watermark);
          etimer_set(&retry_timer, 2 * CLOCK_SECOND);
          retry_active = 1;
        }
      }
#elif ATTACK_MODE == 2
      if (cc_response_arrived == 1)
      {
        cc_response_arrived = 0;
        waiting_for_cc = 0;             /* ← response aaya, timeout cancel */
        etimer_stop(&cc_timeout_timer); /* ← timeout timer band karo */

        uint16_t prob = random_rand() % 100;
        if (prob < 90)
        {
          LOG_INFO("Probability HIT (%u < 80)! Firing next CC Request Attack!\n", prob);
          cc_request_attack();
          /* ← naya attack fire kiya, timeout phir se shuru */
          waiting_for_cc = 1;
          etimer_set(&cc_timeout_timer, CC_RESPONSE_TIMEOUT);
        }
        else
        {
          LOG_INFO("Probability MISS (%u >= 80). Normal operation for 5s.\n", prob);
          etimer_set(&retry_timer, 2 * CLOCK_SECOND);
          retry_active = 1;
        }
      }
#endif
    }

    /* ---------------------------------------------------------------- */
    /* 2. CC Response Timeout — response nahi aaya, directly retry      */
    /* ← YE NAYA BLOCK HAI — PEHLE EXIST NAHI KARTA THA               */
    /* ---------------------------------------------------------------- */
    if (waiting_for_cc && etimer_expired(&cc_timeout_timer))
    {
      waiting_for_cc = 0;

#if ATTACK_MODE == 1
      LOG_INFO("CC timeout! No response in %us. Retrying Zero Counter Attack.\n",
               (unsigned)(CC_RESPONSE_TIMEOUT / CLOCK_SECOND));
      zero_counter_attack();
      /* timeout phir se set karo next response ke liye */
      waiting_for_cc = 1;
      etimer_set(&cc_timeout_timer, CC_RESPONSE_TIMEOUT);

#elif ATTACK_MODE == 2
      LOG_INFO("CC timeout! No response in %us. Retrying CC Request Attack.\n",
               (unsigned)(CC_RESPONSE_TIMEOUT / CLOCK_SECOND));
      cc_request_attack();
      waiting_for_cc = 1;
      etimer_set(&cc_timeout_timer, CC_RESPONSE_TIMEOUT);
#endif
    }

    /* ---------------------------------------------------------------- */
    /* 3. First Attack Trigger                                           */
    /* ---------------------------------------------------------------- */

    /* 2. Trigger the First Attack after 120s post-warmup */
    if (!first_attack_fired && etimer_expired(&first_attack_timer))
    {
      first_attack_fired = 1;
      etimer_stop(&first_attack_timer);

#if ATTACK_MODE == 1
      LOG_INFO("Triggering First Zero Counter Attack (Scenario 3).\n");
      zero_counter_attack();
      waiting_for_cc = 1;                                 /* ← attack ke BAAD */
      etimer_set(&cc_timeout_timer, CC_RESPONSE_TIMEOUT); /* ← attack ke BAAD */
#elif ATTACK_MODE == 2
      LOG_INFO("Triggering First CC Request Attack (Scenario 4).\n");
      cc_request_attack();
      waiting_for_cc = 1;                                 /* ← attack ke BAAD */
      etimer_set(&cc_timeout_timer, CC_RESPONSE_TIMEOUT); /* ← attack ke BAAD */

#endif
    }

    /* ---------------------------------------------------------------- */
    /* 4. Retry after Probability Miss cooldown                          */
    /* ---------------------------------------------------------------- */
    /* 3. Trigger Retry Attack after 10s cooldown on Probability Miss */
    if (retry_active && etimer_expired(&retry_timer))
    {
      retry_active = 0;

#if ATTACK_MODE == 1
      LOG_INFO("5 cooldown finished. Resuming Zero Counter Attack.\n");
      zero_counter_attack();
      waiting_for_cc = 1;
      etimer_set(&cc_timeout_timer, CC_RESPONSE_TIMEOUT);
#elif ATTACK_MODE == 2
      LOG_INFO("5s cooldown finished. Resuming CC Request Attack.\n");
      cc_request_attack();
      waiting_for_cc = 1;
      etimer_set(&cc_timeout_timer, CC_RESPONSE_TIMEOUT);

#endif
    }
  }

  PROCESS_END();
}
#endif