
/*
* Demo for RF remote switch receiver. 
* This example is for the new KaKu / Home Easy type of remotes!
 
* For details, see NewRemoteReceiver.h!
*
* This sketch shows the received signals on the serial port.
* Connect the receiver to digital pin 2.
*/
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

#include <PubSubClient.h>
#include <NewRemoteReceiver.h>
#include <NewRemoteTransmitter.h>

WiFiClient espClient;
PubSubClient moMQTTClient(espClient);

void setupOTA()
{
   // Port defaults to 3232
   // ArduinoOTA.setPort(3232);

   // Hostname defaults to esp3232-[MAC]
   // ArduinoOTA.setHostname("esp-mqtt-kaku-bridge");

   // No authentication by default
   // ArduinoOTA.setPassword("admin");

   // Password can be set with it's md5 value as well
   // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
   // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

   ArduinoOTA.onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
         type = "sketch";
      else // U_SPIFFS
         type = "filesystem";

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println("Start updating " + type);
   });
   ArduinoOTA.onEnd([]() {
      Serial.println("\nEnd");
   });
   ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
   });
   ArduinoOTA.onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR)
         Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR)
         Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR)
         Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR)
         Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR)
         Serial.println("End Failed");
   });

   ArduinoOTA.begin();
}

void reconnectMQTT()
{
   // Loop until we're reconnected
   while (!moMQTTClient.connected())
   {
      Serial.print("Attempting MQTT connection...");
      // Create a random client ID
      String lsClientId = "esp-mqtt-kaku-bridge-";
      lsClientId += String(ESP.getChipId(), HEX);
      // Attempt to connect
      if (moMQTTClient.connect(lsClientId.c_str()))
      {
         Serial.println("connected");
         moMQTTClient.publish(lsClientId.c_str(), WiFi.localIP().toString().c_str());
         moMQTTClient.subscribe("kaku-out/#");
      }
      else
      {
         Serial.print("failed, rc=");
         Serial.print(moMQTTClient.state());
         Serial.println(" try again in 5 seconds");
         // Wait 5 seconds before retrying
         delay(5000);
      }
   }
}

void mqttMessageReceived(char *topic, byte *payload, unsigned int length)
{
   digitalWrite(LED_BUILTIN, LOW);
   // Serial.print("Message arrived [");
   // Serial.print(topic);
   // Serial.print("] ");
   // for (int i = 0; i < (int)length; i++)
   // {
   //    Serial.print((char)payload[i]);
   // }
   // Serial.println();

   // check valid payload
   bool lbOn = false;
   if (length == 0)
   {
      return;
   }
   char lcSwitch = payload[0];
   switch (lcSwitch)
   {
   case '0':
      lbOn = false;
      break;
   case '1':
      lbOn = true;
      break;
   case '2':
      // dim (not yet supported)
      break;
   }

   unsigned long address;
   int unit;
   sscanf(topic, "kaku-out/%lu/%d", &address, &unit);

   char msg[100];
   sprintf(msg, "Transmitting on address %lu for unit %d with value %s", address, unit, lbOn ? "ON" : "OFF");
   Serial.println(msg);

   //Serial.println(address);
   //Serial.println(unit);

   NewRemoteReceiver::disable();

   NewRemoteTransmitter loTransmitter(address, D1);

   // Send to all units if the specified unit-nr is -1
   if (unit < 0)
   {
      loTransmitter.sendGroup(lbOn);
   }
   else
   {
      // Send to a single unit
      loTransmitter.sendUnit(unit, lbOn);
   }

   NewRemoteReceiver::enable();
   delay(100);
   digitalWrite(LED_BUILTIN, HIGH);
}

volatile bool mbCodeReceived = false;
NewRemoteCode moLastReceived;

// Callback function is called only when a valid code is received.
void showCode(NewRemoteCode receivedCode)
{
   if (mbCodeReceived)
   {
      // ignore
      return;
   }
   mbCodeReceived = true;
   moLastReceived = receivedCode;

   // Note: interrupts are disabled. You can re-enable them if needed.

   // Print the received code.
   Serial.print("Addr ");
   Serial.print(receivedCode.address);

   if (receivedCode.groupBit)
   {
      Serial.print(" group");
   }
   else
   {
      Serial.print(" unit ");
      Serial.print(receivedCode.unit);
   }

   switch (receivedCode.switchType)
   {
   case NewRemoteCode::off:
      Serial.print(" off");
      break;
   case NewRemoteCode::on:
      Serial.print(" on");
      break;
   case NewRemoteCode::dim:
      Serial.print(" dim");
      break;
   }

   if (receivedCode.dimLevelPresent)
   {
      Serial.print(", dim level: ");
      Serial.print(receivedCode.dimLevel);
   }

   Serial.print(", period: ");
   Serial.print(receivedCode.period);
   Serial.println("us.");
}

// void retransmitter(NewRemoteCode receivedCode) {
//   // Disable the receiver; otherwise it might pick up the retransmit as well.
//   NewRemoteReceiver::disable();

//   // Need interrupts for delay()
//   interrupts();

//   // Wait 5 seconds before sending.
//   delay(5000);

//   // Create a new transmitter with the received address and period, use digital pin 11 as output pin

//   NewRemoteTransmitter transmitter(receivedCode.address, D1, receivedCode.period);

//   if (receivedCode.switchType == NewRemoteCode::dim ||
//     (receivedCode.switchType == NewRemoteCode::on && receivedCode.dimLevelPresent)) {
//     // Dimmer signal received

//     if (receivedCode.groupBit) {
//       transmitter.sendGroupDim(receivedCode.dimLevel);
//     }
//     else {
//       transmitter.sendDim(receivedCode.unit, receivedCode.dimLevel);
//     }
//   }
//   else {
//     // On/Off signal received
//     bool isOn = receivedCode.switchType == NewRemoteCode::on;

//     if (receivedCode.groupBit) {
//       // Send to the group
//       transmitter.sendGroup(isOn);
//     }
//     else {
//       // Send to a single unit
//       transmitter.sendUnit(receivedCode.unit, isOn);
//     }
//   }

//   NewRemoteReceiver::enable();
// }

void setup()
{
   Serial.begin(9600);
   
   WiFi.persistent(false);
   WiFi.mode(WIFI_AP_STA);
   WiFi.begin("Ziggo1968183", "rmt5knjyHqRn");
   Serial.print("Connecting");
   while (WiFi.status() != WL_CONNECTED)
   {
      delay(500);
      Serial.print(".");
   }
   Serial.println();

   Serial.print("Connected, IP address: ");
   Serial.println(WiFi.localIP());

   moMQTTClient.setServer("192.168.2.95", 1883);
   moMQTTClient.setCallback(mqttMessageReceived);

   setupOTA();

   // NewRemoteTransmitter transmitter(receivedCode.address, D1, receivedCode.period);

   // Initialize receiver on interrupt 0 (= digital pin 2), calls the callback "showCode"
   // after 2 identical codes have been received in a row. (thus, keep the button pressed
   // for a moment)
   //
   // See the interrupt-parameter of attachInterrupt for possible values (and pins)
   // to connect the receiver.
   NewRemoteReceiver::init(digitalPinToInterrupt(D2), 1, showCode);
}

void loop()
{
   // Maintain MQTT connection
   if (!moMQTTClient.connected())
   {
      reconnectMQTT();
   }
   moMQTTClient.loop();

   if (mbCodeReceived)
   {
      char lsTopic[100];
      sprintf(lsTopic, "kaku-in/%lu/%d", moLastReceived.address, moLastReceived.groupBit ? -1 : (int)moLastReceived.unit);
      moMQTTClient.publish(lsTopic, moLastReceived.switchType == NewRemoteCode::on ? "1" : "0");
      mbCodeReceived = false;
   }

   // Support over-the-air firmware upgrades
   ArduinoOTA.handle();

   delay(10);
}