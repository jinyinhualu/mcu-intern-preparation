#include <stdio.h>
#include <stddef.h>

typedef enum
{
    PKT_TEMP = 1,
    PKT_MOTER = 2,
    PKT_ERROR = 3
} PacketType;

typedef struct
{
    short   rmp;
    char    dir;
} MotorInfo;

typedef union 
{
    float       temperature;
    MotorInfo   motor;
    int         errorCode;
    char        raw[7];
} Payload;

typedef struct {
    char  tag;
    int   id;
    short seq;
} PacketA;

typedef struct {
    char       header;
    PacketType type;
    Payload    data;
    char       checksum;
} PacketB;

typedef struct {
    double  timestamp;
    char    flag;
    Payload data;
    short   len;
} PacketC;

typedef struct {
    char    start;
    PacketA a;
    Payload data;
    char    end;
} PacketD;

int main(void)
{
    printf("Size of PacketType: %zu\n", sizeof(PacketType));                                                                                                                    // 4
    printf("Size of MotorInfo: %zu\n", sizeof(MotorInfo));                                                                                                                      // 4
    printf("Size of Payload: %zu\n", sizeof(Payload));                                                                                                                          // 8


    printf("Size of PacketA: %zu\n", sizeof(PacketA));                                                                                                                          // 12
    printf("tag: %zu, id: %zu, seq: %zu\n", offsetof(PacketA, tag), offsetof(PacketA, id), offsetof(PacketA, seq));                                                             // 0, 4, 8

    printf("Size of PacketB: %zu\n", sizeof(PacketB));                                                                                                                          // 20
    printf("header: %zu, type: %zu, data: %zu, checksum: %zu\n", offsetof(PacketB, header), offsetof(PacketB, type), offsetof(PacketB, data), offsetof(PacketB, checksum));     // 0, 4, 8, 16

    printf("Size of PacketC: %zu\n", sizeof(PacketC));                                                                                                                          // 32
    printf("timestamp: %zu, flag: %zu, data: %zu, len: %zu\n", offsetof(PacketC, timestamp), offsetof(PacketC, flag), offsetof(PacketC, data), offsetof(PacketC, len));         // 0, 8, 16, 24

    printf("Size of PacketD: %zu\n", sizeof(PacketD));                                                                                                                          // 28
    printf("start: %zu, a: %zu, data: %zu, end: %zu\n", offsetof(PacketD, start), offsetof(PacketD, a), offsetof(PacketD, data), offsetof(PacketD, end));                       // 0, 4, 16, 24

    return 0;
}