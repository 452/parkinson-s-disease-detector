#include <Arduino.h>
#include <stdio.h>
#include <Wire.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <WebSocketsServer.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <Hash.h>
#include "quaternionFilters.h"
#include "MPU9250.h"
#include <NtpClientLib.h>
#include <lwip/err.h>
#include <lwip/dns.h>
#include "WIFICredentials.h"

#define SerialDebug false

#define    MPU9250_ADDRESS            0x68
#define    MAG_ADDRESS                0x0C
#define    GYRO_FULL_SCALE_250_DPS    0x00
#define    GYRO_FULL_SCALE_500_DPS    0x08
#define    GYRO_FULL_SCALE_1000_DPS   0x10
#define    GYRO_FULL_SCALE_2000_DPS   0x18

#define    ACC_FULL_SCALE_2_G        0x00
#define    ACC_FULL_SCALE_4_G        0x08
#define    ACC_FULL_SCALE_8_G        0x10
#define    ACC_FULL_SCALE_16_G       0x18

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
  volatile float accelerometerX;
  volatile float accelerometerY;
  volatile float accelerometerZ;
  volatile float gyroscopeX;
  volatile float gyroscopeY;
  volatile float gyroscopeZ;
  volatile float magnetometerX;
  volatile float magnetometerY;
  volatile float magnetometerZ;
  volatile float yaw;
  volatile float pitch;
  volatile float roll;
  volatile float vcc = ESP.getVcc();
  volatile unsigned long uptime = millis();
  volatile unsigned long time;
  volatile float temperature;
  volatile float hz;
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
    updateSensorsData();
    DataPacket dataPackage;
    dataPackage.accelerometerX = myIMU.ax;
    dataPackage.accelerometerY = myIMU.ay;
    dataPackage.accelerometerZ = myIMU.az;
    dataPackage.gyroscopeX = myIMU.gx;
    dataPackage.gyroscopeY = myIMU.gy;
    dataPackage.gyroscopeZ = myIMU.gz;
    dataPackage.magnetometerX = myIMU.mx;
    dataPackage.magnetometerY = myIMU.my;
    dataPackage.magnetometerZ = myIMU.mz;
    dataPackage.yaw = myIMU.yaw;
    dataPackage.pitch = myIMU.pitch;
    dataPackage.roll = myIMU.roll;
    dataPackage.time = NTP.getTime();
    dataPackage.temperature = myIMU.temperature;
    dataPackage.hz = (float) myIMU.sumCount / myIMU.sum;
    webSocket.sendBIN(0, (const uint8_t *) &dataPackage, sizeof(dataPackage));
  }
}

void setup() {
  Wire.setClock(400000L);
  Wire.begin();
  Serial.begin(115200);
  setupMPU9250();
  setupWIFI();
  setupWebSocketServer();
  setupHTTPServer();
  setupMDNS();
  setupNTP();
  Serial.println("Device initialized");
}

void loop() {
  webSocket.loop();
  myIMU.delt_t = millis() - myIMU.count;
  //  if (myIMU.delt_t > 3) {
  sendDataToPC();
  myIMU.count = millis();
  myIMU.sumCount = 0;
  myIMU.sum = 0;
  //  }
  //server.handleClient();
}

void setupMPU9250() {
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
  // Set accelerometers low pass filter at 5Hz
  I2CwriteByte(MPU9250_ADDRESS, 29, 0x06);
  // Set gyroscope low pass filter at 5Hz
  I2CwriteByte(MPU9250_ADDRESS, 26, 0x06);
  // Configure gyroscope range
  I2CwriteByte(MPU9250_ADDRESS, 27, GYRO_FULL_SCALE_250_DPS);
  // Configure accelerometers range
  I2CwriteByte(MPU9250_ADDRESS, 28, ACC_FULL_SCALE_2_G);
  // Set by pass mode for the magnetometers
  I2CwriteByte(MPU9250_ADDRESS, 0x37, 0x6);
  I2CwriteByte(MAG_ADDRESS, 0x0A, 0x6);
  // Get sensor resolutions, only need to do this once
  myIMU.getAres();
  myIMU.getGres();
  myIMU.getMres();
}

void updateSensorsData() {
  if (myIMU.readByte(MPU9250_ADDRESS, INT_STATUS) & 0x01) {
    myIMU.tempCount = myIMU.readTempData();  // Read the adc values
    // Temperature in degrees Centigrade
    myIMU.temperature = ((float) myIMU.tempCount);
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
    // User environmental x-axis correction in milliGauss, should be
    // automatically calculated
    myIMU.magbias[0] = +470.;
    // User environmental x-axis correction in milliGauss TODO axis??
    myIMU.magbias[1] = +120.;
    // User environmental x-axis correction in milliGauss
    myIMU.magbias[2] = +125.;
    myIMU.mx = (float)myIMU.magCount[0] * myIMU.mRes * myIMU.magCalibration[0] -
               myIMU.magbias[0];
    myIMU.my = (float)myIMU.magCount[1] * myIMU.mRes * myIMU.magCalibration[1] -
               myIMU.magbias[1];
    myIMU.mz = (float)myIMU.magCount[2] * myIMU.mRes * myIMU.magCalibration[2] -
               myIMU.magbias[2];
  }
  myIMU.updateTime();
  MahonyQuaternionUpdate(myIMU.ax, myIMU.ay, myIMU.az, myIMU.gx * DEG_TO_RAD,
                         myIMU.gy * DEG_TO_RAD, myIMU.gz * DEG_TO_RAD, myIMU.my,
                         myIMU.mx, myIMU.mz, myIMU.deltat);
  myIMU.yaw   = atan2(2.0f * (*(getQ() + 1) * *(getQ() + 2) + *getQ()
                              * *(getQ() + 3)), *getQ() * *getQ() + * (getQ() + 1)
                      * *(getQ() + 1) - * (getQ() + 2) * *(getQ() + 2) - * (getQ() + 3)
                      * *(getQ() + 3));
  myIMU.pitch = -asin(2.0f * (*(getQ() + 1) * *(getQ() + 3) - *getQ()
                              * *(getQ() + 2)));
  myIMU.roll  = atan2(2.0f * (*getQ() * *(getQ() + 1) + * (getQ() + 2)
                              * *(getQ() + 3)), *getQ() * *getQ() - * (getQ() + 1)
                      * *(getQ() + 1) - * (getQ() + 2) * *(getQ() + 2) + * (getQ() + 3)
                      * *(getQ() + 3));
  myIMU.pitch *= RAD_TO_DEG;
  myIMU.yaw   *= RAD_TO_DEG;

  // Declination of SparkFun Electronics (40°05'26.6"N 105°11'05.9"W) is
  //   8° 30' E  ± 0° 21' (or 8.5°) on 2016-07-19
  // Latitude: 50° 26' 53" N
  // Longitude:  30° 30' 8" E
  // Date  Declination
  // 2017-02-15  7° 31' E  ± 0° 22'  changing by  0° 7' E per year
  // - http://www.ngdc.noaa.gov/geomag-web/#declination
  myIMU.yaw  -= 0.7;
  myIMU.roll *= RAD_TO_DEG;
}


void setupWIFI() {
  WiFi.hostname("pd-sensor");
  WiFiMulti.addAP("}RooT{", ROOT_PASSWORD_KIEV);

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
}

void setupMDNS() {
  if (MDNS.begin("pd-sensor")) {
    Serial.println("MDNS responder started");
  }
  // Add service to MDNS
  MDNS.addService("http", "tcp", 80);
  MDNS.addService("ws", "tcp", 81);
}

void setupWebSocketServer() {
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
}

void setupHTTPServer() {
  server.on("/", []() {
    server.send(200, "text/html", "<html><head><script>var connection = new WebSocket('ws://'+location.hostname+':81/', ['arduino']);connection.onopen = function () {  connection.send('Connect ' + new Date()); }; connection.onerror = function (error) {    console.log('WebSocket Error ', error);};connection.onmessage = function (e) {  console.log('Server: ', e.data);};function sendRGB() {  var r = parseInt(document.getElementById('r').value).toString(16);  var g = parseInt(document.getElementById('g').value).toString(16);  var b = parseInt(document.getElementById('b').value).toString(16);  if(r.length < 2) { r = '0' + r; }   if(g.length < 2) { g = '0' + g; }   if(b.length < 2) { b = '0' + b; }   var rgb = '#'+r+g+b;    console.log('RGB: ' + rgb); connection.send(rgb); }</script></head><body>LED Control:<br/><br/>R: <input id=\"r\" type=\"range\" min=\"0\" max=\"255\" step=\"1\" onchange=\"sendRGB();\" /><br/>G: <input id=\"g\" type=\"range\" min=\"0\" max=\"255\" step=\"1\" onchange=\"sendRGB();\" /><br/>B: <input id=\"b\" type=\"range\" min=\"0\" max=\"255\" step=\"1\" onchange=\"sendRGB();\" /><br/></body></html>");
  });
  server.begin();
}

void setupNTP() {
  NTP.onNTPSyncEvent([](NTPSyncEvent_t event) {
    ntpEvent = event;
    syncEventTriggered = true;
  });

  NTP.begin("time.nist.gov", 0, true);
  NTP.setInterval(63);
  Serial.println(NTP.getTimeDateString(NTP.getLastNTPSync()));
}

// This function read Nbytes bytes from I2C device at address Address.
// Put read bytes starting at register Register in the Data array.
void I2Cread(uint8_t Address, uint8_t Register, uint8_t Nbytes, uint8_t*      Data)
{
  // Set register address
  Wire.beginTransmission(Address);
  Wire.write(Register);
  Wire.endTransmission();

  // Read Nbytes
  Wire.requestFrom(Address, Nbytes);
  uint8_t index = 0;
  while (Wire.available())
    Data[index++] = Wire.read();
}


// Write a byte (Data) in device (Address) at register (Register)
void I2CwriteByte(uint8_t Address, uint8_t Register, uint8_t Data)
{
  // Set register address
  Wire.beginTransmission(Address);
  Wire.write(Register);
  Wire.write(Data);
  Wire.endTransmission();
}

