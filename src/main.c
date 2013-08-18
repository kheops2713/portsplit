/*
 * main.c - this file is part of Portsplit.
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

#include "main.h"
#include "log.h"
#include "child.h"
#include "sigs.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <endian.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <strings.h>
#include <string.h>
#include <netdb.h>
#include <errno.h>
#include <pty.h>
#include <time.h>
#include <signal.h>
#include <fcntl.h>

int main (int argc, char **argv)
{
  config cfg;
  int ret;

  if (argc != 2)
    {
      fprintf (stderr, "Usage: %s <config file>\n", argv[0]);
      return -1;
    }

  if (parse_config_file (argv[1], &cfg) == -1)
    return -1;

  ret = main_listen_loop (cfg);

  return ret;
}


int main_listen_loop (config _cfg)
{
  struct sockaddr_in *listen_saddr;
  struct sockaddr_in6 *listen_saddr6;
  struct sockaddr inbound;
  unsigned int i,j, nbconn;
  int *sockfd, *sockfamily, maxfd, selret, ackfd, s, sig, terminate, child_ret, newconfig, status_info;
  ssize_t wret;
  socklen_t sl;
  fd_set fdset;
  struct timeval refresh;
  pid_t deadpid, newpid;
  config oldcfg, *cfg;
  char *cfgfile, *pidfile, pidstr[9];

  cfg = &_cfg;

  sprintf (pidstr, "%d", getpid());

  newconfig = 1;
  nbconn = 0;

  set_caugth_signals();

  if (pipe(sigfd) == -1)
    {
      fprintf (stderr, "ERROR: could not setup pipe for signal catching: %s\n", strerror(errno));
      return -1;
    }

  while (newconfig)
    {
      if (cfg->pidfile)
	{
	  if ((s = open (cfg->pidfile, O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR)) < 0)
	    {
	      fprintf (stderr, "ERROR: could not open file `%s': %s\n", cfg->pidfile, strerror(errno));
	      return -1;
	    }
	  wret = write (s, pidstr, strlen(pidstr));
	  if (wret < 0 || (unsigned int)wret < strlen(pidstr))
	    {
	      fprintf (stderr, "ERROR: could not write PID to file `%s'%s%s\n", cfg->pidfile, (wret < 0 ? ":" : ""), (wret < 0 ? strerror(errno) : ""));
	      close (s);
	      return -1;
	    }
	  
	  close (s);
	}
      
      printf ("%s -> Loaded configuration: %d services defined, %d local addresses to bind\n", now(), cfg->nservices, cfg->nbind);

      newconfig = 0;
      maxfd = 0;

      FD_ZERO (&fdset);
      FD_SET (sigfd[0], &fdset);
      if (sigfd[0] > maxfd)
	maxfd = sigfd[0];

      listen_saddr = malloc (sizeof(struct sockaddr_in) * cfg->nbind);
      listen_saddr6 = malloc (sizeof(struct sockaddr_in6) * cfg->nbind);
      sockfd = malloc(sizeof (int) * cfg->nbind);
      sockfamily = malloc (sizeof (int) * cfg->nbind);

      memset (listen_saddr, 0, sizeof(struct sockaddr_in) * cfg->nbind);
      memset (listen_saddr6, 0, sizeof(struct sockaddr_in6) * cfg->nbind);

      terminate = 0;
      refresh.tv_sec = 10;
      refresh.tv_usec = 0;

      for (i = 0; i < cfg->nbind; i++)
	{
	  if (setup_socket (cfg->addr[i], cfg->port[i], sockfd + i, sockfamily + i, listen_saddr + i, listen_saddr6 + i) == -1)
	    {
	      terminate = 1;
	      i = cfg->nbind;
	    }
	  else
	    {
	      FD_SET (sockfd[i], &fdset);
	      if (sockfd[i] > maxfd)
		maxfd = sockfd[i];
	    }
	}

      while (!terminate)
	{
	  selret = select (maxfd + 1, &fdset, NULL, NULL, &refresh);

	  refresh.tv_sec = 10;
	  refresh.tv_usec = 0;

	  /* Check for dead children */
	  while ((deadpid = waitpid (-1, &status_info, WNOHANG)) > 0)
	    {
	      nbconn--;
	      printf ("%s -> Process %d terminated (exit code %d), connection number is now %d/%d\n", now(), deadpid, WEXITSTATUS(status_info), nbconn, cfg->maxconn);
	    }

	  if (selret >= 0)
	    {
	      /* New connection incuming? */
	      for (i = 0; i < cfg->nbind; i++)
		{
		  if (FD_ISSET (sockfd[i], &fdset))
		    {
		      if (sockfamily[i] == AF_INET)
			sl = sizeof (struct sockaddr_in);
		      else if (sockfamily[i] == AF_INET6)
			sl = sizeof (struct sockaddr_in6);
		      ackfd = accept (sockfd[i], &inbound, &sl);
		      if (ackfd != -1)
			{
			  if (nbconn < cfg->maxconn || cfg->maxconn == 0)
			    {
			      newpid = fork();
			      if (newpid == 0)
				{
				  /* Child process - close all unnecessary file descriptors */
				  for (j = 0; j < cfg->nbind; j++)
				    close (sockfd[j]);
				  child_ret = treat_client (ackfd, &inbound, sockfamily[i], cfg);
				  close (ackfd);
				  exit (child_ret);
				}
			      else if (newpid > 0)
				{
				  nbconn++;
				  printf ("%s -> Spawning child process for connection %d%s%d\n", now(), nbconn, cfg->maxconn ? "/" : "", cfg->maxconn);
				  close(ackfd); /* No worries, it's been duplicated to the child process */
				}
			    }
			  else
			    {
			      printf ("%s -> Dropping incoming connection (no more slots available)\n", now());
			      close (ackfd);
			    }
			}
		    }
		}
	      /* 2012-05-06, 8:00PM: Sarkozy just lost the elections. \o/ */

	      if (FD_ISSET (sigfd[0], &fdset))
		{
		  read (sigfd[0], &sig, sizeof(sig));
		  if (sig == SIGTERM || sig == SIGINT)
		    {
		      printf ("%s -> Got signal %d, exiting\n", now(), sig);
		      terminate = 1;
		    }
		  else if (sig == SIGHUP)
		    {
		      printf ("%s -> Got SIGHUP, reloading configuration\n", now());
		      terminate = 1;

		      cfgfile = malloc (strlen(cfg->filename) + 1);
		      strcpy (cfgfile, cfg->filename);
		      if (cfg->pidfile != NULL)
			{
			  pidfile = malloc (strlen (cfg->pidfile) + 1);
			  strcpy (pidfile, cfg->pidfile);
			}
		      else
			pidfile = NULL;

		      oldcfg = *cfg; /* Copy content, references will still be valid */

		      if (parse_config_file (cfgfile, cfg) == 0)
			{
			  if (pidfile != NULL && remove(pidfile) == -1)
			    fprintf (stderr, "ERROR: Could not remove PID file `%s': %s\n", pidfile, strerror(errno));
			  else
			    newconfig = 1;
			}

		      for (i = 0; i < oldcfg.nbind; i++)
			close (sockfd[i]);

		      free (cfgfile);
		      free (pidfile);
		      free (listen_saddr);
		      free (listen_saddr6);
		      free (sockfd);
		      free (sockfamily);
		      free_config (&oldcfg);
		    }
		  else if (sig == SIGCHLD)
		    {
		      /* Maybe do something? (for now waitpid() is used) */
		    }
		}
	    }

	  if (!newconfig)
	    {
	      FD_ZERO (&fdset);
	      for (i = 0; i < cfg->nbind; i++)
		FD_SET (sockfd[i], &fdset);
	      FD_SET (sigfd[0], &fdset);
	    }
	}

      for (i = 0; !newconfig && i < cfg->nbind; i++)
	close (sockfd[i]);
    }

  close (sigfd[0]);
  close (sigfd[1]);

  if (cfg->pidfile != NULL && remove (cfg->pidfile) == -1)
    fprintf (stderr, "ERROR: Could not remove PID file `%s': %s\n", cfg->pidfile, strerror(errno));

  free (listen_saddr);
  free (listen_saddr6);
  free (sockfd);
  free (sockfamily);
  free_config (cfg);

  return 0;
}

int setup_socket (char const *addr, unsigned int port, int *sockfd, int *sockfamily, struct sockaddr_in *listen_saddr, struct sockaddr_in6 *listen_saddr6)
{
  int s;

  if (index (addr, ':') != NULL)
    {
      *sockfamily = PF_INET6;
      (*listen_saddr6).sin6_family = AF_INET6;
      (*listen_saddr6).sin6_port = htons(port);
      if (inet_pton (AF_INET6, addr, &((*listen_saddr6).sin6_addr)) != 1)
	{
	  fprintf (stderr, "ERROR: IPv6 address `%s' is invalid\n", addr);
	  return -1;
	}
    }
  else
    {
      *sockfamily = PF_INET;
      (*listen_saddr).sin_family = AF_INET;
      (*listen_saddr).sin_port = htons (port);
      if (inet_pton (AF_INET, addr, &((*listen_saddr).sin_addr)) != 1)
	{
	  fprintf (stderr, "ERROR: IPv4 address `%s' is invalid\n", addr);
	  return -1;
	}
    }

  if ((*sockfd = socket (*sockfamily, SOCK_STREAM, 0)) == -1)
    {
      fprintf (stderr, "ERROR: could not create socket: %s\n", strerror(errno));
      return -1;
    }
  s = 1;
  if (setsockopt (*sockfd, SOL_SOCKET, SO_REUSEADDR, &s, sizeof(s)) == -1)
    {
      fprintf (stderr, "ERROR: could not set SO_REUSEADDR to socket: %s\n", strerror(errno));
      return -1;
    }
  if (*sockfamily == PF_INET)
    {
      if (bind (*sockfd, (struct sockaddr*)listen_saddr, sizeof(struct sockaddr_in)) == -1)
	{
	  fprintf (stderr, "ERROR: could not bind socket to %s:%d: %s.\n", addr, port, strerror(errno));
	  return -1;
	}
    }
  else if (*sockfamily == PF_INET6)
    {
      if (bind (*sockfd, (struct sockaddr*) listen_saddr6, sizeof (struct sockaddr_in6)) == -1)
	{
	  fprintf (stderr, "ERROR: could not bind socket to %s:%d: %s.\n", addr, port, strerror(errno));
	  return -1;
	}
    }
  if (listen (*sockfd, 1) == -1)
    {
      fprintf (stderr, "ERROR: could not listen on %s:%d: %s\n", addr, port, strerror(errno));
      return -1;
    }

  return 0;
}
