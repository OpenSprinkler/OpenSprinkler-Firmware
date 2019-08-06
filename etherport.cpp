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
#include <netinet/in.h>
#include <string.h>
#include <errno.h>
#include "defines.h"

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
	fd_set sock_set;
	FD_ZERO(&sock_set);
	FD_SET(m_sock, &sock_set);
	struct timeval timeout;
	timeout.tv_sec = 0;
	timeout.tv_usec = 50 * 1000; // 50ms

	select(m_sock + 1, &sock_set, NULL, NULL, &timeout);
	if (FD_ISSET(m_sock, &sock_set))
	{
		int client_sock = 0;
		struct sockaddr_in6 cli_addr;
		unsigned int clilen = sizeof(cli_addr);
		if ((client_sock = accept(m_sock, (struct sockaddr *) &cli_addr, &clilen)) <= 0)
			return EthernetClient(0);
		return EthernetClient(client_sock);
	}
	return EthernetClient(0);
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
	stop();
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
	fd_set sock_set;
	FD_ZERO(&sock_set);
	FD_SET(m_sock, &sock_set);
	struct timeval timeout;
	timeout.tv_sec = 3;
	timeout.tv_usec = 0;

	select(m_sock + 1, &sock_set, NULL, NULL, &timeout);
	if (FD_ISSET(m_sock, &sock_set))
	{
		int retval = ::read(m_sock, buf, size);
		if (retval <= 0) // socket closed
			m_connected = false;
		return retval;
	}
	m_connected = false;
	return 0;
}

size_t EthernetClient::write(const uint8_t *buf, size_t size)
{
	return ::send(m_sock, buf, size, MSG_NOSIGNAL);
}

#endif
