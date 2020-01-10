

#ifndef  ARDUINO_ARCH_ESP32
#error Wrong architecture, this code is for ESP32 based boards.
#endif

#include <Arduino.h>
#include <WiFi.h>
#include <BalBoaSpa.h>

const char *ssid = "YourWifiNetworkNameHere";   //  Add your Wifi netowrk name here
const char *passphrase = "YourWifiPasswordHere";   //  Add your WiFi password here

namespace
{
    BalBoa::BalBoaSpa Spa;
}

void 
setup() 
{
	Serial.begin(115200);
	while (!Serial);

    WiFi.begin(ssid, passphrase);

    unsigned long tNow = millis();

    //  Wait up to 10 seconds to connect.
    while ((WiFi.status() != WL_CONNECTED) && (millis() - tNow < 10000))
    {
        delay(250);
        Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.println("");
        Serial.println("WiFi connected");
        Serial.print("IP address: "), Serial.println(WiFi.localIP());
    }
    else
    {
        Serial.println();
        Serial.print("Unable to connect to "), Serial.println(ssid);
    }

    //  Default is to retrieve updates from the spa every 60 seconds.  ALL data is
    //  initially marked as 'changed', so you shouldn't need to have special
    //  initialization code.
    Spa.begin();

}

// the loop function runs over and over again forever
void 
loop() 
{
    //  Find out what has changed at the Spa.
    unsigned int changes = Spa.GetChanges();

    //  Acknowledge individual changes by retrieving the corresponding information.
    //  Any particular item you don't care about you can just ignore completely.

    if (changes & BalBoa::scTime)
    {
        BalBoa::SpaTime time = Spa.GetSpaTime();
        Serial.println(F("Time changed!"));
    }

    if (changes & BalBoa::scTemp)
    {
        BalBoa::SpaTemp temp = Spa.GetSpaTemp();
        Serial.println(F("Temp Changed!"));
    }

    if (changes & BalBoa::scSetPoint)
    {
        BalBoa::SpaTemp temp = Spa.GetSetTemp();
        BalBoa::TriState ts1 = Spa.IsHighRange();
    
        Serial.println(F("Set point changed!"));
    }

    if (changes & BalBoa::scPump1)
    {
        BalBoa::PumpSpeed ps = Spa.GetPump1Speed();
        Serial.println(F("Pump 1 speed changed!"));
    }

    if (changes & BalBoa::scPump2)
    {
        BalBoa::PumpSpeed ps = Spa.GetPump2Speed();
        Serial.println(F("Pump 2 speed changed!"));
    }

    if (changes & BalBoa::scRecirc)
    {
        //  Not yet implemented
    }

    if (changes & BalBoa::scHeating)
    {
        BalBoa::TriState ts = Spa.IsHeating();

        Serial.println(F("Heating status changed!"));
    }

    if (changes & BalBoa::scFilterTimes)
    {
        BalBoa::FilterInfo fi = Spa.GetFilterInfo();

        Serial.println(F("Filter info changed!"));
    }

    if (changes & BalBoa::scLights)
    {
        BalBoa::TriState ts = Spa.IsLightOn();

        Serial.println(F("Light changed!"));
    }

    if (changes & BalBoa::scVersion)
    {
        BalBoa::VersionInfo vi = Spa.GetVersion();

        Serial.println(F("Version info changed!"));
    }

    if (changes & BalBoa::scFilterRunning)
    {
        BalBoa::RunningFilter rf = Spa.GetRunningFilter();

        Serial.println(F("Filter running changed!"));
    }

    if (changes & BalBoa::scPanelMessages)
    {
        //  Spa shows messages one at a time, so we expect that only one message would
        //  come back at a time.  Just in case, message IDs are bits.
        uint8_t pm = Spa.GetPanelMessages();
    
        //  There doesn't seem to be a way to dismiss the message except at the touch
        //  panel.

        Serial.print(F("Panel Message!"));
    }
 
    if (!Spa)
    {
        //  Spa networking can be kind of flakey.  This check will try to re-establish 
        //  communications if the spa isn't responding.
     
        //  Again, using default of updates every 60 seconds.
        Spa.begin();
    }
}
