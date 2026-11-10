CONTIKI_PROJECT = coap-example-client

all: $(CONTIKI_PROJECT)


CONTIKI=../../..

# Force Contiki-NG to load your custom configuration header
CFLAGS += -DPROJECT_CONF_H=\"project-conf.h\"

MODULES += os/services/simple-energest


MAKE_MAC = MAKE_MAC_TSCH
MODULES += os/services/orchestra

include $(CONTIKI)/Makefile.dir-variables
MODULES += $(CONTIKI_NG_APP_LAYER_DIR)/coap

include $(CONTIKI)/Makefile.include
