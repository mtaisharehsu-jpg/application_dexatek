#ifndef DEXATEK_MAIN_HELP_H_
#define DEXATEK_MAIN_HELP_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <stdarg.h>
#include <ctype.h>
#include <stdint.h>
#include <stddef.h>

void help(const char *basename);
__attribute__((format(printf, 3, 4))) void help_hexdump(const void *p, size_t len, const char *fmt, ...);
__attribute__((format(printf, 3, 4))) void help_ehexdump(const void *p, size_t len, const char *fmt, ...);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* DEXATEK_MAIN_HELP_H_ */
