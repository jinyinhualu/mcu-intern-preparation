#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
场景题：仓库温湿度采集终端

你在给一块简化的单片机终端写程序，它要做三件事：
1. 上电后显示设备信息。
2. 收到一帧温湿度数据后，缓存、解析、计数。
3. 故意埋一次越界和一次野指针，看看编译器和调试器怎么说。

建议输入帧：
  "T=26.5,H=58"

建议规则：
  - 温度超过 30.0 或湿度超过 80 时，记一次告警。
  - 收到合法帧时，采样计数 +1。

你要完成的内容：
1. 内存区域观察
   - 1 个全局变量
   - 1 个未初始化全局变量
   - 1 个文件静态变量
   - 1 个未初始化文件静态变量
   - 1 个普通局部变量
   - 1 个局部数组
   - 1 个局部静态变量
   - 2 块堆内存

2. 越界实验
   - 用一个固定长度缓冲区接收帧
   - 故意让它越界一次

3. 野指针实验
   - 选一种：
     - 未初始化指针
     - free 之后继续使用
*/

int g_device_id = 1001;
int g_error_count;
static int s_total_samples;
static int s_last_alarm;

typedef struct
{
   int temp_x10;
   int humidity;
   char raw[16];
} SensorFrame;

static void show_memory_areas(void)
{
   int local_var = 42;
   int local_array[5] = {0};
   static int local_static_var = 100;
   int *heap1 = malloc(sizeof( *heap1));
   int *heap2 = malloc(sizeof( *heap2));

   printf("Global variable (g_device_id): %p\n",              (void*)&g_device_id);
   printf("Global variable (g_error_count): %p\n",            (void*)&g_error_count);
   printf("File static (s_total_samples): %p\n",              (void*)&s_total_samples);
   printf("File static (s_last_alarm): %p\n",                 (void*)&s_last_alarm);
   printf("Local variable (local_var): %p\n",                 (void*)&local_var);
   printf("Local array (local_array): %p\n",                  (void*)local_array);
   printf("Local static variable (local_static_var): %p\n",   (void*)&local_static_var);
   printf("Heap memory 1 (heap1): %p\n",                      (void*)heap1);
   printf("Heap memory 2 (heap2): %p\n",                      (void*)heap2);

   free(heap1);
   free(heap2);
}

static void Temperature_Humidity_Data_Input(const char *frame_text)
{
   SensorFrame frame = { 0 };
   int *heap_temp = NULL;
   int *heap_humidity = NULL;
   double temp = 0.0;

   snprintf(frame.raw, sizeof(frame.raw), "%s", frame_text);

   if (sscanf(frame.raw, "T=%lf,H=%d", &temp, &frame.humidity) != 2)
   {
      puts("frame parse failed");
      g_error_count++;
      return;
   }

   frame.temp_x10 = (int)(temp * 10.0);

   heap_temp = malloc(sizeof *heap_temp);
   heap_humidity = malloc(sizeof *heap_humidity);
   if (heap_temp == NULL || heap_humidity == NULL)
   {
      puts("malloc failed");
      free(heap_temp);
      free(heap_humidity);
      g_error_count++;
      return;
   }

   *heap_temp = frame.temp_x10;
   *heap_humidity = frame.humidity;

   s_total_samples++;
   if (frame.temp_x10 > 300 || frame.humidity > 80)
   {
      s_last_alarm++;
   }

   printf("frame.raw = %s\n", frame.raw);
   printf("temp_x10 = %d, humidity = %d\n", *heap_temp, *heap_humidity);
   printf("samples = %d, alarms = %d, errors = %d\n",
           s_total_samples, s_last_alarm, g_error_count);

   free(heap_temp);
   free(heap_humidity);

   (void)frame_text;
   (void)frame;
   (void)heap_temp;
   (void)heap_humidity;
}

static void demo_out_of_bounds(void)
{
   int frame_buffer[4] = {0};
   frame_buffer[4] = 123;
}

static void demo_wild_pointer(void)
{
   int* ptr;
   *ptr = 42;
}

int main(void)
{
   puts("Warehouse Temp/Humidity Terminal");
   puts("Frame example: T=26.5,H=58");

   show_memory_areas();
   Temperature_Humidity_Data_Input("T=26.5,H=58");
   demo_out_of_bounds();
   demo_wild_pointer();

   return 0;
}
