#include <Arduino.h>
#include <stdio.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <WebSocketsServer.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <Hash.h>
#include <MPU9250.h>
#include <NtpClientLib.h>
#include "lwip/err.h"
#include "lwip/dns.h"

#define SerialDebug false

ADC_MODE(ADC_VCC);

MPU9250 myIMU;
ESP8266WiFiMulti WiFiMulti;
ESP8266WebServer server = ESP8266WebServer(80);
WebSocketsServer webSocket = WebSocketsServer(81);

boolean syncEventTriggered = false;
NTPSyncEvent_t ntpEvent;

boolean deviceConnected;

typedef struct {
  volatile unsigned int code = 9;
  volatile float gyroscopeX;
  volatile float gyroscopeY;
  volatile float gyroscopeZ;
  volatile float accelerometerX;
  volatile float accelerometerY;
  volatile float accelerometerZ;
  volatile float magnetometerX;
  volatile float magnetometerY;
  volatile float magnetometerZ;
  volatile float vcc = ESP.getVcc();
  volatile unsigned long uptime = millis();
  volatile unsigned long time;
  volatile float temperature;
} DataPacket;

void processSyncEvent(NTPSyncEvent_t ntpEvent) {
  if (ntpEvent) {
    Serial.print("Time Sync error: ");
    if (ntpEvent == noResponse)
      Serial.println("NTP server not reachable");
    else if (ntpEvent == invalidAddress)
      Serial.println("Invalid NTP server address");
  }
  else {
    Serial.print("Got NTP time: ");
    Serial.println(NTP.getTimeDateString(NTP.getLastNTPSync()));
    setTime(NTP.getTime());
  }
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t lenght) {
  switch (type) {
      Serial.printf("[%u]\n", type);
    case WStype_DISCONNECTED:
      Serial.printf("[%u] Disconnected!\n", num);
      deviceConnected = false;
      break;
    case WStype_CONNECTED: {
        Serial.println("Device Connected");
        IPAddress ip = webSocket.remoteIP(num);
        Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);
        webSocket.sendTXT(num, "Device Connected");
        deviceConnected = true;
      }
      break;
    case WStype_TEXT:
      if (payload[0] == 'g') {
        sendDataToPC();
      }
      break;
  }

}

void sendDataToPC() {
  if (deviceConnected) {
    DataPacket dataPackage;
    updateSensorsData();
    dataPackage.time = NTP.getTime();
    dataPackage.temperature = myIMU.temperature;
    dataPackage.gyroscopeX = myIMU.gx;
    dataPackage.gyroscopeY = myIMU.gy;
    dataPackage.gyroscopeZ = myIMU.gz;
    dataPackage.accelerometerX = myIMU.ax;
    dataPackage.accelerometerY = myIMU.ay;
    dataPackage.accelerometerZ = myIMU.az;
    dataPackage.magnetometerX = myIMU.mx;
    dataPackage.magnetometerY = myIMU.my;
    dataPackage.magnetometerZ = myIMU.mz;
    webSocket.sendBIN(0, (const uint8_t *) &dataPackage, sizeof(dataPackage));
    myIMU.updateTime();
  }
}

void setup() {
  Wire.setClock(400000L);
  Wire.begin();
  Serial.begin(115200);

  byte c = myIMU.readByte(MPU9250_ADDRESS, WHO_AM_I_MPU9250);
  Serial.print("MPU9250 "); Serial.print("I AM "); Serial.print(c, HEX);
  Serial.print(" I should be "); Serial.println(0x71, HEX);
  if (c == 0x71) { // WHO_AM_I should always be 0x68
    Serial.println("MPU9250 is online...");

    // Start by performing self test and reporting values
    myIMU.MPU9250SelfTest(myIMU.SelfTest);
    Serial.print("x-axis self test: acceleration trim within : ");
    Serial.print(myIMU.SelfTest[0], 1); Serial.println("% of factory value");
    Serial.print("y-axis self test: acceleration trim within : ");
    Serial.print(myIMU.SelfTest[1], 1); Serial.println("% of factory value");
    Serial.print("z-axis self test: acceleration trim within : ");
    Serial.print(myIMU.SelfTest[2], 1); Serial.println("% of factory value");
    Serial.print("x-axis self test: gyration trim within : ");
    Serial.print(myIMU.SelfTest[3], 1); Serial.println("% of factory value");
    Serial.print("y-axis self test: gyration trim within : ");
    Serial.print(myIMU.SelfTest[4], 1); Serial.println("% of factory value");
    Serial.print("z-axis self test: gyration trim within : ");
    Serial.print(myIMU.SelfTest[5], 1); Serial.println("% of factory value");

    // Calibrate gyro and accelerometers, load biases in bias registers
    myIMU.calibrateMPU9250(myIMU.gyroBias, myIMU.accelBias);

    myIMU.initMPU9250();
    Serial.println("MPU9250 initialized for active data mode....");
    byte d = myIMU.readByte(AK8963_ADDRESS, WHO_AM_I_AK8963);
    Serial.print("AK8963 "); Serial.print("I AM "); Serial.print(d, HEX);
    Serial.print(" I should be "); Serial.println(0x48, HEX);
    // Get magnetometer calibration from AK8963 ROM
    myIMU.initAK8963(myIMU.magCalibration);
    // Initialize device for active mode read of magnetometer
    Serial.println("AK8963 initialized for active data mode....");
    if (SerialDebug) {
      //  Serial.println("Calibration values: ");
      Serial.print("X-Axis sensitivity adjustment value ");
      Serial.println(myIMU.magCalibration[0], 2);
      Serial.print("Y-Axis sensitivity adjustment value ");
      Serial.println(myIMU.magCalibration[1], 2);
      Serial.print("Z-Axis sensitivity adjustment value ");
      Serial.println(myIMU.magCalibration[2], 2);
    }
  } else {
    Serial.print("Could not connect to MPU9250: 0x");
    Serial.println(c, HEX);
    while (1);
  }
  WiFi.hostname("pd-sensor");
  WiFiMulti.addAP("}RooT{", "");

  while (WiFiMulti.run() != WL_CONNECTED) {
    delay(100);
  }
  Serial.print ( "IP address: " );
  Serial.println ( WiFi.localIP() );
  IPAddress dns(8, 8, 8, 8);
  ip_addr_t d;
  d.addr = static_cast<uint32_t>(dns);
  dns_setserver(1, &d);
  Serial.println ( WiFi.dnsIP(1) );
  long rssi = WiFi.RSSI();
  Serial.print("Signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");

  // start webSocket server
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);

  if (MDNS.begin("pd-sensor")) {
    Serial.println("MDNS responder started");
  }

  // handle index
  server.on("/", []() {
    server.send(200, "text/html", "<html><head><script>var connection = new WebSocket('ws://'+location.hostname+':81/', ['arduino']);connection.onopen = function () {  connection.send('Connect ' + new Date()); }; connection.onerror = function (error) {    console.log('WebSocket Error ', error);};connection.onmessage = function (e) {  console.log('Server: ', e.data);};function sendRGB() {  var r = parseInt(document.getElementById('r').value).toString(16);  var g = parseInt(document.getElementById('g').value).toString(16);  var b = parseInt(document.getElementById('b').value).toString(16);  if(r.length < 2) { r = '0' + r; }   if(g.length < 2) { g = '0' + g; }   if(b.length < 2) { b = '0' + b; }   var rgb = '#'+r+g+b;    console.log('RGB: ' + rgb); connection.send(rgb); }</script></head><body>LED Control:<br/><br/>R: <input id=\"r\" type=\"range\" min=\"0\" max=\"255\" step=\"1\" onchange=\"sendRGB();\" /><br/>G: <input id=\"g\" type=\"range\" min=\"0\" max=\"255\" step=\"1\" onchange=\"sendRGB();\" /><br/>B: <input id=\"b\" type=\"range\" min=\"0\" max=\"255\" step=\"1\" onchange=\"sendRGB();\" /><br/></body></html>");
  });

  server.begin();

  // Add service to MDNS
  MDNS.addService("http", "tcp", 80);
  MDNS.addService("ws", "tcp", 81);

  NTP.onNTPSyncEvent([](NTPSyncEvent_t event) {
    ntpEvent = event;
    syncEventTriggered = true;
  });

  NTP.begin("time.nist.gov", 0, true);
  NTP.setInterval(63);
  Serial.println(NTP.getTimeDateString(NTP.getLastNTPSync()));
  Serial.println("Device initialized");
}

void loop() {
  webSocket.loop();
  myIMU.delt_t = millis() - myIMU.count;
  if (myIMU.delt_t > 20)
  {
    sendDataToPC();
    myIMU.count = millis();
  }
  //server.handleClient();
}

void updateSensorsData() {
  myIMU.tempCount = myIMU.readTempData();  // Read the adc values
  // Temperature in degrees Centigrade
  myIMU.temperature = ((float) myIMU.tempCount) / 333.87 + 21.0;
  myIMU.readAccelData(myIMU.accelCount);  // Read the x/y/z adc values
  myIMU.getAres();
  myIMU.ax = (float)myIMU.accelCount[0] * myIMU.aRes;
  myIMU.ay = (float)myIMU.accelCount[1] * myIMU.aRes;
  myIMU.az = (float)myIMU.accelCount[2] * myIMU.aRes;
  myIMU.readGyroData(myIMU.gyroCount);  // Read the x/y/z adc values
  myIMU.getGres();
  myIMU.gx = (float)myIMU.gyroCount[0] * myIMU.gRes;
  myIMU.gy = (float)myIMU.gyroCount[1] * myIMU.gRes;
  myIMU.gz = (float)myIMU.gyroCount[2] * myIMU.gRes;
  myIMU.readMagData(myIMU.magCount);  // Read the x/y/z adc values
  myIMU.getMres();
  myIMU.mx = (float)myIMU.magCount[0] * myIMU.mRes * myIMU.magCalibration[0];
  myIMU.my = (float)myIMU.magCount[1] * myIMU.mRes * myIMU.magCalibration[1];
  myIMU.mz = (float)myIMU.magCount[2] * myIMU.mRes * myIMU.magCalibration[2];
}
