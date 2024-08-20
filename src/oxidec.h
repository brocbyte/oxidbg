#pragma once
#include "oxiassert.h"
#include "oxiimgui.h"

#ifdef __cplusplus
extern "C" {
#endif
void decodeInstruction(u8 *itext, u32 bytes, UIDataAsmLine *out, u64 nOut,
                       u64 rip, OXIPEMODULE *dll, u32 nDll);
void sourceMe(u64 addr, char *out, u64 szOut, OXIPEMODULE *dll, u32 nDll);
#ifdef __cplusplus
}
#endif
