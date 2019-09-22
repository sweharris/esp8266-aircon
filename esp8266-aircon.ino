// This code is to make a dumb air-con semi smart.
// The idea is to use a photoresistor to detect if there's an
// LED lit on the control panel.  This means the aircon is "on".
// (we can't use power-draw 'cos the aircon has ECO mode, so
// it may be "on" but not drawing power.)
//
// We also have a IR transmittor.  So we can send the POWER
// message to remotely turn on/off the aircon.
//
// We connect to an MQTT server and send on a status channel the
// state of the aircon (ON/OFF) every second, and we listen to a
// control channel to receive commands
//    POWER ON    (turn on aircon; only sends IR signal if state is OFF)
//    POWER OFF   (turn off aircon; only sends IR signal if state is ON)
//    POWER       (always send IR signal)
//
// The MQTT channels are based off the word "aircon" and the last 6 digits of the MAC
//
//  e.g  aircon/123456/status
//       aircon/123456/control
//       aircon/123456/debug
//
// Uses PubSubClient and IRremoteESP8266 libraries on top of the ESP8266WiFi one
//
// On my NodeMCU
//   Connect the photoresister between A0 and 3V3
//   Connect a 10K resister between A0 and GND (to pull-down)
//   Connect the IR transmitter between D2 and Gnd (IR diode + goes to D2)
//
// My Aircon is a Friedrich CP24F30, so the IR code embedded is
// the power signal for that.
// See https://github.com/markszabo/IRremoteESP8266 if you want to learn your
// own IR message
//
// GPL2.0 or later.  See GPL LICENSE file for details.
// Original code by Stephen Harris, May 2019

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>

// This include file needs to define the following
//   const char* ssid       = "your-ssid";
//   const char* password   = "your-password";
//   const char* mqttServer = "your-mqtt-server";
//   const int   mqttPort   = 1883;
// I did it this way so I can ignore the file from git
#include "network_conn.h"

#define _mqttBase    "aircon"

char mqttChannel[30]; // Enough for Base + "/" + 6 hex digits + "/" + channel
char mqttControl[30];
char mqttDebug[30];

// The transition value read from the analog port; below this is "dark" (OFF)
// In practice, in my setup, dark appears to be around < 15 and light > 90, so
// this should(!) be a good middle ground.
// (The values are so low 'cos the ESP8266 only has a 3.3V line, and the AirCon
// LEDs aren't that bright, so the photoresistor is still pretty resisting!
// You may need to tune this value for your setup.
const int threshold = 60;

// The resistance of a photoresistor isn't very stable even when pointing to a
// constant light source.  Most of the time it's close, but there's the occasional
// piece of noise.  So we calculate a 5 point rolling average and use that as the
// value to compare.  This may mean a couple of seconds delays in reacting to change,
// but that's not a big concern
int av[5] = {0, 0, 0, 0, 0};

// The last read state of the light
String state = "-";

WiFiClient espClient;
PubSubClient client(espClient);
IRsend irsend(D2);

// The standard IR routines expect an array such as
//   uint16_t data[20]={....}
// and then call irsend.sendRaw(data,20,38)
// but I put the length of the data as the first element of the array and my do_ir() function
// will read that.
//
// This is DiscreteOn for my Samsung TV; it produces a message on the screen so a good debug string
// uint16_t POWER[]={68,4507, 4507, 573, 1694, 573, 1694, 573, 1694, 573, 573,
//  573, 573, 573, 573, 573, 573, 573, 573, 573, 1694, 573, 1694, 573, 1694, 573,
//  573, 573, 573, 573, 573, 573, 573, 573, 573, 573, 1694, 573, 573, 573, 573, 573,
//  1694, 573, 1694, 573, 573, 573, 573, 573, 1694, 573, 573, 573, 1694, 573, 1694,
//  573, 573, 573, 573, 573, 1694, 573, 1694, 573, 573, 573, 46560};

// Friedrich Aircon.  Apparently PROTON  215 -1 11
uint16_t POWER[] = {38, 7994, 3997, 500, 1499, 500, 1499, 500, 1499, 500, 500,
                    500, 1499, 500, 500, 500, 1499, 500, 1499, 500, 3997, 500,
                    1499, 500, 1499, 500, 500, 500, 1499, 500, 500, 500, 500,
                    500, 500, 500, 500, 500, 20984
                   };

void do_ir(uint16_t raw[])
{
  int l = raw[0];
  raw++;

  irsend.sendRaw(raw, l, 38);
}

// Only makes sense to call this after the WiFi has connected and time determined.
void log_msg(String msg)
{
  time_t now = time(nullptr);
  String tm = ctime(&now);
  tm.trim();
  tm = tm + ": " + msg;
  Serial.println(tm);
  client.publish(mqttDebug, tm.c_str());
}

// This is called via the main loop when a message appears on the MQTT queue
void callback(char* topic, byte* payload, unsigned int length)
{
  // Convert the message to a string.  We can't use a simple constructor 'cos the
  // byte array isn't null terminated
  String msg;
  for (int i = 0; i < length; i++)
  {
    msg += char(payload[i]);
  }
  log_msg("Message arrived [" + String(topic) + "] " + msg);

  // Switch the LED on/off
  if (msg == "POWER ON" && state == "OFF")
  {
    log_msg("Power on");
    do_ir(POWER);
  }
  else if (msg == "POWER OFF" && state == "ON")
  {
    log_msg("Power off");
    do_ir(POWER);
  }
  else if (msg == "POWER")
  {
    log_msg("Toggling power");
    do_ir(POWER);
  }
  else
  {
    log_msg("Ignoring");
  }
}

void setup()
{
  Serial.begin(115200);
  irsend.begin();

  // Let's create the channel names based on the MAC address
  unsigned char mac[6];
  char macstr[7];
  WiFi.macAddress(mac);
  sprintf(macstr, "%02X%02X%02X", mac[3], mac[4], mac[5]);

  sprintf(mqttChannel, "%s/%s/status",  _mqttBase, macstr);
  sprintf(mqttControl, "%s/%s/control", _mqttBase, macstr);
  sprintf(mqttDebug,   "%s/%s/debug",  _mqttBase, macstr);

  delay(500);

  Serial.println();
  Serial.print("MAC: ");
  Serial.println(WiFi.macAddress());

  WiFi.mode(WIFI_STA);  // Ensure we're in station mode and never start AP
  WiFi.begin(ssid, password);
  Serial.print("Connecting to ");
  Serial.print(ssid);
  Serial.print(": ");

  // Wait for the Wi-Fi to connect
  int i = 0;
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(1000);
    Serial.print(++i);
    Serial.print(' ');
  }

  Serial.println();
  Serial.println("Connection established!");
  Serial.print("IP address:\t");
  Serial.println(WiFi.localIP());

  // Get the current time.  Initial log lines may be wrong 'cos
  // it's not instant.  Timezone handling... eh, this is just for
  // debuging, so I don't care.  GMT is always good :-)
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  delay(1000);

  // Now we're on the network, setup the MQTT client
  client.setServer(mqttServer, mqttPort);
  client.setCallback(callback);

#ifdef NETWORK_UPDATE
    __setup_updater();
#endif

}


void loop()
{
  // Try to reconnect to MQTT each time around the loop, in case we disconnect
  while (!client.connected())
  {
    log_msg("Connecting to MQTT Server " + String(mqttServer));

    // Generate a random ID each time
    String clientId = "ESP8266Client-aircon-";
    clientId += String(random(0xffff), HEX);

    if (client.connect(clientId.c_str())) {
      log_msg("MQTT connected.  I am " + WiFi.localIP().toString());
      client.subscribe(mqttControl);
    } else {
      log_msg("failed with state " + client.state());
      delay(2000);
    }
  }

  // Check the MQTT queue for messages
  client.loop();

  // Read the current state of the photoresistor
  int new_val = analogRead(A0);

  // Calculate the new rolling average;
  int new_av = new_val;
  for (int i = 0; i < 4; i++)
  {
    av[i] = av[i + 1];
    new_av += av[i];
  }
  av[4] = new_val;
  new_av = new_av / 5;

  String new_state = (new_av > threshold) ? "ON" : "OFF";

  char dbg_buffer[50];
  snprintf(dbg_buffer, 50, "%4d %4d %4d %4d %4d ==> %4d", av[0], av[1], av[2], av[3], av[4], new_av);
  log_msg(String(dbg_buffer) + " " + new_state);

  // Tell the world!
  client.publish(mqttChannel, new_state.c_str());

  // Debug log message if things have changed.
  if (state != new_state )
  {
    log_msg("Transition from " + state + " to " + new_state);
    state = new_state;
  }

#ifdef NETWORK_UPDATE
  __netupdateServer.handleClient();
#endif

  delay(1000);
}
