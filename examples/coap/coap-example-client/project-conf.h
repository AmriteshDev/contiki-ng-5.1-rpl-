#ifndef PROJECT_CONF_H_
#define PROJECT_CONF_H_

#define ENERGEST_CONF_ON 1

#define TSCH_CONF_AUTOSTART 1
#define TSCH_SCHEDULE_CONF_WITH_ORCHESTRA 1

/* ------------------------------------------------------------------------- */
/* 1. Memory, Neighbor Table & TSCH Schedule Allocations (100 Nodes)         */
/* ------------------------------------------------------------------------- */

#define TSCH_SCHEDULE_CONF_MAX_LINKS 128

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

/* TSCH Channel Hopping */
#define TSCH_CONF_DEFAULT_HOPPING_SEQUENCE TSCH_HOPPING_SEQUENCE_16_16

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