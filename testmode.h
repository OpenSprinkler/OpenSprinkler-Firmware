
#ifndef _TESTMODE_H
#define _TESTMODE_H

#if defined(ESP32)

#ifndef DEBUG_PRINTX
#define DEBUG_PRINTX(x)  {Serial.print(F("0x")); Serial.print(x, HEX); }
#endif

void ESP32_FS_list_dir();
void ESP32_listDir(const char * dirname, uint8_t levels);
void ESP32_readFile(const char * path);
void scan_i2C();

void ESP32_FS_list_dir() {
 
 /*
 
  if (!LittleFS.begin()) {
    DEBUG_PRINTLN("An Error has occurred while mounting FS");
    return;
  }
 */
  File root = LittleFS.open("/");
 
  File file = root.openNextFile();
 
  while(file){
 
      DEBUG_PRINT("FILE: ");
      DEBUG_PRINTLN(file.name());
 
      file = root.openNextFile();
  }
}

void ESP32_listDir(const char * dirname, uint8_t levels){
    Serial.printf("Listing directory: %s\r\n", dirname);

    File root = LittleFS.open(dirname);
    if(!root){
        Serial.println("- failed to open directory");
        return;
    }
    if(!root.isDirectory()){
        Serial.println(" - not a directory");
        return;
    }

    File file = root.openNextFile();
    while(file){
        if(file.isDirectory()){
            Serial.print("  DIR : ");
            Serial.println(file.name());
            if(levels){
                ESP32_listDir(file.path(), levels -1);
            }
            Serial.println(" --- ");
        } else {
            Serial.print("  FILE: ");
            Serial.print(file.name());
            Serial.print("\tSIZE: ");
            Serial.println(file.size());
        }
        file = root.openNextFile();
    }
}

void ESP32_readFile(const char * path){
    Serial.printf("Reading file: %s\r\n", path);

    File file = LittleFS.open(path);
    if(!file || file.isDirectory()){
        Serial.println("- failed to open file for reading");
        return;
    }

    Serial.println("- read from file:");
    while(file.available()){
        Serial.write(file.read());
    }
    file.close();
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
            DEBUG_PRINT("I2C device found at address ");
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
