/*
 * ps2.h - this file is part of Portsplit.
 *
 * Portsplit is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Portsplit is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Portsplit.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef PS2_H
#define PS2_H

#include "config_parse.h"

#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <arpa/inet.h>


int main_listen_loop (config);

int setup_socket (char const*, int, int*, int*, struct sockaddr_in*, struct sockaddr_in6*);


int sigfd[2];

#endif
