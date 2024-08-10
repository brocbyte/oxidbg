#include <tchar.h>
#include <wchar.h>
#include <windows.h>
#include <stdio.h>

int main() {
    volatile int i = 0; 
    i += 255;
    i += 378;
    fprintf(stderr, "%d\n", i);
    while (i++ < 100) {
      OutputDebugString(_TEXT("Hello from subprocess debug!"));
      fprintf(stderr, "Msg to stderr...\n");
      Sleep(100);
    }
    return 0;
}