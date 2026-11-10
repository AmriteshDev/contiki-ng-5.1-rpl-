
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

static struct simple_udp_connection udp_conn;
static uint32_t rx_count = 0;

static void print_raw_energest(void)
{

    energest_flush();

    printf("[ENERGY] CPU: %lu LPM: %lu TX: %lu RX: %lu\n",
           (unsigned long)energest_type_time(ENERGEST_TYPE_CPU),
           (unsigned long)energest_type_time(ENERGEST_TYPE_LPM),
           (unsigned long)energest_type_time(ENERGEST_TYPE_TRANSMIT),
           (unsigned long)energest_type_time(ENERGEST_TYPE_LISTEN));
}

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
    static struct etimer energy_timer;
    static struct etimer warmup_timer;
    static char str[32];
    uip_ipaddr_t dest_ipaddr;
    static uint32_t tx_count;
    static uint32_t missed_tx_count;

#if ATTACK_MODE == 1
    static struct etimer zero_counter_retry_timer;
#elif ATTACK_MODE == 2
    static struct etimer cc_request_retry_timer;
#endif

    PROCESS_BEGIN();

    /* =================================================================== */
    /* 1. NETWORK WARMUP DELAY (60 SECONDS)                                */
    /* =================================================================== */
    LOG_INFO("Waiting 60 seconds for RPL network to establish...\n");
    etimer_set(&warmup_timer, 60 * CLOCK_SECOND);
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&warmup_timer));
    LOG_INFO("Network ready. Starting operations.\n");
    /* =================================================================== */

    simple_udp_register(&udp_conn, UDP_CLIENT_PORT, NULL,
                        UDP_SERVER_PORT, udp_rx_callback);

    /* Energy timer initialize (Energy stats after each 60 sec) */
    etimer_set(&energy_timer, 60 * CLOCK_SECOND);

    //  normal UDP traffic -- Timer for First Transmission

    etimer_set(&periodic_timer, random_rand() % SEND_INTERVAL);

/* Initialize specific timers for first attack (First time attack is triggering (warmup timer + zero_counter_retry_timer---->120) = 180sec)  */
#if ATTACK_MODE == 1
    etimer_set(&zero_counter_retry_timer, 120 * CLOCK_SECOND);
    LOG_INFO("Zero Counter Attack Is Active\n");
/* Initialize specific timers for first attack (First time attack is triggering (warmup timer + cc_request_retry_timer---->120) = 180sec)  */
#elif ATTACK_MODE == 2
    etimer_set(&cc_request_retry_timer, 120 * CLOCK_SECOND);
    LOG_INFO("CC Request Attack Is Active\n");

#endif

    while (1)
    {

        PROCESS_YIELD();

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

                    LOG_INFO("Probability hit! Accepting Re-sync!\n");
                    attacker_accept_resync(cc_received_watermark);
                }

                /* Again Firing zero counter -- if attack is not fired for long time */
                etimer_set(&zero_counter_retry_timer, 20 * CLOCK_SECOND);
            }

#elif ATTACK_MODE == 2

            if (cc_response_arrived == 1)
            {
                cc_response_arrived = 0;

                uint16_t prob = random_rand() % 100;
                if (prob < 80)
                {

                    LOG_INFO("Probability hit! Firing next Request!\n");
                    cc_request_attack();
                }

                /* Again Firing cc request -- if attack is not fired for long time */
                etimer_set(&cc_request_retry_timer, 20 * CLOCK_SECOND);
            }
#endif
        }

        /* 1. Energy Timer Check */
        if (etimer_expired(&energy_timer))
        {
            /* Har 60 second baad raw ticks print karein */
            print_raw_energest();

            /* Timer ko agle cycle ke liye reset zaroor karein */
            etimer_reset(&energy_timer);
        }

#if ATTACK_MODE == 1
        /* Again Firing zero counter -- if attack is not fired for long time (Probability Miss!) */
        if (etimer_expired(&zero_counter_retry_timer))
        {

            // Initiating a new Zero Counter Attack
            zero_counter_attack();
            etimer_set(&zero_counter_retry_timer, 20 * CLOCK_SECOND);
        }
#elif ATTACK_MODE == 2
        /* Again Firing cc request -- if attack is not fired for long time (Probability Miss!) */
        if (etimer_expired(&cc_request_retry_timer))
        {
            // Initiating a new CC Request Attack
            cc_request_attack();
            etimer_set(&cc_request_retry_timer, 20 * CLOCK_SECOND);
        }
#endif

        /* =================================================================== */
        /* NORMAL OPERATIONS: UDP Data Sending              */
        /* =================================================================== */
        if (etimer_expired(&periodic_timer))
        {
            if (NETSTACK_ROUTING.node_is_reachable() &&
                NETSTACK_ROUTING.get_root_ipaddr(&dest_ipaddr))
            {
                if (tx_count % 10 == 0)
                {
                    LOG_INFO("Tx/Rx/MissedTx: %" PRIu32 "/%" PRIu32 "/%" PRIu32 "\n", tx_count, rx_count, missed_tx_count);
                }

                LOG_INFO("Sending request %" PRIu32 " to ", tx_count);
                LOG_INFO_6ADDR(&dest_ipaddr);
                LOG_INFO_("\n");
                snprintf(str, sizeof(str), "hello %" PRIu32 "", tx_count);
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

            etimer_set(&periodic_timer, SEND_INTERVAL - CLOCK_SECOND + (random_rand() % (2 * CLOCK_SECOND)));
        }
    }

    PROCESS_END();
}