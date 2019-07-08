## IoT - Smart thermostat
### How to run the simulation
* Set the Contiki home path by changing the variable `CONTIKI` in **sensor/Makefile** and **border-router/Makefile**
* Open Cooja and load the file **simulation.csc**
* Start the simulation
* Open a terminal in the **tunslip6** folder
* Run `make tunslip6`
* Run `sudo ./tunslip6 -a 127.0.0.1 aaaa::1/64`
* Open another terminal and run `node-red`

### How to view dashboard and data
* The dashboard is available at http://127.0.0.1:1880/ui
* The Thingspeak channel of home temperature is available [here](https://thingspeak.com/channels/803420)
* The Thingspeak channel of sensors temperatures is available [here](https://thingspeak.com/channels/805784)
