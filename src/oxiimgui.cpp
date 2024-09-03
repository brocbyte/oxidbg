#include "oxiimgui.h"
#include "oxidec.h"

#include "imgui.h"
#include "imgui_impl_dx12.h"
#include "imgui_impl_win32.h"

#include <windows.h>
#include <tchar.h>
#include <wchar.h>

#include <string.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd,
                                                             UINT msg,
                                                             WPARAM wParam,
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
extern D3D12_CPU_DESCRIPTOR_HANDLE
    g_mainRenderTargetDescriptor[NUM_BACK_BUFFERS];

FrameContext *WaitForNextFrameResources() {
  UINT nextFrameIndex = g_frameIndex + 1;
  g_frameIndex = nextFrameIndex;

  HANDLE waitableObjects[] = {g_hSwapChainWaitableObject, 0};
  DWORD numWaitableObjects = 1;

  FrameContext *frameCtx =
      &g_frameContext[nextFrameIndex % NUM_FRAMES_IN_FLIGHT];
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

void OXIImGuiInit(HWND hwnd, ID3D12Device *device, int num_frames_in_flight,
                  DXGI_FORMAT rtv_format, ID3D12DescriptorHeap *cbv_srv_heap,
                  D3D12_CPU_DESCRIPTOR_HANDLE font_srv_cpu_desc_handle,
                  D3D12_GPU_DESCRIPTOR_HANDLE font_srv_gpu_desc_handle) {
  // Setup Dear ImGui context
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  io.ConfigFlags |=
      ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
  io.ConfigFlags |=
      ImGuiConfigFlags_NavEnableGamepad; // Enable Gamepad Controls
  // Setup Platform/Renderer backends
  ImGui_ImplWin32_Init(hwnd);
  ImGui_ImplDX12_Init(
      device, num_frames_in_flight, rtv_format, cbv_srv_heap,
      // You'll need to designate a descriptor from your descriptor heap for
      // Dear ImGui to use internally for its font texture's SRV
      font_srv_cpu_desc_handle, font_srv_gpu_desc_handle);

  ImFont *font =
      io.Fonts->AddFontFromFileTTF("fonts\\AcPlus_IBM_EGA_9x8.ttf", 13);

  ImGuiStyle &style = ImGui::GetStyle();

  style.Colors[ImGuiCol_Text] = ImVec4(1.0f, 1.0f, 1.0f, 1.00f);
  style.Colors[ImGuiCol_WindowBg] = ImVec4(0.0f, 0.0f, 0.0f, 1.00f);
  style.Colors[ImGuiCol_Button] = (ImVec4)ImColor(29, 68, 108);
}

static void tableRegisterHelper(const char *regName, i64 regValue) {
  ImGui::TableNextRow();
  ImGui::TableSetColumnIndex(0);
  ImGui::Text(regName);
  ImGui::TableSetColumnIndex(1);
  ImGui::Text("%p", regValue);
}

static i16 findBreakpoint(u64 addr, OXIBreakpoint *breakpoints,
                          u32 nBreakpoints) {
  i16 breakpointIdx = -1;
  for (u32 j = 0; j < nBreakpoints; ++j) {
    if (breakpoints[j].addr == addr) {
      breakpointIdx = j;
    }
  }
  return breakpointIdx;
}

static void breakpointHelper(u32 i, u64 addr, OXIBreakpoint *breakpoints,
                             u32 *nBreakpoints, u32 countOfBreakpoints) {
  ImGui::PushID(i);
  i16 breakpointIdx = findBreakpoint(addr, breakpoints, *nBreakpoints);
  ImVec4 ayanamiBlue = (ImVec4)ImColor(132, 141, 184);

  ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32_BLACK_TRANS);
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32_BLACK_TRANS);
  ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32_BLACK_TRANS);
  ImGui::PushStyleColor(ImGuiCol_Text, ayanamiBlue);

  bool hasBreakpoint = breakpointIdx != -1;
  if (hasBreakpoint) {
    if (ImGui::Button("[*]")) {
      for (u32 j = breakpointIdx + 1; j < *nBreakpoints; ++j) {
        breakpoints[j - 1] = breakpoints[j];
      }
      --(*nBreakpoints);
    }
  } else {
    if (ImGui::Button("[ ]")) {
      if (*nBreakpoints != countOfBreakpoints) {
        breakpoints[*nBreakpoints].addr = addr;
        ++(*nBreakpoints);
      }
    }
  }

  ImGui::PopStyleColor(4);
  ImGui::PopID();
}

void OXIImGuiBegFrame(UIData *data) {
  ImGui_ImplDX12_NewFrame();
  ImGui_ImplWin32_NewFrame();
  ImGui::NewFrame();
  ImGui::ShowDemoWindow();
  ImGui::Begin("Debug");

  ImGui::Text("%s %d", data->reason, data->issueThread);
  EnterCriticalSection(&data->critical_section);

  ImGui::SetNextWindowSizeConstraints(
      ImVec2(0.0f, ImGui::GetTextLineHeightWithSpacing() * 1),
      ImVec2(FLT_MAX, ImGui::GetTextLineHeightWithSpacing() * 22));
  if (ImGui::BeginChild("RegistersChild", ImVec2(0.0f, 0.0f),
                        ImGuiChildFlags_AutoResizeX |
                            ImGuiChildFlags_AutoResizeY)) {

    static ImGuiTableFlags flags = ImGuiTableFlags_SizingFixedFit |
                                   ImGuiTableFlags_Resizable |
                                   ImGuiTableFlags_Borders;

    if (ImGui::BeginTable("Registers", 2, flags)) {
      tableRegisterHelper("eip", data->ctx.Rip);
      tableRegisterHelper("eax", data->ctx.Rax);
      tableRegisterHelper("ecx", data->ctx.Rcx);
      tableRegisterHelper("edx", data->ctx.Rdx);
      tableRegisterHelper("ebx", data->ctx.Rbx);
      tableRegisterHelper("esp", data->ctx.Rsp);
      tableRegisterHelper("ebp", data->ctx.Rbp);
      tableRegisterHelper("esi", data->ctx.Rsi);
      tableRegisterHelper("edi", data->ctx.Rdi);
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
    ImGui::EndChild();
  }
  ImGui::SameLine();
  ImGui::BeginGroup();
  if (ImGui::BeginChild("ModulesChild", ImVec2(0.0f, 200.0f),
                        ImGuiChildFlags_AutoResizeX)) {
    ImGui::Text("Modules");
    ImGui::SameLine();
    static ImGuiTextFilter filter;
    filter.Draw();
    for (u32 i = 0; i < data->nDll; ++i) {
      char nodeDllName[256];
      snprintf(nodeDllName, sizeof(nodeDllName), "%ls",
               data->dll[i].moduleNameByHandle);
      if (ImGui::TreeNode(nodeDllName)) {

        for (u32 j = 0; j < data->dll[i].nSymbols; ++j) {
          if (filter.PassFilter(data->dll[i].aSymbols[j].name)) {
            breakpointHelper(j, data->dll[i].aSymbols[j].addr,
                             data->breakpoints, &data->nBreakpoints,
                             _countof(data->breakpoints));
            ImGui::SameLine();
            ImGui::Text(data->dll[i].aSymbols[j].name);
          }
        }

        ImGui::TreePop();
        ImGui::Spacing();
      }
    }
    ImGui::EndChild();
  }

  ImGui::Text("Log");
  ImGui::SameLine();
  if (ImGui::Button("Clear")) {
    data->nLog = 0;
  }
  if (ImGui::BeginChild("log", ImVec2(0.0f, 150.0f),
                        ImGuiChildFlags_AutoResizeX)) {
    for (int i = 0; i < data->nLog; ++i) {
      ImGui::Text(data->log[i]);
    }
    ImGui::EndChild();
  }
  ImGui::EndGroup();
  ImGui::SameLine();
  ImGui::BeginGroup();
  ImGui::Text("Threads");
  if (ImGui::BeginChild("threads", ImVec2(0.0f, 100.0f),
                        ImGuiChildFlags_AutoResizeX)) {
    for (int i = 0; i < data->nThreads; ++i) {
      char threadBuff[256];
      snprintf(threadBuff, _countof(threadBuff),
               "hThread: %p localBase: %p startAddr: %p",
               data->threads[i].hThread, data->threads[i].lpThreadLocalBase,
               data->threads[i].lpStartAddress);
      ImGui::PushID(threadBuff);
      ImGui::PushItemWidth(
          ImGui::CalcTextSize(threadBuff, threadBuff + strlen(threadBuff) - 1)
              .x +
          2.0f * ImGui::GetStyle().FramePadding.x);
      ImGui::InputText("", threadBuff, sizeof(threadBuff),
                       ImGuiInputTextFlags_ReadOnly);
      ImGui::PopItemWidth();
      ImGui::PopID();
    }
    ImGui::EndChild();
  }

  if (ImGui::BeginChild("callstack", ImVec2(0.0f, 200.0f),
                        ImGuiChildFlags_AutoResizeX)) {
    for (u32 i = 0; i < data->nCallstack; ++i) {
      char src[256];
      sourceMe(data->callstack[i], src, sizeof(src), data->dll, data->nDll, true);
      ImGui::Text("%p %s", data->callstack[i], src);
    }
    ImGui::EndChild();
  }

  ImGui::EndGroup();

  UIDataAsmLine lines[8];
  decodeInstruction(data->itext, sizeof(data->itext), lines, _countof(lines),
                    data->ctx.Rip, data->dll, data->nDll, data->process);

  ImGui::BeginChild("DisassemblyAndControls", ImVec2(0, 300),
                    ImGuiChildFlags_Border, ImGuiWindowFlags_None);
  {
    ImGui::BeginChild("Disassembly", ImVec2(0, 200), ImGuiChildFlags_None,
                      ImGuiWindowFlags_None);

    for (int i = 0; i < _countof(lines); ++i) {
      breakpointHelper(i, lines[i].addr, data->breakpoints, &data->nBreakpoints,
                       _countof(data->breakpoints));

      ImGui::SameLine();

      char line[256];
      snprintf(line, sizeof(line), "%p % 50s % 20s %s", (void *)lines[i].addr,
               lines[i].source, lines[i].itext, lines[i].decoded);

      if (lines[i].addr == data->ctx.Rip) {
        ImDrawList *draw_list = ImGui::GetWindowDrawList();
        ImVec2 textSize = ImGui::CalcTextSize(line, line + strlen(line) - 1);
        ImVec2 framePadding = ImGui::GetStyle().FramePadding;
        ImVec2 gradient_size = {textSize.x + framePadding.x * 2.0f,
                                textSize.y + framePadding.y * 2.0f};

        ImVec2 p0 = ImGui::GetCursorScreenPos();
        ImVec2 p1 = ImVec2(p0.x + gradient_size.x, p0.y + gradient_size.y);
        ImU32 col_a = ImGui::GetColorU32(IM_COL32(29, 68, 108, 255));
        draw_list->AddRectFilled(p0, p1, col_a);
      }

      if (strstr(lines[i].decoded, "call ") == lines[i].decoded) {
        ImGui::PushStyleColor(ImGuiCol_Text, (ImVec4)ImColor(200, 141, 40));
      }
      ImGui::Text(line);
      if (strstr(lines[i].decoded, "call ") == lines[i].decoded) {
        ImGui::PopStyleColor();
      }

      if (lines[i].functionEnd) {
        ImGui::SeparatorText("function end");
      }

    }
    ImGui::EndChild();

    if (ImGui::Button("Step Into")) {
      while (data->commandEntered != OXIDbgCommand_None) {
        SleepConditionVariableCS(&data->condition_variable,
                                 &data->critical_section, INFINITE);
      }
      data->commandEntered = OXIDbgCommand_StepInto;
    }

    if (ImGui::Button("Go")) {
      while (data->commandEntered != OXIDbgCommand_None) {
        SleepConditionVariableCS(&data->condition_variable,
                                 &data->critical_section, INFINITE);
      }
      data->commandEntered = OXIDbgCommand_Go;
    }
  }
  ImGui::EndChild();

  static char breakpoint[256] = {0};
  if (ImGui::Button("Add breakpoint")) {
    ImGui::OpenPopup("breakpoint_popup");
  }
  if (ImGui::BeginPopup("breakpoint_popup")) {
    ImGui::InputText("Address/Name", breakpoint, IM_ARRAYSIZE(breakpoint));
    if (ImGui::Button("OK")) {
      long long parsed = strtoll(breakpoint, 0, 16);
      if (parsed) {
        data->breakpoints[data->nBreakpoints].addr = parsed;
        ++data->nBreakpoints;
      }
    }
    ImGui::EndPopup();
  }

  for (u32 i = 0; i < data->nBreakpoints; ++i) {
    char source[256];
    sourceMe(data->breakpoints[i].addr, source, sizeof(source), data->dll,
             data->nDll, false);
    ImGui::Text("[%d] %p %s", i, data->breakpoints[i].addr, source);
  }

  ImGui::End();

  LeaveCriticalSection(&data->critical_section);
  WakeConditionVariable(&data->condition_variable);
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
  const float clear_color_with_alpha[4] = {
      clear_color.x * clear_color.w, clear_color.y * clear_color.w,
      clear_color.z * clear_color.w, clear_color.w};
  g_pd3dCommandList->ClearRenderTargetView(
      g_mainRenderTargetDescriptor[backBufferIdx], clear_color_with_alpha, 0,
      nullptr);
  g_pd3dCommandList->OMSetRenderTargets(
      1, &g_mainRenderTargetDescriptor[backBufferIdx], FALSE, nullptr);
  g_pd3dCommandList->SetDescriptorHeaps(1, &g_pd3dSrvDescHeap);
  ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), g_pd3dCommandList);
  barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
  barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
  g_pd3dCommandList->ResourceBarrier(1, &barrier);
  g_pd3dCommandList->Close();

  g_pd3dCommandQueue->ExecuteCommandLists(
      1, (ID3D12CommandList *const *)&g_pd3dCommandList);

  // Present
  HRESULT hr = g_pSwapChain->Present(
      1,
      0); // Present with vsync
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