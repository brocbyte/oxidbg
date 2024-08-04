#include <windows.h>

int main() {
    printf("HELLLLLLOOOOO FROM TEST\n");
    OutputDebugStringW(L"Hello from subprocess debug!");
    return 0;
}