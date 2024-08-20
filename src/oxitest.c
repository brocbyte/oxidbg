#include <tchar.h>
#include <wchar.h>
#include <windows.h>
#include <stdio.h>

int f() {
  volatile int a = 2, b = 3;
  OutputDebugString(_TEXT("EVA 00: Rei Ayanami"));
  OutputDebugString(_TEXT("EVA 01: Shinji Ikari"));
  OutputDebugString(_TEXT("EVA 02: Asuka Langley Sohryu"));
  return a + b;
}

int main() {
  f();
  return 0;
}