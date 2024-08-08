cl
if not %errorlevel%==0 (
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64

pushd xed
python .\mfile.py install --install-dir install
popd
)


set src=..\src\

if not exist build_oxitest mkdir build_oxitest
if not exist build_oxidbg mkdir build_oxidbg

set unicode=/D UNICODE /D _UNICODE

pushd build_oxitest
cl /nologo %unicode% %src%oxitest.c
popd

set imgui_headers=/I ..\imgui /I ..\imgui\backends 
set imgui_src=..\imgui\backends\imgui_impl_dx12.cpp ..\imgui\backends\imgui_impl_win32.cpp ..\imgui\imgui*.cpp

set xed_headers=/I ..\xed\install\include

set headers=%imgui_headers% %xed_headers%
set libs=xed.lib user32.lib D3D12.lib DXGI.lib 

set link=/LIBPATH:..\xed\install\lib

pushd build_oxidbg
cl /nologo %unicode% /MT /W3 /Zi %headers% %src%oxidbg.c %src%oxidec.c %src%oxiimgui.cpp %imgui_src% /link %link% %libs% 
popd