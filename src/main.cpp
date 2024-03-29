
#include <Arduino.h>

#include <uFire_SHT20_JSON.h>
#include <uFire_SHT20_MP.h>
#include <uFire_SHT20.h>

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>

#include <ArduinoJson.h>

#include <WebSocketsClient.h>
#include <SocketIOclient.h>

#include <Hash.h>

uFire_SHT20 sht20;
ESP8266WiFiMulti WiFiMulti;
SocketIOclient socketIO;

StaticJsonDocument<200> doc;

#define USE_SERIAL Serial

void execCommand(const char *event, StaticJsonDocument<200> doc)
{
  if (strcmp(event, "exec") != 0)
  {
    return;
  }
  const char *args = ((const char *)doc["command"]);
  USE_SERIAL.printf("[IOc] Connected to url: %s\n", args);
  if (strcmp(args, "LED_ON") == 0)
  {
    digitalWrite(LED_BUILTIN, LOW);
  }
  if (strcmp(args, "LED_OFF") == 0)
  {
    digitalWrite(LED_BUILTIN, HIGH);
  }
}
void parseEvent(uint8_t *payload)
{
  DeserializationError error;
  error = deserializeJson(doc, payload);
  if (error)
  {
    return;
  }
  execCommand((const char *)doc[0], doc[1]);
}
void socketIOEvent(socketIOmessageType_t type, uint8_t *payload, size_t length)
{

  switch (type)
  {
  case sIOtype_DISCONNECT:
    USE_SERIAL.printf("[IOc] Disconnected!\n");
    break;
  case sIOtype_CONNECT:
    USE_SERIAL.printf("[IOc] Connected to url: %s\n", payload);
    break;
  case sIOtype_EVENT:
    parseEvent(payload);
    break;
  case sIOtype_ACK:
    USE_SERIAL.printf("[IOc] get ack: %u\n", length);
    hexdump(payload, length);
    break;
  case sIOtype_ERROR:
    USE_SERIAL.printf("[IOc] get error: %u\n", length);
    hexdump(payload, length);
    break;
  case sIOtype_BINARY_EVENT:
    USE_SERIAL.printf("[IOc] get binary: %u\n", length);
    hexdump(payload, length);
    break;
  case sIOtype_BINARY_ACK:
    USE_SERIAL.printf("[IOc] get binary ack: %u\n", length);
    hexdump(payload, length);
    break;
  }
}

void setup()
{
  Wire.begin();
  sht20.begin();
  pinMode(LED_BUILTIN, OUTPUT);

  USE_SERIAL.begin(115200);

  USE_SERIAL.setDebugOutput(true);

  USE_SERIAL.println();
  USE_SERIAL.println();
  USE_SERIAL.println();

  for (uint8_t t = 2; t > 0; t--)
  {
    USE_SERIAL.printf("[SETUP] BOOT WAIT %d...\n", t);
    USE_SERIAL.flush();
    delay(1000);
  }

  // disable AP
  if (WiFi.getMode() & WIFI_AP)
  {
    WiFi.softAPdisconnect(true);
  }

  WiFiMulti.addAP("licenta", "123456789");

  //WiFi.disconnect();
  while (WiFiMulti.run() != WL_CONNECTED)
  {
    delay(100);
  }

  String ip = WiFi.localIP().toString();
  USE_SERIAL.printf("[SETUP] WiFi Connected %s\n", ip.c_str());

  // server address, port and URL
  socketIO.begin("raspberrypi.local", 3000);

  // event handler
  socketIO.onEvent(socketIOEvent);
}

unsigned long messageTimestamp = 0;
void loop()
{
  socketIO.loop();

  uint64_t now = millis();

  if (now - messageTimestamp > 2000)
  {
    messageTimestamp = now;

    // creat JSON message for Socket.IO (event)
    DynamicJsonDocument doc(1024);
    JsonArray array = doc.to<JsonArray>();

    // add evnet name
    // Hint: socket.on('event_name', ....
    array.add("log");

    // add payload (parameters) for the event
    JsonObject param1 = array.createNestedObject();
    param1["mac"] = WiFi.macAddress();
    param1["uptime"] = (uint32_t)now;
    param1["temperature"] = sht20.temperature();
    param1["humidity"] = sht20.humidity();
    // JSON to String (serializion)
    String output;
    serializeJson(doc, output);

    // Send event
    socketIO.sendEVENT(output);
    // Print JSON for debugging
    // USE_SERIAL.println(output);
  }
}
