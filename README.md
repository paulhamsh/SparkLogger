## SparkLogger
Log commands between Spark app and amp

# UPDATE 20/10/21

Now can handle either BLE or classic bluetooth to the app in a single version :-)

Logs interactions between App on Android or IOS and Spark amp

IOS uses BLE throughout     
Android uses Classic bluetooth to the app and BLE to the amp     

Ensure serial monitor is on!

Works with M5Stack Core2 and Heltec Wifi - change the #define to select the board

Things in <<  >> are *likely* to be commands - unless they are part of a resposne to a preset request (0x0201) , in which case they are rubbish   
So the example below goes:   
022f, 032f   
022f, 032f   
0211, 0311   
0223, 0323   
022a, 032a   
0210, 0310   
0201, 0301   
   
Output like this:

```
Found Spark - trying to connect....
Spark connected
Available for app to connect...

Write to spark:     01 FE 00 00 53 FE 17 00 00 00 00 00 00 00 00 00 
                    F0 01 01 00 << 02 2F >> F7 
Write to app:       01 FE 00 00 41 FF 1D 00 00 00 00 00 00 00 00 00 
                    F0 01 01 77 << 03 2F >> 11 4E 01 04 03 2E F7 
Write to spark:     01 FE 00 00 53 FE 17 00 00 00 00 00 00 00 00 00 
                    F0 01 02 00 << 02 2F >> F7 
Write to app:       01 FE 00 00 41 FF 1D 00 00 00 00 00 00 00 00 00 
                    F0 01 02 77 << 03 2F >> 11 4E 01 04 03 2E F7 
Write to spark:     01 FE 00 00 53 FE 17 00 00 00 00 00 00 00 00 00 
                    F0 01 03 00 << 02 11 >> F7 
Write to app:       01 FE 00 00 41 FF 23 00 00 00 00 00 00 00 00 00 
                    F0 01 03 5D << 03 11 >> 02 08 28 53 70 61 72 6B 
                    00 20 34 30 F7 
Write to spark:     01 FE 00 00 53 FE 17 00 00 00 00 00 00 00 00 00 
                    F0 01 04 00 << 02 23 >> F7 
Write to app:       01 FE 00 00 41 FF 29 00 00 00 00 00 00 00 00 00 
                    F0 01 04 32 << 03 23 >> 02 0D 2D 53 30 34 30 43 
                    00 32 30 35 41 36 34 36 01 77 F7 
Write to spark:     01 FE 00 00 53 FE 1D 00 00 00 00 00 00 00 00 00 
                    F0 01 05 15 << 02 2A >> 01 14 00 01 02 03 F7 
Write to app:       01 FE 00 00 41 FF 1E 00 00 00 00 00 00 00 00 00 
                    F0 01 05 2D << 03 2A >> 0D 14 7D 4C 07 5A 58 F7 
                    
Write to spark:     01 FE 00 00 53 FE 17 00 00 00 00 00 00 00 00 00 
                    F0 01 06 00 << 02 10 >> F7 
Write to app:       01 FE 00 00 41 FF 1A 00 00 00 00 00 00 00 00 00 
                    F0 01 06 03 << 03 10 >> 00 00 03 F7 
Write to spark:     01 FE 00 00 53 FE 3C 00 00 00 00 00 00 00 00 00 
                    F0 01 07 01 << 02 01 >> 00 01 00 00 00 00 00 00 
                    00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 
                    00 00 00 00 00 00 00 00 00 00 00 00 00 F7 
Write to app:       01 FE 00 00 41 FF 6A 00 00 00 00 00 00 00 00 00 
                    F0 01 07 08 << 03 01 >> 20 10 00 19 01 00 59 24 
                    00 43 30 30 39 39 41 37 00 39 2D 42 45 33 35 2D 
                    00 34 38 32 44 2D 41 46 F7 F0 01 07 11 03 01 00 
                    10 01 19 37 30 2D 43 00 36 31 42 36 36 35 39 10 
                    36 31 42 30 27 34 2D 20 4D 65 74 61 6C 23 30 F7 
                    F0 01 07 4C 03 01 20 10 02 19 2E 37 
                    .....




```

If you want to change it to just do passthru and have ability to command the Spark, add:   

```
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
  0x41, 0xFF, 0x17, 0x00,
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,
  0xF0, 0x01, 0x24, 0x00,
  0x03, 0x38, 0x00, 0x00,
  0x00, 0xF7
};

And on some trigger....
  curr_preset = 1   // put your value here
  preset_spk_cmd[preset_cmd_size-2] = curr_preset;
  preset_app_cmd[preset_cmd_size-2] = curr_preset;

And then for Android:   
  pSender_sp->writeValue(preset_spk_cmd, preset_cmd_size);
  BTApp.write(preset_app_cmd, preset_cmd_size);
  
And for IOS:
  pSender_sp->writeValue(preset_spk_cmd, preset_cmd_size);
  pCharacteristic_send->setValue(preset_spk_cmd, preset_cmd_size);
  pCharacteristic_send->notify(true);
  
And remove the code from printhdr() and printval() so they do nothing - which will stop the logging.   
```
  

  
  


```


