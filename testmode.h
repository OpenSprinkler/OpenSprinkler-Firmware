
#ifndef _TESTMODE_H
#define _TESTMODE_H

#if defined(ESP32)
void SPIFFS_list_dir();
void scan_i2C();

void SPIFFS_list_dir() {
 
 
  if (!SPIFFS.begin(true)) {
    DEBUG_PRINTLN("An Error has occurred while mounting SPIFFS");
    return;
  }
 
  File root = SPIFFS.open("/");
 
  File file = root.openNextFile();
 
  while(file){
 
      DEBUG_PRINT("FILE: ");
      DEBUG_PRINTLN(file.name());
 
      file = root.openNextFile();
  }
}

void scan_i2c() {
      
      byte error, address;
      int nDevices;

      DEBUG_PRINTLN("Scanning i2c for devices...");

      nDevices = 0;
      for(address = 1; address < 127; address++ ) 
        {

        Wire.beginTransmission(address);
        error = Wire.endTransmission();

        if (error == 0)
            {
            DEBUG_PRINT("I2C device found at address 0x");
            if (address<16) 
            DEBUG_PRINT("0");
            DEBUG_PRINTX(address);
            DEBUG_PRINTLN("  !");

            nDevices++;
            }
        else if (error==4) 
            {
            DEBUG_PRINT("Unknow error at address 0x");
            if (address<16) 
            DEBUG_PRINT("0");
            DEBUG_PRINTX(address);
            }    
        }
        
        if (nDevices == 0) DEBUG_PRINTLN("No I2C devices found");

}

#endif //ESP32
#endif  // _TESTMODE_H
