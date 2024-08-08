#include "oxidec.h"
#include "oxiassert.h"
#include "xed/xed-interface.h"

#include <stdio.h>
#include <windows.h>

// https://intelxed.github.io/ref-manual/group__SMALLEXAMPLES.html
void decodeInstruction(u8* buff, size_t nbuff, char* out, size_t nout) {
  xed_state_t dstate;
  xed_decoded_inst_t xedd;
  xed_error_enum_t xed_error;

  xed_state_zero(&dstate);
  dstate.mmode = XED_MACHINE_MODE_LONG_64;
  dstate.stack_addr_width = XED_ADDRESS_WIDTH_64b;
  xed_decoded_inst_zero_set_mode(&xedd, &dstate);
  xed_decoded_inst_set_input_chip(&xedd, XED_CHIP_INVALID);

  xed_error = xed_decode(&xedd, XED_REINTERPRET_CAST(const xed_uint8_t*,buff), nbuff);
  if (xed_error != XED_ERROR_NONE) {
    xed_uint_t dec_length = xed_decoded_inst_get_length(&xedd);
    OXILog("ERROR DECODING: %d\n", xed_error);
    return;
  }
    
  snprintf(out, nout, "%s", xed_iclass_enum_t2str(xed_decoded_inst_get_iclass(&xedd)));
}
