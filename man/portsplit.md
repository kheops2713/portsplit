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

| \<**filename**\>
|       Path to the configuration file.

# CONFIGURATION FILE

For those who know the _stunnel_ software, **portsplit**'s configuration file syntax is very similar, although most of the options names and values are obviously different.

Each line of the configuration file can be either:

* an empty line or a line starting with '#' (comment), both being ignored;
* an 'option_name = option_value' pair;
* '[service_name]' indicating a start of service definition, inside which a set of patterns and an action should be defined.

## GLOBAL OPTIONS

Fix me!

## SERVICES

Fix me!

# SIGNALS

Fix me!

# LIMITATIONS

Fix me!

# BUGS

Fix me!
