# NAME

portsplit - a TCP port multiplexer

# SYNOPSIS

**portsplit** \<_filename_\>

# DESCRIPTION

The **portsplit** program is designed to "multiplex" a TCP port, that is, to allow to have several distinct services reachable behind a single TCP port. For instance, one can open a single TCP port so that clients connecting to this port can reach a HTTP server, an IRC server or a PPP daemon, depending on what they request upon connection.

The principle of **portsplit** is simply to listen on a (set of) local port(s) and decide what to do with each incoming connection based on the first bytes sent by the connecting client. It makes it comparable to the software _sslh_.

No protocol or service is hardcoded in **portsplit**: its configuration file must contain **patterns**, i.e. sequences of bytes with which what the client sends will be compared. If a pattern is matched by what a client sent, a corresponding action is taken. An action is taken as well if the client does not send any byte for some period of time or if the bytes sent have no chance to match anything defined in the configuration.

Actions must also be configured, and three of them are possible: transparently forward the connection to a remote server; execute a local process and forward its standard input/output from/to the client; close the connection. Closing the connection is not explicitly configurable and is the default behavior if no pattern is matched and no fallback action has been configured.

A set of patterns and a single action together define a **service** in the **portsplit** configuration. An "http" service could for instance have the string patterns "GET" and "POST" and have the action of connecting to IP address ::1 on port 80, i.e. a locally running web server.

**Portsplit** fully supports both IPv4 and IPv6.

# OPTIONS

\<**filename**\>

Path to the configuration file.

# CONFIGURATION FILE

For those who know the _stunnel_ software, **portsplit**'s configuration file syntax is very similar, although most of the options names and values are obviously different.

Each line of the configuration file can be either:

* an empty line or a line starting with '#' (comment), both being ignored;
* an 'option_name = option_value' pair (all blank spaces just before and just after the '=' are ignored);
* '[service_name]' indicating a start of service definition, inside which a set of patterns and an action should be defined.

There are two special services:

* the service _fallback_ is activated when the connecting client sent bytes that have no chance to match any of the other services defined (if no _fallback_ service  is defined at all, **portsplit** will close the connection);
* the service _timeout_ is activated when the connecting client did not match any service after some amount of time (see _timeout_ option below) but could theoretically match one if it sent more bytes, for instance when the client did not send any data at all because it is using a server-talk-first protocol (if not defined, the connection will be closed if the timeout is hit).

## GLOBAL OPTIONS

**bind** = \<_IP_\>:\<_port_\>

Specify a local address and port to bind. _IP_ can be either an IPv4 or IPv6 in their standard or shortened notation (for IPv6). This option can be specified several times. Both the IP and port specifications are mandatory. The usual wildcards apply in order to listen on all interfaces: '0.0.0.0' for IPv4 and '::' for IPv6.

Listening on all interfaces on both IPv4 and IPv6 on port 9090 is thus done like this:

    bind = 0.0.0.0:9090
    bind = :::9090

On the other hand, binding only the loopback interface, both in IPv4 and IPv6, is done this way:

    bind = 127.0.0.1:9090
    bind = ::1:9090

**maxconn** = \<_number_\>

Specify the maximum number of concurrent connections **portsplit** will accept. When the maximum number is reached, the connection will be accepted and immediately closed.

Note that **portsplit** forks a new process after each new connection, unless the maximum has been reached. The forked process is in charge of matching a service and taking the corresponding action.

**pid** = \<_filename_\>

Specify a filename that will contain the PID of **portsplit**'s main process. It can be used to send signals to **portsplit** such as HUP and TERM to respectively re-read the configuration and exit the program.

**timeout** = \<_seconds_\>

Specify the number of seconds after which the _timeout_ service must be engaged if the client did not send enough data to match a service nor to be sure that no service at all can be matched (in which case the _fallback_ service would have been used).

## SERVICE-LEVEL OPTIONS

At the service level, two types of options are distinguished and they define respectively:

* which data to expect from a client in order to activate the given service (_matching_ options);
* what to do once the service has been matched (_action_ options);

### Matching Options

**string** = \<_byte_ _sequence_\>

Will activate the service if the data sent by the client **starts with** _byte_ _sequence_. It is important to remember that this sequence is compared exactly to the first bytes sent by the client. That is, if we want to match a client that sends 'GET / HTTP/1.1', we must not use 'HTTP' in _byte_ _sequence_ but 'GET'.

It is possible to specify non-printable bytes by using the syntax '\\_XX_' where _XX_ is the byte's hexadecimal code.

Since spaces on each side of the '=' are ignored, matching a string that starts with a space must be done by using its hexadecimal code, '\\20'.

### Action Options

**connect** = \<_host_\>:\<_port_\>

Connects to the given _host_:_port_ pair (an IPv4 or IPv6 may also be specified). Use this option only once for each service. It is mutually exclusive with the _exec_ option.

Once the connection is established, the data is transparently proxied between the client and the server. The first bytes that may have been sent by the client that allowed **portsplit** to match the service are buffered and sent to the server upon connection.

**exec** = \<_command_\>

Execute a command, feed its standard entry with the client's data and send its standard output to the client. Use this option only once for each service. It is mutually exclusive with the _connect_ option.

The first bytes that may have been sent by the client that allowed **portsplit** to match the service are buffered and sent to the new process' standard entry as soon as the process is started.

The new process is started using the standard fork-exec method.

**execarg** = \<_string_\>

Specify one (and only one) argument to the command given in the _exec_ option. It may be used several times and may contain spaces. Arguments are passed to the command in the same order as they appear in the configuration.

**pty** = yes|no

Provide a pseudoterminal (pty) to the process started by _exec_. Defaults to 'no'.

# SIGNALS

Fix me!

# LIMITATIONS

Fix me!

# BUGS

Fix me!

# SEE ALSO

Fix me!
