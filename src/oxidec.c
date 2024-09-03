#include "oxidec.h"
#include "oxiassert.h"
#include "xed/xed-interface.h"

#include <stdio.h>
#include <windows.h>
#include <tchar.h>

#include <windows.h>
#include <strsafe.h>

typedef union _UNWIND_CODE {
  struct {
    u8 CodeOffset;
    u8 UnwindOp : 4;
    u8 OpInfo : 4;
  };
  u16 FrameOffset;
} UNWIND_CODE, *PUNWIND_CODE;

typedef struct _UNWIND_INFO {
  u8 Version : 3;
  u8 Flags : 5;
  u8 SizeOfProlog;
  u8 CountOfCodes;
  u8 FrameRegister : 4;
  u8 FrameOffset : 4;
  UNWIND_CODE UnwindCode[1];
  /*  UNWIND_CODE MoreUnwindCode[((CountOfCodes + 1) & ~1) - 1];
   *   union {
   *       OPTIONAL ULONG ExceptionHandler;
   *       OPTIONAL ULONG FunctionEntry;
   *   };
   *   OPTIONAL ULONG ExceptionData[]; */
} UNWIND_INFO, *PUNWIND_INFO;

u64 *opInfoToRegisterAddr(CONTEXT *ctx, u8 opinfo) {
  switch (opinfo) {
  case 0:
    return &ctx->Rax;
  case 1:
    return &ctx->Rcx;
  case 2:
    return &ctx->Rdx;
  case 3:
    return &ctx->Rbx;
  case 4:
    return &ctx->Rsp;
  case 5:
    return &ctx->Rbp;
  case 6:
    return &ctx->Rsi;
  case 7:
    return &ctx->Rdi;
  case 8:
    return &ctx->R8;
  case 9:
    return &ctx->R9;
  case 10:
    return &ctx->R10;
  case 11:
    return &ctx->R11;
  case 12:
    return &ctx->R12;
  case 13:
    return &ctx->R13;
  case 14:
    return &ctx->R14;
  case 15:
    return &ctx->R15;
  }
  OXIAssert(false);
}

bool unwindContext(CONTEXT *inCtx, OXIPEMODULE *dll, u32 nDll, HANDLE process) {
  for (u32 i = 0; i < nDll; ++i) {
    OXIPEMODULE *m = &dll[i];
    OXILog("functions %ls\n", m->moduleNameByHandle);
    u64 mBeg = (u64)m->base + m->ntHeader.OptionalHeader.BaseOfCode;
    u64 mEnd = mBeg + m->ntHeader.OptionalHeader.SizeOfCode;
    if (inCtx->Rip >= mBeg && inCtx->Rip < mEnd) {
      for (u32 j = 0; j < m->nFunctions; ++j) {
        RUNTIME_FUNCTION *f = &m->functions[j];
        u64 functionBeg = (u64)m->base + f->BeginAddress;
        u64 functionEnd = (u64)m->base + f->EndAddress;
        if (inCtx->Rip >= functionBeg && inCtx->Rip < functionEnd) {
          void *pDebugProcessUnwindInfoAddress =
              adv(m->base, f->UnwindInfoAddress);
          UNWIND_INFO unwindInfo;
          OXIAssert(ReadProcessMemory(process, pDebugProcessUnwindInfoAddress,
                                      &unwindInfo, sizeof(unwindInfo), 0));
          void *pDebugProcessUnwindInfoCodes =
              adv(pDebugProcessUnwindInfoAddress, 4);
          UNWIND_CODE codes[32];
          OXIAssert(ReadProcessMemory(
              process, pDebugProcessUnwindInfoCodes, codes,
              __min(sizeof(codes), unwindInfo.CountOfCodes * sizeof(codes[0])),
              0));
          OXILog("%x %x %x v: %u f: %u szp: %x coc: %x codes: %x\n",
                 f->BeginAddress, f->EndAddress, f->UnwindInfoAddress,
                 unwindInfo.Version, unwindInfo.Flags, unwindInfo.SizeOfProlog,
                 unwindInfo.CountOfCodes, codes[0].CodeOffset);

          for (int i = 0; i < unwindInfo.CountOfCodes; ++i) {
            if (inCtx->Rip < functionBeg + codes[i].CodeOffset)
              continue;
            switch (codes[i].UnwindOp) {
            case 0: { /*UWOP_PUSH_NONVOL*/
              void *srcAddress = (void *)inCtx->Rsp;
              u64 *pRegister = opInfoToRegisterAddr(inCtx, codes[i].OpInfo);
              OXIAssert(
                  ReadProcessMemory(process, srcAddress, pRegister, 8, 0));
              inCtx->Rsp += 8;
            } break;
            case 1: { /*UWOP_ALLOC_LARGE*/
              if (codes[i].OpInfo == 0) {
                OXIAssert(i + 1 < unwindInfo.CountOfCodes);
                inCtx->Rsp += *(u16 *)(&codes[i + 1]) * 8;
                ++i;
              } else {
                OXIAssert(i + 2 < unwindInfo.CountOfCodes);
                inCtx->Rsp += *(u32 *)(&codes[i + 1]);
                i += 2;
              }
            } break;
            case 2: { /*UWOP_ALLOC_SMALL*/
              inCtx->Rsp += codes[i].OpInfo * 8 + 8;
            } break;
            case 3: { /*UWOP_SET_FPREG*/
              inCtx->Rsp =
                  *opInfoToRegisterAddr(inCtx, unwindInfo.FrameRegister) -
                  unwindInfo.FrameOffset;
            } break;
            case 4: { /*UWOP_SAVE_NONVOL*/
              OXIAssert(i + 1 < unwindInfo.CountOfCodes);
              void *srcAddress = (void *)(inCtx->Rsp + *(u16*)&codes[i + 1] * 8);
              u64 *pRegister = opInfoToRegisterAddr(inCtx, codes[i].OpInfo);
              OXIAssert(ReadProcessMemory(process, srcAddress, pRegister, 8, 0));
              ++i;
            } break;
            }
          }
          OXIAssert(ReadProcessMemory(process, (void *)inCtx->Rsp, &inCtx->Rip,
                                      8, 0));
          inCtx->Rsp += 8;
          return true;
        }
      }
      // leaf function
      OXIAssert(
          ReadProcessMemory(process, (void *)inCtx->Rsp, &inCtx->Rip, 8, 0));
      inCtx->Rsp += 8;
      return true;
    }
  }
  return false;
}

static TCHAR *truncateSource(TCHAR *src) {
  TCHAR *truncatedSource = _tcsrchr(src, _TEXT('\\'));
  if (!truncatedSource) {
    truncatedSource = 0;
  } else {
    ++truncatedSource;
  }
  return truncatedSource;
}

bool sourceMe(u64 addr, char *out, u64 szOut, OXIPEMODULE *dll, u32 nDll,
              bool truncate) {
  *out = 0;
  u64 mdiff = 1000000000LL;
  for (u32 i = 0; i < nDll; ++i) {
    TCHAR *dllFileName = dll[i].moduleNameByHandle;
    if (truncate) {
      dllFileName = truncateSource(dllFileName);
    }
    for (u32 j = 0; j < dll[i].nSymbols; ++j) {
      u64 candidate = dll[i].aSymbols[j].addr;
      if (candidate <= addr && (addr - candidate < mdiff)) {
        mdiff = addr - candidate;
        if (mdiff) {
          snprintf(out, szOut, "%ls!%s+%#llx", dllFileName,
                   dll[i].aSymbols[j].name, mdiff);
        } else {
          snprintf(out, szOut, "%ls!%s", dllFileName, dll[i].aSymbols[j].name);
        }
      }
    }
  }
  return mdiff != 1000000000LL;
}

bool isFunctionEnd(u64 addr, OXIPEMODULE *dll, u32 nDll) {
  for (u32 i = 0; i < nDll; ++i) {
    for (u32 j = 0; j < dll[i].nFunctions; ++j) {
      u64 pastEndAddress = dll[i].functions[j].EndAddress + (u64)dll[i].base;
      if (pastEndAddress == addr) {
        return true;
      }
    }
  }
  return false;
}

// https://intelxed.github.io/ref-manual/group__SMALLEXAMPLES.html
void decodeInstruction(u8 *itext, u32 bytes, UIDataAsmLine *out, u64 nOut,
                       u64 rip, OXIPEMODULE *dll, u32 nDll, HANDLE process) {
  xed_state_t dstate;
  xed_error_enum_t xed_error;
  xed_state_zero(&dstate);
  dstate.mmode = XED_MACHINE_MODE_LONG_64;
  dstate.stack_addr_width = XED_ADDRESS_WIDTH_64b;

  u32 szWrittenNonNull = 0;
  u32 nDecoded = 0;
  u64 addr = rip;
  while (nDecoded < nOut) {
    UIDataAsmLine *line = &out[nDecoded++];
    line->addr = addr;

    sourceMe(line->addr, line->source, sizeof(line->source), dll, nDll, true);

    xed_decoded_inst_t xedd;
    xed_decoded_inst_zero_set_mode(&xedd, &dstate);
    xed_decoded_inst_set_input_chip(&xedd, XED_CHIP_TIGER_LAKE);
    xed_error = xed_decode(
        &xedd, XED_REINTERPRET_CAST(const xed_uint8_t *, itext), bytes);

    if (xed_error != XED_ERROR_NONE)
      break;
    u32 decodedsz = xed_decoded_inst_get_length(&xedd);

    xed_print_info_t printInfo;
    xed_init_print_info(&printInfo);
    printInfo.blen = sizeof(line->decoded);
    printInfo.buf = line->decoded;
    printInfo.p = &xedd;
    printInfo.runtime_address = line->addr;
    xed_format_generic(&printInfo);

    char instructionBytes[64];
    u32 ibuffszWrittenNonNull = 0;
    for (u32 i = 0; i < decodedsz; ++i) {
      bool ok = OXIsnprintf(instructionBytes, sizeof(instructionBytes),
                            &ibuffszWrittenNonNull, "%02x", itext[i]);
      if (!ok)
        break;
    }

    u64 instructionEndPastOne = line->addr + decodedsz;
    line->functionEnd = isFunctionEnd(instructionEndPastOne, dll, nDll);

    xed_category_enum_t cat = xed_decoded_inst_get_category(&xedd);
    u64 callAddr = 0;
    char *ins = "";
    if (cat == XED_CATEGORY_CALL || cat == XED_CATEGORY_COND_BR ||
        XED_CATEGORY_UNCOND_BR) {
      char *prefixes[] = {"call ", "jz ", "jnz ", "jmp "};
      for (int i = 0; i < _countof(prefixes); ++i) {
        if (strstr(line->decoded, prefixes[i]) == line->decoded) {
          char *arg = line->decoded + strlen(prefixes[i]);
          if (strstr(arg, "0x") == arg) {
            callAddr = strtoll(arg, 0, 0);
          } else if (strstr(arg, "qword ptr [rip+0x") == arg) {
            i64 offset = strtoll(strstr(arg, "0x"), NULL, 0);
            i64 memoryAddr = line->addr + decodedsz + offset;
            size_t nread;
            OXIAssert(ReadProcessMemory(process, (void *)memoryAddr, &callAddr,
                                        8, &nread));
            OXIAssert(nread == 8);
          }
          char src[256];
          if (callAddr != 0) {
            if (sourceMe(callAddr, src, sizeof(src), dll, nDll, true)) {
              snprintf(line->decoded, _countof(line->decoded), "%s%s",
                       prefixes[i], src);
            } else {
              snprintf(line->decoded, _countof(line->decoded), "%s%p",
                       prefixes[i], (void *)callAddr);
            }
          }
        }
      }
    }

    snprintf(line->itext, _countof(line->itext), "%s", instructionBytes);

    itext += decodedsz;
    bytes -= decodedsz;
    addr += decodedsz;
  }

  return;
}
