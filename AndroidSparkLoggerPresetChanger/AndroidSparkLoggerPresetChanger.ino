// Define your board name below for conditional compilation- currently supports M5Stack Core 2 and Heltec WIFI

#define M5_BOARD
//#define HELTEC_BOARD


#ifdef M5_BOARD
#include <M5Core2.h>
#else
#include "heltec.h"
#endif
 
#include "BLEDevice.h"
#include "BluetoothSerial.h"
#include "RingBuffer.h"

#define C_SERVICE "ffc0"
#define C_CHAR1   "ffc1"
#define C_CHAR2   "ffc2"

BluetoothSerial BTApp;
 
BLEScan *pScan;
BLEScanResults pResults;
BLEAdvertisedDevice device;

BLEClient *pClient_sp;
BLERemoteService *pService_sp;
BLERemoteCharacteristic *pReceiver_sp;
BLERemoteCharacteristic *pSender_sp;

const int preset_cmd_size = 26;

byte preset_spk_cmd[] = {
  0x01, 0xFE, 0x00, 0x00,
  0x53, 0xFE, 0x1A, 0x00,
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,
  0xF0, 0x01, 0x24, 0x00,
  0x01, 0x38, 0x00, 0x00,
  0x00, 0xF7
};

byte preset_app_cmd[] = {
  0x01, 0xFE, 0x00, 0x00,
  0x41, 0xFF, 0x1A, 0x00,
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,
  0xF0, 0x01, 0x50, 0x00,
  0x03, 0x38, 0x00, 0x00,
  0x00, 0xF7
};

int scrpos;

void printval(int a)
{
  if (a < 16) {
    Serial.print("0");
  }
  Serial.print(a, HEX);
  if (scrpos == 19) {
    Serial.print(" << ");
    scrpos++;
  }
  else if (scrpos == 22) {
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
}

void printhdr(char *s)
{
  Serial.println();
  Serial.print(s);
  scrpos = 0;
}

bool connected_sp;
int to_app_pos, to_amp_pos;;
byte to_app_buf[5000], to_amp_buf[5000];

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

int curr_preset;

void setup() {
  int i;
  
  Serial.println("Started");
  
#ifdef M5_BOARD
  M5.begin();
  M5.Lcd.fillScreen(TFT_BLACK);
  M5.Lcd.setTextSize(4);
  M5.Lcd.println("AndroidLogger");
  M5.Lcd.println("-------------");
  M5.Lcd.setTextSize(3);
#else
  Heltec.begin(true /*DisplayEnable Enable*/, false /*LoRa Enable*/, true /*Serial Enable*/);
  Heltec.display->clear();
  Heltec.display->drawString(0, 0, "   Logger");
  Heltec.display->display();
#endif

  to_app_pos = 0;
  to_amp_pos = 0;
  scrpos = 0;
  curr_preset = 0;

  // Create server to act as Spark
  BLEDevice::init("Spark 40 BLE");
  
  pClient_sp = BLEDevice::createClient();
  pScan      = BLEDevice::getScan();

  // Connect to Spark
  connected_sp = false;
  
  while (!connected_sp) {
    pResults = pScan->start(4);
    BLEUUID SpServiceUuid(C_SERVICE);

    Serial.println("------------------------------");
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

  // This is what I was missing - obvious?
  
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
}


void loop() {
  uint8_t b;
  int i;

#ifdef M5_BOARD
  M5.update();
#endif

#ifdef M5_BOARD
  if (M5.BtnA.wasPressed()) {
    Serial.println();
    Serial.println("------------------------------------------");

    curr_preset++;
    if (curr_preset >=4 ) curr_preset = 0;

    preset_spk_cmd[preset_cmd_size-2] = curr_preset;
    preset_app_cmd[preset_cmd_size-2] = curr_preset;
    preset_app_cmd[preset_cmd_size-7] = curr_preset; // set the checksum too    
  
    pSender_sp->writeValue(preset_spk_cmd, preset_cmd_size);
    BTApp.write(preset_app_cmd, preset_cmd_size);
  }
#endif

// process messages from the app (classic bluetooth for android)
// max size 0xad, each block always end in 0x7f

  if (BTApp.available()) {
    b = BTApp.read();
    to_amp_buf[to_amp_pos++] = b;
    if (b == 0xf7) {
      pSender_sp->writeValue(to_amp_buf, to_amp_pos);
      
      printhdr("Write to spark:     ");
      for (i=0; i < to_amp_pos; i++) {
        printval(to_amp_buf[i]);
      }
      to_amp_pos = 0 ;
    }
  }

// process messages from the amp (ble)
// max size 0x6a, blocks don't necessarily end in 0x7f

  if (!amp_to_app.is_empty()) {
     amp_to_app.get(&b);
     to_app_buf[to_app_pos++] = b;  
  }
   
  if ((amp_to_app.is_empty() && to_app_pos > 0) || to_app_pos >= 0x6a) {
     BTApp.write(to_app_buf, to_app_pos);
  
     printhdr("Write to app:       ");
     for (i=0; i < to_app_pos; i++) {
       printval(to_app_buf[i]);
     }
     to_app_pos = 0;
  }
}
