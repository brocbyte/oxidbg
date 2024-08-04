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
      OXILog("LOAD_DLL_DEBUG_EVENT: ");
      LOAD_DLL_DEBUG_INFO info = debugEvent.u.LoadDll;
      TCHAR *buff = malloc(256 * sizeof(TCHAR));
      GetFinalPathNameByHandle(info.hFile, buff, 256, FILE_NAME_NORMALIZED);
      OXILog("\'%ls\'\n", buff);

    } break;
    case OUTPUT_DEBUG_STRING_EVENT: {
      OXILog("OUTPUT_DEBUG_STRING_EVENT: ");
      OUTPUT_DEBUG_STRING_INFO info = debugEvent.u.DebugString;
      void *buff = malloc(info.nDebugStringLength);
      memset(buff, 0, info.nDebugStringLength);
      if (info.fUnicode) {
        size_t written = 0;
        OXIAssert(ReadProcessMemory(processInformation.hProcess,
                                    info.lpDebugStringData, buff,
                                    info.nDebugStringLength, &written));
        if (*(wchar_t *)buff) {
          OXILog("\'%ls\'\n", (wchar_t *)buff);
        }
      } else {
        OXILog("\'%s\'\n", (char *)buff);
      }
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