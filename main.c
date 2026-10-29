#include <stdio.h>
#include <xenos/xenos.h>
#include <console/console.h>

int main(void) {
    xenos_init(VIDEO_MODE_AUTO);
    console_init();
    printf("Xbox App browser\n");
    while (1) {}
    return 0;
}
