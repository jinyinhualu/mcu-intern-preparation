#include <stdio.h>

int main(void)
{
    unsigned long long n = 0x2CAD;
    int bit;

    printf("Enter bit index: ");
    scanf("%d", &bit);

    if (bit < 0 || bit >= 64)
    {
        printf("bit out of range\n");
        return 0;
    }

    if ((n >> bit - 1) & 1ULL) printf("This bit is 1\n");
    else printf("This bit is 0\n");

    return 0;
}