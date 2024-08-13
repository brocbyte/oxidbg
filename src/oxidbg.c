#include <initguid.h>
#include "oxiassert.h"
#include "oxiimgui.h"

#include <tchar.h>
#include <wchar.h>

#include <windows.h>
#include <process.h>

#include <d3d12.h>
#include <dxgi1_4.h>

#ifdef _DEBUG
#define DX12_ENABLE_DEBUG_LAYER
#endif

#ifdef DX12_ENABLE_DEBUG_LAYER
#include <dxgidebug.h>
#pragma comment(lib, "dxguid.lib")
#endif

#include "xed/xed-interface.h"

// Data
#define NUM_FRAMES_IN_FLIGHT 3
FrameContext g_frameContext[NUM_FRAMES_IN_FLIGHT] = {0};
UINT g_frameIndex = 0;

#define NUM_BACK_BUFFERS 3
ID3D12Device *g_pd3dDevice = 0;
ID3D12DescriptorHeap *g_pd3dRtvDescHeap = 0;
ID3D12DescriptorHeap *g_pd3dSrvDescHeap = 0;
ID3D12CommandQueue *g_pd3dCommandQueue = 0;
ID3D12GraphicsCommandList *g_pd3dCommandList = 0;
ID3D12Fence *g_fence = 0;
HANDLE g_fenceEvent = 0;
UINT64 g_fenceLastSignaledValue = 0;
IDXGISwapChain3 *g_pSwapChain = 0;
bool g_SwapChainOccluded = false;
HANDLE g_hSwapChainWaitableObject = 0;
ID3D12Resource *g_mainRenderTargetResource[NUM_BACK_BUFFERS] = {0};
D3D12_CPU_DESCRIPTOR_HANDLE g_mainRenderTargetDescriptor[NUM_BACK_BUFFERS] = {
    0};

// Forward declarations of helper functions
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
void WaitForLastSubmittedFrame();
FrameContext *WaitForNextFrameResources();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

FILE *logFile = 0;
const TCHAR *whitespace = _TEXT(" \f\n\r\t\v");

static void *adv(void *base, u64 rva) { return (void *)((char *)base + rva); }

void dbgThread(void *param) {
  UIData *pData = (UIData *)param;

  TCHAR *processCmdLine = _tcsdup(GetCommandLine());

  TCHAR *space = processCmdLine + _tcscspn(processCmdLine, whitespace);
  OXIAssert(space);
  space = _tcsspnp(space, whitespace);
  OXIAssert(space);

  OXILog("cmdLine: \'%ls\'\n", space);

  STARTUPINFO startupInfo = {.cb = sizeof(STARTUPINFO)};
  PROCESS_INFORMATION process = {0};

  OXIAssert(CreateProcess(0, space, 0, 0, false,
                          DEBUG_ONLY_THIS_PROCESS | CREATE_NEW_CONSOLE, 0, 0,
                          &startupInfo, &process));

  while (true) {
    DEBUG_EVENT debugEvent;
    OXIAssert(WaitForDebugEventEx(&debugEvent, INFINITE));
    EnterCriticalSection(&pData->critical_section);

    DWORD continueStatus = DBG_EXCEPTION_NOT_HANDLED;

    switch (debugEvent.dwDebugEventCode) {
    case CREATE_PROCESS_DEBUG_EVENT: {
      pData->reason = _TEXT("CREATE_PROCESS_DEBUG_EVENT");
    } break;
    case CREATE_THREAD_DEBUG_EVENT: {
      pData->reason = _TEXT("CREATE_THREAD_DEBUG_EVENT");
    } break;
    case EXCEPTION_DEBUG_EVENT: {
      pData->reason = _TEXT("EXCEPTION_DEBUG_EVENT");
      if (debugEvent.u.Exception.ExceptionRecord.ExceptionCode ==
          EXCEPTION_SINGLE_STEP) {
        continueStatus = DBG_CONTINUE;
      }
    } break;
    case EXIT_THREAD_DEBUG_EVENT: {
      pData->reason = _TEXT("EXIT_THREAD_DEBUG_EVENT");
    } break;
    case LOAD_DLL_DEBUG_EVENT: {
      pData->reason = _TEXT("LOAD_DLL_DEBUG_EVENT");
      LOAD_DLL_DEBUG_INFO info = debugEvent.u.LoadDll;

      if (pData->nDll == _countof(pData->dll)) {
        OXILog("Too many modules!\n");
        break;
      }
      OXIPEMODULE *module = pData->dll + pData->nDll;
      pData->nDll++;

      GetFinalPathNameByHandle(info.hFile, module->dllName,
                               _countof(module->dllName), FILE_NAME_NORMALIZED);

      void *pDebugProcessDOSHeader = info.lpBaseOfDll;
      OXIAssert(ReadProcessMemory(process.hProcess, pDebugProcessDOSHeader,
                                  &module->dosHeader, sizeof(module->dosHeader),
                                  0));

      void *pDebugProcessNTHeader =
          adv(info.lpBaseOfDll, module->dosHeader.e_lfanew);
      OXIAssert(ReadProcessMemory(process.hProcess, pDebugProcessNTHeader,
                                  &module->ntHeader, sizeof(module->ntHeader),
                                  0));

      IMAGE_OPTIONAL_HEADER64 *pOptionalHeader =
          &module->ntHeader.OptionalHeader;
      OXIAssert(IMAGE_DIRECTORY_ENTRY_EXPORT /* max used directory_entry */ <
                pOptionalHeader->NumberOfRvaAndSizes);

      // https://learn.microsoft.com/en-us/previous-versions/ms809762(v=msdn.10)?redirectedfrom=MSDN#IMAGE_EXPORT_DIRECTORY
      IMAGE_DATA_DIRECTORY exportTable =
          pOptionalHeader->DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
      IMAGE_EXPORT_DIRECTORY exportDirectory;
      void *pDebugProcessExportDirectoryBase =
          adv(info.lpBaseOfDll, exportTable.VirtualAddress);
      OXIAssert(
          ReadProcessMemory(process.hProcess, pDebugProcessExportDirectoryBase,
                            &exportDirectory, sizeof(exportDirectory), 0));

      char exportDirectoryDLLName[128] = {0}; // couldn't find size for this :(
      void *pDebugProcessDLLName = adv(info.lpBaseOfDll, exportDirectory.Name);
      OXIAssert(ReadProcessMemory(process.hProcess, pDebugProcessDLLName,
                                  exportDirectoryDLLName,
                                  sizeof(exportDirectoryDLLName), 0));
      OXILog("exportDirectoryDLLName: %s %p\n", exportDirectoryDLLName,
             info.lpBaseOfDll);

      void *pDebugProcessExportAddresses =
          adv(info.lpBaseOfDll, exportDirectory.AddressOfFunctions);
      u64 szExportAddresses = sizeof(u32) * exportDirectory.NumberOfFunctions;
      u32 *pExportAddresses = malloc(szExportAddresses);
      OXIAssert(ReadProcessMemory(process.hProcess,
                                  pDebugProcessExportAddresses,
                                  pExportAddresses, szExportAddresses, 0));

      void *pDebugProcessExportNames =
          adv(info.lpBaseOfDll, exportDirectory.AddressOfNames);
      u64 szExportNames = sizeof(u32) * exportDirectory.NumberOfNames;
      u32 *pExportNames = malloc(szExportNames);
      OXIAssert(ReadProcessMemory(process.hProcess, pDebugProcessExportNames,
                                  pExportNames, szExportNames, 0));

      void *pDebugProcessNameOrdinals =
          adv(info.lpBaseOfDll, exportDirectory.AddressOfNameOrdinals);
      u64 szNameOrdinals = sizeof(u16) * exportDirectory.NumberOfNames;
      u16 *pNameOrdinals = malloc(szNameOrdinals);
      OXIAssert(ReadProcessMemory(process.hProcess, pDebugProcessNameOrdinals,
                                  pNameOrdinals, szNameOrdinals, 0));

      OXILog("nNames: %d, nFunctions: %d\n", exportDirectory.NumberOfNames,
             exportDirectory.NumberOfFunctions);
      module->nSymbols = exportDirectory.NumberOfFunctions;
      module->aSymbols = malloc(sizeof(module->aSymbols[0]) * module->nSymbols);

      for (int i = 0; i < module->nSymbols; ++i) {
        module->aSymbols[i].addr =
            (u64)adv(info.lpBaseOfDll, pExportAddresses[i]);

        u32 ordinal = exportDirectory.Base + i;

        bool foundName = false;
        char symname[sizeof(module->aSymbols[0].name)];

        for (int j = 0; j < exportDirectory.NumberOfNames; ++j) {
          if (pNameOrdinals[j] == i) {
            void *pSymbolName = adv(info.lpBaseOfDll, pExportNames[j]);
            OXIAssert(ReadProcessMemory(process.hProcess, pSymbolName, symname,
                                        sizeof(symname), 0));
            foundName = true;
          }
        }

        if (foundName) {
          snprintf(module->aSymbols[i].name, sizeof(module->aSymbols[i].name),
                   "%s", symname);
        } else {
          snprintf(module->aSymbols[i].name, sizeof(module->aSymbols[i].name),
                   "#%u", ordinal);
        }
        OXILog("%s %x\n", module->aSymbols[i].name, pExportAddresses[i]);
      }

    } break;
    case OUTPUT_DEBUG_STRING_EVENT: {
      pData->reason = _TEXT("OUTPUT_DEBUG_STRING_EVENT");
      OUTPUT_DEBUG_STRING_INFO info = debugEvent.u.DebugString;
      void *buff = malloc(info.nDebugStringLength);
      memset(buff, 0, info.nDebugStringLength);
      OXIAssert(ReadProcessMemory(process.hProcess, info.lpDebugStringData,
                                  buff, info.nDebugStringLength, 0));
      if (info.fUnicode) {
        if (*(wchar_t *)buff) {
          OXILog("\'%ls\'\n", (wchar_t *)buff);
        }
      } else {
        OXILog("\'%s\'\n", (char *)buff);
      }
      free(buff);
    } break;
    case RIP_EVENT: {
      pData->reason = _TEXT("RIP_EVENT");
    } break;
    case UNLOAD_DLL_DEBUG_EVENT: {
      pData->reason = _TEXT("UNLOAD_DLL_DEBUG_EVENT");
    } break;
    case EXIT_PROCESS_DEBUG_EVENT: {
      pData->reason = _TEXT("EXIT_PROCESS_DEBUG_EVENT");
    } break;
    }

    HANDLE hThread = OpenThread(THREAD_GET_CONTEXT | THREAD_SET_CONTEXT, FALSE,
                                debugEvent.dwThreadId);
    OXIAssert(hThread);
    CONTEXT threadCtx = {.ContextFlags = CONTEXT_ALL};
    OXIAssert(GetThreadContext(hThread, &threadCtx));

    // registers -> ui
    pData->ctx = threadCtx;

    // instructions -> ui
    OXIAssert(ReadProcessMemory(process.hProcess, (void *)threadCtx.Rip,
                                pData->itext, sizeof(pData->itext), 0));

    // sleep until gui gives us some command to process
    while (pData->commandEntered == OXIDbgCommand_None) {
      SleepConditionVariableCS(&pData->condition_variable,
                               &pData->critical_section, INFINITE);
    }

    // we own critical section and have some command -> process
    enum OXIDbgCommand command = pData->commandEntered;
    pData->commandEntered = OXIDbgCommand_None;
    LeaveCriticalSection(&pData->critical_section);
    WakeConditionVariable(&pData->condition_variable);

    if (command == OXIDbgCommand_StepInto) {
      threadCtx.EFlags |= 1 << 8;
    }
    OXIAssert(SetThreadContext(hThread, &threadCtx));

    ContinueDebugEvent(debugEvent.dwProcessId, debugEvent.dwThreadId,
                       continueStatus);
  }

  _endthread();
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                    PWSTR pCmdLine, int nCmdShow) {
  OXIAssert(!_tfopen_s(&logFile, _TEXT("last_run.log"), _TEXT("w")));

  xed_tables_init();

  WNDCLASSEX wc = {sizeof(wc),         CS_CLASSDC, WndProc, 0L, 0L,
                    GetModuleHandle(0), 0,          0,       0,  0,
                    _TEXT("oxidbgclass"),   0};
  RegisterClassEx(&wc);
  HWND hwnd = CreateWindow(wc.lpszClassName, _TEXT("oxidbg"),
                            WS_OVERLAPPEDWINDOW, 100, 100, 1280, 800, 0, 0,
                            wc.hInstance, 0);

  // Initialize Direct3D
  if (!CreateDeviceD3D(hwnd)) {
    CleanupDeviceD3D();
    UnregisterClassW(wc.lpszClassName, wc.hInstance);
    return -1;
  }

  // Show the window
  ShowWindow(hwnd, SW_SHOWDEFAULT);
  UpdateWindow(hwnd);
  D3D12_CPU_DESCRIPTOR_HANDLE h1;
  D3D12_GPU_DESCRIPTOR_HANDLE h2;
  g_pd3dSrvDescHeap->lpVtbl->GetCPUDescriptorHandleForHeapStart(
      g_pd3dSrvDescHeap, &h1);
  g_pd3dSrvDescHeap->lpVtbl->GetGPUDescriptorHandleForHeapStart(
      g_pd3dSrvDescHeap, &h2);
  OXIImGuiInit(hwnd, g_pd3dDevice, NUM_FRAMES_IN_FLIGHT,
               DXGI_FORMAT_R8G8B8A8_UNORM, g_pd3dSrvDescHeap, h1, h2);

  UIData uiData = {0};
  InitializeConditionVariable(&uiData.condition_variable);
  InitializeCriticalSection(&uiData.critical_section);

  _beginthread(dbgThread, // lpStartAddress
               0, &uiData);

  // Main loop
  bool done = false;
  while (!done) {
    // Poll and handle messages (inputs, window resize, etc.)
    // See the WndProc() function below for our to dispatch events to the Win32
    // backend.
    MSG msg;
    while (PeekMessage(&msg, 0, 0U, 0U, PM_REMOVE)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
      if (msg.message == WM_QUIT)
        done = true;
    }
    if (done)
      break;

    // Handle window screen locked
    if (g_SwapChainOccluded &&
        g_pSwapChain->lpVtbl->Present(g_pSwapChain, 0, DXGI_PRESENT_TEST) ==
            DXGI_STATUS_OCCLUDED) {
      Sleep(10);
      continue;
    }
    g_SwapChainOccluded = false;

    OXIImGuiBegFrame(&uiData);
    OXIImGuiEndFrame();
  }

  return 0;
}

bool CreateDeviceD3D(HWND hWnd) {
  // Setup swap chain
  DXGI_SWAP_CHAIN_DESC1 sd;
  {
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = NUM_BACK_BUFFERS;
    sd.Width = 0;
    sd.Height = 0;
    sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    sd.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
    sd.Scaling = DXGI_SCALING_STRETCH;
    sd.Stereo = FALSE;
  }

  // [DEBUG] Enable debug interface
#ifdef DX12_ENABLE_DEBUG_LAYER
  ID3D12Debug *pdx12Debug = 0;
  if (SUCCEEDED(D3D12GetDebugInterface(&IID_ID3D12Debug, &pdx12Debug)))
    pdx12Debug->lpVtbl->EnableDebugLayer(pdx12Debug);
#endif

  // Create device
  D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
  if (D3D12CreateDevice(0, featureLevel, &IID_ID3D12Device, &g_pd3dDevice) !=
      S_OK)
    return false;

    // [DEBUG] Setup debug interface to break on any warnings/errors
#ifdef DX12_ENABLE_DEBUG_LAYER
  if (pdx12Debug != 0) {
    ID3D12InfoQueue *pInfoQueue = 0;
    g_pd3dDevice->lpVtbl->QueryInterface(g_pd3dDevice, &IID_ID3D12InfoQueue,
                                         &pInfoQueue);
    pInfoQueue->lpVtbl->SetBreakOnSeverity(pInfoQueue,
                                           D3D12_MESSAGE_SEVERITY_ERROR, true);
    pInfoQueue->lpVtbl->SetBreakOnSeverity(
        pInfoQueue, D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
    pInfoQueue->lpVtbl->SetBreakOnSeverity(
        pInfoQueue, D3D12_MESSAGE_SEVERITY_WARNING, true);
    pInfoQueue->lpVtbl->Release(pInfoQueue);
    pdx12Debug->lpVtbl->Release(pdx12Debug);
  }
#endif

  {
    D3D12_DESCRIPTOR_HEAP_DESC desc = {0};
    desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    desc.NumDescriptors = NUM_BACK_BUFFERS;
    desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    desc.NodeMask = 1;
    if (g_pd3dDevice->lpVtbl->CreateDescriptorHeap(
            g_pd3dDevice, &desc, &IID_ID3D12DescriptorHeap,
            (&g_pd3dRtvDescHeap)) != S_OK)
      return false;

    SIZE_T rtvDescriptorSize =
        g_pd3dDevice->lpVtbl->GetDescriptorHandleIncrementSize(
            g_pd3dDevice, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle;
    g_pd3dRtvDescHeap->lpVtbl->GetCPUDescriptorHandleForHeapStart(
        g_pd3dRtvDescHeap, &rtvHandle);
    for (UINT i = 0; i < NUM_BACK_BUFFERS; i++) {
      g_mainRenderTargetDescriptor[i] = rtvHandle;
      rtvHandle.ptr += rtvDescriptorSize;
    }
  }

  {
    D3D12_DESCRIPTOR_HEAP_DESC desc = {0};
    desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    desc.NumDescriptors = 1;
    desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    if (g_pd3dDevice->lpVtbl->CreateDescriptorHeap(
            g_pd3dDevice, &desc, &IID_ID3D12DescriptorHeap,
            (&g_pd3dSrvDescHeap)) != S_OK)
      return false;
  }

  {
    D3D12_COMMAND_QUEUE_DESC desc = {0};
    desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    desc.NodeMask = 1;
    if (g_pd3dDevice->lpVtbl->CreateCommandQueue(g_pd3dDevice, &desc,
                                                 &IID_ID3D12CommandQueue,
                                                 (&g_pd3dCommandQueue)) != S_OK)
      return false;
  }

  for (UINT i = 0; i < NUM_FRAMES_IN_FLIGHT; i++)
    if (g_pd3dDevice->lpVtbl->CreateCommandAllocator(
            g_pd3dDevice, D3D12_COMMAND_LIST_TYPE_DIRECT,
            &IID_ID3D12CommandAllocator,
            (&g_frameContext[i].CommandAllocator)) != S_OK)
      return false;

  if (g_pd3dDevice->lpVtbl->CreateCommandList(
          g_pd3dDevice, 0, D3D12_COMMAND_LIST_TYPE_DIRECT,
          g_frameContext[0].CommandAllocator, 0, &IID_ID3D12GraphicsCommandList,
          (&g_pd3dCommandList)) != S_OK ||
      g_pd3dCommandList->lpVtbl->Close(g_pd3dCommandList) != S_OK)
    return false;

  if (g_pd3dDevice->lpVtbl->CreateFence(g_pd3dDevice, 0, D3D12_FENCE_FLAG_NONE,
                                        &IID_ID3D12Fence, (&g_fence)) != S_OK)
    return false;

  g_fenceEvent = CreateEvent(0, FALSE, FALSE, 0);
  if (g_fenceEvent == 0)
    return false;

  {
    IDXGIFactory4 *dxgiFactory = 0;
    IDXGISwapChain1 *swapChain1 = 0;
    if (CreateDXGIFactory1(&IID_IDXGIFactory4, (&dxgiFactory)) != S_OK)
      return false;
    if (dxgiFactory->lpVtbl->CreateSwapChainForHwnd(
            dxgiFactory, g_pd3dCommandQueue, hWnd, &sd, 0, 0, &swapChain1) !=
        S_OK)
      return false;
    if (swapChain1->lpVtbl->QueryInterface(swapChain1, &IID_IDXGISwapChain3,
                                           (&g_pSwapChain)) != S_OK)
      return false;
    swapChain1->lpVtbl->Release(swapChain1);
    dxgiFactory->lpVtbl->Release(dxgiFactory);
    g_pSwapChain->lpVtbl->SetMaximumFrameLatency(g_pSwapChain,
                                                 NUM_BACK_BUFFERS);
    g_hSwapChainWaitableObject =
        g_pSwapChain->lpVtbl->GetFrameLatencyWaitableObject(g_pSwapChain);
  }

  CreateRenderTarget();
  return true;
}

void CleanupDeviceD3D() {
  CleanupRenderTarget();
  if (g_pSwapChain) {
    g_pSwapChain->lpVtbl->SetFullscreenState(g_pSwapChain, false, 0);
    g_pSwapChain->lpVtbl->Release(g_pSwapChain);
    g_pSwapChain = 0;
  }
  if (g_hSwapChainWaitableObject != 0) {
    CloseHandle(g_hSwapChainWaitableObject);
  }
  for (UINT i = 0; i < NUM_FRAMES_IN_FLIGHT; i++)
    if (g_frameContext[i].CommandAllocator) {
      g_frameContext[i].CommandAllocator->lpVtbl->Release(
          g_frameContext[i].CommandAllocator);
      g_frameContext[i].CommandAllocator = 0;
    }
  if (g_pd3dCommandQueue) {
    g_pd3dCommandQueue->lpVtbl->Release(g_pd3dCommandQueue);
    g_pd3dCommandQueue = 0;
  }
  if (g_pd3dCommandList) {
    g_pd3dCommandList->lpVtbl->Release(g_pd3dCommandList);
    g_pd3dCommandList = 0;
  }
  if (g_pd3dRtvDescHeap) {
    g_pd3dRtvDescHeap->lpVtbl->Release(g_pd3dRtvDescHeap);
    g_pd3dRtvDescHeap = 0;
  }
  if (g_pd3dSrvDescHeap) {
    g_pd3dSrvDescHeap->lpVtbl->Release(g_pd3dSrvDescHeap);
    g_pd3dSrvDescHeap = 0;
  }
  if (g_fence) {
    g_fence->lpVtbl->Release(g_fence);
    g_fence = 0;
  }
  if (g_fenceEvent) {
    CloseHandle(g_fenceEvent);
    g_fenceEvent = 0;
  }
  if (g_pd3dDevice) {
    g_pd3dDevice->lpVtbl->Release(g_pd3dDevice);
    g_pd3dDevice = 0;
  }

#ifdef DX12_ENABLE_DEBUG_LAYER
  IDXGIDebug1 *pDebug = 0;
  if (SUCCEEDED(DXGIGetDebugInterface1(0, &IID_IDXGIDebug1, (&pDebug)))) {
    pDebug->lpVtbl->ReportLiveObjects(pDebug, DXGI_DEBUG_ALL,
                                      DXGI_DEBUG_RLO_SUMMARY);
    pDebug->lpVtbl->Release(pDebug);
  }
#endif
}

void CreateRenderTarget() {
  for (UINT i = 0; i < NUM_BACK_BUFFERS; i++) {
    ID3D12Resource *pBackBuffer = 0;
    g_pSwapChain->lpVtbl->GetBuffer(g_pSwapChain, i, &IID_ID3D12Resource,
                                    (&pBackBuffer));
    g_pd3dDevice->lpVtbl->CreateRenderTargetView(
        g_pd3dDevice, pBackBuffer, 0, g_mainRenderTargetDescriptor[i]);
    g_mainRenderTargetResource[i] = pBackBuffer;
  }
}

void CleanupRenderTarget() {
  WaitForLastSubmittedFrame();

  for (UINT i = 0; i < NUM_BACK_BUFFERS; i++)
    if (g_mainRenderTargetResource[i]) {
      g_mainRenderTargetResource[i]->lpVtbl->Release(
          g_mainRenderTargetResource[i]);
      g_mainRenderTargetResource[i] = 0;
    }
}

void WaitForLastSubmittedFrame() {
  FrameContext *frameCtx = &g_frameContext[g_frameIndex % NUM_FRAMES_IN_FLIGHT];

  UINT64 fenceValue = frameCtx->FenceValue;
  if (fenceValue == 0)
    return; // No fence was signaled

  frameCtx->FenceValue = 0;
  if (g_fence->lpVtbl->GetCompletedValue(g_fence) >= fenceValue)
    return;

  g_fence->lpVtbl->SetEventOnCompletion(g_fence, fenceValue, g_fenceEvent);
  WaitForSingleObject(g_fenceEvent, INFINITE);
}

// Win32 message handler
// You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if
// dear imgui wants to use your inputs.
// - When io.WantCaptureMouse is true, do not dispatch mouse input data to your
// main application, or clear/overwrite your copy of the mouse data.
// - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to
// your main application, or clear/overwrite your copy of the keyboard data.
// Generally you may always pass all inputs to dear imgui, and hide them from
// your application based on those two flags.
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  if (OXIImGuiWndProc(hWnd, msg, wParam, lParam))
    return true;

  switch (msg) {
  case WM_SIZE:
    if (g_pd3dDevice != 0 && wParam != SIZE_MINIMIZED) {
      WaitForLastSubmittedFrame();
      CleanupRenderTarget();
      HRESULT result = g_pSwapChain->lpVtbl->ResizeBuffers(
          g_pSwapChain, 0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam),
          DXGI_FORMAT_UNKNOWN,
          DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT);
      OXIAssertT(SUCCEEDED(result), "Failed to resize swapchain.");
      CreateRenderTarget();
    }
    return 0;
  case WM_SYSCOMMAND:
    if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
      return 0;
    break;
  case WM_DESTROY:
    PostQuitMessage(0);
    return 0;
  }
  return DefWindowProcW(hWnd, msg, wParam, lParam);
}