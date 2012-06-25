/*
 * child.h - this file is part of Portsplit.
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

#ifndef CHILD_H
#define CHILD_H

#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include "config_parse.h"

int treat_client (int, struct sockaddr const*, int, config const*);
int connect_one (struct addrinfo const*);
int connect_host (char const *, int);
int start_proxying (service const *, int *, int *);
int forward_data (int, int, char *, unsigned int, ssize_t*, ssize_t*);

#endif
