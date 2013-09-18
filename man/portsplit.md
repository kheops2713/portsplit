# NAME

portsplit - a TCP port multiplexer

# SYNOPSIS

_portsplit_ <filename>

# DESCRIPTION

The _portsplit_ program is designed to "multiplex" a TCP port, that is, to allow to have several distinct services reachable behind a single TCP port. For instance, one can open a single TCP port so that clients connecting to this port can reach a HTTP server, an IRC server or a PPP daemon, depending on what they request upon connection.

The principle of _portsplit_ is simply to listen on a (set of) local port(s), wait for clients to connect and decide what to do with each incoming connection based on the first bytes sent by the connecting client.

No protocol or service is hardcoded in _portsplit_: its configuration file must contain _patterns_, i.e. sequences of bytes with which what the client sends will be compared. If a pattern is matched by what a client sent, a corresponding action is taken. An action is taken as well if the client does not send any byte for some period of time or if the bytes sent have no chance to match anything defined in the configuration.

Actions must also be configured, and three of them are possible: transparently forward the connection to a remote server; execute a local process and forward its standard input/output from/to the client; close the connection. Closing the connection is not explicitly configurable and is the default behavior if no pattern is matched and no fallback action has been configured.

A set of patterns and a single action together define a _service_ in the _portsplit_ configuration. An "http" service could for instance have the string patterns "GET" and "POST" and have the action of connecting to IP address ::1 on port 80, i.e. a locally running web server.

_Portsplit_ fully supports both IPv4 and IPv6.

# OPTIONS

_Portsplit_ takes only one option, which is the path to its configuration file.

# CONFIGURATION FILE

Fix me!

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
