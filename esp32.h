#ifndef _ESP32_H
#define _ESP32_H

#if defined(ESP32)

// this will enable SPIFFS compatiblity => no "real" folders
//#define CONFIG_LITTLEFS_SPIFFS_COMPAT 1

/** Data file names for esp32 / in filename is needed to work correctly */
#define IOPTS_FILENAME        "/iopts.dat"   // integer options data file
#define SOPTS_FILENAME        "/sopts.dat"   // string options data file
#define STATIONS_FILENAME     "/stns.dat"    // stations data file
#define NVCON_FILENAME        "/nvcon.dat"   // non-volatile controller data file, see OpenSprinkler.h --> struct NVConData
#define PROG_FILENAME         "/prog.dat"    // program data file
#define DONE_FILENAME         "/done.dat"    // used to indicate the completion of all files

#define ESP32_FS_BLOCK_SIZE	4096
// for testing only - factory reset should do it anyway
#define ESP32_FORMAT_FS_IF_FAILED true

// chose LCD type: 0.96 probably SSD1306, 1.3" is probably SH1106
#define LCD_SH1106
//#define LCD_SSD1306

#define MDNS_NAME "opensprinkler" // mDNS name for OS controler
#define OS_HW_VERSION    (OS_HW_VERSION_BASE+40)

//#define RFTX // uncoment when planning to use RX controler
//#define ETHPORT // uncoment when palnning to use wired etherner

// override of default I2C pins 21/22
#define SDA_PIN SDA
#define SCL_PIN SCL
#define LCD_I2CADDR      0x3c // 128x64 OLED display I2C address

#define IOEXP_PIN        0x99 // base for pins on main IO expander

/*  ESP32 port support only AC mode as DC and Latch need dedicated HW
 *  Dont need this to declare and search for Main IO controller and  
 *  ac, dc or latch drivers. However you may use DC nad Latch when dedicated HW builded - not tested
 */
#define MAIN_I2CADDR     0x20 // main IO expander I2C address
#define ACDR_I2CADDR     0x21 // ac driver I2C address
#define DCDR_I2CADDR     0x22 // dc driver I2C address 
#define LADR_I2CADDR     0x23 // latch driver I2C address

#define EXP_I2CADDR_BASE 0x24 // base of expander I2C address

 
  #define ETHER_BUFFER_SIZE   8192 // was 8192 

  /* To accommodate different OS30 versions, we use software defines pins */ 
  extern byte PIN_BUTTON_1;
  extern byte PIN_BUTTON_2;
  extern byte PIN_BUTTON_3;
  extern byte PIN_RFRX;
  extern byte PIN_RFTX;
  extern byte PIN_BOOST;
  extern byte PIN_BOOST_EN;
  extern byte PIN_LATCH_COM;
  extern byte PIN_LATCH_COMA;
  extern byte PIN_LATCH_COMK;
  extern byte PIN_SENSOR1;
  extern byte PIN_SENSOR2;
  extern byte PIN_IOEXP_INT;


  #define E0_PIN_BUTTON_1      25 // button 1 - v1pr 18
  #define E0_PIN_BUTTON_2      5 // button 2 - v1pr 5, originally 0
  #define E0_PIN_BUTTON_3      26 // button 3 - vipr 17
  #define E0_PIN_RFRX          255
  #define E0_PIN_RFTX          255
  #define E0_PIN_BOOST         255// special HW needed
  #define E0_PIN_BOOST_EN      255// special HW needed
  #define E0_PIN_LATCH_COM     255// not needed for ESP32
  #define E0_PIN_SENSOR1       36 // sensor 1 - v1pr 39
  #define E0_PIN_SENSOR2       2 // sensor 2  - v1pr 33
  #define E0_PIN_IOEXP_INT     255// not needed for ESP32
 
  #define PIN_ETHER_CS         255 // ENC28J60 CS (chip select pin) is 16 on OS 3.2.

  #define USE_IOEXP_SR 0 // use Shift-register as station setting - uncomment this to use built-in gpio style
  
  #define ON_BOARD_GPIN_LIST     {12,13,14,15,16,255,255,255} //  ESP32 on board gpins to be usead as sections, 255 - pin not defined
  #define PIN_FREE_LIST     {} // no free GPIO pin at the moment

  // if set to a real ADC pin, than it means the board has current sensor capabilities
  #define PIN_CURR_SENSE      39 // not used on v1pr's board, so 255
  
  #define STATION_LOGIC  0 // Zone output logic for relays - 1 => HIGH in ON, 0 => LOW is ON - v1pr board: 1

  // Rotary Encoder instead of buttons - not used for now
  //#define USE_ROTARY_ENCODER
  #define ROTARY_ENCODER_A_PIN 35 // must be interrupt capable PIN!
  #define ROTARY_ENCODER_B_PIN 34
  #define ROTARY_ENCODER_BUTTON_PIN 33 // this should be same, BUTTON_2
  
  #define BOOT_MENU_V2

  #define SEPARATE_MASTER_VALVE 19

  // 74HC595 shift reg
  // #define IOEXP_SR_OE_PIN // output enable pin, not used now
  #define IOEXP_SR_DATA_PIN 18
  #define IOEXP_SR_CLK_PIN 27
  #define IOEXP_SR_LATCH_PIN 32

  #define SYS_STATUS_LED_PIN  13

  // this it not nice, should be cleaned up
 	/* Original OS30 pin defines */
	//#define V0_MAIN_INPUTMASK 0b00001010 // main input pin mask
	// pins on main PCF8574 IO expander have pin numbers IOEXP_PIN+i
	#define V0_PIN_BUTTON_1      IOEXP_PIN+1 // button 1
	#define V0_PIN_BUTTON_2      0           // button 2
	#define V0_PIN_BUTTON_3      IOEXP_PIN+3 // button 3
	#define V0_PIN_RFRX          14
	#define V0_PIN_PWR_RX        IOEXP_PIN+0
	#define V0_PIN_RFTX          16
	#define V0_PIN_PWR_TX        IOEXP_PIN+2
	#define V0_PIN_BOOST         IOEXP_PIN+6
	#define V0_PIN_BOOST_EN      IOEXP_PIN+7
	#define V0_PIN_SENSOR1       12 // sensor 1
	#define V0_PIN_SENSOR2       13 // sensor 2

	/* OS31 pin defines */
	// pins on PCA9555A IO expander have pin numbers IOEXP_PIN+i
	#define V1_IO_CONFIG         0x1F00 // config bits
	#define V1_IO_OUTPUT         0x1F00 // output bits
	#define V1_PIN_BUTTON_1      IOEXP_PIN+10 // button 1
	#define V1_PIN_BUTTON_2      IOEXP_PIN+11 // button 2
	#define V1_PIN_BUTTON_3      IOEXP_PIN+12 // button 3
	#define V1_PIN_RFRX          14
	#define V1_PIN_RFTX          16
	#define V1_PIN_IOEXP_INT     12
	#define V1_PIN_BOOST         IOEXP_PIN+13
	#define V1_PIN_BOOST_EN      IOEXP_PIN+14
	#define V1_PIN_LATCH_COM     IOEXP_PIN+15
	#define V1_PIN_SENSOR1       IOEXP_PIN+8 // sensor 1
	#define V1_PIN_SENSOR2       IOEXP_PIN+9 // sensor 2

	/* OS32 pin defines */
	// pins on PCA9555A IO expander have pin numbers IOEXP_PIN+i
	#define V2_IO_CONFIG         0x1000 // config bits
	#define V2_IO_OUTPUT         0x1E00 // output bits
	#define V2_PIN_BUTTON_1      2 // button 1
	#define V2_PIN_BUTTON_2      0 // button 2
	#define V2_PIN_BUTTON_3      IOEXP_PIN+12 // button 3
	#define V2_PIN_RFTX          15
	#define V2_PIN_BOOST         IOEXP_PIN+13
	#define V2_PIN_BOOST_EN      IOEXP_PIN+14
	#define V2_PIN_LATCH_COMA    IOEXP_PIN+8  // latch COM+ (anode)
	#define V2_PIN_SRLAT         IOEXP_PIN+9  // shift register latch
	#define V2_PIN_SRCLK         IOEXP_PIN+10 // shift register clock
	#define V2_PIN_SRDAT         IOEXP_PIN+11 // shift register data
	#define V2_PIN_LATCH_COMK    IOEXP_PIN+15 // latch COM- (cathode)
	#define V2_PIN_SENSOR1       3  // sensor 1
	#define V2_PIN_SENSOR2       10 // sensor 2

#endif
#endif //_ESP32_H
