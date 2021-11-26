/*
Rules for system supervision. If any of these functions returns RULES_RESET,
reboot the system.
*/

#define RULES_OK 0
#define RULES_RESET 1
#define RULES_ERROR (-1)

/* Check syslog line against rules. Reset the system if returns 1 */
int rules_logcheck(const char *LogLine);

/* Check general condition, advance state machine. Call every few seconds. */
int rules_poll(void);

extern char Exit_Reason[128];
