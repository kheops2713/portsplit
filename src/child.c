/*
 * child.c - this file is part of Portsplit.
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

#include "child.h"
#include "log.h"
#include "sigs.h"

#include <unistd.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <sys/select.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

int treat_client (int clientfd, struct sockaddr const *inbound, int family, config const *_cfg)
{
  config *cfg;
  char datetime[64], ip[128];
  unsigned int ipv, readbuflen, i, j, prefill, nbytes, matched_pattern;
  int maxfd;
  char *prebuffer, *readbuffer, *toflush;
  struct timeval stimeout, *ptimeout;
  pid_t server_process = -1;
  int serverread, serverwrite, selret, r, w, terminate, overflow, established, fwd, couldmatch;
  fd_set fdset;
  service const * serv;
  service const * servfallback;
  
  set_child_ignored_signals();

  cfg = malloc (sizeof(config));
  copy_config (cfg, _cfg);

  stimeout.tv_sec = cfg->timeout;
  stimeout.tv_usec = 0;

  if (cfg->timeout)
    ptimeout = &stimeout;
  else
    ptimeout = NULL;

  readbuflen = cfg->prebuflen / 2;
  if (readbuflen == 0)
    readbuflen = 1;
  prebuffer = malloc (cfg->prebuflen);
  readbuffer = malloc (readbuflen);
  
  if (family == AF_INET)
    {
      inet_ntop (family, &(((struct sockaddr_in*)inbound)->sin_addr), ip, 128);
      ipv = 4;
    }
  else if (family == AF_INET6)
    {
      inet_ntop (family, &(((struct sockaddr_in6*)inbound)->sin6_addr), ip, 128);
      ipv = 6;
    }

  now_r (DT_FMT, datetime);
  printf ("%s %d: New connection from IPv%d %s\n", datetime, getpid(), ipv, ip);

  FD_ZERO (&fdset);
  FD_SET (clientfd, &fdset);
  maxfd = clientfd;

  serverread = serverwrite = -1;
  prefill = 0;
  terminate = 0;
  established = 0;
  overflow = 0;
  while (!terminate && (selret = select (maxfd + 1, &fdset, NULL, NULL, ptimeout)) >= 0)
    {
      if (selret == 0) /* timeout */
	{
	  serv = service_by_name (cfg, "timeout");
	  if (serv)
	    {
	      /* NB: There can be a buffer to be flushed (only the prebuffer) */
	      if (start_proxying (serv, &serverread, &serverwrite) == -1)
		terminate = 1;
	      else 
		{
		  established = 1;
		  if (prefill)
		    {
		      w = write (serverwrite, prebuffer, prefill);
		      if (w < prefill)
			{
			  now_r (DT_FMT, datetime);
			  printf ("%s %d: Write error to server while flushing\n", datetime, getpid());
			  terminate = 1;
			  established = 0;
			}
		    }
		  free (readbuffer);
		  readbuffer = malloc (cfg->readbuflen);
		  readbuflen = cfg->readbuflen;
		}
	    }
	  else
	    {
	      now_r (DT_FMT, datetime);
	      printf ("%s %d: No timeout service defined, giving up.\n", datetime, getpid());
	      terminate = 1;
	    }
	}
      else
	{
	  if (FD_ISSET (clientfd, &fdset)) /* the client said something */
	    {
	      if (established)
		{
		  if ((fwd = forward_data (clientfd, serverwrite, readbuffer, readbuflen, &r, &w)) <= 0)
		    terminate = 1;
		  now_r (DT_FMT, datetime);
		  if (fwd == 0)
		    printf ("%s %d: EOF from %s\n", datetime, getpid(), (r == 0 ? "client" : "server"));
		  else if (fwd < 0)
		    printf ("%s %d: %s error from %s\n", datetime, getpid(), (r < 0 ? "Read" : "Write"), (r < 0 ? "client" : "server"));
		}
	      else
		{
		  r = read (clientfd, readbuffer, readbuflen);
	      
		  if (r > 0) /* let's say it's successful */
		    {
		      if (r <= cfg->prebuflen - prefill)
			{
			  nbytes = r;
			  overflow = 0;
			}
		      else
			{
			  nbytes = cfg->prebuflen - prefill;
			  overflow = 1;
			}
		      
		      memcpy (prebuffer + prefill, readbuffer, nbytes);
		      prefill += nbytes;

		      serv = match_service (cfg, prebuffer, prefill, &matched_pattern);
		      
		      if ((serv == NULL && prefill < cfg->prebuflen && (couldmatch = could_match_any (cfg, prebuffer, prefill, NULL, NULL)) == 0)
			  || (serv == NULL && prefill >= cfg->prebuflen))
			serv = service_by_name (cfg, "fallback");

		      if (serv != NULL)
			{
			  now_r (DT_FMT, datetime);
			  if (strcmp (serv->name, "fallback"))
			    printf ("%s %d: Engaging service `%s' (matched pattern %d).\n", datetime, getpid(), serv->name, matched_pattern);
			  else
			    printf ("%s %d: Engaging service `%s' (no matched pattern).\n", datetime, getpid(), serv->name);

			  if (start_proxying (serv, &serverread, &serverwrite) == -1)
			    terminate = 1;
			  else
			    {
			      /* Flush pre-buffer + possible overflow */
			      /* set established to 1 */
			      toflush = malloc (prefill + r - nbytes);

			      memcpy (toflush, prebuffer, prefill);
			      if (overflow)
				memcpy (toflush + prefill, readbuffer + nbytes, r - nbytes);

			      w = write (serverwrite, toflush, prefill + r - nbytes);
			      
			      if (w < prefill + r - nbytes)
				{
				  now_r (DT_FMT, datetime);
				  printf ("%s %d: Error when flushing buffered data to server\n", datetime, getpid());
				  terminate = 1;
				}
			      else
				{
				  established = 1;
				  free (readbuffer);
				  readbuffer = malloc (cfg->readbuflen);
				  readbuflen = cfg->readbuflen;
				}
			      
			      free (toflush);
			    }
			}
		      else if (prefill >= cfg->prebuflen || couldmatch == 0) /* also serv is NULL so there is no fallback */
			{
			  now_r (DT_FMT, datetime);
			  printf ("%s %d: Don't know what to do, throwing out client.\n", datetime, getpid());
			  terminate = 1;
			}
		    }
		  else if (r == 0)
		    {
		      now_r (DT_FMT, datetime);
		      printf ("%s %d: End of file from client.\n", datetime, getpid());
		      terminate = 1;
		    }
		  else if (r < 0)
		    {
		      now_r (DT_FMT, datetime);
		      printf ("%s %d: Read error from client: %s\n", datetime, getpid(), strerror(errno));
		      terminate = 1;
		    }
		}
	    }

	  if (serverread > 0 && FD_ISSET (serverread, &fdset) && !terminate)
	    {
	      if ((fwd = forward_data (serverread, clientfd, readbuffer, readbuflen, &r, &w)) <= 0)
		terminate = 1;

	      now_r (DT_FMT, datetime);
	      if (fwd == 0)
		printf ("%s %d: EOF from %s\n", datetime, getpid(), (r == 0 ? "server" : "client"));
	      else if (fwd < 0)
		printf ("%s %d: %s error from %s\n", datetime, getpid(), (r < 0 ? "Read" : "Write"), (r < 0 ? "server" : "client"));
	    }
	}

      if (!terminate)
	{
	  FD_ZERO (&fdset);
	  FD_SET (clientfd, &fdset);
	  if (serverread > 0)
	    FD_SET (serverread, &fdset);
	  
	  maxfd = ((clientfd > serverread) ? clientfd : serverread);

	  if (!established && ptimeout != NULL)
	    {
	      ptimeout->tv_usec = 0;
	      ptimeout->tv_sec = cfg->timeout;
	    }
	  else if (established)
	    {
	      ptimeout = NULL;
	    }
	}
    }

  now_r (DT_FMT, datetime);
  printf ("%s %d: Terminating connection with %s.\n", datetime, getpid(), ip);

  close (clientfd);
  if (established)
    {
      close (serverwrite);
      close (serverread);
    }

  free (prebuffer);
  free (readbuffer);
  free_config (cfg);
  free (cfg);
  return 0;
}

int connect_one (struct addrinfo const *ai)
{
  int sockfd = -1, success = 0;
  struct addrinfo const *p;
  char ip[128], datetime[64];
 
  for (p = ai; p != NULL && success == 0; p = p->ai_next)
    {
      if (p->ai_family == AF_INET || p->ai_family == AF_INET6)
	{
	  now_r (DT_FMT, datetime);
	  if (p->ai_family == AF_INET)
	    inet_ntop (p->ai_family, &(((struct sockaddr_in*)p->ai_addr)->sin_addr), ip, 128);
	  else if (p->ai_family == AF_INET6)
	    inet_ntop (p->ai_family, &(((struct sockaddr_in6*)p->ai_addr)->sin6_addr), ip, 128);
	  
	  printf ("%s %d: Trying %s...\n", datetime, getpid(), ip);
      
	  if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
	    perror("socket()");
	  }
	  else
	    {
	      if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1)
		{
		  perror("connect()");
		  close (sockfd);
		}
	      else
		{
		  now_r (DT_FMT, datetime);
		  success = 1;
		  printf ("%s %d: Connected.\n", datetime, getpid());
		}
	    }
	}
    }
  
  return ((success == 1) ? sockfd : -1);
}

int connect_host (char const *hostname, int port)
{
  int serverfd = -1;
  char portstr[10], datetime[64];

  printf ("%s %d: Looking up %s...\n", now(DT_FMT), getpid(), hostname);
  
  struct addrinfo *result;
  struct addrinfo hint;
  int retdns;

  memset (portstr, 0, 10 * sizeof(char));
  memset (&hint, 0, sizeof(struct addrinfo));
  hint.ai_family = AF_UNSPEC;
  hint.ai_socktype = SOCK_STREAM;
  sprintf (portstr, "%d", port);

  if ((retdns = getaddrinfo(hostname, portstr, &hint, &result)) != 0)
    {
      printf ("%s %d: DNS resolution failure: %s\n", now(DT_FMT), getpid(), gai_strerror(retdns));
      return -1;
    }
  
  if ((serverfd = connect_one (result)) == -1)
    {
      freeaddrinfo (result);
      return -1;
    }
  
  freeaddrinfo (result);
  
  return serverfd;
}

int start_proxying (service const *s, int *serverread, int *serverwrite)
{
  int serverfd, server_process = 0;
  int pty[2], pipefd[4];
  char datetime[64], tty[128];

  if (s->host != NULL) /* proxy to another server */
    {
      now_r (DT_FMT, datetime);
      printf ("%s %d: Service `%s': connecting to %s:%d\n", datetime, getpid(), s->name, s->host, s->port);
      if ((serverfd = connect_host (s->host, s->port)) == -1)
	return -1;
      else
	*serverread = *serverwrite = serverfd;
    }
  else if (s->command != NULL)
    {
      now_r (DT_FMT, datetime);
      printf ("%s %d: Service `%s': piping\n", datetime, getpid(), s->name);

      if (s->pty)
	{
	  if (openpty (pty, pty+1, tty, NULL, NULL) == -1)
	    {
	      printf ("%s %d: Could not open PTY: %s\n", datetime, getpid(), strerror(errno));
	      return -1;
	    }
	  printf ("%s %d: Opened PTY `%s' for server subprocess communication\n", datetime, getpid(), tty);
	  *serverread = *serverwrite = pty[0];
	}
      else
	{
	  pipe (pipefd);
	  pipe (pipefd + 2);

	  *serverread = pipefd[0];
	  *serverwrite = pipefd[3];

	  printf ("%s %d: Pipe open for server subprocess communication\n", datetime, getpid());
	}

      server_process = fork();
      if (server_process == 0)
	{
	  if (s->pty)
	    {
	      close (pty[0]);
	      dup2 (pty[1], 1);
	      dup2 (pty[1], 0);
	      close (pty[1]);
	    }
	  else
	    {
	      dup2 (pipefd[1], 1);
	      dup2 (pipefd[2], 0);

	      close (pipefd[0]);
	      close (pipefd[3]);
	    }

	  execvp (s->command, s->args);

	  /* if we get there, it means execvp failed */
	  printf ("%s %d: Subprocess did not start: %s\n", datetime, getpid(), strerror(errno));
	  return -1;
	}
      else if (server_process > 0)
	{
	  if (s->pty)
	    close (pty[1]);
	  else
	    {
	      close (pipefd[1]);
	      close (pipefd[2]);
	    }
	}
      else
	{
	  printf ("%s %d: Could not fork: %s\n", datetime, getpid(), strerror(errno));
	  return -1;
	}      
    }
  else
    {
      now_r (DT_FMT, datetime);
      printf ("%s %d: No action configured for service `%s'\n", datetime, getpid(), s->name);
      return -1;
    }

  return server_process;
}

int forward_data (int fd_from, int fd_to, char *buffer, unsigned int len, int *readret, int *writeret) /* assume buffer is malloced to len size */
{
  int r, w;

  r = read (fd_from, buffer, len);
  if (readret)
    *readret = r;

  if (r <= 0)
    return r;

  w = write (fd_to, buffer, r);
  if (writeret)
    *writeret = w;

  if (w == 0)
    return 0;
  else if (w < r)
    return -1;
  else
    return w;
}
