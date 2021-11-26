
The wsdl2h tool can import multiple WSDL files at once to combine multiple
clients and service into one executable. However, if there is a need to build
clients and services from multiple gSOAP header files, then an alternative
approach is required.

The C and C++ examples in this directory illustrate how multiple clients and
services can be compiled and linked into one executable from multiple gSOAP
header files. The C examples accomplish this by merging statically declared
serializers. The C++ examples accomplish this by using C++ namespaces to separate
the serializers.

See the README.txt in the c and cpp directories for more details.
