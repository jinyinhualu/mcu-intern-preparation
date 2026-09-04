#include <stdio.h>

typedef struct
{
	unsigned long long id;
	char name [20];
	int grade;
} Student;

int main ()
{
	Student std = {2400880134, "张三", 99};
	unsigned long long id_find;

	printf ("请输入你要查询的学号：");
	scanf("%llu", &id_find);

	if (id_find == std.id) printf ("学号：%llu\t姓名：%s\t成绩：%d\n", std.id, std.name, std.grade);
	else printf ("未找到该学生\n");

	return 0;
}