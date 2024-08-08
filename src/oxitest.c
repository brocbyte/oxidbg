#include <tchar.h>
#include <wchar.h>
#include <windows.h>

int main() {
    int i = 0; 
    while (i++ < 100) {
      OutputDebugString(_TEXT("Hello from subprocess debug!"));
      fprintf(stderr, "Msg to stderr...\n");
      Sleep(100);
    }
    return 0;
}