#pragma once
#include "oxiassert.h"
#include "oxiimgui.h"
#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif
inline void *adv(void *base, u64 rva) { return (void *)((char *)base + rva); }

void decodeInstruction(u8 *itext, u32 bytes, UIDataAsmLine *out, u64 nOut,
                       u64 rip, OXIPEMODULE *dll, u32 nDll, HANDLE process);
bool sourceMe(u64 addr, char *out, u64 szOut, OXIPEMODULE *dll, u32 nDll,
              bool truncate);
bool unwindContext(CONTEXT *inCtx, OXIPEMODULE *dll, u32 nDll, HANDLE process);

#ifdef __cplusplus
}
#endif
