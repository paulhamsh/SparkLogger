#define M5_BOARD
//#define HELTEC_BOARD

#ifdef M5_BOARD
#include <M5Core2.h>
#else
#include "heltec.h"
#endif

 
#include "BLEDevice.h"
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#include "BluetoothSerial.h"

#include "RingBuffer.h"

#define C_SERVICE "ffc0"
#define C_CHAR1   "ffc1"
#define C_CHAR2   "ffc2"

#define S_SERVICE "ffc0"
#define S_CHAR1   "ffc1"
#define S_CHAR2   "ffc2"


BluetoothSerial BTApp;

BLEServer *pServer;
BLEService *pService;
BLECharacteristic *pCharacteristic_receive;
BLECharacteristic *pCharacteristic_send;

BLEAdvertising *pAdvertising;
  
BLEScan *pScan;
BLEScanResults pResults;
BLEAdvertisedDevice device;

BLEClient *pClient_sp;
BLERemoteService *pService_sp;
BLERemoteCharacteristic *pReceiver_sp;
BLERemoteCharacteristic *pSender_sp;


int scrpos;
int datapos;

void printval(int a)
{
  
  if (a < 16) {
    Serial.print("0");
  }
  Serial.print(a, HEX);

  if (datapos == 17 || datapos == 19 ) {
    Serial.print(" << ");
    scrpos++;
  }
  else if (datapos == 18 || datapos == 21) {
    Serial.print(" >> ");
    scrpos++;
  }
  else
    Serial.print(" ");

  if (scrpos % 16 == 15) {
    Serial.println();
    Serial.print("                    ");
  }
  scrpos++;
  datapos++;
  
}

void printhdr(char *s)
{
  Serial.println();
  Serial.println(s);
  Serial.print("                    ");
  scrpos = 0;
  datapos = 0;
}

bool connected_sp;
int to_app_pos, to_amp_pos;;
byte to_app_buf[5000], to_amp_buf[5000];

RingBuffer app_to_amp;
RingBuffer amp_to_app;

void notifyCB_sp(BLERemoteCharacteristic* pRemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify){
  int i;
  byte b;

  for (i = 0; i < length; i++) {
    b = pData[i];
    amp_to_app.add(b);
  }
  amp_to_app.commit();
}



class CharacteristicCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* pCharacteristic) {
        int j, l;
        const char *p;
        byte b;

        l = pCharacteristic->getValue().length();
        p = pCharacteristic->getValue().c_str();
        for (j=0; j < l; j++) {
          b = p[j];
          app_to_amp.add(b);
          if (b == 0xf7) app_to_amp.commit();
        }
    };
};



static CharacteristicCallbacks chrCallbacks_s, chrCallbacks_r;
bool use_ble;


void setup() {
  int i;
  
#ifdef M5_BOARD
  M5.begin();
  M5.Lcd.fillScreen(TFT_BLACK);
  M5.Lcd.setTextSize(4);
  M5.Lcd.println("Spark Logger");
  M5.Lcd.println("------------");
  M5.Lcd.setTextSize(3);
#else
  Heltec.begin(true /*DisplayEnable Enable*/, false /*LoRa Enable*/, true /*Serial Enable*/);
  Heltec.display->clear();
  Heltec.display->drawString(0, 0, "pPark Logger");
  Heltec.display->display();
#endif

  Serial.println("Started");
  
  to_app_pos = 0;
  to_amp_pos = 0;
  scrpos = 0;
  datapos = 0;

  use_ble = true;

  
  // Create server to act as Spark
  BLEDevice::init("Spark 40 BLE");
  pClient_sp = BLEDevice::createClient();
  pScan      = BLEDevice::getScan();
    
  // Set up server
  pServer = BLEDevice::createServer();
  pService = pServer->createService(S_SERVICE);
  pCharacteristic_receive = pService->createCharacteristic(S_CHAR1, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR);
  pCharacteristic_send = pService->createCharacteristic(S_CHAR2, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);

  pCharacteristic_receive->setCallbacks(&chrCallbacks_r);
  pCharacteristic_send->setCallbacks(&chrCallbacks_s);

// +++
  pCharacteristic_send->addDescriptor(new BLE2902());
// +++

  pService->start();
//++  pServer->start();

  pAdvertising = BLEDevice::getAdvertising(); // create advertising instance
  pAdvertising->addServiceUUID(pService->getUUID()); // tell advertising the UUID of our service
  pAdvertising->setScanResponse(true);  

  Serial.println("Service set up");

  
  // Connect to Spark
  connected_sp = false;
  
  while (!connected_sp) {
    pResults = pScan->start(4);
    BLEUUID SpServiceUuid(C_SERVICE);

    for(i = 0; i < pResults.getCount()  && (!connected_sp); i++) {
      device = pResults.getDevice(i);

      if (device.isAdvertisingService(SpServiceUuid)) {
        Serial.println("Found Spark - trying to connect....");
        if(pClient_sp->connect(&device)) {
          connected_sp = true;
          Serial.println("Spark connected");
        }
      }

    }

    // Set up client
    if (connected_sp) {
      pService_sp = pClient_sp->getService(SpServiceUuid);
      if (pService_sp != nullptr) {
        pSender_sp   = pService_sp->getCharacteristic(C_CHAR1);
        pReceiver_sp = pService_sp->getCharacteristic(C_CHAR2);
        if (pReceiver_sp && pReceiver_sp->canNotify()) {
          pReceiver_sp->registerForNotify(notifyCB_sp);
        }
      }
    }

  }

  const uint8_t notifyOn[] = {0x1, 0x0};
  
  BLERemoteDescriptor* p2902 = pReceiver_sp->getDescriptor(BLEUUID((uint16_t)0x2902));
  if(p2902 != nullptr)
  {
    p2902->writeValue((uint8_t*)notifyOn, 2, true);
  }

  if (!BTApp.begin ("Spark 40 Audio")) {
    Serial.println("Spark bluetooth init fail");
    while (true);
  }   
  
  Serial.println("Available for app to connect...");
    // start advertising
  pAdvertising->start(); 

}


void loop() {
  int i;
  byte b;
  
#ifdef M5_BOARD
  M5.update();
#endif

#ifdef M5_BOARD
  if (M5.BtnA.wasPressed()) {
    Serial.println();
    Serial.println("------------------------------------------");
  }
#endif

// process messages from the app (classic bluetooth for android)
// max size 0xad, each block always end in 0x7f

  if (!app_to_amp.is_empty()) {
    use_ble = true;
    app_to_amp.get(&b);
    to_amp_buf[to_amp_pos++] = b;      
    if (b == 0xf7) {
      pSender_sp->writeValue(to_amp_buf, to_amp_pos, false);

      printhdr("Write to spark (BLE App):     ");
      for (i=0; i<to_amp_pos; i++) {
        printval(to_amp_buf[i]);
      }
      to_amp_pos = 0;
    }
  }

  
  if (BTApp.available()) {
    use_ble = false;
    while (BTApp.available()) {
      b = BTApp.read();
      to_amp_buf[to_amp_pos++] = b;     
      if (b == 0xf7) {
        pSender_sp->writeValue(to_amp_buf, to_amp_pos, false);

        printhdr("Write to spark (Classic BT App):     ");
          for (i=0; i<to_amp_pos; i++) {
            printval(to_amp_buf[i]);
          }
        to_amp_pos = 0;
      }
    }
  }

// process messages from the amp (ble)
// max size 0x6a, blocks don't necessarily end in 0x7f

  if (!amp_to_app.is_empty()) {
     amp_to_app.get(&b);
     to_app_buf[to_app_pos++] = b;  
  }
  
  if ((amp_to_app.is_empty() && to_app_pos > 0)   // no more but there is something to send
       || to_app_pos >= 0x6a) {                   // block full so need to send
    
    if (use_ble) {
      pCharacteristic_send->setValue(to_app_buf, to_app_pos);
      pCharacteristic_send->notify(true);
    } 
    else {
      BTApp.write(to_app_buf, to_app_pos);
    }

    if (use_ble) 
      printhdr("Write to app (BLE):       ");
    else
      printhdr("Write to app (Classic BT):       ");
      
    for (i=0; i < to_app_pos; i++) {
      printval(to_app_buf[i]);
    }
    to_app_pos = 0;
  }
}