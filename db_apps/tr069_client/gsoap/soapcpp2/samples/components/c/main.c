
// must include envH.h first: it declares the SOAP Header and Fault structures
// shared among the clients and services
#include "envH.h"

// include the quote and rate stubs and calc skeleton
#include "quoteH.h"
#include "rateH.h"
#include "calcH.h"

// include the XML namespace mapping tables
#include "quote.nsmap"
#include "rate.nsmap"
#include "calc.nsmap"

int main(int argc, char *argv[])
{ struct soap *soap = soap_new();
  if (argc <= 1)
    return calc_serve(soap);
  else
  { float q;
    if (soap_call_ns__getQuote(soap, NULL, NULL, argv[1], &q))
      soap_print_fault(soap, stderr);
    else
    { if (argc > 2)
      { float r;
        if (soap_call_ns__getRate(soap, NULL, NULL, "us", argv[2], &r))
          soap_print_fault(soap, stderr);
        else
          q *= r;
      }
      printf("%s: %g\n", argv[1], q);
    }
  }
  soap_end(soap);
  soap_done(soap);
  free(soap);
  return 0;
}

int ns__add(struct soap *soap, double a, double b, double *result)
{ *result = a + b;
  return SOAP_OK;
}

int ns__sub(struct soap *soap, double a, double b, double *result)
{ *result = a - b;
  return SOAP_OK;
}

int ns__mul(struct soap *soap, double a, double b, double *result)
{ *result = a * b;
  return SOAP_OK;
}

int ns__div(struct soap *soap, double a, double b, double *result)
{ *result = a / b;
  return SOAP_OK;
}
