#include <ArduinoOTA.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <PubSubClient.h>
#include <secrets.h>

#define MQTT_VERSION MQTT_VERSION_3_1_1
const char* MQTT_CLIENT_ID = "bedroom_ledstrip";
const char* MQTT_LIGHT_STATE_TOPIC = "/house/bedroom/rgb_table/light/status";
const char* MQTT_LIGHT_COMMAND_TOPIC = "/house/bedroom/rgb_table/light/switch";
const char* MQTT_LIGHT_BRIGHTNESS_STATE_TOPIC = "/house/bedroom/rgb_table/brightness/status";
const char* MQTT_LIGHT_BRIGHTNESS_COMMAND_TOPIC = "/house/bedroom/rgb_table/brightness/set";
const char* MQTT_LIGHT_RGB_STATE_TOPIC = "/house/bedroom/rgb_table/rgb/status";
const char* MQTT_LIGHT_RGB_COMMAND_TOPIC = "/house/bedroom/rgb_table/rgb/set";
const char* LIGHT_ON = "ON";
const char* LIGHT_OFF = "OFF";

// variables used to store the state, the brightness and the color of the light
boolean m_rgb_state = false;
uint8_t m_rgb_brightness = 100;
uint8_t m_rgb_red = 255;
uint8_t m_rgb_green = 255;
uint8_t m_rgb_blue = 255;

// pins used for the rgb led (PWM)
const PROGMEM uint8_t RGB_LIGHT_RED_PIN = 12;
const PROGMEM uint8_t RGB_LIGHT_GREEN_PIN = 14;
const PROGMEM uint8_t RGB_LIGHT_BLUE_PIN = 13;

// buffer used to send/receive data with MQTT
const uint8_t MSG_BUFFER_SIZE = 20;
char m_msg_buffer[MSG_BUFFER_SIZE];

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

// function called to adapt the brightness and the color of the led
void setColor(uint8_t p_red, uint8_t p_green, uint8_t p_blue) {
  // convert the brightness in % (0-100%) into a digital value (0-255)
  uint8_t brightness = map(m_rgb_brightness, 0, 100, 0, 255);

  analogWrite(RGB_LIGHT_RED_PIN, map(p_red, 0, 255, 0, brightness));
  analogWrite(RGB_LIGHT_GREEN_PIN, map(p_green, 0, 255, 0, brightness));
  analogWrite(RGB_LIGHT_BLUE_PIN, map(p_blue, 0, 255, 0, brightness));
}

// function called to publish the state of the led (on/off)
void publishRGBState() {
  if (m_rgb_state) {
    mqttClient.publish(MQTT_LIGHT_STATE_TOPIC, LIGHT_ON, true);
  } else {
    mqttClient.publish(MQTT_LIGHT_STATE_TOPIC, LIGHT_OFF, true);
  }
}

// function called to publish the brightness of the led (0-100)
void publishRGBBrightness() {
  snprintf(m_msg_buffer, MSG_BUFFER_SIZE, "%d", m_rgb_brightness);
  mqttClient.publish(MQTT_LIGHT_BRIGHTNESS_STATE_TOPIC, m_msg_buffer, true);
}

// function called to publish the colors of the led (xx(x),xx(x),xx(x))
void publishRGBColor() {
  snprintf(m_msg_buffer, MSG_BUFFER_SIZE, "%d,%d,%d", m_rgb_red, m_rgb_green, m_rgb_blue);
  mqttClient.publish(MQTT_LIGHT_RGB_STATE_TOPIC, m_msg_buffer, true);
}

// function called when a MQTT message arrived
void mqttCallback(char* p_topic, byte* p_payload, unsigned int p_length) {
  // concat the payload into a string
  String payload;
  for (uint8_t i = 0; i < p_length; i++) {
    payload.concat((char)p_payload[i]);
  }
  // handle message topic
  if (String(MQTT_LIGHT_COMMAND_TOPIC).equals(p_topic)) {
    // test if the payload is equal to "ON" or "OFF"
    if (payload.equals(String(LIGHT_ON))) {
      if (m_rgb_state != true) {
        m_rgb_state = true;
        setColor(m_rgb_red, m_rgb_green, m_rgb_blue);
        publishRGBState();
      }
    } else if (payload.equals(String(LIGHT_OFF))) {
      if (m_rgb_state != false) {
        m_rgb_state = false;
        setColor(0, 0, 0);
        publishRGBState();
      }
    }
  } else if (String(MQTT_LIGHT_BRIGHTNESS_COMMAND_TOPIC).equals(p_topic)) {
    uint8_t brightness = payload.toInt();
    if (brightness < 0 || brightness > 255) {
      // do nothing...
      return;
    } else {
      m_rgb_brightness = brightness;
      setColor(m_rgb_red, m_rgb_green, m_rgb_blue);
      publishRGBBrightness();
    }
  } else if (String(MQTT_LIGHT_RGB_COMMAND_TOPIC).equals(p_topic)) {
    // get the position of the first and second commas
    uint8_t firstIndex = payload.indexOf(',');
    uint8_t lastIndex = payload.lastIndexOf(',');

    uint8_t rgb_red = payload.substring(0, firstIndex).toInt();
    if (rgb_red < 0 || rgb_red > 255) {
      return;
    } else {
      m_rgb_red = rgb_red;
    }

    uint8_t rgb_green = payload.substring(firstIndex + 1, lastIndex).toInt();
    if (rgb_green < 0 || rgb_green > 255) {
      return;
    } else {
      m_rgb_green = rgb_green;
    }

    uint8_t rgb_blue = payload.substring(lastIndex + 1).toInt();
    if (rgb_blue < 0 || rgb_blue > 255) {
      return;
    } else {
      m_rgb_blue = rgb_blue;
    }

    setColor(m_rgb_red, m_rgb_green, m_rgb_blue);
    publishRGBColor();
  }
}

void mqttReconnect() {
  while (!mqttClient.connected()) { //loop until we're reconnected
    Serial.println("[MQTT] INFO: Attempting connection...");
    if (mqttClient.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASSWORD)) {
      Serial.println("[MQTT] INFO: connected");
      Serial.println("[MQTT] INFO: subscribing to:");
      Serial.println(MQTT_LIGHT_BRIGHTNESS_COMMAND_TOPIC);
      Serial.println(MQTT_LIGHT_COMMAND_TOPIC);
      Serial.println(MQTT_LIGHT_RGB_COMMAND_TOPIC);
      mqttClient.subscribe(MQTT_LIGHT_BRIGHTNESS_COMMAND_TOPIC);
      mqttClient.subscribe(MQTT_LIGHT_COMMAND_TOPIC);
      mqttClient.subscribe(MQTT_LIGHT_RGB_COMMAND_TOPIC);
      publishRGBState();
      publishRGBBrightness();
      publishRGBColor();
    } else {
      Serial.print("[MQTT] ERROR: failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println("[MQTT] DEBUG: try again in 5 seconds");
      delay(2000); //wait 5 seconds before retrying
    }
  }
}

void wifiSetup() {
  Serial.print("[WIFI] INFO: Connecting to ");
  Serial.println(WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.hostname(MQTT_CLIENT_ID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD); //connect to wifi
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("[WIFI] INFO: WiFi connected");
  Serial.println("[WIFI] INFO: IP address: ");
  Serial.println(WiFi.localIP());
}

void setup() {
  // init the serial
  Serial.begin(115200);
  ArduinoOTA.onStart([]() {
    String type;
    type = "sketch";
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
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  wifiSetup();

  // init the RGB led
  pinMode(RGB_LIGHT_BLUE_PIN, OUTPUT);
  pinMode(RGB_LIGHT_RED_PIN, OUTPUT);
  pinMode(RGB_LIGHT_GREEN_PIN, OUTPUT);
  analogWriteRange(255);
  setColor(0, 0, 0);

  // init the MQTT connection
  mqttClient.setServer(MQTT_SERVER_IP, MQTT_SERVER_PORT);
  mqttClient.setCallback(mqttCallback);
}

void loop() {
  yield();
  if (!mqttClient.connected()) {
    mqttReconnect();
  }
  mqttClient.loop();
}
