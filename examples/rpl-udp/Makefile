CONTIKI_PROJECT = udp-client udp-server udp-attacker
all: $(CONTIKI_PROJECT)

# Properly link the custom configuration file
CFLAGS += -DPROJECT_CONF_H=\"project-conf.h\"

# ---> YEH LINE MISSING THI: Yeh energest ke data ko har 60 sec mein print karti hai <---
MODULES += os/services/simple-energest


# --- ADD THESE TWO LINES FOR TSCH ---
MAKE_MAC = MAKE_MAC_TSCH
MODULES += os/services/orchestra
# ------------------------------------

# Point to the Contiki-NG root directory (adjust path if needed)
CONTIKI = ../..


# Include the main Contiki-NG Makefile
include $(CONTIKI)/Makefile.include