#include <Wire.h>

#if defined(ESP8266) || defined(ESP32)
	struct tcp_pcb;
	extern struct tcp_pcb* tcp_tw_pcbs;
	extern "C" void tcp_abort (struct tcp_pcb* pcb);
	void tcpCleanup() { // losing bytes work around
		while(tcp_tw_pcbs) { tcp_abort(tcp_tw_pcbs); }
	}
#else
  #include <SdFat.h>
#endif

#include "OpenSprinkler.h"

extern OpenSprinkler os;

void do_setup();
void do_loop();

void setup() {
#if defined(ESP32)
/* Seting internal station pins to prevent unstable behavior on startup */
  int i;
  unsigned int pin_list[] = ON_BOARD_GPIN_LIST;
  for( i=0; i<8; i++ ){
    if(pin_list[i] !=255){
      pinMode(pin_list[i], OUTPUT);
      digitalWrite(pin_list[i], ~STATION_LOGIC);
    }
  }

#endif
  do_setup();
}

void loop() {
  do_loop();
#if defined(ESP8266)
  tcpCleanup();
#endif
}
