/*
 * config_parse.c - this file is part of Portsplit.
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

#include "config_parse.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>

#define CONFIG_BUFSIZE 1024

int parse_config_file (char const *filename, config *cfg)
{
  FILE *stream;
  int fd;
  unsigned int line;
  char buf[CONFIG_BUFSIZE], *current_section;
  service *current_service, **tmplist;
  unsigned int i, j;

  init_config_defaults (cfg);

  fd = open (filename, O_RDONLY);
  if (fd == -1)
    {
      fprintf (stderr, "ERROR: unable to open configuration file `%s': %s.\n", filename, strerror(errno));
      return -1;
    }
  stream = (FILE*) fdopen (fd, "r");
  cfg->filename = malloc (strlen(filename) + 1);
  strcpy (cfg->filename, filename);

  current_section = NULL;
  fscanf (stream, "%[^\n\r]", buf);
  line = 1;
  while (!feof(stream))
    {
      if (buf[0] != '#')
	{
	  if (buf[0] != '[')
	    {
	      if (current_section == NULL)
		{
		  if (parse_config_line (buf, cfg) == -1)
		    {
		      fprintf (stderr, "%s:%d: error parsing this line\n", filename, line);
		      return -1;
		    }
		}
	      else
		{
		  if (parse_service_line (buf, current_service) == -1)
		    {
		      fprintf (stderr, "%s:%d: error parsing this line\n", filename, line);
		      return -1;
 		    }
		}
	    }
	  else if (buf[0] == '[' && buf[strlen(buf) - 1] == ']')
	    {
	      current_section = malloc (strlen(buf) - 1);
	      sscanf (buf, "[%[^]]]", current_section);

	      current_service = malloc (sizeof(service));
	      init_service_defaults (current_service);
	      current_service->name = current_section;

	      tmplist = malloc (sizeof(service*) * (cfg->nservices + 1));
	      memcpy (tmplist, cfg->services, sizeof(service*) * cfg->nservices);
	      tmplist[cfg->nservices] = current_service;

	      if (cfg->nservices)
		free (cfg->services);

	      cfg->services = tmplist;
	      cfg->nservices++;
	    }
	  else
	    {
	      printf ("%s:%d: error parsing this line\n", filename, line);
	      return -1;
	    }
	}

      fscanf (stream, "%*[\n\r]");
      fscanf (stream, "%[^\n\r]", buf);
      line++;
    }

  fclose (stream);
  close (fd);

  cfg->prebuflen = 0;
  for (i = 0; i < cfg->nservices; i++)
    {
      for (j = 0; cfg->services[i]->patterns[j] != NULL; j++)
	{
	  if (cfg->prebuflen < cfg->services[i]->patterns[j]->len)
	    cfg->prebuflen = cfg->services[i]->patterns[j]->len;
	}
    }


  return 0;
}

int parse_config_line (char const *line, config *cfg)
{
  char param[CONFIG_BUFSIZE], value[CONFIG_BUFSIZE], *eq, **addr, *newaddr;
  unsigned int i, *port, newport;

  eq = index (line, '=');

  if (eq != NULL)
    {
      *eq = '\0';
      sscanf (line, "%s", param);
      for (eq++; *eq == ' ' || *eq == '\t'; eq++);
      strcpy (value, eq);

      if (strcmp (param, "bind") == 0)
	{
	  if (split_host_port (value, &newaddr, &newport) == -1)
	    {
	      fprintf (stderr, "Invalid address:port binding specification: `%s'\n", value);
	      return -1;
	    }

	  addr = malloc (sizeof(char*) * (cfg->nbind + 1));
	  port = malloc (sizeof(unsigned int) * (cfg->nbind + 1));
	  for (i = 0; i < cfg->nbind; i++)
	    {
	      addr[i] = cfg->addr[i];
	      port[i] = cfg->port[i];
	    }
	  addr[cfg->nbind] = newaddr;
	  port[cfg->nbind] = newport;
	  if (cfg->nbind)
	    {
	      free (cfg->addr);
	      free (cfg->port);
	    }
	  cfg->addr = addr;
	  cfg->port = port;
	  cfg->nbind++;
	}
      else if (strcmp(param, "pid") == 0)
	{
	  cfg->pidfile = malloc (strlen(value) + 1);
	  strcpy (cfg->pidfile, value);
	}
      else if (strcmp(param, "maxconn") == 0)
	{
	  cfg->maxconn = atoi(value);
	}
      else if (strcmp(param, "timeout") == 0)
	{
	  cfg->timeout = atoi(value);
	}
      else
	{
	  fprintf (stderr, "Unrecognized parameter `%s'\n", param);
	}
    }
  else
    {
      fprintf (stderr, "Syntax error: invalid line `%s'\n", line);
      return -1;
    }

  return 0;
}

int parse_service_line (char const *line, service * s)
{
  char param[CONFIG_BUFSIZE], value[CONFIG_BUFSIZE], *eq, *newaddr, **arraytmp, *matchstr;
  unsigned int newport, count, stringlen;
  pattern *newpattern, **patterns;

  eq = index (line, '=');

  if (eq != NULL)
    {
      *eq = '\0';
      sscanf (line, "%s", param);
      for (eq++; *eq == ' ' || *eq == '\t'; eq++);
      strcpy (value, eq);

      if (strcmp (param, "string") == 0)
	{
	  if (strcmp(s->name, "fallback") && strcmp(s->name, "timeout"))
	    {
	      matchstr = interpret_string (value, &stringlen);
	      newpattern = malloc (sizeof(pattern));

	      newpattern->str = matchstr;
	      newpattern->len = stringlen;
	      newpattern->type = PATTERN_TYPE_STRING;

	      for (count = 0; s->patterns[count] != NULL; count++);

	      patterns = malloc (sizeof(pattern*) * (count + 2));
	      for (count = 0; s->patterns[count] != NULL; count++)
		patterns[count] = s->patterns[count];

	      patterns[count] = newpattern;
	      patterns[count+1] = NULL;

	      free (s->patterns);
	      s->patterns = patterns;
	    }
	  else
	    {
	      fprintf (stderr, "Parameter `%s' invalid for service `%s'\n", param, s->name);
	      return -1;
	    }
	}
      else if (strcmp (param, "connect") == 0)
	{
	  if (split_host_port (value, &newaddr, &newport) == -1)
	    {
	      fprintf (stderr, "Invalid address:port binding specification: `%s'\n", value);
	      return -1;
	    }

	  s->host = newaddr;
	  s->port = newport;
	}
      else if (strcmp (param, "exec") == 0)
	{
	  s->command = malloc (strlen(value) + 1);
	  strcpy (s->command, value);

	  free (s->args);

	  s->args = malloc (sizeof(char*) * 2);
	  s->args[0] = malloc (strlen(value) + 1);
	  strcpy (s->args[0], value);
	  s->args[1] = NULL;
	}
      else if (strcmp (param, "execarg") == 0)
	{
	  if (s->command == NULL)
	    {
	      fprintf (stderr, "Parameter `execarg' must be placed after `exec'\n");
	      return -1;
	    }

	  for (count = 0; s->args[count] != NULL; count++);

	  arraytmp = malloc (sizeof(char*) * (count + 2));

	  for (count = 0; s->args[count] != NULL; count++)
	    arraytmp[count] = s->args[count];

	  arraytmp[count] = malloc (strlen(value) + 1);
	  strcpy (arraytmp[count], value);
	  arraytmp[count+1] = NULL;

	  free (s->args);

	  s->args = arraytmp;
	}
      else if (strcmp (param, "pty") == 0)
	{
	  if (strcmp (value, "yes") == 0)
	    s->pty = 1;
	  else if (strcmp (value, "no") == 0)
	    s->pty = 0;
	  else
	    {
	      fprintf (stderr, "Invalid value `%s' for parameter `pty'\n", value);
	      return -1;
	    }
	}
      else
	{
	  fprintf (stderr, "Unknown parameter `%s'\n", param);
	  return -1;
	}
    }
  else
    {
      fprintf (stderr, "Syntax error: invalid line `%s'\n", line);
      return -1;
    }

  return 0;
}

int split_host_port (char const *str, char **host, unsigned int *port)
{
  char *colon;

  colon = rindex (str, ':');

  if (colon == NULL)
    return -1;

  *colon = '\0';

  *host = malloc (strlen (str) + 1);
  strcpy (*host, str);
  *port = atoi (colon + 1);

  return 0;
}

char *interpret_string (char const *str, unsigned int *rlen)
{
  unsigned int len, i, hexval;
  int scanres, consumed;
  char *result, *tmp, hex[3], special;

  len = strlen(str);
  tmp = malloc(len);
  *rlen = 0;
  hex[2] = 0;

  for (i = 0; i < len; i++)
    {
      if (str[i] != '\\' || i > len - 3)
	{
	  tmp[*rlen] = str[i];
	  (*rlen)++;
	}
      else
	{
	  hex[0] = str[i+1];
	  hex[1] = str[i+2];
	  consumed = 0;
	  scanres = sscanf (hex, "%x%n", &hexval, &consumed); /* consumed must be equal to 2, otherwise fuck you */
	  if (scanres == 1 && consumed == 2)
	    {
	      special = (char) hexval;
	      tmp[*rlen] = special;
	      (*rlen)++;
	      i = i + 2;
	    }
	  else
	    {
	      tmp[*rlen] = str[i];
	      (*rlen)++;
	    }
	}
    }

  result = malloc (*rlen);
  memcpy (result, tmp, *rlen);

  free (tmp);

  return result;
}

void init_service_defaults (service *s)
{
  s->name = NULL;

  s->patterns = malloc (sizeof (pattern*));
  s->patterns[0] = NULL;

  s->command = NULL;
  s->pty = 0;

  s->args = malloc (sizeof(char*));
  s->args[0] = NULL;

  s->host = NULL;
  s->port = 0;
}

void init_config_defaults (config *cfg)
{
  cfg->filename = NULL;
  cfg->pidfile = NULL;
  cfg->nservices = 0;
  cfg->nbind = 0;
  cfg->maxconn = 10;
  cfg->timeout = 0;
  cfg->readbuflen = 512;
}

void free_config (config *cfg)
{
  unsigned int i;

  free (cfg->filename);
  free (cfg->pidfile);

  for (i = 0; i < cfg->nbind; i++)
    free (cfg->addr[i]);

  free (cfg->addr);
  free (cfg->port);

  for (i = 0; i < cfg->nservices; i++)
    free_service (cfg->services[i]);

  free (cfg->services);
}

void free_service (service *s)
{
  unsigned int i;

  free (s->name);

  for (i = 0; s->patterns[i] != NULL; i++)
    {
      free (s->patterns[i]->str);
      free (s->patterns[i]);
    }
  free (s->patterns);

  free (s->command);

  for (i = 0; s->args[i] != NULL; i++)
    free (s->args[i]);
  free (s->args);

  free (s->host);

  free (s);
}

service const * service_by_name (config const *cfg, char const *search) /* nb: search has to be null-terminated */
{
  service const *ret;
  unsigned int i;

  ret = NULL;

  for (i = 0; i < cfg->nservices && ret == NULL; i++)
    if (strcmp(cfg->services[i]->name, search) == 0)
      ret = cfg->services[i];

  return ret;
}

service const * match_service (config const *cfg, char const *match, unsigned int rcvdlen, unsigned int *pattern_index)
{
  service const *serv, *ret;
  pattern const *pat;
  unsigned int i, j, matchidx = 0;

  ret = NULL;

  for (i = 0; i < cfg->nservices && ret == NULL; i++)
    {
      serv = cfg->services[i];
      for (j = 0; serv->patterns[j] != NULL && ret == NULL; j++)
	{
	  pat = serv->patterns[j];
	  if (rcvdlen >= pat->len)
	    {
	      if (memcmp (match, pat->str, pat->len) == 0)
		{
		  ret = serv;
		  matchidx = j;
		}
	    }
	}
    }

  if (pattern_index != NULL)
    *pattern_index = matchidx;

  return ret;
}

int could_match (service const *s, char const *str, unsigned int len, unsigned int *pattern_index)
{
  unsigned int i, idx = 0, compare_len;
  int couldmatch = 0;

  for (i = 0; s->patterns[i] != NULL && couldmatch == 0; i++)
    {
      compare_len = ((len < s->patterns[i]->len) ? len : s->patterns[i]->len);
      if (memcmp (str, s->patterns[i]->str, compare_len) == 0)
	{
	  couldmatch = 1;
	  idx = i;
	}
    }

  if (pattern_index != NULL)
    *pattern_index = idx;

  return couldmatch;
}

int could_match_any (config const *cfg, char const *str, unsigned int len, service const **s, unsigned int *pattern_index)
{
  unsigned int i;
  int found = 0;
  service const *cur = NULL;

  for (i = 0; i < cfg->nservices && !found; i++)
    {
      cur = cfg->services[i];
      found = could_match (cur, str, len, pattern_index);
    }

  if (s != NULL)
    *s = cur;

  return found;
}

void copy_service (service *dst, service const *src)
{
  unsigned int i;
  /*
   * Assume dst is previously allocated
   */
  dst->name = malloc (strlen(src->name) + 1);
  strcpy (dst->name, src->name);

  dst->pty = src->pty;

  if (src->command != NULL)
    {
      dst->command = malloc (strlen(src->command) + 1);
      strcpy (dst->command, src->command);
    }
  else
    dst->command = NULL;

  if (src->host != NULL)
    {
      dst->host = malloc (strlen (src->host) + 1);
      strcpy (dst->host, src->host);
    }
  else
    dst->host = NULL;

  dst->port = src->port;

  for (i = 0; src->args[i] != NULL; i++);

  dst->args = malloc ((i+1) * sizeof(char*));
  dst->args[i] = NULL;

  for (i = 0; src->args[i] != NULL; i++)
    {
      dst->args[i] = malloc (strlen(src->args[i]) + 1);
      strcpy (dst->args[i], src->args[i]);
    }

  for (i = 0; src->patterns[i] != NULL; i++);

  dst->patterns = malloc ((i+1) * sizeof(pattern*));
  dst->patterns[i] = NULL;

  for (i = 0; src->patterns[i] != NULL; i++)
    {
      dst->patterns[i] = malloc (sizeof(pattern));
      dst->patterns[i]->len = src->patterns[i]->len;
      dst->patterns[i]->type = src->patterns[i]->type;
      dst->patterns[i]->str = malloc (dst->patterns[i]->len);
      memcpy (dst->patterns[i]->str, src->patterns[i]->str, dst->patterns[i]->len);
    }
}

void copy_config (config *dst, config const *src)
{
  unsigned int i;

  dst->nbind = src->nbind;
  dst->maxconn = src->maxconn;
  dst->timeout = src->timeout;
  dst->nservices = src->nservices;
  dst->prebuflen = src->prebuflen;
  dst->readbuflen = src->readbuflen;

  dst->filename = malloc (strlen(src->filename) + 1);
  strcpy (dst->filename, src->filename);

  if (src->pidfile != NULL)
    {
      dst->pidfile = malloc (strlen(src->pidfile) + 1);
      strcpy (dst->pidfile, src->pidfile);
    }
  else
    dst->pidfile = NULL;

  dst->port = malloc (dst->nbind * sizeof(unsigned int));
  dst->addr = malloc (dst->nbind * sizeof(char*));
  for (i = 0; i < dst->nbind; i++)
    {
      dst->port[i] = src->port[i];
      dst->addr[i] = malloc (strlen(src->addr[i]) + 1);
      strcpy (dst->addr[i], src->addr[i]);
    }

  dst->services = malloc (dst->nservices * sizeof(service*));
  for (i = 0; i < dst->nservices; i++)
    {
      dst->services[i] = malloc (sizeof(service));
      copy_service (dst->services[i], src->services[i]);
    }
}
