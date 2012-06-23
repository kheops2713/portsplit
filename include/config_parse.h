/*
 * config_parse.h - this file is part of Portsplit.
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

#ifndef CONFIG_PARSE_H
#define CONFIG_PARSE_H

typedef struct pattern_t
{
  char *str;
  unsigned int len;
  int type;
} pattern;

typedef struct service_t
{
  char *name;
  pattern **patterns; /* will be null-terminated */
  int pty;
  char *command; /* null-terminated command */
  char **args; /* null-terminated list of argument */
  char *host; /* null-terminated hostname */
  unsigned int port;
} service;

typedef struct config_t
{
  char *filename;
  char *pidfile;
  char **addr;
  unsigned int *port;
  unsigned int nbind;
  unsigned int maxconn;
  unsigned int timeout;
  service **services;
  unsigned int nservices;
  unsigned int prebuflen;
  unsigned int readbuflen;
} config;

enum { PATTERN_TYPE_STRING, PATTERN_TYPE_MAX };

int parse_config_file (char const*, config*);
int parse_service_line (char const*, service*);
int parse_config_line (char const*, config*);
int split_host_port (char const *, char **, unsigned int*);
void init_service_defaults (service*);
void init_config_defaults (config*);
char *interpret_string (char const *, unsigned int *);
void free_config (config*);
void free_service (service*);
void copy_config (config *, config const *);
void copy_service (service *, service const *);
service const * service_by_name (config const*, char const*);
service const * match_service (config const *, char const*, unsigned int, unsigned int*);
int could_match (service const*, char const *, unsigned int, unsigned int*);

#endif
