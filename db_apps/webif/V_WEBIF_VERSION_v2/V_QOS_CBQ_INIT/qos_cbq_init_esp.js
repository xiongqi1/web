// QOS cbq.init server side script (AppWeb ESP)

if (request['REQUEST_METHOD'] == "POST") {

  // Set the RDB(s)
  set_single_direct("-p", "qos.cbqInit.enable", form["id_enabled"]);

  // Set the trigger last.
  set_single_direct("", "qos.cbqInit.trigger", "1");

  // Show success message.
  redirect(request["SCRIPT_NAME"] + "?success");

} else { // GET request
  // Return the current files
  exec_cmd("/www/cgi-bin/qos_cbqInit_list.cgi");
}
