This example demonstrates the use of the *ClientLib.c and *ServerLib.c
generated files to combine multiple clients and service into one executable.

Clients: XMethods delayed quote and exchange rate
Service: calculator service

To compile:
make

Usage:
./main				runs CGI calculator service (install as CGI)
./main <stock>			returns stock quote
./main <stock> <currency>	returns stock quote in currency
