#include <stdio.h>

int foo(int a, int b) {
    int sum = 0;
    for (int i = 0; i < b; ++i) {
        sum += (a * i); // 这里会有 add, mul 指令
    }
    return sum;
}