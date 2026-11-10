#ifndef PROJECT_CONF_H_
#define PROJECT_CONF_H_

// #define ENERGEST_CONF_ON 1

/* ------------------------------------------------------------------------- */
/* 1. Memory, Neighbor Table & TSCH Schedule Allocations (100 Nodes)         */
/* ------------------------------------------------------------------------- */
#define NETSTACK_MAX_ROUTE_ENTRIES 150
#define NBR_TABLE_CONF_MAX_NEIGHBORS 128
#define TSCH_SCHEDULE_CONF_MAX_LINKS 256

#define QUEUEBUF_CONF_NUM 128
#define TSCH_QUEUE_CONF_NUM_PER_NEIGHBOR 32
#define TSCH_CONF_MAX_INCOMING_PACKETS 16

/* ------------------------------------------------------------------------- */
/* 2. Orchestra Scheduler Configuration                                      */
/* ------------------------------------------------------------------------- */
/* Increased bandwidth: Period 11 provides 50% more Tx/Rx slots per second
   than 17, draining the MAC queues significantly faster under heavy load */
#define ORCHESTRA_CONF_UNICAST_PERIOD 11
#define ORCHESTRA_CONF_COMMON_SHARED_PERIOD 31
#define ORCHESTRA_CONF_EBSF_PERIOD 397

/* ------------------------------------------------------------------------- */
/* 3. TSCH MAC Retries, Backoff & Synchronization                            */
/* ------------------------------------------------------------------------- */
#define TSCH_CONF_MAC_MIN_BE 2
#define TSCH_CONF_MAC_MAX_BE 5
#define TSCH_CONF_MAC_MAX_FRAME_RETRIES 7

#define TSCH_CONF_EB_PERIOD (4 * CLOCK_SECOND)
#define TSCH_CONF_MAX_EB_PERIOD (16 * CLOCK_SECOND)
#define TSCH_CONF_KEEPALIVE_TIMEOUT (60 * CLOCK_SECOND)
#define TSCH_CONF_DESYNC_THRESHOLD (120 * CLOCK_SECOND)

/* ------------------------------------------------------------------------- */
/* 4. RPL Routing (Fixes the Unicast DIO Loops flooding the TSCH queue)      */
/* ------------------------------------------------------------------------- */
#define RPL_CONF_DIO_INTERVAL_MIN 12
#define RPL_CONF_DIO_INTERVAL_DOUBLINGS 8
#define RPL_CONF_DIS_INTERVAL (60 * CLOCK_SECOND)

#define RPL_CONF_DAO_DELAY (15 * CLOCK_SECOND)
#define RPL_CONF_DAO_RETRANSMISSION_TIMEOUT (30 * CLOCK_SECOND)
#define RPL_CONF_DAO_MAX_RETRANSMISSIONS 10

/* CRITICAL FIX: Link metric remains 2048 to prevent early parent drops,
   but Significant Change Threshold MUST be 512. At 1536, nodes were hiding
   topology changes, creating Rank Errors, and triggering infinite loops
   of Unicast DIOs that jammed the 15/16 MAC queues in log_3.txt. */
#define RPL_MRHOF_CONF_MAX_LINK_METRIC 2048
#define RPL_CONF_SIGNIFICANT_CHANGE_THRESHOLD 512

#define RPL_CONF_PROBING_INTERVAL (180 * CLOCK_SECOND)

/* ------------------------------------------------------------------------- */
/* 5. Logging Configuration                                                  */
/* ------------------------------------------------------------------------- */
#undef LOG_CONF_LEVEL_RPL
#define LOG_CONF_LEVEL_RPL LOG_LEVEL_WARN

#undef LOG_CONF_LEVEL_MAC
#define LOG_CONF_LEVEL_MAC LOG_LEVEL_WARN

#undef LOG_CONF_LEVEL_FRAMER
#define LOG_CONF_LEVEL_FRAMER LOG_LEVEL_NONE

// #undef LOG_CONF_LEVEL_COAP
// #define LOG_CONF_LEVEL_COAP LOG_LEVEL_DBG

#endif /* PROJECT_CONF_H_ */