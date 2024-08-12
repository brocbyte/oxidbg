#include <tchar.h>
#include <wchar.h>
#include <windows.h>
#include <stdio.h>

int f() {
  volatile int a = 2, b = 3;
  OutputDebugString(_TEXT("HELLO FROM F"));
  return a + b;
}

int main() {
  f();
  return 0;
}