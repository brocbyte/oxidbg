#include "oxiimgui.h"

#include "imgui.h"
#include "imgui_impl_dx12.h"
#include "imgui_impl_win32.h"

#include <windows.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam,
                                                             LPARAM lParam);

extern "C" {
#define NUM_FRAMES_IN_FLIGHT 3
extern FrameContext g_frameContext[NUM_FRAMES_IN_FLIGHT];
extern UINT g_frameIndex;

#define NUM_BACK_BUFFERS 3
extern ID3D12Device *g_pd3dDevice;
extern ID3D12DescriptorHeap *g_pd3dRtvDescHeap;
extern ID3D12DescriptorHeap *g_pd3dSrvDescHeap;
extern ID3D12CommandQueue *g_pd3dCommandQueue;
extern ID3D12GraphicsCommandList *g_pd3dCommandList;
extern ID3D12Fence *g_fence;
extern HANDLE g_fenceEvent;
extern UINT64 g_fenceLastSignaledValue;
extern IDXGISwapChain3 *g_pSwapChain;
extern bool g_SwapChainOccluded;
extern HANDLE g_hSwapChainWaitableObject;
extern ID3D12Resource *g_mainRenderTargetResource[NUM_BACK_BUFFERS];
extern D3D12_CPU_DESCRIPTOR_HANDLE g_mainRenderTargetDescriptor[NUM_BACK_BUFFERS];

FrameContext *WaitForNextFrameResources() {
  UINT nextFrameIndex = g_frameIndex + 1;
  g_frameIndex = nextFrameIndex;

  HANDLE waitableObjects[] = {g_hSwapChainWaitableObject, 0};
  DWORD numWaitableObjects = 1;

  FrameContext *frameCtx = &g_frameContext[nextFrameIndex % NUM_FRAMES_IN_FLIGHT];
  UINT64 fenceValue = frameCtx->FenceValue;
  if (fenceValue != 0) // means no fence was signaled
  {
    frameCtx->FenceValue = 0;
    g_fence->SetEventOnCompletion(fenceValue, g_fenceEvent);
    waitableObjects[1] = g_fenceEvent;
    numWaitableObjects = 2;
  }

  WaitForMultipleObjects(numWaitableObjects, waitableObjects, TRUE, INFINITE);

  return frameCtx;
}

void OXIImGuiInit(HWND hwnd, ID3D12Device *device, int num_frames_in_flight, DXGI_FORMAT rtv_format,
                  ID3D12DescriptorHeap *cbv_srv_heap,
                  D3D12_CPU_DESCRIPTOR_HANDLE font_srv_cpu_desc_handle,
                  D3D12_GPU_DESCRIPTOR_HANDLE font_srv_gpu_desc_handle) {
  // Setup Dear ImGui context
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;  // Enable Gamepad Controls

  // Setup Platform/Renderer backends
  ImGui_ImplWin32_Init(hwnd);
  ImGui_ImplDX12_Init(device, num_frames_in_flight, rtv_format, cbv_srv_heap,
                      // You'll need to designate a descriptor from your descriptor heap for
                      // Dear ImGui to use internally for its font texture's SRV
                      font_srv_cpu_desc_handle, font_srv_gpu_desc_handle);
}

static void tableRegisterHelper(const char* regName, i64 regValue) {
  ImGui::TableNextRow();
  ImGui::TableSetColumnIndex(0);
  ImGui::Text(regName);
  ImGui::TableSetColumnIndex(1);
  ImGui::Text("%p", regValue);
}

void OXIImGuiBegFrame(UIData* data) {
  ImGui_ImplDX12_NewFrame();
  ImGui_ImplWin32_NewFrame();
  ImGui::NewFrame();

  if (ImGui::BeginTable("Registers", 2, ImGuiTableFlags_Borders)) {
    tableRegisterHelper("rip", data->ctx.Rip);
    tableRegisterHelper("rax", data->ctx.Rax);
    tableRegisterHelper("rcx", data->ctx.Rcx);
    tableRegisterHelper("rdx", data->ctx.Rdx);
    tableRegisterHelper("rbx", data->ctx.Rbx);
    tableRegisterHelper("rsp", data->ctx.Rsp);
    tableRegisterHelper("rbp", data->ctx.Rbp);
    tableRegisterHelper("rsi", data->ctx.Rsi);
    tableRegisterHelper("rdi", data->ctx.Rdi);
    tableRegisterHelper("r8", data->ctx.R8);
    tableRegisterHelper("r9", data->ctx.R9);
    tableRegisterHelper("r10", data->ctx.R10);
    tableRegisterHelper("r11", data->ctx.R11);
    tableRegisterHelper("r12", data->ctx.R12);
    tableRegisterHelper("r13", data->ctx.R13);
    tableRegisterHelper("r14", data->ctx.R14);
    tableRegisterHelper("r15", data->ctx.R15);
    ImGui::EndTable();
  }

  for (u32 i = 0; i < data->nDll; ++i) {
    ImGui::Text("%ls", data->dll[i]);
  }

  if (ImGui::Button("Step Into")) {
    EnterCriticalSection(&data->critical_section);
    while (data->commandEntered != OXIDbgCommand_None) {
      SleepConditionVariableCS(&data->condition_variable, &data->critical_section, INFINITE);
    }
    data->commandEntered = OXIDbgCommand_StepInto;
    LeaveCriticalSection(&data->critical_section);
    WakeConditionVariable(&data->condition_variable); 
  }

  if (ImGui::Button("Go")) {
    EnterCriticalSection(&data->critical_section);
    while (data->commandEntered != OXIDbgCommand_None) {
      SleepConditionVariableCS(&data->condition_variable, &data->critical_section, INFINITE);
    }
    data->commandEntered = OXIDbgCommand_Go;
    LeaveCriticalSection(&data->critical_section);
    WakeConditionVariable(&data->condition_variable); 
  }
}

void OXIImGuiEndFrame() {
  // Rendering
  ImGui::Render();

  FrameContext *frameCtx = WaitForNextFrameResources();
  UINT backBufferIdx = g_pSwapChain->GetCurrentBackBufferIndex();
  frameCtx->CommandAllocator->Reset();

  D3D12_RESOURCE_BARRIER barrier = {};
  barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
  barrier.Transition.pResource = g_mainRenderTargetResource[backBufferIdx];
  barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
  barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
  barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
  g_pd3dCommandList->Reset(frameCtx->CommandAllocator, nullptr);
  g_pd3dCommandList->ResourceBarrier(1, &barrier);

  // Render Dear ImGui graphics
  ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
  const float clear_color_with_alpha[4] = {clear_color.x * clear_color.w,
                                           clear_color.y * clear_color.w,
                                           clear_color.z * clear_color.w, clear_color.w};
  g_pd3dCommandList->ClearRenderTargetView(g_mainRenderTargetDescriptor[backBufferIdx],
                                           clear_color_with_alpha, 0, nullptr);
  g_pd3dCommandList->OMSetRenderTargets(1, &g_mainRenderTargetDescriptor[backBufferIdx], FALSE,
                                        nullptr);
  g_pd3dCommandList->SetDescriptorHeaps(1, &g_pd3dSrvDescHeap);
  ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), g_pd3dCommandList);
  barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
  barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
  g_pd3dCommandList->ResourceBarrier(1, &barrier);
  g_pd3dCommandList->Close();

  g_pd3dCommandQueue->ExecuteCommandLists(1, (ID3D12CommandList *const *)&g_pd3dCommandList);

  // Present
  HRESULT hr = g_pSwapChain->Present(1, 0); // Present with vsync
  // HRESULT hr = g_pSwapChain->Present(0, 0); // Present without vsync
  g_SwapChainOccluded = (hr == DXGI_STATUS_OCCLUDED);

  UINT64 fenceValue = g_fenceLastSignaledValue + 1;
  g_pd3dCommandQueue->Signal(g_fence, fenceValue);
  g_fenceLastSignaledValue = fenceValue;
  frameCtx->FenceValue = fenceValue;
}

bool OXIImGuiWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
    return true;
  return false;
}
}