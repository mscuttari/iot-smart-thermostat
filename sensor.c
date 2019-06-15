#include <stdlib.h>
#include <string.h>

/** Enable or disable debug messages */
#define DEBUG 1

/** Temperature random initialization range */
#define TEMP_RANDOM_MIN		10
#define TEMP_RANDOM_MAX		30

/** How much frequently the temperature should be notified */
#define TEMP_SENSING_INTERVAL	5

/** Temperature change simulation interval */
#define TEMP_SIM_INTERVAL	20

/** Enable or disable the individual components */
#define SIMULATION_ENABLED	1
#define COOLING_ENABLED		1
#define HEATING_ENABLED		1
#define VENTILATION_ENABLED	1
#define REST_SERVER_ENABLED	1


// C doesn't natively have the bool type
typedef enum{false, true} bool;


// If the debug is enabled, include the standard I/O library
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif

// If the temperature change simulation is enabled, include the random library
// in order to simulate an initial temperature
#if SIMULATION_ENABLED
#include "random.h"
#endif

// If any of the cooling, heating or ventilation systems is enabled, include
// the LEDs library in order to simulate their activity
#if COOLING_ENABLED || HEATING_ENABLED || VENTILATION_ENABLED
#include "dev/leds.h"
#endif

// If the REST server is enabled, include the required libraries to create the server
#if REST_SERVER_ENABLED
#include "contiki.h"
#include "contiki-net.h"
#include "erbium.h"

#if WITH_COAP == 3
#include "er-coap-03.h"
#elif WITH_COAP == 7
#include "er-coap-07.h"
#elif WITH_COAP == 12
#include "er-coap-12.h"
#elif WITH_COAP == 13
#include "er-coap-13.h"
#else
#error "CoAP implementation missing or invalid"
#endif

#endif


/**
 * Processes definitions.
 * Each of them is individually described when implemented.
 */
PROCESS(boot_process, "Boot");
PROCESS(temperature_sensing, "Temperature sensing and notification");

#if SIMULATION_ENABLED
PROCESS(temperature_simulation, "Temperature change simulation");
#endif

#if REST_SERVER_ENABLED
PROCESS(rest_server, "REST server");
#endif


/**
 * Read the temperature.
 *
 * As explained later, in this example the function simply returns the value of the global
 * temperature variable, which is updated by the simulation process. In a real system it
 * would be sufficient to implement this function according to the real hardware specifications.
 */
int read_temperature();


/**
 * Functions devoted to the start and stop of the room systems.
 *
 * In this example the systems are activated by just turning on or off some leds, but more
 * complex systems can be managed by just modifying theses functions and thus without touching
 * the rest of the code.
 */
#if COOLING_ENABLED
void start_cooling_system();
void stop_cooling_system();
#endif

#if HEATING_ENABLED
void start_heating_system();
void stop_heating_system();
#endif

#if VENTILATION_ENABLED
void start_ventilation_system();
void stop_ventilation_system();
#endif


/** Mote and environment variables */
static int temperature;
typedef enum{NONE, COOLING, HEATING} status_t;
static status_t status;
static bool ventilation;


/** Resources available to the network */
#if REST_SERVER_ENABLED
PERIODIC_RESOURCE(temperature, METHOD_GET, "temp", "title=\"Temperature\";rt=\"Text\";obs", TEMP_SENSING_INTERVAL * CLOCK_SECOND);
RESOURCE(systems, METHOD_GET, "systems", "title=\"Systems\";rt=\"Text\"");
RESOURCE(cooling, METHOD_POST, "systems/cooling", "title=\"Cooling\";rt=\"Text\"");
RESOURCE(heating, METHOD_POST, "systems/heating", "title=\"Heating\";rt=\"Text\"");
RESOURCE(ventilation, METHOD_POST, "systems/ventilation", "title=\"Ventilation\";rt=\"Text\"");
#endif


// Start the boot process
AUTOSTART_PROCESSES(&boot_process);


/**
 * Boot process.
 * Used to initialize some variables, such as the temperature, and start all the services.
 */
PROCESS_THREAD(boot_process, ev, data) {
	PROCESS_BEGIN();
	
	#if SIMULATION_ENABLED
	// Generate a random value in the range [TEMP_RANDOM_MIN, TEMP_RANDOM_MAX]
	temperature = (random_rand() % (TEMP_RANDOM_MAX + 1 - TEMP_RANDOM_MIN)) + TEMP_RANDOM_MIN;
	PRINTF("Initial temperature randomly set to %d\n", temperature);
	#endif
	
	status = NONE;
	ventilation = false;
	
	// Initialization is finished. Start the other processes.
	process_start(&temperature_sensing, NULL);
	
	#if REST_SERVER_ENABLED
	process_start(&rest_server, NULL);
	#endif
	
	#if SIMULATION_ENABLED
	process_start(&temperature_simulation, NULL);
	#endif
	
	PRINTF("[BOOT] Completed\n");
	PROCESS_END();
}


/**
 * Periodically sense the temperature and notify the border router.
 */
PROCESS_THREAD(temperature_sensing, ev, data) {
	static struct etimer timer;
	
	PROCESS_BEGIN();
	
	etimer_set(&timer, TEMP_SENSING_INTERVAL * CLOCK_SECOND);
	
	while (1) {
		PROCESS_WAIT_EVENT_UNTIL(ev == PROCESS_EVENT_TIMER && data == &timer);
		
		temperature = read_temperature();
		PRINTF("[SENSING] Temperature: %d\n", temperature);
		
		// Make the timer periodic
		etimer_restart(&timer);
	}
	
	PROCESS_END();
}


/**
 * Simulate the temperature change according to the active systems.
 *
 * When the systems status is changed, this process must be notified with the PROCESS_EVENT_MSG event.
 * This simulation process could be implemented directly in the cooling / heating processes and this
 * way have it started and stopped automatically according to the processes lifecycles. Anyway, the
 * current design has been preferred in order to decouple as much as possible the simulation logic
 * (which would be deleted in a real environment) from the control one.
 */
#if SIMULATION_ENABLED
PROCESS_THREAD(temperature_simulation, ev, data) {
	static struct etimer timer;
	static status_t previous_status;

	PROCESS_BEGIN();
	
	// We keep track of the previous status in order to handle the case where
	// the process is notified but the status stays the same.
	previous_status = status;
	
	// Start the simulation
	etimer_set(&timer, TEMP_SIM_INTERVAL * CLOCK_SECOND);
	
	while (1) {
		PROCESS_WAIT_EVENT();
		
		if (ev == PROCESS_EVENT_TIMER && data == &timer) {
			// TEMP_SIM_INTERVAL seconds of continuous operations have elapsed.
			
			int factor = ventilation ? 2 : 1;
			
			if (status == COOLING) {
				temperature -= 1 * factor;
			} else if (status == HEATING) {
				temperature += 1 * factor;
			}
			
			PRINTF("[SIMULATION] Temperature updated to %d\n", temperature);
			etimer_restart(&timer);
			
		} else if (ev == PROCESS_EVENT_MSG && previous_status != status) {
			// The activated systems have changed, so restart the timer (the temperature
			// change should take place only after TEMP_SIM_INTERVAL seconds of continuous
			// operation).
			
			PRINTF("[SIMULATION] Active systems changed. Restarting the simulation.\n");
			previous_status = status;
			etimer_restart(&timer);
		}
	}
	
	PROCESS_END();
}
#endif


/**
 * REST server
 */
#if REST_SERVER_ENABLED
PROCESS_THREAD(rest_server, ev, data) {
	PROCESS_BEGIN();
	PRINTF("[REST] starting server\n");
	
	// Initialize the REST engine
	rest_init_engine();
	
	// Activate the resources
	rest_activate_periodic_resource(&periodic_resource_temperature);
	rest_activate_resource(&resource_systems);
	rest_activate_resource(&resource_cooling);
	rest_activate_resource(&resource_heating);
	rest_activate_resource(&resource_ventilation);

	PRINTF("[REST] server started\n");
	PROCESS_END();
}


/**
 * Send the current temperature in JSON format
 */
void temperature_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset) {
	REST.set_header_content_type(response, REST.type.TEXT_PLAIN);

	static char payload[20];
	snprintf(payload, sizeof(payload), "{\n\"temp\":%d\n}", temperature);
	REST.set_response_payload(response, payload, strlen(payload));
}

/**
 * Periodically send the temperature in JSON format to the subscribed devices
 */
void temperature_periodic_handler(resource_t *r) {
	static uint16_t counter = 0;
 	static char payload[20];

  	coap_packet_t message[1];
	coap_init_message(message, COAP_TYPE_NON, REST.status.OK, 0);
	int length = snprintf(payload, sizeof(payload), "{\n\"temperature\":%d\n}", temperature);
	coap_set_payload(message, payload, length);

	REST.notify_subscribers(r, ++counter, message);
}


/**
 * Send a summary about the systems status
 */
void systems_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset) {
	int length = snprintf(
		(char*) buffer,
		REST_MAX_CHUNK_SIZE,
		"{\n\"heating\":\"%s\",\n\"cooling\":\"%s\",\n\"ventilation\":\"%s\"\n}",
		status == HEATING ? "\"true\"" : "\"false\"",
		status == COOLING ? "\"true\"" : "\"false\"",
		ventilation ? "\"true\"" : "\"false\"");

	REST.set_header_content_type(response, REST.type.APPLICATION_JSON);
	REST.set_response_payload(response, buffer, length);
}


/**
 * Start / stop the cooling system upon user request
 */
void cooling_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset) {
	if (status != NONE && status != COOLING) {
		REST.set_response_status(response, REST.status.BAD_REQUEST);
	} else {
		REST.set_response_status(response, REST.status.OK);

		if (status == NONE) {
			PRINTF("[COOLING] starting\n");
			status = COOLING;
			start_cooling_system();
			PRINTF("[COOLING] started\n");
		} else {
			PRINTF("[COOLING] stopping\n");
			stop_cooling_system();
			status = NONE;
			PRINTF("[COOLING] stopped\n");
		}

		#if SIMULATION
		process_post(&temperature_simulation, PROCESS_EVENT_MSG, NULL);
		#endif
	}
}


/**
 * Start / stop the heating system upon user request
 */
void heating_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset) {
	if (status != NONE && status != HEATING) {
		REST.set_response_status(response, REST.status.BAD_REQUEST);
	} else {
		REST.set_response_status(response, REST.status.OK);

		if (status == NONE) {
			PRINTF("[HEATING] starting\n");
			status = HEATING;
			start_heating_system();
			PRINTF("[HEATING] started\n");
		} else {
			PRINTF("[HEATING] stopping\n");
			stop_heating_system();
			status = NONE;
			PRINTF("[HEATING] stopped\n");
		}

		#if SIMULATION
		process_post(&temperature_simulation, PROCESS_EVENT_MSG, NULL);
		#endif
	}
}


/**
 * Start / stop the ventilation system upon user request
 */
void ventilation_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset) {
	if (!ventilation) {
		PRINTF("[HEATING] starting\n");
		ventilation = true;
		start_ventilation_system();
		PRINTF("[HEATING] started\n");
	} else {
		PRINTF("[HEATING] stopping\n");
		stop_ventilation_system();
		ventilation = false;
		PRINTF("[HEATING] stopped\n");
	}

	REST.set_response_status(response, REST.status.OK);
}

#endif


/**
 * Read the temperature from the sensor.
 *
 * In this example, the environment is entirely simulated and therefore the function
 * simply returns the temperature that in turn has been determined by the simulation
 * process.
 */
int read_temperature() {
	return temperature;
}


#if COOLING_ENABLED
/**
 * Start the cooling system.
 */
void start_cooling_system() {
	leds_on(LEDS_BLUE);
}


/**
 * Stop the cooling system.
 */
void stop_cooling_system() {
	leds_off(LEDS_BLUE);
}
#endif


#if HEATING_ENABLED
/**
 * Start the heating system.
 */
void start_heating_system() {
	leds_on(LEDS_RED);
}


/**
 * Stop the heating system.
 */
void stop_heating_system() {
	leds_off(LEDS_RED);
}
#endif


#if VENTILATION_ENABLED
/**
 * Start the ventilation system.
 */
void start_ventilation_system() {
	leds_on(LEDS_GREEN);
}


/**
 * Stop the ventilation system.
 */
void stop_ventilation_system() {
	leds_off(LEDS_GREEN);
}
#endif

