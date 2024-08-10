#include "oxidec.h"
#include "oxiassert.h"
#include "xed/xed-interface.h"

#include <stdio.h>
#include <windows.h>
#include <stdarg.h>
#define TBUFSZ 90

bool OXIsnprintf(char* buf, u32 szbuf, u32 *pszWrittenNonNull, const char* fmt, ...) {
  u32 szWrittenNonNull = *pszWrittenNonNull;
  OXIAssert(szWrittenNonNull + 1 <= szbuf);
  if (szWrittenNonNull + 1 < szbuf) {
    va_list ap;
    va_start(ap, fmt);
    int res = vsnprintf(buf + szWrittenNonNull, szbuf - szWrittenNonNull, fmt, ap);
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

// https://intelxed.github.io/ref-manual/group__SMALLEXAMPLES.html
void decodeInstruction(u8 *itext, u32 bytes, char *out, u32 szout) {
  OXIAssert(itext && bytes && out && szout);
  memset(out, 0, szout);

  xed_state_t dstate;
  xed_error_enum_t xed_error;
  xed_state_zero(&dstate);
  dstate.mmode = XED_MACHINE_MODE_LONG_64;
  dstate.stack_addr_width = XED_ADDRESS_WIDTH_64b;

  u32 szWrittenNonNull = 0;
  while (true) {
    xed_decoded_inst_t xedd;
    xed_decoded_inst_zero_set_mode(&xedd, &dstate);
    xed_decoded_inst_set_input_chip(&xedd, XED_CHIP_TIGER_LAKE);
    xed_error = xed_decode(&xedd, XED_REINTERPRET_CAST(const xed_uint8_t *, itext), bytes);

    if (xed_error != XED_ERROR_NONE)
      break;
    u32 decodedsz = xed_decoded_inst_get_length(&xedd);

    char buf[64] = {0};
    xed_print_info_t printInfo;
    xed_init_print_info(&printInfo);
    printInfo.blen = sizeof(buf);
    printInfo.buf = buf;
    printInfo.p = &xedd;
    xed_format_generic(&printInfo);

    char ibuff[64];
    u32 ibuffszWrittenNonNull = 0;
    OXIsnprintf(ibuff, sizeof(ibuff), &ibuffszWrittenNonNull, "%*c", 30 - 2 * decodedsz, ' ');
    for (u32 i = 0; i < decodedsz; ++i) {
      bool ok = OXIsnprintf(ibuff, sizeof(ibuff), &ibuffszWrittenNonNull, "%02x", itext[i]);
      if (!ok)
        return;
    }

    bool ok = OXIsnprintf(ibuff, sizeof(ibuff), &ibuffszWrittenNonNull, " %s\n", buf);
    if (!ok)
      return;

    size_t ibufflen = strlen(ibuff);
    if (szWrittenNonNull + 1 + ibufflen < szout) {
      OXIsnprintf(out, szout, &szWrittenNonNull, "%s", ibuff);
    } else return;

    itext += decodedsz;
    bytes -= decodedsz;
  }

  return;
}
