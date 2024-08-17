/*
 * EMail Sender Arduino, esp8266, stm32 and esp32 library to send email
 *
 * AUTHOR:  Renzo Mischianti
 * VERSION: 3.0.14
 *
 * https://www.mischianti.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2017 Renzo Mischianti www.mischianti.org All right reserved.
 *
 * You may copy, alter and reuse this code in any way you like, but please leave
 * reference to www.mischianti.org in your comments if you redistribute this code.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "EMailSender.h"
#include <stdio.h>
#if defined(ESP32)
#include <mbedtls/base64.h>
#endif

//#include <SPIFFS.h>
//#include <LittleFS.h>

//#define SD SPIFFS

// BASE64 -----------------------------------------------------------
const char PROGMEM b64_alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                    "abcdefghijklmnopqrstuvwxyz"
                                    "0123456789+/";

#define encode64(arr) encode64_f(arr,strlen(arr))

inline void a3_to_a4(unsigned char * a4, unsigned char * a3) {
  a4[0] = (a3[0] & 0xfc) >> 2;
  a4[1] = ((a3[0] & 0x03) << 4) + ((a3[1] & 0xf0) >> 4);
  a4[2] = ((a3[1] & 0x0f) << 2) + ((a3[2] & 0xc0) >> 6);
  a4[3] = (a3[2] & 0x3f);
}

int base64_encode(char *output, char *input, int inputLen) {
  int i = 0, j = 0;
  int encLen = 0;
  unsigned char a3[3];
  unsigned char a4[4];

  while (inputLen--) {
    a3[i++] = *(input++);
    if (i == 3) {
      a3_to_a4(a4, a3);

      for (i = 0; i < 4; i++) {
        output[encLen++] = pgm_read_byte(&b64_alphabet[a4[i]]);
      }

      i = 0;
    }
  }

  if (i) {
    for (j = i; j < 3; j++) {
      a3[j] = '\0';
    }

    a3_to_a4(a4, a3);

    for (j = 0; j < i + 1; j++) {
      output[encLen++] = pgm_read_byte(&b64_alphabet[a4[j]]);
    }

    while ((i++ < 3)) {
      output[encLen++] = '=';
    }
  }
  output[encLen] = '\0';
  return encLen;
}

int base64_enc_length(int plainLen) {
  int n = plainLen;
  return (n + 2 - ((n + 2) % 3)) / 3 * 4;
}

const char* encode64_f(char* input, uint8_t len) {
  // encoding

	EMAIL_DEBUG_PRINTLN(F("Encoding"));
	EMAIL_DEBUG_PRINTLN(input);
	EMAIL_DEBUG_PRINTLN(len);

  //int encodedLen =
 base64_enc_length(len);
  static char encoded[256];
  // note input is consumed in this step: it will be empty afterwards
  base64_encode(encoded, input, len);
  return encoded;
}

// END BASE64 ---------------------------------------------------------

EMailSender::EMailSender(const char* email_login, const char* email_password, const char* email_from, const char* name_from ,
		const char* smtp_server, uint16_t smtp_port) {
	this->setEMailLogin(email_login);
	this->setEMailFrom(email_from);
	this->setEMailPassword(email_password);
	this->setSMTPServer(smtp_server);
	this->setSMTPPort(smtp_port);
	this->setNameFrom(name_from);
//	this->isSecure = isSecure;
}
EMailSender::EMailSender(const char* email_login, const char* email_password, const char* email_from,
		const char* smtp_server, uint16_t smtp_port) {
	this->setEMailLogin(email_login);
	this->setEMailFrom(email_from);
	this->setEMailPassword(email_password);
	this->setSMTPServer(smtp_server);
	this->setSMTPPort(smtp_port);

//	this->isSecure = isSecure;
}

EMailSender::EMailSender(const char* email_login, const char* email_password, const char* email_from, const char* name_from ) {
	this->setEMailLogin(email_login);
	this->setEMailFrom(email_from);
	this->setEMailPassword(email_password);
	this->setNameFrom(name_from);
	this->setNameFrom(name_from);

//	this->isSecure = isSecure;
}
EMailSender::EMailSender(const char* email_login, const char* email_password, const char* email_from) {
	this->setEMailLogin(email_login);
	this->setEMailFrom(email_from);
	this->setEMailPassword(email_password);

//	this->isSecure = isSecure;
}

EMailSender::EMailSender(const char* email_login, const char* email_password){
	this->setEMailLogin(email_login);
	this->setEMailFrom(email_login);
	this->setEMailPassword(email_password);

//	this->isSecure = isSecure;
}

void EMailSender::setSMTPPort(uint16_t smtp_port){
	this->smtp_port = smtp_port;
};
void EMailSender::setSMTPServer(const char* smtp_server){
	delete [] this->smtp_server;
	this->smtp_server = new char[strlen(smtp_server)+1];
	strcpy(this->smtp_server, smtp_server);
};

void EMailSender::setEMailLogin(const char* email_login){
	delete [] this->email_login;
	this->email_login = new char[strlen(email_login)+1];
	strcpy(this->email_login, email_login);
};
void EMailSender::setEMailFrom(const char* email_from){
	delete [] this->email_from;
	this->email_from = new char[strlen(email_from)+1];
	strcpy(this->email_from, email_from);
};
void EMailSender::setNameFrom(const char* name_from){
	delete [] this->name_from;
	this->name_from = new char[strlen(name_from)+1];
	strcpy(this->name_from, name_from);
};
void EMailSender::setEMailPassword(const char* email_password){
	delete [] this->email_password;
	this->email_password = new char[strlen(email_password)+1];
	strcpy(this->email_password, email_password);
};

void EMailSender::setIsSecure(bool isSecure) {
	this->isSecure = isSecure;
}

#ifdef SSLCLIENT_WRAPPER
EMailSender::Response EMailSender::awaitSMTPResponse(SSLClient &client,
		const char* resp, const char* respDesc, uint16_t timeOut) {
	EMailSender::Response response;
	uint32_t ts = millis();
	while (!client.available()) {
		if (millis() > (ts + timeOut)) {
			response.code = F("1");
			response.desc = String(respDesc) + "! " + F("SMTP Response TIMEOUT!");
			response.status = false;

			return response;
		}
	}
	_serverResponce = client.readStringUntil('\n');

	EMAIL_DEBUG_PRINTLN(_serverResponce);
	if (resp && _serverResponce.indexOf(resp) == -1){
		response.code = resp;
		response.desc = respDesc +String(" (") + _serverResponce + String(")");
		response.status = false;

		return response;
	}

	response.status = true;
	return response;
}
#else
EMailSender::Response EMailSender::awaitSMTPResponse(EMAIL_NETWORK_CLASS &client,
		const char* resp, const char* respDesc, uint16_t timeOut) {
	EMailSender::Response response;
	uint32_t ts = millis();
	while (!client.available()) {
		if (millis() > (ts + timeOut)) {
			response.code = F("1");
			response.desc = String(respDesc) + "! " + F("SMTP Response TIMEOUT!");
			response.status = false;
			return response;
		}
	}
	_serverResponce = client.readStringUntil('\n');

	EMAIL_DEBUG_PRINTLN(_serverResponce);
	if (resp && _serverResponce.indexOf(resp) == -1){
		response.code = resp;
		response.desc = respDesc +String(" (") + _serverResponce + String(")");
		response.status = false;
		return response;
	}

	response.status = true;
	return response;
}
#endif

static const char cb64[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
void encodeblock(unsigned char in[3],unsigned char out[4],int len) {
 out[0]=cb64[in[0]>>2]; out[1]=cb64[((in[0]&0x03)<<4)|((in[1]&0xF0)>>4)];
 out[2]=(unsigned char) (len>1 ? cb64[((in[1]&0x0F)<<2)|((in[2]&0xC0)>>6)] : '=');
 out[3]=(unsigned char) (len>2 ? cb64[in[2]&0x3F] : '=');
}

#ifdef ENABLE_ATTACHMENTS
	#ifdef STORAGE_EXTERNAL_ENABLED
		#if (defined(DIFFERENT_FILE_MANAGE) && defined(EMAIL_FILE_EX)) || !defined(STORAGE_INTERNAL_ENABLED)
			#ifdef SSLCLIENT_WRAPPER
						void encode(EMAIL_FILE_EX *file, SSLClient *client) {
						unsigned char in[3],out[4];
						int i,len,blocksout=0;

						while (file->available()!=0) {
						len=0;
							for (i=0;i<3;i++){
								in[i]=(unsigned char) file->read();
									if (file->available()!=0) len++;
											else in[i]=0;
							}
							if (len){
								encodeblock(in,out,len);
						//         for(i=0;i<4;i++) client->write(out[i]);
								client->write(out, 4);
								blocksout++; }
							if (blocksout>=19||file->available()==0){
								if (blocksout) {
									client->print("\r\n");
								}
								blocksout=0;
							}
						}
						}

			#else
					void encode(EMAIL_FILE_EX *file, EMAIL_NETWORK_CLASS *client) {
						unsigned char in[3],out[4];
						int i,len,blocksout=0;

						while (file->available()!=0) {
						len=0;
							for (i=0;i<3;i++){
								in[i]=(unsigned char) file->read();
									if (file->available()!=0) len++;
											else in[i]=0;
							}
							if (len){
								encodeblock(in,out,len);
						//         for(i=0;i<4;i++) client->write(out[i]);
								client->write(out, 4);
								blocksout++; }
							if (blocksout>=19||file->available()==0){
								if (blocksout) {
									client->print("\r\n");
								}
								blocksout=0;
							}
						}
						}
			#endif

		#endif
	#endif
	#ifdef STORAGE_INTERNAL_ENABLED
		#if defined(DIFFERENT_FILE_MANAGE) || (!defined(DIFFERENT_FILE_MANAGE) && defined(EMAIL_FILE)) || !defined(STORAGE_EXTERNAL_ENABLED)

			#ifdef SSLCLIENT_WRAPPER
					void encode(EMAIL_FILE *file, SSLClient *client) {
					 unsigned char in[3],out[4];
					 int i,len,blocksout=0;

					 while (file->available()!=0) {
					   len=0;
						 for (i=0;i<3;i++){
							   in[i]=(unsigned char) file->read();
								   if (file->available()!=0) len++;
										 else in[i]=0;
						 }
						 if (len){
							 encodeblock(in,out,len);
					//         for(i=0;i<4;i++) client->write(out[i]);
							 client->write(out, 4);
							 blocksout++; }
						 if (blocksout>=19||file->available()==0){
							 if (blocksout) {
								 client->print("\r\n");
							 }
							 blocksout=0;
						 }
					  }
					}

			#else
					void encode(EMAIL_FILE *file, EMAIL_NETWORK_CLASS *client) {
					 unsigned char in[3],out[4];
					 int i,len,blocksout=0;

					 while (file->available()!=0) {
					   len=0;
						 for (i=0;i<3;i++){
							   in[i]=(unsigned char) file->read();
								   if (file->available()!=0) len++;
										 else in[i]=0;
						 }
						 if (len){
							 encodeblock(in,out,len);
					//         for(i=0;i<4;i++) client->write(out[i]);
							 client->write(out, 4);
							 blocksout++; }
						 if (blocksout>=19||file->available()==0){
							 if (blocksout) {
								 client->print("\r\n");
							 }
							 blocksout=0;
						 }
					  }
					}
			#endif
		#endif
	#endif
#endif

const char** toCharArray(String arr[], int num) {
    // If we ever alloc with new with have to delete
    const char** buffer = new const char*[num];

    for(int i = 0; i < num; i++) {
        buffer[i] = arr[i].c_str();
    }

    return buffer;
}
const char** toCharArray(char* arr[], int num) {
    // If we ever alloc with new with have to delete
    const char** buffer = new const char*[num];

    for(int i = 0; i < num; i++) {
        buffer[i] = arr[i];
    }

    return buffer;
}

EMailSender::Response EMailSender::send(char* tos[], byte sizeOfTo, EMailMessage &email, Attachments attachments) {
	return send(toCharArray(tos, sizeOfTo), sizeOfTo, 0, 0, email, attachments);
}
EMailSender::Response EMailSender::send(char* tos[], byte sizeOfTo,  byte sizeOfCc,  EMailMessage &email, Attachments attachments) {
	return send(toCharArray(tos, sizeOfTo+sizeOfCc), sizeOfTo, sizeOfCc, 0, email, attachments);
}
EMailSender::Response EMailSender::send(char* tos[], byte sizeOfTo,  byte sizeOfCc,byte sizeOfCCn, EMailMessage &email, Attachments attachments){
	return send(toCharArray(tos, sizeOfTo+sizeOfCc+sizeOfCCn), sizeOfTo, sizeOfCc, sizeOfCCn, email, attachments);
}


EMailSender::Response EMailSender::send(String to, EMailMessage &email, Attachments attachments){
	  EMAIL_DEBUG_PRINT(F("ONLY ONE RECIPIENT"));

	const char* arrEmail[] =  {to.c_str()};
	return send(arrEmail, 1, email, attachments);
}

EMailSender::Response EMailSender::send(String tos[], byte sizeOfTo, EMailMessage &email, Attachments attachments) {
	return send(toCharArray(tos, sizeOfTo), sizeOfTo, 0, 0, email, attachments);
}

EMailSender::Response EMailSender::send(String tos[], byte sizeOfTo,  byte sizeOfCc,  EMailMessage &email, Attachments attachments) {
	return send(toCharArray(tos, sizeOfTo+sizeOfCc), sizeOfTo, sizeOfCc, 0, email, attachments);
}

EMailSender::Response EMailSender::send(String tos[], byte sizeOfTo,  byte sizeOfCc,byte sizeOfCCn, EMailMessage &email, Attachments attachments){
	return send(toCharArray(tos, sizeOfTo+sizeOfCc+sizeOfCCn), sizeOfTo, sizeOfCc, sizeOfCCn, email, attachments);
}

EMailSender::Response EMailSender::send(const char* to, EMailMessage &email, Attachments attachments){
	  EMAIL_DEBUG_PRINT(F("ONLY ONE RECIPIENT"));

	const char* arrEmail[] =  {to};
	return send(arrEmail, 1, email, attachments);
}

EMailSender::Response EMailSender::send(const char* to[], byte sizeOfTo, EMailMessage &email, Attachments attachments) {
	EMAIL_DEBUG_PRINTLN(F("miltiple destination and attachments"));
	return send(to, sizeOfTo, 0, email, attachments);
}

EMailSender::Response EMailSender::send(const char* to[], byte sizeOfTo,  byte sizeOfCc,  EMailMessage &email, Attachments attachments)
{
	return send(to, sizeOfTo, sizeOfCc, 0, email, attachments);
}

#ifdef SSLCLIENT_WRAPPER
#ifdef PUT_OUTSIDE_SCOPE_CLIENT_DECLARATION
	// Initialize the SSL client library
	// We input an EthernetClient, our trust anchors, and the analog pin
	EMAIL_NETWORK_CLASS base_client;
	SSLClient client(base_client, TAs, (size_t)TAs_NUM, ANALOG_PIN, 2);
#else
	#error "You must put outside scope the client declaration if you want use SSLClient!"
#endif
#else
	#ifdef PUT_OUTSIDE_SCOPE_CLIENT_DECLARATION
		EMAIL_NETWORK_CLASS client;
	#endif
#endif

EMailSender::Response EMailSender::send(const char* to[], byte sizeOfTo,  byte sizeOfCc,byte sizeOfCCn, EMailMessage &email, Attachments attachments)
{
#ifdef SSLCLIENT_WRAPPER
	EMAIL_DEBUG_PRINTLN(F("SSLClient active!"));
#else
	#ifndef PUT_OUTSIDE_SCOPE_CLIENT_DECLARATION
	  EMAIL_NETWORK_CLASS client;
	#endif

  EMAIL_DEBUG_PRINT(F("Insecure client:"));
  EMAIL_DEBUG_PRINTLN(this->isSecure);
	#ifndef FORCE_DISABLE_SSL
		#if (EMAIL_NETWORK_TYPE == NETWORK_ESP8266 || EMAIL_NETWORK_TYPE == NETWORK_ESP8266_242)
			#ifndef ARDUINO_ESP8266_RELEASE_2_4_2
			  if (this->isSecure == false){
				  client.setInsecure();
				  bool mfln = client.probeMaxFragmentLength(this->smtp_server, this->smtp_port, 512);

				  EMAIL_DEBUG_PRINT("MFLN supported: ");
				  EMAIL_DEBUG_PRINTLN(mfln?"yes":"no");

				  if (mfln) {
					  client.setBufferSizes(512, 512);
				  } else {
					  client.setBufferSizes(2048, 2048);
				  }
			  }
			#endif
		#elif (EMAIL_NETWORK_TYPE == NETWORK_ESP32)
		//	  String coreVersion = String(ESP.getSdkVersion());
		//	  uint8_t firstdot = coreVersion.indexOf('.');
		//
		//	  EMAIL_DEBUG_PRINTLN(coreVersion.substring(1, coreVersion.indexOf('.', firstdot+1)).toFloat());
		//	  EMAIL_DEBUG_PRINTLN(coreVersion.substring(1, coreVersion.indexOf('.', firstdot+1)).toFloat() >= 3.3f);
		//	  if (coreVersion.substring(1, coreVersion.indexOf('.', firstdot+1)).toFloat() >= 3.3f) {
		//		  client.setInsecure();
		//	  }
			#ifndef ARDUINO_ARCH_RP2040
				#include <core_version.h>
				#if ((!defined(ARDUINO_ESP32_RELEASE_1_0_4)) && (!defined(ARDUINO_ESP32_RELEASE_1_0_3)) && (!defined(ARDUINO_ESP32_RELEASE_1_0_2)))
					  client.setInsecure();
				#endif
			#else
			    client.setInsecure();
			#endif
		#endif
	#endif
#endif
  EMailSender::Response response;

  EMAIL_DEBUG_PRINTLN(this->smtp_server);
  EMAIL_DEBUG_PRINTLN(this->smtp_port);

  if(!client.connect(this->smtp_server, this->smtp_port)) {
	  response.desc = F("Could not connect to mail server");
	  response.code = F("2");
	  response.status = false;

	  client.flush();
	  client.stop();

	  return response;
  }

  response = awaitSMTPResponse(client, "220", "Connection Error");
  if (!response.status) {
	  client.flush();
	  client.stop();
	  return response;
  }

  if (this->additionalResponseLineOnConnection == 0) {
  	  if (DEFAULT_CONNECTION_RESPONSE_COUNT == 'a'){
  	    this->additionalResponseLineOnConnection = 255;
  	  } else {
  	    this->additionalResponseLineOnConnection = DEFAULT_CONNECTION_RESPONSE_COUNT;
  	  }
  }

  if (this->additionalResponseLineOnConnection > 0){
	  for (int i = 0; i<=this->additionalResponseLineOnConnection; i++) {
		response = awaitSMTPResponse(client, "220", "Connection response error ", 2500);
		//if additionalResponseLineOnConnection is set to 255: wait out all code 250 responses, then continue
        if (this->additionalResponseLineOnConnection == 255) break;
        else {
            if (!response.status && response.code == F("1")) {
                response.desc = F("Connection error! Reduce the HELO response line!");
                client.flush();
                client.stop();
                return response;
            }
		}
	  }
  }

  String commandHELO = "HELO";
  if (this->useEHLO == true) {
	  commandHELO = "EHLO";
  }
  String helo = commandHELO + " "+String(publicIPDescriptor)+" ";
  EMAIL_DEBUG_PRINTLN(helo);
  client.println(helo);

  response = awaitSMTPResponse(client, "250", "Identification error");
  if (!response.status) {
	  client.flush();
	  client.stop();
	  return response;
  }

  if (this->useEHLO == true && this->additionalResponseLineOnHELO == 0) {
	  if (DEFAULT_EHLO_RESPONSE_COUNT == 'a'){
	    this->additionalResponseLineOnHELO = 255;
	  } else {
	    this->additionalResponseLineOnHELO = DEFAULT_EHLO_RESPONSE_COUNT;
	  }
  }

  if (this->additionalResponseLineOnHELO > 0){
	  for (int i = 0; i<=this->additionalResponseLineOnHELO; i++) {
		response = awaitSMTPResponse(client, "250", "EHLO error", 2500);
		if (!response.status && response.code == F("1")) {
			//if additionalResponseLineOnHELO is set to 255: wait out all code 250 responses, then continue
			if (this->additionalResponseLineOnHELO == 255) break;
			else {
				response.desc = F("Timeout! Reduce additional HELO response line count or set it to 255!");
				client.flush();
				client.stop();
				return response;
			}
		}
	  }
  }

  if (useAuth){
	  if (this->isSASLLogin == true){

		  int size = 1 + strlen(this->email_login)+ strlen(this->email_password)+2;
	      char * logPass = (char *) malloc(size);

//	      strcpy(logPass, " ");
//	      strcat(logPass, this->email_login);
//	      strcat(logPass, " ");
//	      strcat(logPass, this->email_password);

//		  String logPass;
	      int maincont = 0;

	      logPass[maincont++] = ' ';
	      logPass[maincont++] = (char) 0;

	      for (unsigned int i = 0;i<strlen(this->email_login);i++){
	    	  logPass[maincont++] = this->email_login[i];
	      }
	      logPass[maincont++] = (char) 0;
	      for (unsigned int i = 0;i<strlen(this->email_password);i++){
	    	  logPass[maincont++] = this->email_password[i];
	      }


//	      strcpy(logPass, "\0");
//	      strcat(logPass, this->email_login);
//	      strcat(logPass, "\0");
//	      strcat(logPass, this->email_password);

		  String auth = "AUTH PLAIN "+String(encode64_f(logPass, size));
//		  String auth = "AUTH PLAIN "+String(encode64(logPass));
		  EMAIL_DEBUG_PRINTLN(auth);
		  client.println(auth);
          }
#if defined(ESP32)
	  else if (this->isCramMD5Login == true) {
		  EMAIL_DEBUG_PRINTLN(F("AUTH CRAM-MD5"));
		  client.println(F("AUTH CRAM-MD5"));

                  // read back the base64 encoded digest.
		  //
                  response = awaitSMTPResponse(client,"334","No digest error");
                  if (!response.status) {
			client.flush(); 
			client.stop(); 
			return response; 
		  };
		  _serverResponce = _serverResponce.substring(4); // '334<space>'

		  size_t b64l = _serverResponce.length()-1; // C vs C++ counting of \0
		  const unsigned char * b64 = (const unsigned char *)_serverResponce.c_str();
                  EMAIL_DEBUG_PRINTLN("B64digest="+String((char *)b64) + " Len=" + String((int)b64l));

		  unsigned char digest[256];
		  size_t len;

		  int e = mbedtls_base64_decode(digest, sizeof(digest), &len, b64, b64l);
                  if (e || len < 3 || len >= 256) {
			response.code = F("999"); 
			response.desc = F("Invalid digest"); 
			response.status = false; 
			client.flush(); 
			client.stop(); 
			return response; 
		  };

		  // calculate HMAC with the password as the key of this digest.
		  //
		  mbedtls_md_context_t ctx;
		  mbedtls_md_type_t md_type = MBEDTLS_MD_MD5;
		  unsigned char md5[16];

		  mbedtls_md_init(&ctx);
		  mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 1);
		  mbedtls_md_hmac_starts(&ctx, 
			(const unsigned char *)this->email_password, strlen(this->email_password));
		  mbedtls_md_hmac_update(&ctx, digest, len);
		  mbedtls_md_hmac_finish(&ctx, md5);
		  mbedtls_md_free(&ctx);

                  // build an output string of the username followed by the __lowercase__ hex of the md5
		  //
                  String rsp = String(this->email_login) + " ";
                  for(int i = 0; i < sizeof(md5); i++) {
			unsigned char c = md5[i];
			char h[16+1] = "0123456789abcdef";
			rsp += String(h[ (c >> 4) &0xF]) +  String(h[ (c >> 0) &0xF]);
		  };

		  // And submit this to the server as a login string.
		  EMAIL_DEBUG_PRINTLN(encode64((char*)rsp.c_str()));
		  client.println(encode64((char*)rsp.c_str()));

 		  // now exepct the normal login confirmation to continue.
	}
#endif 
	  else{
		  EMAIL_DEBUG_PRINTLN(F("AUTH LOGIN:"));
		  client.println(F("AUTH LOGIN"));
		  awaitSMTPResponse(client);

		  EMAIL_DEBUG_PRINTLN(encode64(this->email_login));
		  client.println(encode64(this->email_login));
		  awaitSMTPResponse(client);

		  EMAIL_DEBUG_PRINTLN(encode64(this->email_password));
		  client.println(encode64(this->email_password));
	  }
	  response = awaitSMTPResponse(client, "235", "SMTP AUTH error");
	  if (!response.status) {
		  client.flush();
		  client.stop();
		  return response;
	  }
  }
  EMAIL_DEBUG_PRINT(F("MAIL FROM: <"));
  EMAIL_DEBUG_PRINT(this->email_from);
  EMAIL_DEBUG_PRINTLN(F(">"));

  client.print(F("MAIL FROM: <"));
  client.print(this->email_from);
  client.println(F(">"));
  awaitSMTPResponse(client);

//  String rcpt = "RCPT TO: <" + String(to) + '>';
//
//  EMAIL_DEBUG_PRINTLN(rcpt);
//  client.println(rcpt);

  int cont;
  for (cont=0;cont<(sizeOfTo+sizeOfCc+sizeOfCCn);cont++){
	  EMAIL_DEBUG_PRINT(F("RCPT TO: <"));
	  EMAIL_DEBUG_PRINT(to[cont]);
	  EMAIL_DEBUG_PRINTLN(F(">"));

	  client.print(F("RCPT TO: <"));
	  client.print(to[cont]);
	  client.println(F(">"));
	  awaitSMTPResponse(client);
  }

  EMAIL_DEBUG_PRINTLN(F("DATA:"));
  client.println(F("DATA"));

  response = awaitSMTPResponse(client, "354", "SMTP DATA error");
  if (!response.status) {
	  client.flush();
	  client.stop();
	  return response;
  }

//  client.println("From: <" + String(this->email_from) + '>');

  client.print(F("From: "));
  if (this->name_from){
	  client.print(this->name_from);
  }
  client.print(F(" <"));
  client.print(this->email_from);
  client.println(F(">"));

//  client.println("To: <" + String(to) + '>');

  client.print(F("To: "));
  for (cont=0;cont<sizeOfTo;cont++){
	  client.print(F("<"));
	  client.print(to[cont]);
	  client.print(">");
	  if (cont!=sizeOfTo-1){
		  client.print(",");
	  }
  }
  client.println();

  if (sizeOfCc>0){
	  client.print(F("Cc: "));
	  for (;cont<sizeOfTo+sizeOfCc;cont++){
		  client.print(F("<"));
		  client.print(to[cont]);
		  client.print(">");
		  if (cont!=sizeOfCc-1){
			  client.print(",");
		  }
	  }
	  client.println();
  }

  if (sizeOfCCn>0){
	  client.print(F("CCn: "));
	  for (;cont<sizeOfTo+sizeOfCc+sizeOfCCn;cont++){
		  client.print(F("<"));
		  client.print(to[cont]);
		  client.print(">");
		  if (cont!=sizeOfCCn-1){
			  client.print(",");
		  }
	  }
	  client.println();
  }

  client.print(F("Subject: "));
  client.println(email.subject);

//  client.println(F("Mime-Version: 1.0"));

  client.println(F("MIME-Version: 1.0"));
  client.println(F("Content-Type: Multipart/mixed; boundary=frontier"));

  client.println(F("--frontier"));

    client.print(F("Content-Type: "));
    client.print(email.mime);
    client.println(F("; charset=\"UTF-8\""));
//  client.println(F("Content-Type: text/html; charset=\"UTF-8\""));
  client.println(F("Content-Transfer-Encoding: 7bit"));
  client.println();
  if (email.mime==F("text/html")){
//	  String body = "<!DOCTYPE html><html lang=\"en\">" + String(email.message) + "</html>";

	  client.print(F("<!DOCTYPE html><html lang=\"en\">"));
	  client.print(email.message);
	  client.println(F("</html>"));

//	  client.println(body);
  }else{
	  client.println(email.message);
  }
  client.println();

#ifdef STORAGE_INTERNAL_ENABLED
  bool spiffsActive = false;
#endif
#ifdef STORAGE_EXTERNAL_ENABLED
  bool sdActive = false;
#endif

#if defined(ENABLE_ATTACHMENTS) && (defined(STORAGE_EXTERNAL_ENABLED) || defined(STORAGE_INTERNAL_ENABLED))
//  if ((sizeof(attachs) / sizeof(attachs[0]))>0){
  if (sizeof(attachments)>0 && attachments.number>0){

	  EMAIL_DEBUG_PRINT(F("Array: "));
//	  for (int i = 0; i<(sizeof(attachs) / sizeof(attachs[0])); i++){
	  for (int i = 0; i<attachments.number; i++){
		  uint8_t tBuf[64];

		  if (attachments.fileDescriptor[i].url.length()==0){
			  EMailSender::Response response;
			  response.code = F("400");
			  response.desc = "Error no filename specified for the file "+attachments.fileDescriptor[i].filename;
			  response.status = false;
			  client.flush();
			  client.stop();

			  return response;
		  }
		  if (attachments.fileDescriptor[i].mime.length()==0){
			  EMailSender::Response response;
			  response.code = F("400");
			  response.desc = "Error no mime type specified for the file "+attachments.fileDescriptor[i].url;
			  response.status = false;
			  client.flush();
			  client.stop();

			  return response;
		  }
		  if (attachments.fileDescriptor[i].filename.length()==0){
			  EMailSender::Response response;
			  response.code = F("400");
			  response.desc = "Error no filename specified for the file "+attachments.fileDescriptor[i].url;
			  response.status = false;
			  client.flush();
			  client.stop();

			  return response;
		  }

		  EMAIL_DEBUG_PRINTLN(attachments.fileDescriptor[i].filename);
		  EMAIL_DEBUG_PRINTLN(F("--frontier"));
		  client.println(F("--frontier"));
		  EMAIL_DEBUG_PRINTLN(F("Content-Type: "));
		  client.print(F("Content-Type: "));
		  EMAIL_DEBUG_PRINTLN(attachments.fileDescriptor[i].mime);
		  client.print(attachments.fileDescriptor[i].mime);
		  EMAIL_DEBUG_PRINTLN(F("; charset=\"UTF-8\""));
		  client.println(F("; charset=\"UTF-8\""));

		  if (attachments.fileDescriptor[i].encode64){
			  client.println(F("Content-Transfer-Encoding: base64"));
		  }

		  client.print(F("Content-Disposition: attachment; filename="));
		  client.print(attachments.fileDescriptor[i].filename);
		  client.println(F("\n"));
		  EMAIL_DEBUG_PRINT(F("Readed filename: "));
		  EMAIL_DEBUG_PRINTLN(attachments.fileDescriptor[i].filename);

//		  EMAIL_DEBUG_PRINT(F("Check if exist: "));
//		  EMAIL_DEBUG_PRINTLN(INTERNAL_STORAGE_CLASS.exists(attachments.fileDescriptor[i].url.c_str()));

		  int clientCount = 0;

#ifdef STORAGE_INTERNAL_ENABLED
			if (attachments.fileDescriptor[i].storageType==EMAIL_STORAGE_TYPE_SPIFFS ||
				attachments.fileDescriptor[i].storageType==EMAIL_STORAGE_TYPE_LITTLE_FS ||
				attachments.fileDescriptor[i].storageType==EMAIL_STORAGE_TYPE_SPIFM ||
				attachments.fileDescriptor[i].storageType==EMAIL_STORAGE_TYPE_FFAT){
	#ifdef OPEN_CLOSE_INTERNAL
				if (!INTERNAL_STORAGE_CLASS.exists(attachments.fileDescriptor[i].url.c_str())){
				    EMAIL_DEBUG_PRINTLN(F("Begin internal storage!"));

					#if (INTERNAL_STORAGE == STORAGE_SPIFM)
					Adafruit_FlashTransport_SPI flashTransport(SPIFM_CS_PIN, SPI); // Set CS and SPI interface
					Adafruit_SPIFlash flash(&flashTransport);

						  // Initialize flash library and check its chip ID.
					if (!flash.begin()) {
						EMailSender::Response response;
						  response.code = F("500");
						  response.desc = F("Error, failed to initialize flash chip!");
						  response.status = false;
						  client.flush();
						  client.stop();

						  return response;
					} // close flash.begin()

					if(!(INTERNAL_STORAGE_CLASS.begin(&flash))){
					#else
					if(!(INTERNAL_STORAGE_CLASS.begin())){
					#endif
						  EMailSender::Response response;
						  response.code = F("500");
						  response.desc = F("Error on startup filesystem!");
						  response.status = false;
						  client.flush();
						  client.stop();

						  return response;
					} // Close INTERNAL_STORAGE_CLASS.begin

					spiffsActive = true;
					EMAIL_DEBUG_PRINTLN("SPIFFS BEGIN, ACTIVE");
				} // Close INTERNAL_STORAGE_CLASS.exists

	#endif

				EMAIL_DEBUG_PRINT(F("Try to open "));
				EMAIL_DEBUG_PRINTLN(attachments.fileDescriptor[i].url);
				EMAIL_FILE myFile = INTERNAL_STORAGE_CLASS.open(attachments.fileDescriptor[i].url, EMAIL_FILE_READ);
				  if(myFile) {
					  EMAIL_DEBUG_PRINT(F("Filename -> "));
					  EMAIL_DEBUG_PRINTLN(myFile.name());
					  if (attachments.fileDescriptor[i].encode64){
						  encode(&myFile, &client);
					  }else{
						while(myFile.available()) {
						clientCount = myFile.read(tBuf,64);
						EMAIL_DEBUG_PRINTLN(clientCount);
						  client.write((byte*)tBuf,clientCount);
						}
					  }
					myFile.close();

					client.println();
				  } // Else myfile
				  else {
					  EMailSender::Response response;
					  response.code = F("404");
					  response.desc = "Error opening attachments file "+attachments.fileDescriptor[i].url;
					  response.status = false;
					  client.flush();
					  client.stop();

					  return response;
				  } // Close myfile
				} // Close storageType

#endif
#ifdef STORAGE_EXTERNAL_ENABLED
			if (attachments.fileDescriptor[i].storageType==EMAIL_STORAGE_TYPE_SD){
#ifdef OPEN_CLOSE_SD
				 EMAIL_DEBUG_PRINTLN(F("SD Check"));
				 if (!EXTERNAL_STORAGE_CLASS.exists(attachments.fileDescriptor[i].url.c_str())){
#if EXTERNAL_STORAGE == STORAGE_SD || EXTERNAL_STORAGE == STORAGE_SDFAT2 || EXTERNAL_STORAGE == STORAGE_SDFAT_RP2040_ESP8266
					if(!EXTERNAL_STORAGE_CLASS.begin(SD_CS_PIN)){
						  response.code = F("500");
						  response.desc = F("Error on startup SD filesystem!");
						  response.status = false;
						  client.flush();
						  client.stop();

						  return response;
					} // Close EXTERNAL_STORAGE_CLASS.begin
#elif EXTERNAL_STORAGE == STORAGE_SPIFM
					Adafruit_FlashTransport_SPI flashTransport(SS, SPI); // Set CS and SPI interface
					Adafruit_SPIFlash flash(&flashTransport);

				  if (!EXTERNAL_STORAGE_CLASS.begin(&flash)) {
					  response.code = F("500");
					  response.desc = F("Error on startup SDFAT2 filesystem!");
					  response.status = false;
					  client.flush();
					  client.stop();

					  return response;
				  }
#endif
					sdActive = true;
				 } // Close EXTERNAL_STORAGE_CLASS.exists
#endif

			    EMAIL_DEBUG_PRINTLN(F("Open file: "));
			EMAIL_FILE_EX myFile = EXTERNAL_STORAGE_CLASS.open(attachments.fileDescriptor[i].url.c_str());

			  if(myFile) {
				  myFile.seek(0);
				  EMAIL_DEBUG_PRINTLN(F("OK"));
				  if (attachments.fileDescriptor[i].encode64){
					  EMAIL_DEBUG_PRINTLN(F("BASE 64"));
					  encode(&myFile, &client);
				  }else{
					  EMAIL_DEBUG_PRINTLN(F("NORMAL"));
					while(myFile.available()) {
						clientCount = myFile.read(tBuf,64);
						client.write((byte*)tBuf,clientCount);
					}
				  }
				myFile.close();

				client.println();
			  } // Else myfile
			  else {
				  response.code = F("404");
				  response.desc = "Error opening attachments file "+attachments.fileDescriptor[i].url;
				  response.status = false;
				  client.flush();
				  client.stop();

				  return response;
			  } // Close myFile

			} // Close storageType==EMAIL_STORAGE_TYPE_SD
#else
	if (attachments.fileDescriptor[i].storageType==EMAIL_STORAGE_TYPE_SD){
		response.code = F("500");
		response.desc = F("EMAIL_STORAGE_TYPE_SD not enabled on EMailSenderKey.h");
		response.status = false;
		  client.flush();
		  client.stop();

		return response;
	}
#endif

	  } // Close attachment cycle
	  client.println();
	  client.println(F("--frontier--"));
#ifdef STORAGE_EXTERNAL_ENABLED
	  #ifdef OPEN_CLOSE_SD
		  if (sdActive){
			  EMAIL_DEBUG_PRINTLN(F("SD end"));
		#ifndef ARDUINO_ESP8266_RELEASE_2_4_2
			#if EXTERNAL_STORAGE != STORAGE_SDFAT_RP2040_ESP8266
			  EXTERNAL_STORAGE_CLASS.end();
			#endif
		#endif
			  EMAIL_DEBUG_PRINTLN(F("SD end 2"));
		  }
	#endif
#endif

#ifdef STORAGE_INTERNAL_ENABLED
	#ifdef OPEN_CLOSE_INTERNAL
		#if INTERNAL_STORAGE != STORAGE_SPIFM
		  if (spiffsActive){
			  INTERNAL_STORAGE_CLASS.end();
			  EMAIL_DEBUG_PRINTLN(F("SPIFFS END"));
		  }
	#endif
#endif


#endif
	  } // Close attachement enable

#endif
  EMAIL_DEBUG_PRINTLN(F("Message end"));
  client.println(F("."));

  response = awaitSMTPResponse(client, "250", "Sending message error");
  if (!response.status) {
	  client.flush();
	  client.stop();
	  return response;
  }

  client.println(F("QUIT"));

  response = awaitSMTPResponse(client, "221", "SMTP QUIT error");
  if (!response.status) {
	  client.flush();
	  client.stop();
	  return response;
  }

  response.status = true;
  response.code = F("0");
  response.desc = F("Message sent!");

  client.flush();
  client.stop();

  return response;
}

