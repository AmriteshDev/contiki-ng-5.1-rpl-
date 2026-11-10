#include <stdio.h>
#include <string.h>

#include "contiki.h"
#include "coap-engine.h"
#include "sys/node-id.h"

#include "net/mac/tsch/tsch.h"
#include "net/netstack.h"
#include "net/routing/routing.h"

#include "sys/log.h"

// #include "net/mac/tsch/sixtop/sixtop.h"

#define LOG_MODULE "SERVER"
#define LOG_LEVEL LOG_LEVEL_INFO

static void
res_post_handler(coap_message_t *request,
                 coap_message_t *response,
                 uint8_t *buffer,
                 uint16_t preferred_size,
                 int32_t *offset)
{
  const uint8_t *payload;
  int len;

  len = coap_get_payload(request, &payload);

  if (len > 0)
  {
    printf("[Server] Request received from Client: %.*s\n", len, (char *)payload);
  }

  const char *reply_msg = "Hi!";

  /* Send reply payload back to the client */
  coap_set_payload(response, buffer, snprintf((char *)buffer, preferred_size, "%s", reply_msg));

  printf("[Server] Replied with ACK and message!\n");
}

/*---------------------------------------------------------------------------*/
/* CoAP Resource */
/*---------------------------------------------------------------------------*/
RESOURCE(res_hello,
         "title=\"Hello Resource\";rt=\"Text\"",
         NULL,             /* GET handler (Removed) */
         res_post_handler, /* POST handler */
         NULL,             /* PUT handler */
         NULL);            /* DELETE handler */

PROCESS(er_example_server, "Erbium Example Server");
AUTOSTART_PROCESSES(&er_example_server);

PROCESS_THREAD(er_example_server, ev, data)
{
  PROCESS_BEGIN();

  LOG_INFO("Starting 6TiSCH CoAP Server\n");

  /* Start TSCH Coordinator */
  tsch_set_coordinator(1);
  // LOG_INFO("TSCH Coordinator Started\n");

  /* Start RPL Root */
  NETSTACK_ROUTING.root_start();
  // LOG_INFO("RPL Root Started\n");

  /* Activate CoAP Resource */
  coap_activate_resource(&res_hello, "test/hello");

  while (1)
  {
    PROCESS_WAIT_EVENT();
  }

  PROCESS_END();
}