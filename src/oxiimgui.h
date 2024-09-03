#pragma once
#include <d3d12.h>
#include <dxgi1_4.h>
#include <windows.h>
#include "oxiassert.h"

typedef struct FrameContext {
  ID3D12CommandAllocator *CommandAllocator;
  UINT64 FenceValue;
} FrameContext;

enum OXIDbgCommand {
  OXIDbgCommand_None,
  OXIDbgCommand_Go,
  OXIDbgCommand_StepInto
};

typedef struct OXISymbol {
  u64 addr;
  u64 ordinal;
  char name[128];
} OXISymbol;

typedef struct OXIPEMODULE {
  void *base;
  IMAGE_NT_HEADERS ntHeader;

  TCHAR moduleNameByHandle[256];

  // IMAGE_DIRECTORY_ENTRY_EXPORT
  char exportDirectoryDLLName[128]; // couldn't find size for this :(
  OXISymbol *aSymbols;
  u32 nSymbols;

  // IMAGE_DIRECTORY_ENTRY_EXCEPTION
  RUNTIME_FUNCTION functions[4096 * 2];
  u32 nFunctions;
} OXIPEMODULE;

typedef struct UIDataAsmLine {
  u64 addr;
  u8 itext[15];
  char decoded[64];
  char source[128];
  bool functionBeg;
  bool functionEnd;
} UIDataAsmLine;

typedef struct OXIBreakpoint {
  u64 addr;
} OXIBreakpoint;

typedef struct UIData {
  HANDLE process;
  CONTEXT ctx;
  enum OXIDbgCommand commandEntered;
  CONDITION_VARIABLE condition_variable;
  CRITICAL_SECTION critical_section;

  OXIPEMODULE dll[64];
  u32 nDll;

  // binary instructions frame
  u8 itext[256];

  char reason[64];

  // breakpoints
  OXIBreakpoint breakpoints[16];
  u32 nBreakpoints;

  // log
  char log[16][256];
  u64 nLog;

  // threads
  CREATE_THREAD_DEBUG_INFO threads[8];
  i32 nThreads;

  DWORD issueThread;

  // stack (must be per thread actually)
  u64 callstack[16];
  u32 nCallstack;
} UIData;

#ifdef __cplusplus
extern "C" {
#endif
void OXIImGuiInit(HWND hwnd, ID3D12Device *device, int num_frames_in_flight,
                  DXGI_FORMAT rtv_format, ID3D12DescriptorHeap *cbv_srv_heap,
                  D3D12_CPU_DESCRIPTOR_HANDLE font_srv_cpu_desc_handle,
                  D3D12_GPU_DESCRIPTOR_HANDLE font_srv_gpu_desc_handle);
void OXIImGuiBegFrame(UIData *data);
void OXIImGuiEndFrame();
bool OXIImGuiWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

#ifdef __cplusplus
}
#endif