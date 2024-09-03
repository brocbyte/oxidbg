#pragma once
#include <stdint.h>
#include <stdbool.h>

#include <stdio.h>
#include <stdarg.h>

extern FILE *logFile;

#define OXILog(...)                                                            \
  do {                                                                         \
    if (logFile) {                                                             \
      fprintf(logFile, __VA_ARGS__);                                           \
      fflush(logFile);                                                         \
    }                                                                          \
  } while (false)

#define OXIAssertT(exp, ...)                                                   \
  do {                                                                         \
    if (!(exp)) {                                                              \
      OXILog(__VA_ARGS__);                                                     \
      exit(-1);                                                                \
    }                                                                          \
  } while (false)

#define OXIAssert(exp) OXIAssertT(exp, "%s:%d", __FILE__, __LINE__)

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;
typedef i32 b32;
typedef float f32;
typedef double f64;

inline bool OXIsnprintf(char *buf, u32 szbuf, u32 *pszWrittenNonNull,
                        const char *fmt, ...) {
  u32 szWrittenNonNull = *pszWrittenNonNull;
  if (szWrittenNonNull + 1 < szbuf) {
    va_list ap;
    va_start(ap, fmt);
    int res =
        vsnprintf(buf + szWrittenNonNull, szbuf - szWrittenNonNull, fmt, ap);
    va_end(ap);
    if (res <= 0 || res > (i32)(szbuf - szWrittenNonNull - 1)) {
      *pszWrittenNonNull = szbuf - 1;
      return false;
    }
    *pszWrittenNonNull += res;
    return true;
  }
  return false;
}



