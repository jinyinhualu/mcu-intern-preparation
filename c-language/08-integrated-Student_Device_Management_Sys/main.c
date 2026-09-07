#include <stdio.h>
#include <stddef.h>

typedef enum
{
    IDLE = 0,
    WORKING = 1,
    ERROR = 2
} Status;

typedef union
{
    int   i;
    char  c[5];
    short s;
    float f;
} Value;

typedef struct
{
    char   a;
    int    b;
    short  c;
} S1;

typedef struct
{
    char   a;
    Value  v;
    char   b;
} S2;

typedef struct
{
    Status st;
    char   a;
    double d;
    short  s;
} S3;

typedef struct
{
    char   name[3];
    int    id;
    Value  v;
    Status st;
} S4;

typedef struct
{
    char   a;
    S1     s1;
    Value  v;
    short  s;
} S5;

int main(void)
{
    printf("sizeof(Status) = %zu\n", sizeof(Status));
    printf("sizeof(Value)  = %zu\n\n", sizeof(Value));

    printf("S1: size=%zu, a=%zu, b=%zu, c=%zu\n",
           sizeof(S1), offsetof(S1, a), offsetof(S1, b), offsetof(S1, c));

    printf("S2: size=%zu, a=%zu, v=%zu, b=%zu\n",
           sizeof(S2), offsetof(S2, a), offsetof(S2, v), offsetof(S2, b));

    printf("S3: size=%zu, st=%zu, a=%zu, d=%zu, s=%zu\n",
           sizeof(S3), offsetof(S3, st), offsetof(S3, a), offsetof(S3, d), offsetof(S3, s));

    printf("S4: size=%zu, name=%zu, id=%zu, v=%zu, st=%zu\n",
           sizeof(S4), offsetof(S4, name), offsetof(S4, id), offsetof(S4, v), offsetof(S4, st));

    printf("S5: size=%zu, a=%zu, s1=%zu, v=%zu, s=%zu\n",
           sizeof(S5), offsetof(S5, a), offsetof(S5, s1), offsetof(S5, v), offsetof(S5, s));

    return 0;
}