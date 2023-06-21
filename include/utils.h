#ifndef UTILS_H_INCLUDED
#define UTILS_H_INCLUDED

// #include <stdint.h>

extern int printf(const char *fmt, ...);

#define LOG_FILE_FMT "%s:%u "
#define LOG_FILE_ARGS __FILE__, __LINE__

#if LOG_LEVEL < 60
#define LOG_ERROR(fmt, ...)                                                                                            \
	printf("\e[1;91mERROR\e[0;0m " LOG_FILE_FMT fmt "\n", LOG_FILE_ARGS __VA_OPT__(, ) __VA_ARGS__)
#else
#define LOG_ERROR(...) (void)0
#endif

#if LOG_LEVEL < 50
#define LOG_WARN(fmt, ...)                                                                                             \
	printf("\e[1;93mWARN\e[0;0m  " LOG_FILE_FMT fmt "\n", LOG_FILE_ARGS __VA_OPT__(, ) __VA_ARGS__)
#else
#define LOG_WARN(...) (void)0
#endif

#if LOG_LEVEL < 40
#define LOG_INFO(fmt, ...)                                                                                             \
	printf("\e[1;92mINFO\e[0;0m  " LOG_FILE_FMT fmt "\n", LOG_FILE_ARGS __VA_OPT__(, ) __VA_ARGS__)
#else
#define LOG_INFO(...) (void)0
#endif

#if LOG_LEVEL < 30
#define LOG_DEBUG(fmt, ...)                                                                                            \
	printf("\e[1;94mDEBUG\e[0;0m " LOG_FILE_FMT fmt "\n", LOG_FILE_ARGS __VA_OPT__(, ) __VA_ARGS__)
#else
#define LOG_DEBUG(...) (void)0
#endif

void wait_itm_available();

// uint32_t rand();
//
// extern uint32_t rand_seed;

#endif
