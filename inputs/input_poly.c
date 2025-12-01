#include <stdio.h>

void foo() { printf("I am Foo\n"); }
void bar() { printf("I am Bar\n"); }

// 接受函数指针作为参数
void runner(void (*func_ptr)()) {
    func_ptr(); // <--- 这是一个间接调用 (Indirect Call)
}

int main() {
    foo();        // <--- 直接调用
    runner(bar);  // <--- 直接调用 runner
    return 0;
}