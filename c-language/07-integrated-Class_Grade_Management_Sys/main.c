#include <stdio.h>
#include <string.h>

int score[10][3];
char name[10][20];

int state = 0;

int Input(char* ptr_name, int* ptr_score);
void OutputAll(int score[10][3], char name[10][20], int count);
void CalculateAverage(const int score[10][3], int count);
void OutputSingle(int score[10][3], char name[10][20], int count);
void FindTopStudent(const int score[10][3], const char name[10][20], int count);

int main(void) 
{
    int input_count = 0;
    int* ptr_score = score[0];
    char* ptr_name = name[0];

    while (state != 6) {
        printf("1. Input\n");
        printf("2. Search all records\n");
        printf("3. Search single record\n");
        printf("4. Calculate single subject average\n");
        printf("5. Find Top Student\n");
        printf("6. Exit\n");
        printf("Select: ");
        scanf("%d", &state);
        switch (state)
        {
            case 1: input_count = Input(ptr_name, ptr_score);       break;
            case 2: OutputAll(score, name, input_count);            break;
            case 3: OutputSingle(score, name, input_count);         break;
            case 4: CalculateAverage(score, input_count);           break;
            case 5: FindTopStudent(score, name, input_count);       break;
            case 6: printf("Exiting...\n");                         break;
            default:printf("Invalid option. Please try again.\n");  break;
        }
    }

    return 0;
}

int Input(char* ptr_name, int* ptr_score)
{
    static int input_count = 0;
    if (input_count >= 10)
    {
        printf("Maximum limit exceeded!\n");
        return input_count;
    }

    char *cur_name = ptr_name + input_count * 20;
    int  *cur_score = ptr_score + input_count * 3;

    printf("Student %d Enter name: ", input_count + 1);
    scanf("%19s", cur_name);

    printf("Enter score1: ");
    scanf("%d", cur_score + 0);
    printf("Enter score2: ");
    scanf("%d", cur_score + 1);
    printf("Enter score3: ");
    scanf("%d", cur_score + 2);

    input_count++;
    return input_count;
}

void OutputAll(int score[10][3], char name[10][20], int count)
{
    if (count <= 0)
    {
        printf("No records to display.\n");
        return;
    }

    printf("Name\tScore1\tScore2\tScore3\n");
    for (int i = 0; i < count; i++)
    {
        printf("%s\t%d\t%d\t%d\n", name[i], score[i][0], score[i][1], score[i][2]);
    }
}

void OutputSingle(int score[10][3], char name[10][20], int count)
{
    if (count <= 0)
    {
        printf("No records to search.\n");
        return;
    }

    char search_name[20];
    printf("Enter name to search: ");
    scanf("%19s", search_name);

    char* ptr_name = NULL;
    int* ptr_score = NULL;

    for (int i = 0; i < count; i++)
    {
        if (strcmp(name[i], search_name) == 0)
        {
            ptr_name = name[i];
            ptr_score = score[i];
            break;
        }
    }
    if (ptr_name == NULL || ptr_score == NULL)
    {
        printf("Student not found.\n");
        return;
    }
    printf("Name: %s\n", ptr_name);
    printf("Score1: %d\n", *ptr_score);
    printf("Score2: %d\n", *(ptr_score + 1));
    printf("Score3: %d\n", *(ptr_score + 2));
}

void CalculateAverage(const int score[10][3], int count)
{
    if (count <= 0)
    {
        printf("No records to calculate average.\n");
        return;
    }

    float average[3] = {0.0, 0.0, 0.0};
    for (int i = 0; i < count; i++)
    {
        average[0] += score[i][0];
        average[1] += score[i][1];
        average[2] += score[i][2];
    }
    for (int i = 0; i < 3; i++)
    {
        average[i] /= count;
    }
    printf("Average scores:\n");
    printf("Subject 1: %.2f\n", average[0]);
    printf("Subject 2: %.2f\n", average[1]);
    printf("Subject 3: %.2f\n", average[2]);
}

void FindTopStudent(const int score[10][3], const char name[10][20], int count)
{
    if (count <= 0)
    {
        printf("No records to find top student.\n");
        return;
    }

    int max_total = -1;
    int top_student_index = -1;

    for (int i = 0; i < count; i++)
    {
        int total = score[i][0] + score[i][1] + score[i][2];
        if (total > max_total)
        {
            max_total = total;
            top_student_index = i;
        }
    }

    if (top_student_index >= 0)
    {
        printf("Top Student: %s\n", name[top_student_index]);
        printf("Total Score: %d\n", max_total);
    }
    else
    {
        printf("No students found.\n");
    }
}
