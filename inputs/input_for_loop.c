#include <stdio.h>

void foo() {
    printf("Hello, World!\n");
}

int main()
{
    
    for (int i = 0; i < 10; i++) {
        foo();
    }
    return 0;
}
