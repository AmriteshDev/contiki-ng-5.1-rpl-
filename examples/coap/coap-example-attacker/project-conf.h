#ifndef PROJECT_CONF_H_
#define PROJECT_CONF_H_

/* ------------------------------------------------------------------------- */
/*  Attack Mode Configuration                                              */
/* ------------------------------------------------------------------------- */
/* 0 = Normal Operation (No Attack)
 * 1 = Zero Counter Attack
 * 2 = CC Request Attack
 */
#define ATTACK_MODE 0

/* ------------------------------------------------------------------------- */

#define ENERGEST_CONF_ON 1

#define TSCH_CONF_AUTOSTART 1

#define TSCH_SCHEDULE_CONF_WITH_ORCHESTRA 1

// #undef TSCH_CONF_CCA_ENABLED
// #define TSCH_CONF_CCA_ENABLED 1

/* ------------------------------------------------------------------------- */
/* 1. Memory, Neighbor Table & TSCH Schedule Allocations (100 Nodes)         */
/* ------------------------------------------------------------------------- */
// #define NETSTACK_MAX_ROUTE_ENTRIES 150
// #define NBR_TABLE_CONF_MAX_NEIGHBORS 128
#define TSCH_SCHEDULE_CONF_MAX_LINKS 128

// #define QUEUEBUF_CONF_NUM 256
// #define TSCH_QUEUE_CONF_NUM_PER_NEIGHBOR 64
// #define TSCH_CONF_MAX_INCOMING_PACKETS 32

/* ------------------------------------------------------------------------- */
/* 2. Orchestra Scheduler Configuration                                      */
/* ------------------------------------------------------------------------- */

#define ORCHESTRA_CONF_UNICAST_PERIOD 7
#define ORCHESTRA_CONF_COMMON_SHARED_PERIOD 31
#define ORCHESTRA_CONF_EBSF_PERIOD 397
#define ORCHESTRA_CONF_RULES {&eb_per_time_source, &unicast_per_neighbor_rpl_ns, &default_common}

/* ------------------------------------------------------------------------- */
/* 3. TSCH MAC Retries, Backoff & Synchronization                            */
/* ------------------------------------------------------------------------- */
#define TSCH_CONF_MAC_MIN_BE 2
// #define TSCH_CONF_MAC_MAX_BE 5
// #define TSCH_CONF_MAC_MAX_FRAME_RETRIES 7

// #define TSCH_CONF_EB_PERIOD (4 * CLOCK_SECOND)
// #define TSCH_CONF_MAX_EB_PERIOD (16 * CLOCK_SECOND)
// #define TSCH_CONF_KEEPALIVE_TIMEOUT (60 * CLOCK_SECOND)
// #define TSCH_CONF_DESYNC_THRESHOLD (120 * CLOCK_SECOND)

/* TSCH Channel Hopping */
#define TSCH_CONF_DEFAULT_HOPPING_SEQUENCE TSCH_HOPPING_SEQUENCE_16_16
// #define TSCH_CONF_JOIN_HOPPING_SEQUENCE TSCH_HOPPING_SEQUENCE_16_16

/* ------------------------------------------------------------------------- */
/* 5. Logging Configuration                                                  */
/* ------------------------------------------------------------------------- */
// #undef LOG_CONF_LEVEL_RPL
// #define LOG_CONF_LEVEL_RPL LOG_LEVEL_INFO

// #undef LOG_CONF_LEVEL_MAC
// #define LOG_CONF_LEVEL_MAC LOG_LEVEL_INFO

// #undef LOG_CONF_LEVEL_FRAMER
// #define LOG_CONF_LEVEL_FRAMER LOG_LEVEL_NONE

// #undef LOG_CONF_LEVEL_COAP
// #define LOG_CONF_LEVEL_COAP LOG_LEVEL_DBG

#endif /* PROJECT_CONF_H_ */