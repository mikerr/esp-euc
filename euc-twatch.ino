
#include <BLEDevice.h>

/*
#include <TFT_eSPI.h> // Hardware-specific library
#include <SPI.h>

TFT_eSPI tft = TFT_eSPI();       // Invoke custom library

*/

 #define LILYGO_WATCH_2020_V1  
#include <LilyGoWatch.h>
TTGOClass *ttgo;

#define TFT_GREY 0x5AEB
#define TFT_ORANGE      0xFD20      /* 255, 165,   0 */

float ltx = 0;    // Saved x coord of bottom of needle
uint16_t osx = 120, osy = 120; // Saved x & y coords
uint32_t updateTime = 0;       // time for next update

int old_analog =  -999; // Value last displayed

int value[6] = {0, 0, 0, 0, 0, 0};
int old_value[6] = { -1, -1, -1, -1, -1, -1};

// Ninebot remote service
static BLEUUID serviceUUID("6e400001-b5a3-f393-e0a9-e50e24dcca9e"); 
//write - send commands / requests to wheel
static BLEUUID    writecharUUID("6e400002-b5a3-f393-e0a9-e50e24dcca9e");
//read - responses from wheel come in here
static BLEUUID     readcharUUID("6e400003-b5a3-f393-e0a9-e50e24dcca9e");

static boolean doConnect = false;
static boolean connected = false;
static boolean doScan = false;
static boolean found = false;

static BLERemoteCharacteristic *pReadCharacteristic, *pWriteCharacteristic;
static BLEAdvertisedDevice *myDevice;

int wheelstat = 0;

static void notifyCallback(  BLERemoteCharacteristic* pBLERemoteCharacteristic,  uint8_t* pData,  size_t length,  bool isNotify) {
      
    uint8_t value1, value2;
    value1 = *(pData + 6);
    value2 = *(pData + 7);
    
    wheelstat = value1 + (256 * value2);
}

class MyClientCallback : public BLEClientCallbacks {
  void onConnect(BLEClient* pclient) {  }

  void onDisconnect(BLEClient* pclient) {
    connected = false;
    Serial.println("Disconnected");
    exit(0);
  }
};

bool connectToServer() {
  BLEClient*  pClient  = BLEDevice::createClient();

    pClient->setClientCallbacks(new MyClientCallback());
    pClient->connect(myDevice); 

    // Obtain a reference to the service 
    BLERemoteService* pRemoteService = pClient->getService(serviceUUID);
    if (pRemoteService == nullptr) {
      pClient->disconnect();
      return false;
    }

    // Obtain a reference to the characteristic in the service of the remote BLE server.
    pReadCharacteristic = pRemoteService->getCharacteristic(readcharUUID);
    if (pReadCharacteristic == nullptr) {
      pClient->disconnect();
      return false;
    }
    pWriteCharacteristic = pRemoteService->getCharacteristic(writecharUUID);
    if (pWriteCharacteristic == nullptr) {
      pClient->disconnect();
      return false;
    }
    
    if(pReadCharacteristic->canNotify())
      pReadCharacteristic->registerForNotify(notifyCallback);

    connected = true;
}

class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {

  void onResult(BLEAdvertisedDevice advertisedDevice) {
     
      //ttgo->tft->setTextColor(TFT_BLUE);
      //ttgo->tft->print(".");
      // check for BT service ID
      if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(serviceUUID)) {
         BLEDevice::getScan()->stop();
         myDevice = new BLEAdvertisedDevice(advertisedDevice);
         doConnect = true;
         doScan = true;
         found = true;
      }     
  } 
}; 

void setup(void) {
  Serial.begin(115200); // For debug
  ttgo = TTGOClass::getWatch();
  ttgo->begin();
  ttgo->openBL(); //turn on backlight
  ttgo->motor_begin();
    
  ttgo->tft->setRotation(2);
  ttgo->tft->fillScreen(TFT_BLACK);
  
  int level = ttgo->power->getBattPercentage();
  ttgo->tft->setTextColor(TFT_BLUE);
  ttgo->tft->print("Battery level: "); 
  ttgo->tft->print(level);
  ttgo->tft->println(" %\n\n");
  ttgo->tft->setTextColor(TFT_WHITE); 
  ttgo->tft->println("Scanning for bluetooth device ..");
  
  BLEDevice::init("");
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setInterval(1349);
  pBLEScan->setWindow(449);
  pBLEScan->setActiveScan(true);
  pBLEScan->start(10, false); 

  delay(1000);
  if (found == false) {
    ttgo->tft->setTextColor(TFT_RED); 
    ttgo->tft->println("\nFailed to find bluetooth device");
    delay(10000);
    esp_deep_sleep_start(); // < 6mA     
    }    
  ttgo->tft->fillScreen(TFT_BLACK);
}

void loop() {
int Batt;
static float Limit=12;
float Range,Speed,Trip;

   if (doConnect == true) {
    if (!connectToServer()) Serial.println("\nFailed to connect");
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

    // only check battery and range every 10 seconds
    if (updateTime <= millis()) {
      updateTime = millis() + 10000;
      pWriteCharacteristic->writeValue(batterylevel, 7);
      Batt = wheelstat;
    
      pWriteCharacteristic->writeValue(rangeleft, 7);
      Range = wheelstat / 160.0f;

      pWriteCharacteristic->writeValue(tripmileage, 7);
      Trip = wheelstat / 160.0f;  
     
      ttgo->tft->fillRect(0, 0, 239, 24, TFT_BLACK);
      ttgo->tft->setTextColor(TFT_CYAN);
      ttgo->tft->drawString(myDevice->getName().c_str(), 120, 0, 2); //  at top right
      ttgo->tft->setTextColor(TFT_WHITE);
      ttgo->tft->drawString(String(Trip) +" miles", 10, 0, 2); //  at top left
      ttgo->tft->fillRect(0, 100, 239, 131, TFT_BLACK);
      ttgo->tft->drawString(String(Range) + " mi", 10, 100 , 2); //  at bottom left
      ttgo->tft->drawString("remaining", 10, 115, 2); //  at bottom left
      ttgo->tft->drawString(String(Batt) + " %", 193, 100, 2); //  at bottom right
      ttgo->tft->drawString("battery", 193, 115, 2);
      
      }

    // check speed every second ( or as quick as possible)
    
    pWriteCharacteristic->writeValue(currentspeed, 7);
    Speed = wheelstat / 1600.0f;

    analogMeter();
  
    //ttgo->tft->fillRect(100, 75, 70 ,40, TFT_BLACK);
    //ttgo->tft->drawString(String(Speed), 100, 75, 4); 
    //ttgo->tft->drawString("mph",130, 95, 2);   

    ttgo->tft->fillRect(0, 175, 150 ,90, TFT_BLACK);
    int xx = 60;
    if (Speed >= 10) xx = 40;
    ttgo->tft->drawString(String(Speed), xx, 175, 7); 
    ttgo->tft->drawString("mph",190, 195, 2);   
    
    plotNeedle((Speed / 20) * 100, 0); 
    delay(200);
    if (Speed > Limit) ttgo->motor->onec();

    int16_t x, y;
    if (ttgo->getTouch(x, y)) {
      
      if (y > 180) {
        if (x > 190) Limit++;  
        if (x < 50) Limit--; 
        
        ttgo->tft->fillRect(0, 150, 170 ,80, TFT_BLACK);
        ttgo->tft->drawString("limit",100, 150, 2);
        xx = 60;
        if (Limit >= 10) xx = 40;
        ttgo->tft->drawString(String(Limit), xx, 175, 7); 
        delay(1000); 
      }
    }
  }
 
  
}


// #########################################################################
//  Draw the analogue meter on the screen
// #########################################################################
void analogMeter()
{
  // Draw ticks every 5 degrees from -50 to +50 degrees (100 deg. FSD swing)
  for (int i = -50; i < 51; i += 5) {
    // Long scale tick length
    int tl = 15;

    // Coodinates of tick to draw
    float sx = cos((i - 90) * 0.0174532925);
    float sy = sin((i - 90) * 0.0174532925);
    uint16_t x0 = sx * (100 + tl) + 120;
    uint16_t y0 = sy * (100 + tl) + 150;
    uint16_t x1 = sx * 100 + 120;
    uint16_t y1 = sy * 100 + 150;

    // Coordinates of next tick for zone fill
    float sx2 = cos((i + 5 - 90) * 0.0174532925);
    float sy2 = sin((i + 5 - 90) * 0.0174532925);
    int x2 = sx2 * (100 + tl) + 120;
    int y2 = sy2 * (100 + tl) + 150;
    int x3 = sx2 * 100 + 120;
    int y3 = sy2 * 100 + 150;

    // Yellow zone limits
    if (i >= -50 && i < 0) {
      ttgo->tft->fillTriangle(x0, y0, x1, y1, x2, y2, TFT_YELLOW);
      ttgo->tft->fillTriangle(x1, y1, x2, y2, x3, y3, TFT_YELLOW);
    }

    // Green zone limits
    if (i >= 0 && i < 25) {
      ttgo->tft->fillTriangle(x0, y0, x1, y1, x2, y2, TFT_GREEN);
      ttgo->tft->fillTriangle(x1, y1, x2, y2, x3, y3, TFT_GREEN);
    }

    // Orange zone limits
    if (i >= 25 && i < 50) {
      ttgo->tft->fillTriangle(x0, y0, x1, y1, x2, y2, TFT_ORANGE);
      ttgo->tft->fillTriangle(x1, y1, x2, y2, x3, y3, TFT_ORANGE);
    }

    // Short scale tick length
    if (i % 25 != 0) tl = 8;

    // Recalculate coords incase tick lenght changed
    x0 = sx * (100 + tl) + 120;
    y0 = sy * (100 + tl) + 150;
    x1 = sx * 100 + 120;
    y1 = sy * 100 + 150;

    // Draw tick
    ttgo->tft->drawLine(x0, y0, x1, y1, TFT_BLACK);

    // Check if labels should be drawn, with position tweaks
    if (i % 25 == 0) {
      // Calculate label positions
      x0 = sx * (100 + tl + 10) + 120;
      y0 = sy * (100 + tl + 10) + 150;
      switch (i / 25) {
        case -2: ttgo->tft->drawCentreString("0", x0+4, y0-4, 1); break;
        case -1: ttgo->tft->drawCentreString("5", x0+2, y0, 1); break;
        case 0: ttgo->tft->drawCentreString("10", x0, y0, 1); break;
        case 1: ttgo->tft->drawCentreString("15", x0, y0, 1); break;
        case 2: ttgo->tft->drawCentreString("20", x0-2, y0-4, 1); break;
      }
    }

    // Now draw the arc of the scale
    sx = cos((i + 5 - 90) * 0.0174532925);
    sy = sin((i + 5 - 90) * 0.0174532925);
    x0 = sx * 100 + 120;
    y0 = sy * 100 + 150;
    // Draw scale arc, don't draw the last part
    if (i < 50) ttgo->tft->drawLine(x0, y0, x1, y1, TFT_BLACK);
  }
}

void plotNeedle(int value, byte ms_delay)
{
  if (value < -10) value = -10; // Limit value to emulate needle end stops
  if (value > 110) value = 110;

  // Move the needle until new value reached
  while (!(value == old_analog)) {
    if (old_analog < value) old_analog++;
    else old_analog--;

    if (ms_delay == 0) old_analog = value; // Update immediately if delay is 0

    float sdeg = map(old_analog, -10, 110, -150, -30); // Map value to angle
    // Calculate tip of needle coords
    float sx = cos(sdeg * 0.0174532925);
    float sy = sin(sdeg * 0.0174532925);

    // Calculate x delta of needle start (does not start at pivot point)
    float tx = tan((sdeg + 90) * 0.0174532925);

    // Erase old needle image
    ttgo->tft->drawLine((120 + 24 * ltx) - 1, (150 - 24), osx - 1, osy, TFT_BLACK);
    ttgo->tft->drawLine((120 + 24 * ltx), (150 - 24), osx, osy, TFT_BLACK);
    ttgo->tft->drawLine((120 + 24 * ltx) + 1, (150 - 24), osx + 1, osy, TFT_BLACK);

    // Store new needle end coords for next erase
    ltx = tx;
    osx = (sx * 98 + 120);
    osy = (sy * 98 + 150);

    // Draw the needle in the new postion, magenta makes needle a bit bolder
    // draws 3 lines to thicken needle
    ttgo->tft->drawLine((120 + 24 * ltx) - 1, (150 - 24), osx - 1, osy, TFT_RED);
    ttgo->tft->drawLine((120 + 24 * ltx), (150 - 24), osx, osy, TFT_MAGENTA);
    ttgo->tft->drawLine((120 + 24 * ltx) + 1, (150 - 24), osx + 1, osy, TFT_RED);

    // Slow needle down slightly as it approaches new postion
    if (abs(old_analog - value) < 10) ms_delay += ms_delay / 5;

    // Wait before next update
    delay(ms_delay);
  }
}
