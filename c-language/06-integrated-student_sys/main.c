#include <stdio.h>

typedef struct
{
    char name[30];
    int grade;
} Course;

typedef struct
{
    unsigned long long id;
    char name[20];
    Course cs;
} Student;

volatile int g_flag = 0;

void Information_Show(const Student std[], int n);
void Search_Extreme(const Student std[], int n);
void Exchange(Student *idx);
void Volatile_Demo(void);

int main(void)
{
    Student std[3] = {
        {2400880132ULL, "Jone", {"Computer System", 80}},
        {2400880133ULL, "Bon", {"Database", 90}},
        {2400880134ULL, "Lili", {"Math", 50}}
    };

    Information_Show(std, 3);
    Search_Extreme(std, 3);
    Exchange(std);
    Information_Show(std, 3);
    Volatile_Demo();

    return 0;
}

void Information_Show(const Student std[], int n)
{
    static int count = 0;
    count++;
    printf("Information_Show called %d times\n", count);

    for (int i = 0; i < n; i++) printf("%llu\t%s\t%s\t%d\n", std[i].id, std[i].name, std[i].cs.name, std[i].cs.grade);
}

void Search_Extreme(const Student std[], int n)
{
    int max_i = 0, min_i = 0;

    for (int i = 1; i < n; i++)
    {
        if (std[i].cs.grade > std[max_i].cs.grade) max_i = i;
        if (std[i].cs.grade < std[min_i].cs.grade) min_i = i;
    }

    printf("Highest: %llu\t%s\t%s\t%d\n", std[max_i].id, std[max_i].name, std[max_i].cs.name, std[max_i].cs.grade);
    printf("Lowest : %llu\t%s\t%s\t%d\n", std[min_i].id, std[min_i].name, std[min_i].cs.name, std[min_i].cs.grade);
}

void Exchange(Student *idx)
{
    Student temp = idx[0];
    idx[0] = idx[1];
    idx[1] = temp;
}

void Volatile_Demo(void)
{
    if (g_flag == 0) printf("volatile flag = %d\n", g_flag);
}