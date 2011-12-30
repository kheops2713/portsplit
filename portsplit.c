/********************************************************************
 * This is a DIRTY port multiplexer. It listens on a given TCP port,
 * reads the first bytes sent by client and decides where to forward
 * the request, according to the protocol it detected.
 *
 * Explicitly recognized protocols are for now only OpenVPN and HTTP.
 * A third service is used as fallback (in case none of the previous
 * ones are recognized). It is expected that the case of "server must
 * speak first" will be taken into consideration in a near future.
 * 
 * Connections can be either:
 * - proxied to another host and port
 * - passed to a local subprocess through stdin/stdout (inetd-style),
 *   with the possibility of using a pseudoterminal (PTY)
 *
 * Author: KheOps <kheops@ceops.eu>
 * For the sake of freedom of speech and the spreading of ideas.
 ********************************************************************
 * Copyleft, please have fun, I'd be glad of any feedback/improvements
 * made to this little thing.
 ********************************************************************
 * Compile with -lutil
 *                               ~~~~~~=:)
 *         ~~~~~~=:)
 */


#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/select.h>
//#include <linux/in.h>
#include <sys/types.h>
#include <sys/wait.h>
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

#define BUFLEN 10240
#define MAXARGS 128
#define DT_FMT "%F %T"

enum { http, openvpn, timeout, other, nservices };

char *connectHost[nservices];
int connectPort[nservices];
char const optchar[nservices] = { 'H', 'V', 'T', 'O' };
char *execcmd[nservices];
char **execarg[nservices];
char const *descriptions[nservices] = { "HTTP", "OpenVPN", "Timeout [NOT YET IMPLEMENTED]", "Other" };
int is_pty[nservices];
int is_set[nservices];
int timeout_sec;

void close_all (int const[], int const[], int const[], int);
void start_child (int, int, struct sockaddr_in);
int treat_client (int, struct sockaddr_in);
int connect_host (char const*, int);
int connect_one (struct addrinfo const*);
static char* now (char const*);

int main (int argc, char **argv)
{
  memset (connectPort, 0, nservices * sizeof(int));
  memset (connectHost, 0, nservices * sizeof(char*));
  memset (execcmd, 0, nservices * sizeof(char*));
  memset (is_pty, 0, nservices * sizeof(int));
  memset (is_set, 0, nservices * sizeof(int));

  timeout_sec = 10;

  unsigned int iservice;
  for (iservice = 0; iservice < nservices; iservice++)
    {
      execarg[iservice] = (char**) malloc (MAXARGS * sizeof(char*));
      execarg[iservice][0] = execarg[iservice][MAXARGS - 1] = NULL;
    }

  char *listenIP = NULL;
  char *listenExec = NULL;
  int  listenPort = 0;

  char const localhost[] = "127.0.0.1";
  unsigned int maxconn = 1024;
  int sockfd, fdlisten, maxfd, ackfd;
  unsigned int nbconn = 0;
  fd_set fdset;
  int pipefd[2];

  int optch;
  int prevsrv = nservices;
  while ((optch = getopt (argc, argv, "H:O:V:T:l:a:pt:")) != -1)
    {
      switch (optch)
	{
	case 'H':
	case 'O':
	case 'V':
	case 'T':
	case 'l':
	  if (optarg[0] != '!')
	    {
	      char *colon, *host;
	      int port;
	      
	      if ((colon = index(optarg, ':')) != NULL)
		{
		  host = (char*) calloc (colon - optarg + 1, 1);
		  strncpy (host, optarg, colon - optarg);
		  
		  port = atoi (colon + 1);
		}
	      else
		{
		  host = (char*) calloc (strlen(localhost) + 1, 1);
		  strncpy (host, localhost, strlen(localhost));
		  
		  port = atoi(optarg);
		}
	      
	      if (optch == 'l')
		{
		  listenIP = host;
		  listenPort = port;
		  prevsrv = nservices;
		}
	      
	      int foundChar = 0;
	      int iservice = 0;
	      for (; iservice < nservices && !foundChar; iservice++)
		{
		  if (((char) optch) == optchar[iservice])
		    {
		      foundChar = 1;
		      prevsrv = iservice;
		      is_set[iservice] = 1;

		      connectHost[iservice] = host;
		      connectPort[iservice] = port;
		    }
		}
	    }
	  else
	    {
	      optarg++;
	      
	      int foundChar = 0;
	      int iservice = 0;
	      for (; iservice < nservices && !foundChar; iservice++)
		{
		  if (((char) optch) == optchar[iservice])
		    {
		      foundChar = 1;
		      prevsrv = iservice;
		      is_set[iservice] = 1;

		      execcmd[iservice] = (char*) malloc (strlen(optarg) + 1);
		      strcpy (execcmd[iservice], optarg);

		      execarg[iservice][0] = (char*) malloc (strlen(optarg) + 1);
		      strcpy (execarg[iservice][0], optarg);
		      execarg[iservice][1] = NULL;
		    }
		}
	    }
	  break;

	case 'a':
	  if (prevsrv != nservices && execcmd[prevsrv] != NULL)
	    {
	      unsigned int iarg;
	      for (iarg = 0; execarg[prevsrv][iarg] != NULL; iarg++);
	      
	      if (iarg < MAXARGS - 1)
		{
		  execarg[prevsrv][iarg] = (char*) malloc (strlen(optarg) + 1);
		  strcpy (execarg[prevsrv][iarg], optarg);
		  execarg[prevsrv][iarg + 1] = NULL;
		}
	    }
	  else
	    {
	      printf ("SYNTAX ERROR: -a should follow an exec command for a given service\n");
	      return -1;
	    }
	  break;

	case 'p':
	  if (prevsrv != nservices && execcmd[prevsrv] != NULL)
	    {
	      is_pty[prevsrv] = 1;
	    }
	  else
	    {
	      printf ("SYNTAX ERROR: -p should follow an exec command for a given service\n");
	      return -1;
	    }
	  break;

	case 't':
	  timeout_sec = atoi(optarg);
	  break;
	  
	}
    }
    
  if (!listenPort || (!connectPort[other] && execcmd[other] == NULL))
    {
      printf ("Syntax: %s ", argv[0]);

      unsigned int iservice;
      printf ("-[");
      for (iservice = 0; iservice < nservices; iservice++)
	printf ("%c%s", optchar[iservice], (iservice == nservices - 1 ? "]" : "|"));

      printf (" [!command [-a arg1 -a arg2 ...] [-p]|[host:]port] ... -l [IP:]port [-t timeout]\n\n");
       
      printf ("Proxyable services - at least 'O' must be defined:\n");
      for (iservice = 0; iservice < nservices; iservice++)
	printf ("-%c - %s\n", optchar[iservice], descriptions[iservice]);

      printf ("\nListening options (mandatory):\n");
      printf ("-l [IP:]port - Listen on given TCP port, bind on specified IP (default 127.0.0.1)\n");
      printf ("-t timeout - Timeout value (in seconds) to handle connection to \"Timeout\" service\n\n");

      printf ("Proxying syntax (one or the other for each service):\n");
      printf ("[host:]port - Connect to given TCP port on given host (default 127.0.0.1)\n");
      printf ("!command - Execute given command (e.g. /usr/sbin/pppd) as server subprocess\n");
      printf ("-a arg1 -a arg2 ... - Only with !command. Pass arguments to the command.\n");
      printf ("-p - Make stdin/stdout of subprocess into a pseudoterminal (useful for pppd)\n");

      return -1;
    }

  for (iservice = 0; iservice < nservices; iservice++)
    {
      if (execcmd[iservice] == NULL)
	printf ("%s connect: %s:%d\n", descriptions[iservice], connectHost[iservice], connectPort[iservice]);
      else
	{
	  printf ("%s execute: %s ", descriptions[iservice], execcmd[iservice]);
	  unsigned int iarg;
	  for (iarg = 0; execarg[iservice][iarg] != NULL; iarg++)
	    printf ("[%s] ", execarg[iservice][iarg]);
	  printf ("\n");
	}
      
    }
  printf ("Listen: %s:%d\n", listenIP, listenPort);

  /***********************************/
  /****** START SERIOUS THINGS *******/
  /***********************************/

  struct sockaddr_in listen_saddr;
 
  if ((sockfd = socket (PF_INET, SOCK_STREAM, 0)) == -1)
    {
      perror ("socket()");
      return -1;
    }
  
  memset (&listen_saddr, 0, sizeof(struct sockaddr_in));
  listen_saddr.sin_family = AF_INET;
  listen_saddr.sin_port = htons(listenPort);
  inet_aton (listenIP, &listen_saddr.sin_addr);

  if (bind(sockfd, (struct sockaddr*)&listen_saddr, sizeof(struct sockaddr)) == -1)
    {
      perror ("bind()");
      return -1;
    }
  if (listen (sockfd, 1) == -1)
    {
      perror ("listen()");
      return -1;
    }

  FD_ZERO (&fdset);
  FD_SET (sockfd, &fdset);

  if (pipe(pipefd) == -1)
    {
      perror ("pipe()");
      return -1;
    }
  
  FD_SET (pipefd[0], &fdset);

  maxfd = ((pipefd[0] > sockfd) ? pipefd[0] : sockfd);

  while (select (maxfd + 1, &fdset, NULL, NULL, NULL))
    {
      /* New connection incuming */
      if (FD_ISSET (sockfd, &fdset))
	{
	  struct sockaddr_in inbound;
	  int s = sizeof(struct sockaddr);
	  if ((ackfd = accept (sockfd, (struct sockaddr*)&inbound, &s)) != -1)
	    {

	      pid_t pid;
	      pid = fork();
	      if (pid == 0)
		{
		  start_child (pipefd[1], ackfd, inbound);
		  exit (0);
		}
	      else if (pid > 0)
		{
		  nbconn++;
		  printf ("%s -> Incoming connection %d/%d\n", now(DT_FMT), nbconn, maxconn);
		  close(ackfd); /* No worries, it's been duplicated to the child process */
		}
	    }
	}
      
      /* A child told us something */
      if (FD_ISSET (pipefd[0], &fdset))
	{
	  char l;
	  read (pipefd[0], &l, 1);

	  char fd_to_close[(int)l + 1];
	  read (pipefd[0], fd_to_close, (int)l);
	  fd_to_close[(int)l] = 0;

	  nbconn--;

	  printf ("%s -> Connection number is now %d/%d\n", now(DT_FMT), nbconn, maxconn);
	}

      FD_SET (sockfd, &fdset);
      FD_SET (pipefd[0], &fdset);
    }
  
  return 0;
}

void start_child (int pipefd, int clientfd, struct sockaddr_in addr)
{
  char clientfdstr[42];
  memset (clientfdstr, 0, 42 * sizeof(char));
  sprintf (clientfdstr, "%d", clientfd);
  char l = (char) strlen (clientfdstr);

  printf ("[%s/%d] Connection accepted on FD %d from IP %s\n", now(DT_FMT), getpid(), clientfd, inet_ntoa(addr.sin_addr));
  treat_client (clientfd, addr);
  printf ("[%s/%d] Connection terminated\n", now(DT_FMT), getpid());
  
  /* Just tell the father process we are terminating - guess we should use wait() instead */
  write (pipefd, &l, 1);
  write (pipefd, clientfdstr, (int)l);
}

int treat_client (int clientfd, struct sockaddr_in addr)
{
  unsigned int prebuflen = 2048, prefill = 0, buflen = 512;
  char prebuffer[prebuflen], buffer[buflen];
  int serverfd = 0, serverread = 0, serverwrite = 0, maxfd, selret;
  fd_set fdset, timeout_fdset;
  pid_t server_process = -1;
  struct timeval timeout = { timeout_sec, 0 };
  
  FD_ZERO (&fdset);
  FD_ZERO (&timeout_fdset);

  FD_SET (clientfd, &fdset);
  FD_SET (clientfd, &timeout_fdset);
  maxfd = clientfd;

  /* select() returns ZERO in case of timeout ~o~ */
  while (select (maxfd + 1, &fdset, NULL, NULL, &timeout))
    {
      if (FD_ISSET (clientfd, &fdset))
	{
	  int r = read (clientfd, buffer, buflen);

	  if (r > 0)
	    {
#ifdef DEBUG
	      printf ("[%s/%d] Read %d bytes from FD %d\n", now(DT_FMT), getpid(), r, clientfd);
#endif

	      int overflow = 0;

	      if (serverwrite == 0)
		{
#ifdef DEBUG
		  printf ("[%s/%d] Buffering...\n", now(DT_FMT), getpid());
#endif
		  if (prefill + r <= prebuflen)
		    {
		      memcpy (prebuffer + prefill, buffer, r);
		      prefill += r;
		    }
		  else
		    overflow = 1;

		  if (prefill > 5) // need to decide where to connect
		    {
		      int service_id;

		      /* HTTP case */
		      if (strncmp (prebuffer, "GET", 3) == 0 ||
			  strncmp (prebuffer, "POST", 4) == 0 ||
			  strncmp (prebuffer, "TRACE", 5) == 0 ||
			  strncmp (prebuffer, "HEAD", 4) == 0)
			{
			  service_id = http;
			}
		      
		      /* OpenVPN case */
		      else if (prebuffer[0] == 0x00 && prebuffer[1] == 0x0E && prebuffer[2] == 0x38)
			{
			  service_id = openvpn;
			}
		      
		      /* Fallback case */
		      else
			{
			  service_id = other;
			}

		      printf ("[%s/%d] Service: %s\n", now(DT_FMT), getpid(), descriptions[service_id]);

		      if (!is_set[service_id])
			{
			  close (clientfd);
			  return 0;
			}		       

		      if (execcmd[service_id] == NULL) /* proxy to another server */
			{
			  printf ("[%s/%d] Connect <> %s (%s:%d) [first bytes are 0x%08.8X 0x%08.8X 0x%08.8X 0x%08.8X 0x%08.8X]\n",
				  now(DT_FMT),
				  getpid(),
				  descriptions[service_id],
				  connectHost[service_id],
				  connectPort[service_id],
				  prebuffer[0], prebuffer[1], prebuffer[2], prebuffer[3], prebuffer[4]);
			  

			  if ((serverfd = connect_host (connectHost[service_id], connectPort[service_id])) == -1)
			    {
			      close (clientfd);
			      return -1;
			    }
			  else
			    {
			      serverread = serverwrite = serverfd;
			    }

			}
		      else /* launch a subprocess, possibly using a PTY */
			{
			  printf ("[%s/%d] Pipe <> %s [first bytes are 0x%08.8X 0x%08.8X 0x%08.8X 0x%08.8X 0x%08.8X]\n",
				  now(DT_FMT),
				  getpid(),
				  descriptions[service_id],
				  prebuffer[0], prebuffer[1], prebuffer[2], prebuffer[3], prebuffer[4]);
			  
			  int pty[2];
			  char tty[128];
			  int pipefd[4];
			  
			  if (is_pty[service_id])
			    {
			      if (openpty (pty, pty+1, tty, NULL, NULL) == -1)
				{
				  printf ("[%s/%d] Could not open PTY: %s\n", now(DT_FMT), getpid(), strerror(errno));
				  return -1;
				}
			      printf ("[%s/%d] Opened %s for server subprocess communication via standard input/output\n", now(DT_FMT), getpid(), tty);
			      serverread = serverwrite = pty[0];
			    }
			  else
			    {
			      pipe (pipefd);
			      pipe (pipefd + 2);

			      serverread = pipefd[0];
			      serverwrite = pipefd[3];

			      printf ("[%s/%d] Read from server subprocess on FD %d, write on FD %d\n", now(DT_FMT), getpid(), serverread, serverwrite);
			    }

			  server_process = fork();
			  if (server_process == 0) /* child process */
			    {
			      /* stdout is fd 1, stderr is fd 2, stdin is fd 0 */
			      /* child's stdout will be  ... serverread for us */
			      /* child's stdin will be serverwrite for us */

			      if (is_pty[service_id])
				{
				  close (pty[0]);
				  dup2 (pty[1], 1);
				  dup2 (pty[1], 0);
				  close(pty[1]); /* Don't know why: taken from stunnel source code */
				}
			      else
				{
				  dup2 (pipefd[1], 1); // send stdout to the pipe (for reading by main process)
				  dup2 (pipefd[2], 0); // read stdin from the pipe (for writing by main process)
			      
				  close (pipefd[0]);
				  close (pipefd[3]);
				}

			      execvp (execcmd[service_id], execarg[service_id]);

			      printf ("Subprocess did not start: %s\n", strerror(errno));
			    }
			  else if (server_process > 0) /* parent process  */
			    {
			      if (is_pty[service_id])
				close (pty[1]);
			      else
				{
				  close (pipefd[1]);
				  close (pipefd[2]);
				}
			    }
			  else /* fork did not find its knife */
			    {
			      printf ("[%d] Could not fork: %s\n", getpid(), strerror(errno));
			      return -1;
			    }
			}

		      /* Here we need to write to the server the prebuffer content, after a successful connection */
		      int pw;
		      if ((pw = write (serverwrite, prebuffer, prefill)) < prefill)
			{
			  printf ("[%s/%d] Pre-buffer short write to server (%d out of %d), giving up.\n", now(DT_FMT), getpid(), pw, prefill);
			  close (clientfd);
			  close (serverwrite);
			  close (serverread);
			  return -1;
			}
		      if (overflow && (pw = write (serverfd, buffer, r)) < r)
			{
			  printf ("[%s/%d] Pre-buffer overflow short write to server (%d out of %d)\n", now(DT_FMT), getpid(), pw, r);
			  close (clientfd);
			  close (serverwrite);
			  close (serverread);
			  return -1;
			}
		    }
		}
	      else /* already connected to server */
		{
		  int w = write (serverwrite, buffer, r);
		  if (w < r && w >= 0)
		    {
		      printf ("[%s/%d] Short write to server (%d out of %d), giving up.\n", now(DT_FMT), getpid(), w, r);
		      close (clientfd);
		      close (serverwrite);
		      close (serverread);
		      return -1;
		    }
		  else if (r <0)
		    {
		      perror("write()");
		      close (clientfd);
		      close(serverwrite);
		      close(serverread);
		      return -1;
		    }	  
		}
	    }
	  else
	    {
	      close (clientfd);
	      printf ("[%s/%d] Disconnected from client FD %d\n", now(DT_FMT), getpid(), clientfd);
	      close (serverread);
	      close (serverwrite);
	      return 0;
	    }
	}

      if (serverread > 0 && FD_ISSET(serverread, &fdset))
	{
	  int r = read (serverread, buffer, buflen);
#ifdef DEBUG
	  printf ("[%s/%d] Read %d bytes from server (FD %d)\n", now(DT_FMT), getpid(), r, serverread);
#endif
	  if (r < 0)
	    {
	      printf ("[%s/%d] Error reading from server :/\n", now(DT_FMT), getpid());
	      close (serverread);
	      close (serverwrite);
	      close (clientfd);
	      return -1;
	    }
	  else if (r == 0)
	    {
	      printf ("[%s/%d] Server closed connection\n", now(DT_FMT), getpid());
	      close (serverread);
	      close (serverwrite);
	      close (clientfd);
	      return 0;
	    }
	  else
	    {
	      int w;
	      if ((w = write(clientfd, buffer, r)) < r)
		{
		  printf ("[%s/%d] Short write to client (%d out of %d), giving up.\n", now(DT_FMT), getpid(), w, r);
		  close (clientfd);
		  close (serverread);
		  close (serverwrite);
		  return -1;
		}
	    }
	}

      FD_ZERO (&fdset);
      FD_SET (clientfd, &fdset);
      if (serverread > 0)
	FD_SET (serverread, &fdset);

      maxfd = ((clientfd > serverread) ? clientfd : serverread);
    }

  printf ("PROUTE !!! :D\n");

  return 0;
}

int connect_host (char const *hostname, int port)
{
  int serverfd = -1;
  char portstr[10];

  printf ("[%d] Looking up %s...\n", getpid(), hostname);
  
  struct addrinfo *result;
  struct addrinfo hint;
  int retdns;

  memset (portstr, 0, 10 * sizeof(char));
  memset (&hint, 0, sizeof(struct addrinfo));
  hint.ai_family = AF_INET;
  hint.ai_socktype = SOCK_STREAM;
  sprintf (portstr, "%d", port);

  if ((retdns = getaddrinfo(hostname, portstr, &hint, &result)) != 0)
    {
      printf ("[%d] Resolv Fail: %s\n", getpid(), gai_strerror(retdns));
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

int connect_one (struct addrinfo const *ai)
{
  int sockfd = -1, success = 0;
  struct addrinfo const *p;

  for (p = ai; p != NULL && success == 0; p = p->ai_next)
    {
      if (p->ai_family == AF_INET)
	printf ("[%d] Trying %s...\n", getpid(), inet_ntoa(((struct sockaddr_in*)p->ai_addr)->sin_addr));

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
	      success = 1;
	      printf ("[%d] Connected.\n", getpid());
	    }
	}
    }

  return ((success == 1) ? sockfd : -1);
}

void close_all (int const sock[], int const fd[], int const established[], int n)
{
  int i;
  for (i = 0; i < n; i++)
    {
      if (established[i])
	{
	  close (fd[i]);
	  perror ("close()");
	}

      close (sock[i]);
      perror ("close()");
    }
}

static char* now (char const *fmt)
{
  static char str[64];
  time_t epochsec = time(NULL);
  struct tm *proute;

  proute = localtime(&epochsec);

  strftime (str, 64, fmt, proute);

  return str;
}
