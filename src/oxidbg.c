#include "oxiassert.h"

#include <tchar.h>
#include <wchar.h>

#include <windows.h>

FILE *logFile;
const TCHAR *whitespace = _TEXT(" \f\n\r\t\v");

int main() {
  logFile = stderr;
  TCHAR *processCmdLine = _tcsdup(GetCommandLine());

  TCHAR *space = processCmdLine + _tcscspn(processCmdLine, whitespace);
  OXIAssert(space);
  space = _tcsspnp(space, whitespace);
  OXIAssert(space);

  OXILog("cmdLine: \'%ls\'\n", space);

  STARTUPINFO startupInfo = {.cb = sizeof(STARTUPINFO)};
  PROCESS_INFORMATION processInformation = {0};

  OXIAssert(CreateProcess(0, space, 0, 0, false,
                          DEBUG_ONLY_THIS_PROCESS | CREATE_NEW_CONSOLE, 0, 0,
                          &startupInfo, &processInformation));
  while (true) {
    DEBUG_EVENT debugEvent;
    OXIAssert(WaitForDebugEventEx(&debugEvent, INFINITE));
    switch (debugEvent.dwDebugEventCode) {
    case CREATE_PROCESS_DEBUG_EVENT: {
      OXILog("CREATE_PROCESS_DEBUG_EVENT\n");
    } break;
    case CREATE_THREAD_DEBUG_EVENT: {
      OXILog("CREATE_THREAD_DEBUG_EVENT\n");
    } break;
    case EXCEPTION_DEBUG_EVENT: {
      OXILog("EXCEPTION_DEBUG_EVENT\n");
    } break;
    case EXIT_THREAD_DEBUG_EVENT: {
      OXILog("EXIT_THREAD_DEBUG_EVENT\n");
    } break;
    case LOAD_DLL_DEBUG_EVENT: {
      OXILog("LOAD_DLL_DEBUG_EVENT\n");
    } break;
    case OUTPUT_DEBUG_STRING_EVENT: {
      OXILog("OUTPUT_DEBUG_STRING_EVENT\n");
    } break;
    case RIP_EVENT: {
      OXILog("RIP_EVENT\n");
    } break;
    case UNLOAD_DLL_DEBUG_EVENT: {
      OXILog("UNLOAD_DLL_DEBUG_EVENT\n");
    } break;
    case EXIT_PROCESS_DEBUG_EVENT: {
      OXILog("EXIT_PROCESS_DEBUG_EVENT\n");
      exit(0);
    } break;
    }
    ContinueDebugEvent(debugEvent.dwProcessId, debugEvent.dwThreadId,
                       DBG_EXCEPTION_NOT_HANDLED);
  }
  return 0;
}