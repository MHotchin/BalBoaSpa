# BalBoaSpa
Arduino comms library for Balboa Spas

Monitor and contro your Wifi connected Balboa Spa.  

You can monitor and change most 'day-to-day' settings.  Notably absent right now:
 - WiFi setup.  Best still done with the Balboa App.
 - Filter cycle times.
 
 ## Examples

Cross platform networking is a bit of a mess for Arduino, so there are three different examples that differ by how the network is declared and initialized.
 - Mega_2560:  Assumes a Wiznet style shield or add-on board, controlled over SPI
 - Wemos D1 R1: ESP8266 based board in an Uno form factor.
 - Wemos D1 R32: ESP32 based board in an Uno form factor.
 
If your project isn't covered by these examples, then you would need to:
 - Get basic netowrking up and running.  Need both UDP and TCP.
 - Change BalBoaSpa.cpp to add a new section that defines the networking classes.
 - Send the changes to me or create a pull request to get them into the project.
