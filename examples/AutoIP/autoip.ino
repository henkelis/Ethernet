/*
  Ethernet AUTOIP example
  (tested on Adafruit Feather ESP32S3 with Adafruit Ethernet Featherwing with W5500)
*/

#include <Wire.h>
#include <SPI.h>
#include <Ethernet.h>

#define MAX_IP_LEN 16

const int ethInterval = 60000;
unsigned long ethPreviousMillis = 0;

// you can specify a saved/seed link local IP if you like
char linkLocalIp[MAX_IP_LEN] = "";

// I2C address of the 24AA02E48
#define I2C_ADDRESS 0x50
// Default MAC address for the ethernet controller, will be 
// overwritten with the value read from the 24AA02E48
byte mac[] = { 0x0E, 0xAD, 0xBE, 0xEF, 0xFE, 0xE0 };

EthernetClient client;
bool ethernetConnected = false;
bool ethernetDhcp = false;
bool ethernetAutoip = false;

byte readRegister(byte r) {
  unsigned char v;
  Wire.beginTransmission(I2C_ADDRESS);
  Wire.write(r);  // Register to read
  Wire.endTransmission();
  Wire.requestFrom(I2C_ADDRESS, 1); // Read a byte
  v = Wire.read();
  return v;
}

void ethernetConnect() {
  Wire.begin();
  // check if 24AA02E48 chip is wired in
  Wire.beginTransmission(I2C_ADDRESS);
  // see if something acks at this address
  if (Wire.endTransmission() == 0) {
    printf("Getting ethernet mac address\n");
    // Read the MAC programmed in the 24AA02E48 chip
    mac[0] = readRegister(0xFA);
    mac[1] = readRegister(0xFB);
    mac[2] = readRegister(0xFC);
    mac[3] = readRegister(0xFD);
    mac[4] = readRegister(0xFE);
    mac[5] = readRegister(0xFF);
  }

  // configure CS pin for W5500 (A14=pin13)
  Ethernet.init(A14);

  // start the Ethernet connection:
  ethernetConnected = false;
  ethernetDhcp = false;
  ethernetAutoip = false;
  printf("Initialize Ethernet with DHCP:\n");
  if (Ethernet.begin(mac, 5000) == 0) {
    if (Ethernet.hardwareStatus() == EthernetNoHardware) {
      printf("Ethernet device was not found.\n");
      return;
    }
    if (Ethernet.linkStatus() == LinkOFF) {
      printf("Ethernet cable is not connected.\n");
      return;
    }
    printf("Failed to configure Ethernet using DHCP\n");
  } else {
    printf("  DHCP assigned IP %s\n", Ethernet.localIP().toString().c_str());
    ethernetConnected = true;
    ethernetDhcp = true;
  }
  if (ethernetConnected == false) {
    // try to get a link-local address, with a seed of any current saved address
    printf("Initialize Ethernet with AUTOIP:\n");
    IPAddress ip;
    ip.fromString(linkLocalIp);
    if (Ethernet.beginautoipseed(mac, ip, 30000, 1000) == 0) {
      printf("Failed to configure Ethernet using AUTOIP\n");
      return;
    } else {
      sprintf(linkLocalIp, "%s", Ethernet.localIP().toString().c_str());
      printf("  AUTOIP assigned IP %s\n", linkLocalIp);
      ethernetConnected = true;
      ethernetAutoip = true;
    }
  }
    
  // give the Ethernet controller a second to initialize:
  delay(1000);
}

void ethernetDisconnect() {
  client.stop();
  ethernetConnected = false;
  ethernetDhcp = false;
  ethernetAutoip = false;
}

void setup() {
  Serial.begin(115200);

#if DEBUG
  while (!Serial && millis() < 3000);
#endif

  ethernetConnect();
}

unsigned long currentMillis;
void loop() {

  currentMillis = millis();
  if ((currentMillis - ethPreviousMillis) >= ethInterval) {
    ethPreviousMillis = currentMillis;
    if (ethernetConnected && ethernetDhcp) {
      Ethernet.maintain();
    }
  }

  if (ethernetConnected == false && Ethernet.linkStatus() == LinkON) {
    ethernetConnect();
  }
  if (ethernetConnected == true && Ethernet.linkStatus() == LinkOFF) {
    ethernetDisconnect();
  }

}