/* OpenSprinkler Unified (AVR/RPI/BBB/LINUX) Firmware
 * Copyright (C) 2015 by Ray Wang (ray@opensprinkler.com)
 *
 * Linux Ethernet functions
 * This file is based on Richard Zimmerman's sprinklers_pi program
 * Copyright (c) 2013 Richard Zimmerman
 *
 * This file is part of the OpenSprinkler library
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see
 * <http://www.gnu.org/licenses/>.
 */

#if defined(ARDUINO)

#else

#include "etherport.h"
#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <netinet/in.h>
#include <string.h>
#include <errno.h>
#include "defines.h"

#include<openssl/bio.h>
#include<openssl/ssl.h>
#include<openssl/err.h>
#include<openssl/pem.h>
#include<openssl/x509.h>
#include<openssl/x509_vfy.h>
#include "utils.h"

EthernetServer::EthernetServer(uint16_t port)
		: m_port(port), m_sock(0)
{
}

EthernetServer::~EthernetServer()
{
	close(m_sock);
}

bool EthernetServer::begin()
{
	struct sockaddr_in6 sin = {0};
	sin.sin6_family = AF_INET6;
	sin.sin6_port = htons(m_port);
	sin.sin6_addr = in6addr_any;

	if ((m_sock = socket(PF_INET6, SOCK_STREAM, 0)) < 0)
	{
		DEBUG_PRINTLN("can't create shell listen socket");
		return false;
	}
	int on = 1;
	if (setsockopt(m_sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0)
	{
		DEBUG_PRINTLN("can't setsockopt SO_REUSEADDR");
		return false;
	}
	int off = 0;
	if (setsockopt(m_sock, IPPROTO_IPV6, IPV6_V6ONLY, &off, sizeof(off)) < 0)
	{
		DEBUG_PRINTLN("can't setsockopt IPV6_V6ONLY");
		return false;
	}
	if (bind(m_sock, (struct sockaddr *) &sin, sizeof(sin)) < 0)
	{
		DEBUG_PRINTLN("shell bind error");
		return false;
	}
	if (ioctl(m_sock, FIONBIO, (char*) &on) < 0)
	{
		DEBUG_PRINTLN("setting nonblock failed");
		return false;
	}
	if (listen(m_sock, 2) < 0)
	{
		DEBUG_PRINTLN("shell listen error");
		return false;
	}
	return true;
}

//	This function blocks until we get a client connected.
//	 It will timeout after 50ms and return a blank client.
//	 If it succeeds it will return an EthernetClient.
EthernetClient EthernetServer::available()
{
	struct pollfd fds;
	memset(&fds, 0, sizeof(fds));
	fds.fd = m_sock;
	fds.events = POLLIN;
	int timeout = 50;

	int rc = poll(&fds, 1, timeout);
	if (rc > 0)
	{
		int client_sock = 0;
		if ((client_sock = accept(m_sock, NULL, NULL)) <= 0)
			return EthernetClient();
		return EthernetClient(client_sock);
	}
	return EthernetClient();
}

EthernetClient::EthernetClient()
		: m_sock(0), m_connected(false)
{
}

EthernetClient::EthernetClient(int sock)
		: m_sock(sock), m_connected(true)
{
}

EthernetClient::~EthernetClient()
{
	if (tmpbuf) free(tmpbuf);
}

int EthernetClient::connect(uint8_t ip[4], uint16_t port)
{
	if (m_sock)
		return 0;
	struct sockaddr_in sin = {0};
	sin.sin_family = AF_INET;
	sin.sin_port = htons(port);
	sin.sin_addr.s_addr = *(uint32_t*) (ip);
	m_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (::connect(m_sock, (struct sockaddr *) &sin, sizeof(sin)) < 0)
	{
		DEBUG_PRINTLN("error connecting to server");
		return 0;
	}
	struct timeval timeout;
	timeout.tv_sec = 10;
	timeout.tv_usec = 0;
	setsockopt (m_sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
	setsockopt (m_sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

	m_connected = true;
	return 1;
}

bool EthernetClient::connected()
{
	if (!m_sock)
		return false;
	int error = 0;
	socklen_t len = sizeof(error);
	int retval = getsockopt(m_sock, SOL_SOCKET, SO_ERROR, &error, &len);
	return (retval == 0) && m_connected;
}

void EthernetClient::stop()
{
	if (m_sock)
	{
		close(m_sock);
		m_sock = 0;
		m_connected = false;
	}
}

EthernetClient::operator bool()
{
	return m_sock != 0;
}

// read data from the client into the buffer provided
//	This function will block until either data is received OR a timeout happens.
//	If an error occurs or a timeout happens, we set the disconnect flag on the socket
//	and return 0;
int EthernetClient::read(uint8_t *buf, size_t size)
{
	if (tmpbufidx < tmpbufsize) {
		size_t tmpsize = tmpbufsize-tmpbufidx;
		if (tmpsize > size)
			tmpsize = size;
		memcpy(buf, &tmpbuf[tmpbufidx], tmpsize);
		tmpbufidx += tmpsize;
		return tmpsize;
	}

	struct pollfd fds;
	memset(&fds, 0, sizeof(fds));
	fds.fd = m_sock;
	fds.events = POLLIN;
	int timeout = 3000;

	int rc = poll(&fds, 1, timeout);
	if (rc > 0)
	{
		rc = recv(m_sock, buf, size, 0);
		if (errno == EWOULDBLOCK)
			return 0;

		if (rc <= 0) // socket closed
			m_connected = false;
		return rc;
	}
	if (rc < 0)
		m_connected = false;
	return 0;
}

int EthernetClient::timedRead() {
	if (!tmpbuf) tmpbuf = (uint8_t*)malloc(TMPBUF);
	if (tmpbufidx < tmpbufsize)
		return tmpbuf[tmpbufidx++];
		
	tmpbufidx = 0;
	tmpbufsize = read(tmpbuf, TMPBUF);
	
	if (tmpbufsize <= 0)
		return -1;
		
	return tmpbuf[tmpbufidx++];
}

size_t EthernetClient::readBytesUntil(char terminator, char *buffer, size_t length) {
	size_t n = 0;
	int c = timedRead();
  	while (c >= 0 && c != terminator && length>0)
  	{
		buffer[n] = (char)c;
		length--;
		n++;
		c = timedRead();
	}
	return n;
}

std::string EthernetClient::readStringUntil(char terminator) {

  String ret;
  int c = timedRead();
  while (c >= 0 && c != terminator)
  {
    ret += (char)c;
    c = timedRead();
  }
  return ret;
}

void EthernetClient::flush() {
	//for compatibility only
}

bool EthernetClient::available() {
	if (tmpbufidx < tmpbufsize)
		return true;

	if (!m_connected)
		return false;

	struct pollfd fds;
	memset(&fds, 0, sizeof(fds));
	fds.fd = m_sock;
	fds.events = POLLIN;
	int timeout = 50;

	int rc = poll(&fds, 1, timeout);
	return rc > 0;
}

size_t EthernetClient::write(const uint8_t *buf, size_t size)
{
	return ::send(m_sock, buf, size, MSG_NOSIGNAL);
}

/**
 * SSL Client
*/

EthernetClientSsl::EthernetClientSsl()
		: m_sock(0), m_connected(false)
{
}

EthernetClientSsl::EthernetClientSsl(int sock)
		: m_sock(sock), m_connected(true)
{
}

EthernetClientSsl::~EthernetClientSsl()
{
	stop();
}

static bool sslInit;
//static BIO* certbio;
static SSL_CTX* ctx;

/**
 * https://github.com/angstyloop/c-web/blob/main/openssl-fetch-example.c
*/
int EthernetClientSsl::connect(uint8_t ip[4], uint16_t port)
{
	if (m_sock)
		return 0;

	if (!sslInit) {
	    OpenSSL_add_all_algorithms();
    	//ERR_load_BIO_strings();
    	ERR_load_crypto_strings();
    	SSL_load_error_strings();	
		//BIO* certbio = BIO_new(BIO_s_file());
		if (SSL_library_init() < 0) 
		{
	        DEBUG_PRINTLN("Could not initialize the OpenSSL library.\n");
			return 0;
		}
		const SSL_METHOD* method = SSLv23_client_method();
	    ctx = SSL_CTX_new(method);
	    if (!ctx) {
        	DEBUG_PRINTLN("Unable to create SSL context.\n");
			return 0;
		}
		SSL_CTX_set_options(ctx, SSL_OP_NO_SSLv2|SSL_OP_NO_SSLv3);
		SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, NULL); //Accept all certs

		sslInit = true;
	}


    // Create a new SSL session. This does not connect the socket.
    ssl = SSL_new(ctx);

	struct sockaddr_in sin = {0};
	sin.sin_family = AF_INET;
	sin.sin_port = htons(port);
	sin.sin_addr.s_addr = *(uint32_t*) (ip);
	m_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (::connect(m_sock, (struct sockaddr *) &sin, sizeof(sin)) < 0)
	{
		DEBUG_PRINTLN("error connecting to server");
		return 0;
	}
	SSL_set_fd(ssl, m_sock);
	if (SSL_connect(ssl) < 1) {
		close(m_sock);
		m_sock = 0;
       	DEBUG_PRINTLN("Error: Could not build an SSL session");
		return 0;
	}
	m_connected = true;
	return 1;
}

bool EthernetClientSsl::connected()
{
	if (!m_sock || !ssl)
		return false;
	int error = 0;
	socklen_t len = sizeof(error);
	int retval = getsockopt(m_sock, SOL_SOCKET, SO_ERROR, &error, &len);
	return (retval == 0) && m_connected;
}

void EthernetClientSsl::stop()
{
	if (m_sock) {
		close(m_sock);
		m_sock = 0;
	}
	if (ssl) {
		SSL_free(ssl);
		ssl = NULL;
	}
	m_connected = false;
}

EthernetClientSsl::operator bool()
{
	return m_sock != 0 && ssl != NULL;
}

// read data from the client into the buffer provided
//	This function will block until either data is received OR a timeout happens.
//	If an error occurs or a timeout happens, we set the disconnect flag on the socket
//	and return 0;
int EthernetClientSsl::read(uint8_t *buf, size_t size)
{
	fd_set sock_set;
	FD_ZERO(&sock_set);
	FD_SET(m_sock, &sock_set);
	struct timeval timeout;
	timeout.tv_sec = 3;
	timeout.tv_usec = 0;

	//select(m_sock + 1, &sock_set, NULL, NULL, &timeout);
	//if (FD_ISSET(m_sock, &sock_set))
	size_t pending = SSL_pending(ssl);
	if (pending > 0)
	{
		if (size > pending)
			size = pending;
		int retval = SSL_read(ssl, buf, size);
		if (retval <= 0) // socket closed
			m_connected = false;
		return retval;
	}
	m_connected = false;
	return 0;
}

size_t EthernetClientSsl::write(const uint8_t *buf, size_t size)
{
	return SSL_write(ssl, buf, size);
	//return ::send(m_sock, buf, size, MSG_NOSIGNAL);
}
#endif
