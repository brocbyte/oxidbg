#include <d3d12.h>
#include <dxgi1_4.h>
#include <windows.h>
#include "oxiassert.h"

typedef struct FrameContext {
  ID3D12CommandAllocator *CommandAllocator;
  UINT64 FenceValue;
} FrameContext;

typedef struct UIData {
  CONTEXT ctx;
  bool commandEntered;
  CONDITION_VARIABLE condition_variable;
  CRITICAL_SECTION critical_section;
} UIData;

#ifdef __cplusplus
extern "C" {
#endif
void OXIImGuiInit(HWND hwnd, ID3D12Device *device, int num_frames_in_flight,
                  DXGI_FORMAT rtv_format, ID3D12DescriptorHeap *cbv_srv_heap,
                  D3D12_CPU_DESCRIPTOR_HANDLE font_srv_cpu_desc_handle,
                  D3D12_GPU_DESCRIPTOR_HANDLE font_srv_gpu_desc_handle);
void OXIImGuiBegFrame(UIData* data);
void OXIImGuiEndFrame();
bool OXIImGuiWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

#ifdef __cplusplus
}
#endif