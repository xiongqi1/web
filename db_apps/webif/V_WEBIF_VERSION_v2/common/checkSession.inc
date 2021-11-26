#if 0
// must be included within an ESP block
// CHECK_SESSION_NO_CODE can be defined to do some action before exit
#endif
useSession();
if (
#ifdef CHECK_ROOT_USER
    (session["userlevel"] != '0') ||
#elif defined CHECK_ADMIN_USER
    (session["userlevel"] != '1') ||
#endif
    request["SESSION_ID"] != session["sessionid"]) {
#ifdef CHECK_SESSION_NO_CODE
  CHECK_SESSION_NO_CODE
#endif
  exit(403);
}
