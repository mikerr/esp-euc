/**
Connect to ninebot one S2
 */

#include "BLEDevice.h"
    
// Ninebot remote service
static BLEUUID serviceUUID("6e400001-b5a3-f393-e0a9-e50e24dcca9e"); 
//write - send commands / requests to wheel
static BLEUUID    writecharUUID("6e400002-b5a3-f393-e0a9-e50e24dcca9e");
//read - responses from wheel come in here
static BLEUUID     readcharUUID("6e400003-b5a3-f393-e0a9-e50e24dcca9e");

static boolean doConnect = false;
static boolean connected = false;
static boolean doScan = false;
static BLERemoteCharacteristic *pReadCharacteristic, *pWriteCharacteristic;
static BLEAdvertisedDevice *myDevice;

static void notifyCallback(
  BLERemoteCharacteristic* pBLERemoteCharacteristic,
  uint8_t* pData,
  size_t length,
  bool isNotify) {
    Serial.print("Notify callback for characteristic ");
    Serial.print(pBLERemoteCharacteristic->getUUID().toString().c_str());
    Serial.print(" of data length ");
    Serial.println(length);
    Serial.print("data: ");
    
    //Serial.println((char*)pData);

    uint8_t* response = pData;
    for (int i=0; i< length;i++) {
      response++; 
      Serial.print(*response);
      Serial.print(" ");
    }
   
    uint8_t value1, value2;
    value1 = *(pData + 6);
    value2 = *(pData + 7);
    
    int value = value1 + (256 * value2);
    Serial.println("");
    Serial.print("Value = ");
    Serial.println( value);
}

class MyClientCallback : public BLEClientCallbacks {
  void onConnect(BLEClient* pclient) {
  }

  void onDisconnect(BLEClient* pclient) {
    connected = false;
    Serial.println("Disconnected");
  }
};

bool connectToServer() {
    Serial.print("Connecting to ");
    Serial.print(myDevice->getName().c_str());
    Serial.print(" ( ");
    Serial.print(myDevice->getAddress().toString().c_str());
    Serial.println(" ) ");
    
    BLEClient*  pClient  = BLEDevice::createClient();

    pClient->setClientCallbacks(new MyClientCallback());

    pClient->connect(myDevice); 
    Serial.println(" - Connected !");

    // Obtain a reference to the service we are after in the remote BLE server.
    BLERemoteService* pRemoteService = pClient->getService(serviceUUID);
    if (pRemoteService == nullptr) {
      Serial.print("Failed to find service UUID: ");
      Serial.println(serviceUUID.toString().c_str());
      pClient->disconnect();
      return false;
    }
    Serial.println(" - Found service");

    // Obtain a reference to the characteristic in the service of the remote BLE server.
    pReadCharacteristic = pRemoteService->getCharacteristic(readcharUUID);
    if (pReadCharacteristic == nullptr) {
      Serial.print("Failed to find READ characteristic UUID: ");
      Serial.println(readcharUUID.toString().c_str());
      pClient->disconnect();
      return false;
    }
    pWriteCharacteristic = pRemoteService->getCharacteristic(writecharUUID);
    if (pWriteCharacteristic == nullptr) {
      Serial.print("Failed to find WRITE characteristic UUID: ");
      Serial.println(writecharUUID.toString().c_str());
      pClient->disconnect();
      return false;
    }
    
    Serial.println(" - Found READ & WRITE characteristics");

    if(pReadCharacteristic->canNotify())
      pReadCharacteristic->registerForNotify(notifyCallback);

    connected = true;
}
/**
 * Scan for BLE servers and find the first one that advertises the service we are looking for.
 */
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
 /**
   * Called for each advertising BLE server.
   */
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    //Serial.print("BLE Advertised Device found: ");
   // Serial.println(advertisedDevice.toString().c_str());

    // We have found a device, let us now see if it contains the service we are looking for.
   // if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(serviceUUID)) {

   // Ninebot S2 begins N2OSL...
      String name = advertisedDevice.getName().c_str();
      if (name.startsWith("N2O")) {
         BLEDevice::getScan()->stop();
         myDevice = new BLEAdvertisedDevice(advertisedDevice);
         doConnect = true;
         doScan = true;
      }     
  } 
}; 


void setup() {
  Serial.begin(115200);
  Serial.println("Starting bluetooth");
  BLEDevice::init("");
  Serial.println("Scanning");
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setInterval(1349);
  pBLEScan->setWindow(449);
  pBLEScan->setActiveScan(true);
  pBLEScan->start(0, false); // run for 0 seconds (infinite)
} 

void loop() {

  // If the flag "doConnect" is true then we have scanned and found the wheel
  
  if (doConnect == true) {
    if (!connectToServer()) Serial.println("Failed to connect");
    doConnect = false;
  }

  if (connected) {
    // ninebot one variables at http://www.gorina.es/9BMetrics/variables.html
    // modified for S2
    
    //  0x55, 0xAA, 0x03, 0x11, 0x01, 0x22, 0x02, 0xC6, 0xFF, 0x00
    //  0x55, 0xAA, 0x03, 0x11,0x01,  cmd,  0x02, chk1  chk2
     // checksum chk = sum [2,4,5,6] ^ 0xFFFF
    
    char batterylevel[] = {0x55, 0xAA, 0x03, 0x11, 0x01, 0x22, 0x02, 0xC6, 0xFF, 0x00}; //  battery percent 
    char rangeleft[]    = {0x55, 0xAA, 0x03, 0x11, 0x01, 0x25, 0x02, 0xC3, 0xFF, 0x00}; // range left (km * 100)
    char currentspeed[] = {0x55, 0xAA, 0x03, 0x11, 0x01, 0x26, 0x02, 0xC2, 0xFF, 0x00}; // current speed (kmh * 1000)
    char tripmileage[]  = {0x55, 0xAA, 0x03, 0x11, 0x01, 0xb9, 0x02, 0x2F, 0xFF, 0x00}; // current mileage (km *100)
    
    Serial.println("sending command batterylevel");
    pWriteCharacteristic->writeValue(batterylevel, 7);
    delay (1000);
    
    Serial.println("sending command rangeleft");
    pWriteCharacteristic->writeValue(rangeleft, 7);
    delay (1000);
    
    Serial.println("sending command current speed");
    pWriteCharacteristic->writeValue(currentspeed, 7);
    delay (1000);
    
    Serial.println("sending command current mileage");
    pWriteCharacteristic->writeValue(tripmileage, 7);
    delay (1000);
  }
  delay(1000); // Delay 1 second between loops.
} 
