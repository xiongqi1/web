This example demonstrates the use of C++ namespaces to combine clients and a
service into a single executable.

The client proxies and server object are separated by C++ namespaces.

Clients: XMethods delayed quote and exchange rate
Service: calculator service

Compile:
make

Usage:
./main				runs CGI calculator service (install as CGI)
./main <stock>			returns stock quote
./main <stock> <currency>	returns stock quote in currency
