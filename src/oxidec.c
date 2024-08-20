#include "oxidec.h"
#include "oxiassert.h"
#include "xed/xed-interface.h"

#include <stdio.h>
#include <windows.h>

void sourceMe(u64 addr, char *out, u64 szOut, OXIPEMODULE *dll, u32 nDll) {
  *out = 0;
  u64 mdiff = 1000000000LL;
  for (u32 i = 0; i < nDll; ++i) {
    for (u32 j = 0; j < dll[i].nSymbols; ++j) {
      u64 candidate = dll[i].aSymbols[j].addr;
      if (candidate <= addr && (addr - candidate < mdiff)) {
        mdiff = addr - candidate;
        if (mdiff) {
          snprintf(out, szOut, "%ls!%s+%#llx", dll[i].dllName,
                   dll[i].aSymbols[j].name, mdiff);
        } else {
          snprintf(out, szOut, "%ls!%s", dll[i].dllName,
                   dll[i].aSymbols[j].name);
        }
      }
    }
  }
}

// https://intelxed.github.io/ref-manual/group__SMALLEXAMPLES.html
void decodeInstruction(u8 *itext, u32 bytes, UIDataAsmLine *out, u64 nOut,
                       u64 rip, OXIPEMODULE *dll, u32 nDll) {
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

    sourceMe(line->addr, line->source, sizeof(line->source), dll, nDll);

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
    xed_format_generic(&printInfo);

    char instructionBytes[64];
    u32 ibuffszWrittenNonNull = 0;
    for (u32 i = 0; i < decodedsz; ++i) {
      bool ok = OXIsnprintf(instructionBytes, sizeof(instructionBytes),
                            &ibuffszWrittenNonNull, "%02x", itext[i]);
      if (!ok)
        break;
    }
    snprintf(line->itext, _countof(line->itext), "%s", instructionBytes);

    itext += decodedsz;
    bytes -= decodedsz;
    addr += decodedsz;
  }

  return;
}
