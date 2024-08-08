#pragma once
#include "oxiassert.h"

#ifdef __cplusplus
extern "C" {
#endif
  void decodeInstruction(u8* buff, size_t nbuff, char* out, size_t nout);
#ifdef __cplusplus
}
#endif

