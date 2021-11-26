// Combine all javascript into one file at built time due to NRB200 limitations
#include "common.js"
#include "create_html.js"
//
// For now, include only one of the two following include lines
// To use websocket (more efficient but not as well supported), include ws_client.js
// To use Ajax get methods for page updates, include agax_get.js
// May turn this into a V_VARIABLE one day
//
#include "ws_client.js"
//#include "ajax_get.js"
