#ifndef APP_SUPPORT_H
#define APP_SUPPORT_H
const char *error_hint(int code);
/* Creates a fresh dataset under the current directory, returns absolute paths.
 * Caller owns returned strings. Partial demo data is retained on failure. */
char *create_demo_source(char **destination);
#endif
