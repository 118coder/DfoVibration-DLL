#include <windows.h>
/* 32-bit victim process used to verify cross-arch injection (x64 host -> x86 target). */
int main(void) {
    /* stay alive long enough for the injector test */
    for (int i = 0; i < 300; i++) {
        Sleep(1000);
    }
    return 0;
}