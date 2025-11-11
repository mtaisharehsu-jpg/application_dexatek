#ifndef APPLICATION_LOG_H
#define APPLICATION_LOG_H

#ifdef __cplusplus
extern "C" {
#endif

#if 0
#define here( tag )		                printf( "\033[1;36m[%lld]\033[1;39m\033[1;37m HERE  | %s | %s:%d \033[0m\n", time_get_current_ms(), tag, __FUNCTION__, __LINE__)
#define error( tag, format, ... )		printf( "\033[1;36m[%lld]\033[1;39m\033[1;31m ERROR | %s | %s:%d | "format"\033[0m\n", time_get_current_ms(), tag, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define warn( tag, format, ... )		printf( "\033[1;36m[%lld]\033[1;39m\033[1;35m WARN  | %s | %s:%d | "format"\033[0m\n", time_get_current_ms(), tag, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define critical( tag, format, ... )	printf( "\033[1;s36m[%lld]\033[1;39m\033[1;35m CRITI | %s | %s:%d | "format"\033[0m\n", time_get_current_ms(), tag, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define trace( tag, format, ... )		printf( "\033[1;36m[%lld]\033[1;39m\033[1;34m TRACE | %s | %s:%d | "format"\033[0m\n", time_get_current_ms(), tag, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define info( tag, format, ... )		printf( "\033[1;36m[%lld]\033[1;39m\033[1;33m INFO  | %s | %s:%d | "format"\033[0m\n", time_get_current_ms(), tag, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define debug( tag, format, ... )		printf( "\033[1;36m[%lld]\033[1;39m\033[1;32m DEBUG | %s | "format"\033[0m\n", time_get_current_ms(), tag, ##__VA_ARGS__)
#else
#  define here( tag ) do { char tstr[32]; printf( "\033[1;36m%s\033[1;39m\033[1;37m | HERE  | %s | %s:%d \033[0m\n", time_get_current_date_string_r(tstr, sizeof(tstr)), tag, __FUNCTION__, __LINE__); } while(0)
#  define error( tag, format, ... ) do { char tstr[32]; printf( "\033[1;36m%s\033[1;39m\033[1;31m | ERROR | %s | %s:%d | "format"\033[0m\n", time_get_current_date_string_r(tstr, sizeof(tstr)), tag, __FUNCTION__, __LINE__, ##__VA_ARGS__); } while(0)
#  define warn( tag, format, ... ) do { char tstr[32]; printf( "\033[1;36m%s\033[1;39m\033[1;35m | WARN  | %s | %s:%d | "format"\033[0m\n", time_get_current_date_string_r(tstr, sizeof(tstr)), tag, __FUNCTION__, __LINE__, ##__VA_ARGS__); } while(0)
#  define critical( tag, format, ... ) do { char tstr[32]; printf( "\033[1;36m%s\033[1;39m\033[45m | CRITI | %s | %s:%d | "format"\033[0m\n", time_get_current_date_string_r(tstr, sizeof(tstr)), tag, __FUNCTION__, __LINE__, ##__VA_ARGS__); } while(0)
#  define trace( tag, format, ... ) do { char tstr[32]; printf( "\033[1;36m%s\033[1;39m\033[1;34m | TRACE | %s | %s:%d | "format"\033[0m\n", time_get_current_date_string_r(tstr, sizeof(tstr)), tag, __FUNCTION__, __LINE__, ##__VA_ARGS__); } while(0)
#  define info( tag, format, ... ) do { char tstr[32]; printf( "\033[1;36m%s\033[1;39m\033[1;33m | INFO  | %s | %s:%d | "format"\033[0m\n", time_get_current_date_string_r(tstr, sizeof(tstr)), tag, __FUNCTION__, __LINE__, ##__VA_ARGS__); } while(0)
#  define debug( tag, format, ... ) do { char tstr[32]; printf( "\033[1;36m%s\033[1;39m\033[1;32m | DEBUG | %s | %s:%d | "format"\033[0m\n", time_get_current_date_string_r(tstr, sizeof(tstr)), tag, __FUNCTION__, __LINE__, ##__VA_ARGS__); } while(0)
#endif

#define GREEN(x) "\033[0;32m" x "\033[0;39m"

#ifdef __cplusplus
}
#endif

#endif
