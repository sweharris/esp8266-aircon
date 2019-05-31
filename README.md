# esp8266-aircon
Use an esp8266 with a photoresistor and a IR transmitter to allow network control of an aircon

You will need to make a file `network_conn.h` with contents similar to

    const char* ssid       = "your-ssid";
    const char* password   = "your-wpa-passwd";
    const char* mqttServer = "your-MQTT-server";
    const int mqttPort     = 1883;

in order to provide details of your network and MQTT server

    This code is to make a dumb air-con semi smart.
    The idea is to use a photoresistor to detect if there's an
    LED lit on the control panel.  This means the aircon is "on".
    (we can't use power-draw 'cos the aircon has ECO mode, so
    it may be "on" but not drawing power.)
   
    We also have a IR transmittor.  So we can send the POWER
    message to remotely turn on/off the aircon.
   
    We connect to an MQTT server and send on a status channel the
    state of the aircon (ON/OFF) every second, and we listen to a
    control channel to receive commands
       POWER ON    (turn on aircon; only sends IR signal if state is OFF)
       POWER OFF   (turn off aircon; only sends IR signal if state is ON)
       POWER       (always send IR signal)
   
    The MQTT channels are based off the word "aircon" and the last 6 digits of the MAC
   
     e.g  aircon/123456/status
          aircon/123456/control
          aircon/123456/debug
   
    Uses PubSubClient and IRremoteESP8266 libraries on top of the ESP8266WiFi one
   
    On my NodeMCU
      Connect the photoresister between A0 and 3V3
      Connect a 10K resister between A0 and GND (to pull-down)
      Connect the IR transmitter between D2 and Gnd (IR diode + goes to D2)
   
    My Aircon is a Friedrich CP24F30, so the IR code embedded is
    the power signal for that.
    See https://github.com/markszabo/IRremoteESP8266 if you want to learn your
    own IR message
   
    GPL2.0 or later.  See GPL LICENSE file for details.
    Original code by Stephen Harris, May 2019
