#define UNICODE
#define _UNICODE

#include <tchar.h>
#include <wchar.h>
#include <windows.h>

int main() {
    int i = 0; 
    while (i++ < 100) {
      OutputDebugString(_TEXT("Hello from subprocess debug!"));
      Sleep(100);
    }
    return 0;
}