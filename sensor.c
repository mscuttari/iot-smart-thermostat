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

#if COOLING_ENABLED
PROCESS(cooling_process, "Cooling management");
#endif

#if HEATING_ENABLED
PROCESS(heating_process, "Heating management");
#endif

#if VENTILATION_ENABLED
PROCESS(ventilation_process, "Ventilation management");
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

RESOURCE(info, METHOD_GET, "info", "title=\"Get sensor info\";rt=\"Text\"");

void info_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset) {
  const char *len = NULL;
  char const * const message = "Hello World!";
  int length = 12;

  if (REST.get_query_variable(request, "len", &len)) {
    length = atoi(len);
    if (length<0) length = 0;
    if (length>REST_MAX_CHUNK_SIZE) length = REST_MAX_CHUNK_SIZE;
    memcpy(buffer, message, length);
  } else {
    memcpy(buffer, message, length);
  }

  REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
  REST.set_header_etag(response, (uint8_t *) &length, 1);
  REST.set_response_payload(response, buffer, length);
}


// Start the boot process
AUTOSTART_PROCESSES(&boot_process);


/**
 * Boot process.
 * Used to initialize some variables, such as the temperature, and start all the services.
 */
PROCESS_THREAD(boot_process, ev, data) {
	PROCESS_BEGIN();
	PRINTF("[BOOT] Starting...\n");
	
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
	
	process_start(&cooling_process, NULL);
	
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
 * Manage the cooling system.
 */
#if COOLING_ENABLED
PROCESS_THREAD(cooling_process, ev, data) {
	PROCESS_BEGIN();
	
	PRINTF("[COOLING] starting\n");
	status = COOLING;
	start_cooling_system();
	
	#if SIMULATION
	process_post(&temperature_simulation, PROCESS_EVENT_MSG, NULL);
	#endif
	
	PRINTF("[COOLING] started\n");
	
	while (1) {
		PROCESS_WAIT_EVENT_UNTIL(ev == PROCESS_EVENT_EXIT);
	
		PRINTF("[COOLING] stopping\n");
		stop_cooling_system();
		status = NONE;
		
		#if SIMULATION
		process_post(&temperature_simulation, PROCESS_EVENT_MSG, NULL);
		#endif
		
		PRINTF("[COOLING] stopped\n");
	}
	
	PROCESS_END();
}
#endif


/**
 * Manage the heating system.
 */
#if HEATING_ENABLED
PROCESS_THREAD(heating_process, ev, data) {
	PROCESS_BEGIN();
	
	PRINTF("[HEATING] starting\n");
	status = HEATING;
	start_heating_system();
	
	#if SIMULATION
	process_post(&temperature_simulation, PROCESS_EVENT_MSG, NULL);
	#endif
	
	PRINTF("[HEATING] started\n");
	
	while (1) {
		PROCESS_WAIT_EVENT_UNTIL(ev == PROCESS_EVENT_EXIT);
	
		PRINTF("[HEATING] stopping\n");
		stop_heating_system();
		status = NONE;
		
		#if SIMULATION
		process_post(&temperature_simulation, PROCESS_EVENT_MSG, NULL);
		#endif
		
		PRINTF("[HEATING] stopped\n");
	}
	
	PROCESS_END();
}
#endif


/**
 * Manage the ventilation system.
 */
#if VENTILATION_ENABLED
PROCESS_THREAD(ventilation_process, ev, data) {
	PROCESS_BEGIN();
	
	PRINTF("[VENTILATION] starting\n");
	ventilation = true;
	start_ventilation_system();
	PRINTF("[VENTILATION] started\n");
	
	while (1) {
		PROCESS_WAIT_EVENT_UNTIL(ev == PROCESS_EVENT_EXIT);
	
		PRINTF("[VENTILATION] stopping\n");
		ventilation = false;
		stop_ventilation_system();
		PRINTF("[VENTILATION] stopped\n");
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
	rest_activate_resource(&resource_info);
	//rest_activate_resource(&res_cooling, "systems/cooling");
	//rest_activate_resource(&res_heating, "systems/heating");
	//rest_activate_resource(&res_ventilation, "systems/ventilation");

	PRINTF("[REST] server started\n");
	PROCESS_END();
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

