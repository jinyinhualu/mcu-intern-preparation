#include <stdint.h>
#include <stdio.h>

/*
综合题：电机控制板寄存器掩码练习

场景：
  你在调一块简化的电机控制板。控制寄存器 CTRL_REG 用来开关功能，
  状态寄存器 STATUS_REG 用来读取当前模式和故障码。

你要独立完成下面 3 个宏：
  1. SET_BIT(REG, BIT)        设置 REG 的某一位
  2. CLEAR_BIT(REG, BIT)      清除 REG 的某一位
  3. READ_FIELD(REG, MASK, POS) 从 REG 中读取字段值

要求：
  - 宏参数和整体表达式都要认真加括号。
  - SET_BIT / CLEAR_BIT 用作语句时，宏定义末尾不要自带分号。
  - 注意副作用：像 bank[next_index()] 这样的参数不应该被重复求值。
  - 建议使用 UINT32_C(1) 生成 32 位位掩码。
*/

#define SET_BIT(REG, BIT)                   (REG |= (UINT32_C(0x00000001) << (BIT)))
#define CLEAR_BIT(REG, BIT)                 (REG &= (~(UINT32_C(0x00000001) << (BIT))))
#define READ_FIELD(REG, MASK, POS)          (((REG) & (MASK)) >> (POS))
#define WRITE_FIELD(REG, MASK, POS, VALUE)  ((REG) = ((REG) & ~(MASK)) | ((((uint32_t)(VALUE)) << (POS)) & (MASK)))

#define CTRL_ENABLE_BIT       0u
#define CTRL_FAULT_RESET_BIT  2u
#define CTRL_IRQ_ENABLE_BIT   3u

#define STATUS_MODE_POS       4u
#define STATUS_MODE_MASK      (UINT32_C(0x7) << STATUS_MODE_POS)

#define STATUS_FAULT_POS      8u
#define STATUS_FAULT_MASK     (UINT32_C(0xF) << STATUS_FAULT_POS)

#define MAKE_FIELD(VALUE, POS, MASK)  (((uint32_t)(VALUE) << (POS)) & (MASK))

static int g_total_tests = 0;       // 总测试数
static int g_failed_tests = 0;      // 失败测试数

static int g_index_calls = 0;       // next_index() 调用次数
static int g_bit_calls = 0;         // next_bit() 调用次数
static int g_status_calls = 0;      // read_status_register() 调用次数
static int g_mask_calls = 0;        // read_fault_mask() 调用次数
static int g_pos_calls = 0;         // read_fault_pos() 调用次数

typedef struct
{
    volatile uint32_t CTRL;
    volatile uint32_t STATUS;
} MotorPeripheral;

static void check_u32(const char *name, uint32_t actual, uint32_t expected)
{
    g_total_tests++;
    if (actual == expected)
    {
        printf("[PASS] %s => 0x%08X\n", name, actual);
    }
    else
    {
        g_failed_tests++;
        printf("[FAIL] %s => actual 0x%08X, expected 0x%08X\n",
               name, actual, expected);
    }
}

static void check_int(const char *name, int actual, int expected)
{
    g_total_tests++;
    if (actual == expected)
    {
        printf("[PASS] %s => %d\n", name, actual);
    }
    else
    {
        g_failed_tests++;
        printf("[FAIL] %s => actual %d, expected %d\n",
               name, actual, expected);
    }
}

static uint32_t build_motor_config(uint32_t mode, uint32_t fault_code)
{
    uint32_t reg = 0u; 

    SET_BIT(reg, CTRL_ENABLE_BIT);
    SET_BIT(reg, CTRL_IRQ_ENABLE_BIT);
    WRITE_FIELD(reg, STATUS_MODE_MASK, STATUS_MODE_POS, mode);
    WRITE_FIELD(reg, STATUS_FAULT_MASK, STATUS_FAULT_POS, fault_code);

    return reg;
}

static size_t next_index(void)
{
    g_index_calls++;
    return 1u;
}

static unsigned int next_bit(void)
{
    g_bit_calls++;
    return CTRL_IRQ_ENABLE_BIT;
}

static uint32_t read_status_register(void)
{
    g_status_calls++;
    return MAKE_FIELD(5u, STATUS_MODE_POS, STATUS_MODE_MASK)
         | MAKE_FIELD(9u, STATUS_FAULT_POS, STATUS_FAULT_MASK);
}

static uint32_t read_fault_mask(void)
{
    g_mask_calls++;
    return STATUS_FAULT_MASK;
}

static unsigned int read_fault_pos(void)
{
    g_pos_calls++;
    return STATUS_FAULT_POS;
}

static void set_bit_reg(volatile uint32_t *reg, unsigned int bit)
{
    SET_BIT(*reg, bit);
}

static void clear_bit_reg(volatile uint32_t *reg, unsigned int bit)
{
    CLEAR_BIT(*reg, bit);
}

static void write_field_reg(volatile uint32_t *reg, uint32_t mask, unsigned int pos, uint32_t value)
{
    WRITE_FIELD(*reg, mask, pos, value);
}

static void motor_init(MotorPeripheral *motor, uint32_t mode, uint32_t fault_code)
{
    CLEAR_BIT(motor->STATUS, STATUS_MODE_POS);
    CLEAR_BIT(motor->STATUS, STATUS_FAULT_POS);
    SET_BIT(motor->CTRL, CTRL_ENABLE_BIT);
    SET_BIT(motor->CTRL, CTRL_IRQ_ENABLE_BIT);

    WRITE_FIELD(motor->STATUS, STATUS_MODE_MASK, STATUS_MODE_POS, mode);
    WRITE_FIELD(motor->STATUS, STATUS_FAULT_MASK, STATUS_FAULT_POS, fault_code);
}

static void run_basic_register_flow(void)
{
    uint32_t ctrl_reg = 0u;
    uint32_t status_reg = 0u;
    uint32_t mode = 0u;
    uint32_t fault = 0u;

    puts("\n== Basic register flow ==");

    SET_BIT(ctrl_reg, CTRL_ENABLE_BIT);
    SET_BIT(ctrl_reg, CTRL_IRQ_ENABLE_BIT);
    SET_BIT(ctrl_reg, CTRL_FAULT_RESET_BIT);
    CLEAR_BIT(ctrl_reg, CTRL_FAULT_RESET_BIT);

    check_u32("CTRL enable + irq, fault reset cleared", ctrl_reg, UINT32_C(0x00000009));

    status_reg = MAKE_FIELD(3u, STATUS_MODE_POS, STATUS_MODE_MASK)
               | MAKE_FIELD(12u, STATUS_FAULT_POS, STATUS_FAULT_MASK);

    mode = READ_FIELD(status_reg, STATUS_MODE_MASK, STATUS_MODE_POS);
    fault = READ_FIELD(status_reg, STATUS_FAULT_MASK, STATUS_FAULT_POS);

    (void)status_reg;

    check_u32("READ_FIELD mode", mode, 3u);
    check_u32("READ_FIELD fault", fault, 12u);
}

static void run_parentheses_tests(void)
{
    uint32_t ctrl_reg = 0u;
    uint32_t status_reg = MAKE_FIELD(5u, STATUS_MODE_POS, STATUS_MODE_MASK);
    uint32_t combined = 0u;

    puts("\n== Parentheses tests ==");

    SET_BIT(ctrl_reg, 1u | 2u);
    check_u32("SET_BIT with bit expression", ctrl_reg, UINT32_C(0x00000008));

    ctrl_reg = UINT32_C(0xFFFFFFFF);
    CLEAR_BIT(ctrl_reg, 1u | 2u);
    check_u32("CLEAR_BIT with bit expression", ctrl_reg, UINT32_C(0xFFFFFFF7));

    combined = 10u + READ_FIELD(status_reg, STATUS_MODE_MASK, STATUS_MODE_POS) * 2u;
    (void)status_reg;
    check_u32("READ_FIELD used inside arithmetic", combined, 20u);
}

static void run_side_effect_tests(void)
{
    uint32_t bank[2] = {0u, 0u};
    uint32_t fault = 0u;

    puts("\n== Side-effect tests ==");

    g_index_calls = 0;
    g_bit_calls = 0;
    SET_BIT(bank[next_index()], next_bit());
    check_u32("SET_BIT side-effect target", bank[1], UINT32_C(0x00000008));
    check_int("SET_BIT index evaluated once", g_index_calls, 1);
    check_int("SET_BIT bit evaluated once", g_bit_calls, 1);

    g_index_calls = 0;
    g_bit_calls = 0;
    CLEAR_BIT(bank[next_index()], next_bit());
    check_u32("CLEAR_BIT side-effect target", bank[1], UINT32_C(0x00000000));
    check_int("CLEAR_BIT index evaluated once", g_index_calls, 1);
    check_int("CLEAR_BIT bit evaluated once", g_bit_calls, 1);

    g_status_calls = 0;
    g_mask_calls = 0;
    g_pos_calls = 0;
    fault = READ_FIELD(read_status_register(), read_fault_mask(), read_fault_pos());
    check_u32("READ_FIELD side-effect result", fault, 9u);
    check_int("READ_FIELD reg evaluated once", g_status_calls, 1);
    check_int("READ_FIELD mask evaluated once", g_mask_calls, 1);
    check_int("READ_FIELD pos evaluated once", g_pos_calls, 1);
}

static void run_write_field_tests(void)
{
    uint32_t reg = UINT32_C(0xFFFFFFFF);

    puts("\n== Write field tests ==");

    WRITE_FIELD(reg, STATUS_MODE_MASK, STATUS_MODE_POS, 5u);
    check_u32("WRITE_FIELD mode = 5", reg, UINT32_C(0xFFFFFFDF));

    reg = 0u;
    WRITE_FIELD(reg, STATUS_MODE_MASK, STATUS_MODE_POS, 15u);
    check_u32("WRITE_FIELD clips oversized value", reg, UINT32_C(0x00000070));
}

static void run_motor_config_tests(void)
{
    uint32_t reg = 0u;

    puts("\n== Motor config tests ==");

    reg = build_motor_config(6u, 10u);

    check_u32("motor config enable + irq + mode + fault", reg, UINT32_C(0x00000A69));
}

static void run_volatile_register_tests(void)
{
    volatile uint32_t reg = 0u;

    puts("\n== Volatile register tests ==");

    set_bit_reg(&reg, CTRL_ENABLE_BIT);
    set_bit_reg(&reg, CTRL_IRQ_ENABLE_BIT);
    write_field_reg(&reg, STATUS_MODE_MASK, STATUS_MODE_POS, 4u);
    clear_bit_reg(&reg, CTRL_IRQ_ENABLE_BIT);

    check_u32("volatile register config",
              (uint32_t)reg,
              UINT32_C(0x00000041));
}

static void run_peripheral_tests(void)
{
    MotorPeripheral motor = {0u, 0u};

    puts("\n== Motor peripheral tests ==");

    motor_init(&motor, 2u, 7u);

    check_u32("motor CTRL register", (uint32_t)motor.CTRL, UINT32_C(0x00000009));
    check_u32("motor STATUS register", (uint32_t)motor.STATUS, UINT32_C(0x00000720));
}

int main(void)
{
    puts("Register Mask Lab");
    puts("Goal: implement SET_BIT, CLEAR_BIT, READ_FIELD.");

    (void)&next_index;
    (void)&next_bit;
    (void)&read_status_register;
    (void)&read_fault_mask;
    (void)&read_fault_pos;

    run_basic_register_flow();
    run_parentheses_tests();
    run_side_effect_tests();
    run_write_field_tests();
    run_motor_config_tests();
    run_volatile_register_tests();
    run_peripheral_tests();

    printf("\nSummary: %d/%d passed\n", g_total_tests - g_failed_tests, g_total_tests);
    if (g_failed_tests == 0)
    {
        puts("ALL TESTS PASSED");
    }
    else
    {
        puts("Keep going: finish the three TODO macros at the top of main.c.");
    }

    return g_failed_tests == 0 ? 0 : 1;
}
