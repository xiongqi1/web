/*	event.c

	C-style client

	Events are based on asynchronous one-way SOAP messaging using HTTP
	keep-alive for persistent connections

	The 'synchronous' global flag illustrates SOAP one-way messaging,
	which requires an HTTP OK response with an empty body to be returned
	by the server.

	Copyright (C) 2000-2002 Robert A. van Engelen. All Rights Reserved.

	Compile:
	soapcpp2 -c event.h
	cc -o event event.c stdsoap2.c soapC.c soapClient.c

	Run (first start the event handler on localhost port 18000):
	event

*/

#include "soapH.h"
#include "Event.nsmap"

int synchronous = 0; /* =1: SOAP interoperable synchronous one-way messaging over HTTP */

/* Service details copied from event.h: */
const char *event_handler_endpoint = "http://localhost:18000";
const char *event_handler_action = "event";

int main()
{ struct soap soap;
  soap_init2(&soap, SOAP_IO_KEEPALIVE, SOAP_IO_KEEPALIVE);
  if (soap_send_ns__handle(&soap, event_handler_endpoint, event_handler_action, EVENT_A))
    soap_print_fault(&soap, stderr);
  if (synchronous && soap_recv_empty_response(&soap))
    soap_print_fault(&soap, stderr);
  if (soap_send_ns__handle(&soap, event_handler_endpoint, event_handler_action, EVENT_B))
    soap_print_fault(&soap, stderr);
  if (synchronous && soap_recv_empty_response(&soap))
    soap_print_fault(&soap, stderr);
  /* reset keep-alive when client needs to inform the server that it will close the connection. It may reconnect later */
  soap_clr_omode(&soap, SOAP_IO_KEEPALIVE);
  if (soap_send_ns__handle(&soap, event_handler_endpoint, event_handler_action, EVENT_C))
    soap_print_fault(&soap, stderr);
  if (synchronous && soap_recv_empty_response(&soap))
    soap_print_fault(&soap, stderr);
  /* close the socket */
  soap_closesock(&soap);
  /* enable keep-alive which is required to accept and execute multiple receives */
  soap_set_omode(&soap, SOAP_IO_KEEPALIVE);
  if (soap_send_ns__handle(&soap, event_handler_endpoint, event_handler_action, EVENT_Z))
    soap_print_fault(&soap, stderr);
  else
  { struct ns__handle response;
    for (;;)
    { if (!soap_valid_socket(soap.socket))
      { fprintf(stderr, "Connection was terminated (keep alive disabled?)\n");
        break;
      }
      if (soap_recv_ns__handle(&soap, &response))
      { if (soap.error == SOAP_EOF)
          fprintf(stderr, "Connection was gracefully closed by server\n");
        else
	  soap_print_fault(&soap, stderr);
	break;
      }
      else
      { switch (response.event)
        { case EVENT_A: fprintf(stderr, "Client Event: A\n"); break;
          case EVENT_B: fprintf(stderr, "Client Event: B\n"); break;
          case EVENT_C: fprintf(stderr, "Client Event: C\n"); break;
          case EVENT_Z: fprintf(stderr, "Client Event: Z\n"); break;
        }
      }
    }
  }
  soap_closesock(&soap); /* soap_send operations keep the socket open to possibly accept responses, so we need to explicitly close the socket now */
  soap_end(&soap); /* this will close the socket too (if keep alive is off), just in case */
  soap_done(&soap); /* detach environment (also closes sockets even with keep-alive) */
  return 0;
}
