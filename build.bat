cl
if not %errorlevel%==0 (
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
)

set src=..\src\

if not exist build_oxitest mkdir build_oxitest
if not exist build_oxidbg mkdir build_oxidbg

pushd build_oxitest
cl /nologo /D UNICODE /D _UNICODE %src%oxitest.c
popd

pushd build_oxidbg
cl /D UNICODE /D _UNICODE /MT /W3 /Zi /nologo /I ..\imgui /I ..\imgui\backends %src%oxidbg.c %src%oxiimgui.cpp user32.lib D3D12.lib DXGI.lib ..\imgui\backends\imgui_impl_dx12.cpp ..\imgui\backends\imgui_impl_win32.cpp ..\imgui\imgui*.cpp
popd