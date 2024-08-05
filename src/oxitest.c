#define UNICODE
#define _UNICODE

#include <tchar.h>
#include <wchar.h>
#include <windows.h>

int main() {
    OutputDebugString(_TEXT("Hello from subprocess debug!"));
    return 0;
}