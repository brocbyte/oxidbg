#include <tchar.h>
#include <wchar.h>
#include <windows.h>
#include <stdio.h>

__declspec(dllexport) int f() {
  FILE* f = fopen("test.txt", "w");
  const char* greet = "hi there";
  fwrite((void*)greet, 1, strlen(greet), f);
  fclose(f);
  return 0;
}

__declspec(dllexport) int main() {
  return f();
}