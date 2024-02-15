#ifndef __OCLEA_LOG_H__
#define __OCLEA_LOG_H__

#include <cstring>
#include <iostream>
#include <unistd.h>
#include <sys/syscall.h>

#define OCLEA_LOG_LEVEL_CRITICAL 0
#define OCLEA_LOG_LEVEL_ERROR    1
#define OCLEA_LOG_LEVEL_WARNING  2
#define OCLEA_LOG_LEVEL_NOTICE   3
#define OCLEA_LOG_LEVEL_INFO     4
#define OCLEA_LOG_LEVEL_DEBUG    5
#define OCLEA_LOG_LEVEL_TRACE    6


static inline const char* strip_file_(const char* file) {
    const char* str = strrchr(file, '/');
    return str ? str + 1 : file;
}

static const struct {
    const char* name;
    const char* prio;
} log_level_strings[] = {
    [OCLEA_LOG_LEVEL_CRITICAL] = {"CRITICAL", "<2>"},
    [OCLEA_LOG_LEVEL_ERROR]    = {"ERROR",    "<3>"},
    [OCLEA_LOG_LEVEL_WARNING]  = {"WARN",     "<4>"},
    [OCLEA_LOG_LEVEL_NOTICE]   = {"NOTICE",   "<5>"},
    [OCLEA_LOG_LEVEL_INFO]     = {"INFO",     "<6>"},
    [OCLEA_LOG_LEVEL_DEBUG]    = {"DEBUG",    "<7>"},
    [OCLEA_LOG_LEVEL_TRACE]    = {"TRACE",    "<7>"}
};


#ifndef OCLEA_LOG_BUILD_LEVEL
#define OCLEA_LOG_BUILD_LEVEL OCLEA_LOG_LEVEL_INFO
#endif

#define OCLEA_LOG_CHECK_LEVEL(level) (level <= OCLEA_LOG_BUILD_LEVEL)

#define OCLEA_LOG(level, fmt, ...) do { \
    if (OCLEA_LOG_CHECK_LEVEL(level)) { \
        fprintf(stdout, "%s[%s](tid=%d) " fmt "\n", log_level_strings[level].prio, log_level_strings[level].name, syscall(SYS_gettid), ##__VA_ARGS__); \
        fflush(stdout); \
    } \
} while(0)

#define OCLEA_LOG_WITH_TRACE(level, fmt, ...) do { \
    if (OCLEA_LOG_CHECK_LEVEL(level)) { \
        fprintf(stdout, "%s[%s](tid=%d) %s:%s:%d: " fmt "\n", log_level_strings[level].prio, log_level_strings[level].name, syscall(SYS_gettid), strip_file_(__FILE__), __FUNCTION__,__LINE__, ##__VA_ARGS__); \
        fflush(stdout); \
    } \
} while(0)

#define OCLEA_LOG_STREAM(level, args) do { \
    if (OCLEA_LOG_CHECK_LEVEL(level)) { \
        std::cout << log_level_strings[level].prio << "[" << log_level_strings[level].name << "](tid=" << syscall(SYS_gettid) << ") " \
                  << args \
                  << std::endl; \
    } \
} while(0)

#define OCLEA_LOG_STREAM_WITH_TRACE(level, args) do { \
    if (OCLEA_LOG_CHECK_LEVEL(level)) { \
        std::cout << log_level_strings[level].prio << "[" << log_level_strings[level].name << "](tid=" << syscall(SYS_gettid) << ") " \
                  << strip_file_(__FILE__) << ":" << __FUNCTION__ << ":" << __LINE__ << ": " \
                  << args \
                  << std::endl; \
    } \
} while(0)


#define OLOG_OFF(fmt, ... )        {}
#define OLOG_CRITICAL(fmt, ... )   OCLEA_LOG(OCLEA_LOG_LEVEL_CRITICAL, fmt, ##__VA_ARGS__)
#define OLOG_ERROR(fmt, ... )      OCLEA_LOG(OCLEA_LOG_LEVEL_ERROR, fmt, ##__VA_ARGS__)
#define OLOG_WARN(fmt, ... )       OCLEA_LOG(OCLEA_LOG_LEVEL_WARNING, fmt, ##__VA_ARGS__)
#define OLOG_NOTICE(fmt,... )      OCLEA_LOG(OCLEA_LOG_LEVEL_NOTICE, fmt, ##__VA_ARGS__)
#define OLOG_INFO(fmt, ... )       OCLEA_LOG(OCLEA_LOG_LEVEL_INFO, fmt, ##__VA_ARGS__)
#define OLOG_DEBUG(fmt, ... )      OCLEA_LOG(OCLEA_LOG_LEVEL_DEBUG, fmt, ##__VA_ARGS__)
#define OLOG_TRACE(fmt, ... )      OCLEA_LOG_WITH_TRACE(OCLEA_LOG_LEVEL_TRACE, fmt, ##__VA_ARGS__)

#define OLOG_STREAM OCLEA_LOG_STREAM
#define OLOG_OFF_STREAM(args)      {}
#define OLOG_CRITICAL_STREAM(args) OCLEA_LOG_STREAM(OCLEA_LOG_LEVEL_CRITICAL, args)
#define OLOG_ERROR_STREAM(args)    OCLEA_LOG_STREAM(OCLEA_LOG_LEVEL_ERROR, args)
#define OLOG_WARN_STREAM(args)     OCLEA_LOG_STREAM(OCLEA_LOG_LEVEL_WARNING, args)
#define OLOG_NOTICE_STREAM(args)   OCLEA_LOG_STREAM(OCLEA_LOG_LEVEL_NOTICE, args)
#define OLOG_INFO_STREAM(args)     OCLEA_LOG_STREAM(OCLEA_LOG_LEVEL_INFO, args)
#define OLOG_DEBUG_STREAM(args)    OCLEA_LOG_STREAM(OCLEA_LOG_LEVEL_DEBUG, args)
#define OLOG_TRACE_STREAM(args)    OCLEA_LOG_STREAM_WITH_TRACE(OCLEA_LOG_LEVEL_TRACE, args)

#endif  // __OCLEA_LOG_H__
