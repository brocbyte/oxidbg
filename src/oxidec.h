#pragma once
#include "oxiassert.h"

#ifdef __cplusplus
extern "C" {
#endif
  void decodeInstruction(u8 *itext, u32 bytes, char *buff, u32 blen);
#ifdef __cplusplus
}
#endif

