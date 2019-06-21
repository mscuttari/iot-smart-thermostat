all: sensor

# Contiki home path
CONTIKI = /home/michele/contiki

# Project configuration header
CFLAGS += -DPROJECT_CONF_H=\"project-conf.h\"

# Enable IPv6 and Ripple
WITH_UIP6=1
UIP_CONF_IPV6=1
CFLAGS += -DUIP_CONF_IPV6=1
CFLAGS += -DUIP_CONF_IPV6_RPL=1

# Linker optimizations
SMALL=1

# CoAP implementation (3|7|12|13) (er-coap-07 also supports CoAP draft 08)
WITH_COAP=13

# REST framework
ifeq ($(WITH_COAP), 13)
${info INFO: compiling with CoAP-13}
CFLAGS += -DWITH_COAP=13
CFLAGS += -DREST=coap_rest_implementation
CFLAGS += -DUIP_CONF_TCP=0
APPS += er-coap-13
else ifeq ($(WITH_COAP), 12)
${info INFO: compiling with CoAP-12}
CFLAGS += -DWITH_COAP=12
CFLAGS += -DREST=coap_rest_implementation
CFLAGS += -DUIP_CONF_TCP=0
APPS += er-coap-12
else ifeq ($(WITH_COAP), 7)
${info INFO: compiling with CoAP-07}
CFLAGS += -DWITH_COAP=7
CFLAGS += -DREST=coap_rest_implementation
CFLAGS += -DUIP_CONF_TCP=0
APPS += er-coap-07
else ifeq ($(WITH_COAP), 3)
${info INFO: compiling with CoAP-03}
CFLAGS += -DWITH_COAP=3
CFLAGS += -DREST=coap_rest_implementation
CFLAGS += -DUIP_CONF_TCP=0
APPS += er-coap-03
else
${info INFO: compiling with HTTP}
CFLAGS += -DWITH_HTTP
CFLAGS += -DREST=http_rest_implementation
CFLAGS += -DUIP_CONF_TCP=1
APPS += er-http-engine
endif

# REST Engine shall use Erbium CoAP implementation
APPS += erbium

# Disable the webserver of the border router
WITH_WEBSERVER = 0

# Include the main Contiki makefile
include $(CONTIKI)/Makefile.include

# Border router rules
$(CONTIKI)/tools/tunslip6: $(CONTIKI)/tools/tunslip6.c
	(cd $(CONTIKI)/tools && $(MAKE) tunslip6)

connect-router:	$(CONTIKI)/tools/tunslip6
	sudo $(CONTIKI)/tools/tunslip6 fd00::1/64

connect-router-cooja: $(CONTIKI)/tools/tunslip6
	sudo $(CONTIKI)/tools/tunslip6 -a 127.0.0.1 -p 60001 fd00::1/64
