#include <stdio.h>

void show_call_count(void)
{
    static int count = 0;
    count++;
    printf("This function has been called %d times.\n", count);
}

int main(void)
{
    show_call_count();
    show_call_count();
    show_call_count();
    return 0;
}