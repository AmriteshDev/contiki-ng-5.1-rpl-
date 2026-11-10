#include "contiki.h"
#include "net/routing/routing.h"
#include "random.h"
#include "net/netstack.h"
#include "net/ipv6/simple-udp.h"
#include <stdint.h>
#include <inttypes.h>

#include "sys/log.h"
#include "sys/node-id.h"
#include "sys/energest.h"

/* 1. ADDED THIS HEADER */
#include "net/queuebuf.h"

#define LOG_MODULE "COMPROMISE NODE"
#define LOG_LEVEL LOG_LEVEL_INFO

#define WITH_SERVER_REPLY 1
#define UDP_CLIENT_PORT 8765
#define UDP_SERVER_PORT 5678
#define SEND_INTERVAL (10 * CLOCK_SECOND)

//  === ATTACK MODE SELECTION ===

/*
 * 0 = Normal Operation (No Attack)
 * 1 = Zero Counter Attack
 * 2 = CC Request Attack
 */
#define ATTACK_MODE 2

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
// static struct etimer cc_response_timeout_timer;
// static uint8_t cc_response_timeout_active = 0;
#endif

static struct simple_udp_connection udp_conn;
static uint32_t rx_count = 0;

PROCESS(udp_attacker_process, "UDP attacker process");
AUTOSTART_PROCESSES(&udp_attacker_process);

static void
udp_rx_callback(struct simple_udp_connection *c, const uip_ipaddr_t *sender_addr, uint16_t sender_port, const uip_ipaddr_t *receiver_addr,
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

PROCESS_THREAD(udp_attacker_process, ev, data)
{
    static struct etimer periodic_timer;
    // static struct etimer energy_timer;
    static struct etimer warmup_timer;

    static char str[32];
    uip_ipaddr_t dest_ipaddr;
    static uint32_t tx_count;
    static uint32_t missed_tx_count;
    /* 2. DECLARED ONCE HERE */
    static int peak_buffer = 0;

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
    /* 1. NETWORK WARMUP DELAY (60 SECONDS)                                */
    /* =================================================================== */
    // LOG_INFO("Waiting 300 seconds for RPL network to establish...\n");
    etimer_set(&warmup_timer, 300 * CLOCK_SECOND);
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&warmup_timer));
    // LOG_INFO("Network ready. Starting operations.\n");
    /* =================================================================== */

    simple_udp_register(&udp_conn, UDP_CLIENT_PORT, NULL,
                        UDP_SERVER_PORT, udp_rx_callback);

    //  normal UDP traffic -- Timer for First Transmission

    etimer_set(&periodic_timer, random_rand() % SEND_INTERVAL);

/* Initialize timers for first attack (First time attack is triggering (warmup timer + first_attack_timer---->120) = 180sec)  */
/* First attack after 120 seconds */
#if ATTACK_MODE != 0
    etimer_set(&first_attack_timer, 120 * CLOCK_SECOND);
#endif

    while (1)
    {

        PROCESS_YIELD();

        /* 3. CHECK BUFFER AFTER YIELD (when process wakes up) */
        int current = queuebuf_num_elements();
        if (current > peak_buffer)
        {
            peak_buffer = current;
        }

        if (ev == PROCESS_EVENT_POLL)
        {
#if ATTACK_MODE == 1

            if (cc_response_arrived_zero == 1)
            {
                cc_response_arrived_zero = 0;

                // if (cc_response_timeout_active)
                // {
                //     etimer_stop(&cc_response_timeout_timer);
                //     cc_response_timeout_active = 0;
                // }

                uint16_t prob = random_rand() % 100;
                if (prob < 80)
                {
                    LOG_INFO("Probability hit! Rejecting Sync and Firing next zero counter!\n");
                    zero_counter_attack();

                    // etimer_set(&cc_response_timeout_timer, 10 * CLOCK_SECOND);
                    // cc_response_timeout_active = 1;
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

                // if (cc_response_timeout_active)
                // {
                //     etimer_stop(&cc_response_timeout_timer);
                //     cc_response_timeout_active = 0;
                // }

                uint16_t prob = random_rand() % 100;

                if (prob < 80)
                {
                    LOG_INFO("Probability HIT. Firing next CC Request!\n");

                    cc_request_attack();

                    // etimer_set(&cc_response_timeout_timer, 10 * CLOCK_SECOND);
                    // cc_response_timeout_active = 1;
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

#if ATTACK_MODE != 0
        if (!first_attack_fired && etimer_expired(&first_attack_timer))
        {
            first_attack_fired = 1;
            etimer_stop(&first_attack_timer);

#if ATTACK_MODE == 1

            LOG_INFO("First Zero Counter Attack.\n");
            zero_counter_attack();
            // etimer_set(&cc_response_timeout_timer, 10 * CLOCK_SECOND);
            // cc_response_timeout_active = 1;

#elif ATTACK_MODE == 2

            // LOG_INFO("First CC Request Attack.\n");
            cc_request_attack();
            // etimer_set(&cc_response_timeout_timer, 10 * CLOCK_SECOND);
            // cc_response_timeout_active = 1;

#endif
        }

#endif

        // #if ATTACK_MODE != 0
        //         if (cc_response_timeout_active &&
        //             etimer_expired(&cc_response_timeout_timer))
        //         {
        //             cc_response_timeout_active = 0;
        // #if ATTACK_MODE == 1
        //             LOG_INFO("No CC response for 10 seconds. Starting next Zero Counter Attack.\n");

        //             zero_counter_attack();
        //             etimer_set(&cc_response_timeout_timer, 10 * CLOCK_SECOND);
        //             cc_response_timeout_active = 1;

        // #elif ATTACK_MODE == 2

        //             LOG_INFO("No CC response for 10 seconds. Starting next CC Request Attack.\n");

        //             cc_request_attack();
        //             etimer_set(&cc_response_timeout_timer, 10 * CLOCK_SECOND);
        //             cc_response_timeout_active = 1;
        // #endif
        //         }

        // #endif

#if ATTACK_MODE == 1
        /* Again Firing zero counter -- if attack is not fired for long time (Probability Miss!) */
        if (retry_active && etimer_expired(&zero_counter_retry_timer))
        {
            // LOG_INFO("10 seconds completed. Starting next Zero Counter Attack.\n");

            retry_active = 0;

            // Initiating a new Zero Counter Attack
            zero_counter_attack();
            // etimer_set(&cc_response_timeout_timer, 10 * CLOCK_SECOND);
            // cc_response_timeout_active = 1;
        }
#elif ATTACK_MODE == 2
        /* Again Firing cc request -- if attack is not fired for long time (Probability Miss!) */
        if (retry_active && etimer_expired(&cc_request_retry_timer))
        {
            // LOG_INFO("10 seconds completed. Starting next CC Request Attack.\n");

            retry_active = 0;

            // Initiating a new CC Request Attack
            cc_request_attack();

            // etimer_set(&cc_response_timeout_timer, 10 * CLOCK_SECOND);
            // cc_response_timeout_active = 1;
        }
#endif

        /* =================================================================== */
        /* NORMAL OPERATIONS: UDP Data Sending              */
        /* =================================================================== */
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