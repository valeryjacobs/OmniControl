#include <Bounce2.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>
#include <EEPROM.h>
#include <EepromUtil.h>

#include <DNSServer.h>            //Local DNS Server used for redirecting all requests to the configuration portal
#include <ESP8266WebServer.h>     //Local WebServer used to serve the configuration portal
#include <WiFiManager.h>

#include <Ticker.h>
Ticker ticker;

String pw;
const char* mqtt_server = "test.mosquitto.org";

WiFiClient espClient;
PubSubClient mqttClient(espClient);
long lastMsg = 0;
char msg[50];
int value = 0;

const char* outTopic = "ORClientOut";
const char* inTopic = "ORClientIn";

String displayState = "-";
String displayInitState = "State:";

String dIP = "0.0.0.0";
String dWifiStrength = "0.0";
String dIncomingMessage = "none";
String dCurrentCommand = "Default command";
String dLastButtonClick = "Button -";
String dLog = "-";

//int relay_pin = 0;
int button1_pin = 13;
int button2_pin = 12;
int button3_pin = 14;
bool relayState = LOW;

const int BUFSIZE = 50;
char buf[BUFSIZE];

Bounce debouncerButton1 = Bounce();
Bounce debouncerButton2 = Bounce();
Bounce debouncerButton3 = Bounce();

#include <Wire.h> 
#include "SSD1306.h"
SSD1306  display(0x3c, 4, 5);

void tick()
{
  //toggle state
  int state = digitalRead(BUILTIN_LED);  // get the current state of GPIO1 pin
  digitalWrite(BUILTIN_LED, !state);     // set pin to the opposite state
}

void setup() {
  EEPROM.begin(512);              // Begin eeprom to store on/off state

  //pinMode(relay_pin, OUTPUT);
  pinMode(button1_pin, INPUT_PULLUP);
  pinMode(button2_pin, INPUT_PULLUP);
  pinMode(button3_pin, INPUT_PULLUP);
  //pinMode(13, OUTPUT);
  relayState = EEPROM.read(0);
  //digitalWrite(relay_pin, relayState);

  debouncerButton1.attach(button1_pin);   // Use the bounce2 library to debounce the built in button
  debouncerButton2.attach(button2_pin);
  debouncerButton3.attach(button3_pin);

  debouncerButton1.interval(5);         // Input must be low for 50 ms
  debouncerButton2.interval(5);
  debouncerButton3.interval(5);

  digitalWrite(BUILTIN_LED, LOW);          // Blink to indicate setup
  delay(500);
  digitalWrite(BUILTIN_LED, HIGH);
  delay(500);

  Serial.begin(115200);
  setup_wifi();                   // Connect to wifi
  setup_Display();
  mqttClient.setServer(mqtt_server, 1883);
  mqttClient.setCallback(callback);
}

void setup_Display()
{
  display.init();
  display.flipScreenVertically();
  display.setContrast(255);

  ArduinoOTA.begin();
  ArduinoOTA.onStart([]() {
    display.clear();
    display.setFont(ArialMT_Plain_10);
    display.setTextAlignment(TEXT_ALIGN_CENTER_BOTH);
    display.drawString(DISPLAY_WIDTH / 2, DISPLAY_HEIGHT / 2 - 10, "Update");
    display.display();
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    display.drawProgressBar(4, 32, 120, 8, progress / (total / 100) );
    display.display();
  });

  ArduinoOTA.onEnd([]() {
    display.clear();
    display.setFont(ArialMT_Plain_10);
    display.setTextAlignment(TEXT_ALIGN_CENTER_BOTH);
    display.drawString(DISPLAY_WIDTH / 2, DISPLAY_HEIGHT / 2, "Restart");
    display.display();
  });

  // Align text vertical/horizontal center
  display.setTextAlignment(TEXT_ALIGN_CENTER_BOTH);
  display.setFont(ArialMT_Plain_10);
  display.drawString(DISPLAY_WIDTH / 2, DISPLAY_HEIGHT / 2, "Ready for update:\n" + WiFi.localIP().toString());
  display.display();
}

void setup_wifi() {
  pinMode(BUILTIN_LED, OUTPUT);
  // start ticker with 0.5 because we start in AP mode and try to connect
  ticker.attach(0.6, tick);
  
  dLog = "Connecting...";
  
  WiFiManager wifiManager;
  //wifiManager.resetSettings();
  wifiManager.setAPCallback(configModeCallback);

  if (!wifiManager.autoConnect()) {
    Serial.println("failed to connect and hit timeout");
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(1000);
  }

  Serial.println("Connected!)");
  ticker.detach();
  //keep LED on
  digitalWrite(BUILTIN_LED, LOW);
  

  dWifiStrength = WiFi.RSSI();
  dIP = IpAddress2String(WiFi.localIP());

  delay(10);

  while (WiFi.status() != WL_CONNECTED) {
    extButton();
    for (int i = 0; i < 500; i++) {
      extButton();
      delay(1);
    }
    Serial.print(".");
  }
  digitalWrite(BUILTIN_LED, LOW);
  delay(500);
  digitalWrite(BUILTIN_LED, HIGH);
  delay(500);
  digitalWrite(BUILTIN_LED, LOW);
  delay(500);
  digitalWrite(BUILTIN_LED, HIGH);
  Serial.println("");
  Serial.println("WiFi connected");

  dLog = "Connected";

  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());

  Serial.println(myWiFiManager->getConfigPortalSSID());
  ticker.attach(0.2, tick);
}

void loop() {

  if (!mqttClient.connected()) {
    Serial.println("Disconnect detected...");
    reconnect();
  }
  
  mqttClient.loop();
  extButton();

  ArduinoOTA.handle();

  UpdateUI();
}

void UpdateUI()
{
  display.clear();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawString(0, 0, dIP); //"ABCDEFGHIJKLMNOPQRS"
  display.drawString(100, 0, dWifiStrength); //"ABCDEFGHIJKLMNOPQRS"
  display.drawString(0, 13, dCurrentCommand);
  display.drawHorizontalLine(0, 24, 200);
  display.setFont(ArialMT_Plain_16);
  display.drawString(0, 26, dIncomingMessage);
  display.drawHorizontalLine(0, 44, 200);
  display.setFont(ArialMT_Plain_10);
  display.drawString(0, 52, dLastButtonClick);
  display.drawString(79, 52, dLog);
  display.display();
}



void ShowScreen1()
{
  display.clear();
  display.setFont(ArialMT_Plain_24);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawString(0, 0, "Submitting command..."); //"ABCDEFGHIJKLMNOPQRS"
  display.display();
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  dLog = "Message arrived";

  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }

  Serial.println(String((char *)payload));
  dIncomingMessage = String((char *)payload);
  dCurrentCommand = dIncomingMessage;

  Serial.println();

  // Switch on the LED if an 1 was received as first character
  if ((char)payload[0] == '0') {
    //digitalWrite(relay_pin, LOW);   // Turn the LED on (Note that LOW is the voltage level
    Serial.println("relay_pin -> LOW");
    relayState = LOW;
    EEPROM.write(0, relayState);    // Write state to EEPROM
    EEPROM.commit();
  } else if ((char)payload[0] == '1') {
   // digitalWrite(relay_pin, HIGH);  // Turn the LED off by making the voltage HIGH
    Serial.println("relay_pin -> HIGH");
    relayState = HIGH;
    EEPROM.write(0, relayState);    // Write state to EEPROM
    EEPROM.commit();
  } else if ((char)payload[0] == '2') {
    relayState = !relayState;
    //digitalWrite(relay_pin, relayState);  // Turn the LED off by making the voltage HIGH
    Serial.print("relay_pin -> switched to ");
    Serial.println(relayState);
    EEPROM.write(0, relayState);    // Write state to EEPROM
    EEPROM.commit();
  }
}

void reconnect() {
  // Loop until we're reconnected
  while (!mqttClient.connected()) {
    //Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (mqttClient.connect("ESP8266Client")) {
      Serial.println("Connected to MQTT broker.");
      // Once connected, publish an announcement...
      mqttClient.publish(outTopic, "ESP8266 booted");
      // ... and resubscribe
      mqttClient.subscribe(inTopic);
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
     // for (int i = 0; i < 5000; i++) {
      //  extButton();
      //  delay(1);
     // }
    }
  }
}

void extButton() {
  debouncerButton1.update();
  debouncerButton2.update();
  debouncerButton3.update();

  // Call code if Bounce fell (transition from HIGH to LOW) :
  if ( debouncerButton1.fell() ) {

    dWifiStrength = WiFi.RSSI();
    dLastButtonClick = "Button 1";

    Serial.println("debouncerButton1 fell");
    // Toggle relay state :
    relayState = !relayState;
   // digitalWrite(relay_pin, relayState);
    EEPROM.write(0, relayState);    // Write state to EEPROM
    ShowScreen1();
    mqttClient.publish(outTopic, "1");
    
  }

  if ( debouncerButton2.fell() ) {
    Serial.println("debouncerButton2 fell");
    ShowScreen1();
    mqttClient.publish(outTopic, "2");
  }

  if ( debouncerButton3.fell() ) {
    Serial.println("debouncerButton3 fell");
    ShowScreen1();
    mqttClient.publish(outTopic, "3");
  }
}

String IpAddress2String(const IPAddress& ipAddress)
{
  return String(ipAddress[0]) + String(".") + \
         String(ipAddress[1]) + String(".") + \
         String(ipAddress[2]) + String(".") + \
         String(ipAddress[3])  ;
}
