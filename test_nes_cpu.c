#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>

#include <cmocka.h>

#include "nes_cpu.h"

#define DEFAULT_PC 0x1234
#define DEFAULT_A 0xAB
#define DEFAULT_X 0x35
#define DEFAULT_Y 0x87
#define DEFAULT_STATUS (FLAG_CONSTANT)
#define DEFAULT_SP 0xFD

#define set_instruction(...) do {                                       \
        uint8_t inst[] = { __VA_ARGS__ };                               \
        set_instruction_helper(inst , sizeof(inst) / sizeof(inst[0]));  \
    } while(0)

void set_instruction_helper(uint8_t *data, int num)
{
    uint16_t a = DEFAULT_PC;

    for(int i = 0; i < num; i++) {
        expect_value(read_mem, address, a + i);
        will_return(read_mem, data[i]);
    }
}

uint8_t read_mem(uint16_t address)
{
    // printf("read_mem: %04X\n", address);
    check_expected(address);
    return mock();
}

void write_mem(uint16_t address, uint8_t value)
{
    check_expected(address);
    check_expected(value);
}

struct nes_cpu expected_cpu_state;

void assert_cpu_state(void)
{
    assert_int_equal(cpu.pc, expected_cpu_state.pc);
    assert_int_equal(cpu.a, expected_cpu_state.a);
    assert_int_equal(cpu.x, expected_cpu_state.x);
    assert_int_equal(cpu.y, expected_cpu_state.y);
    assert_int_equal(cpu.sp, expected_cpu_state.sp);
    assert_int_equal(cpu.status, expected_cpu_state.status);
}

void expect_read(uint16_t addr, uint8_t ret)
{
    expect_value(read_mem, address, addr);
    will_return(read_mem, ret);
}

void expect_write(uint16_t addr, uint8_t val)
{
    expect_value(write_mem, address, addr);
    expect_value(write_mem, value, val);
}


/* LDA */

static void test__lda_imm(void **states)
{
    set_instruction(0xA9, 0x12); // LDA #$12

    cpu_step();

    expected_cpu_state.pc += 2;
    expected_cpu_state.a = 0x12;

    assert_cpu_state();
}

static void test__lda_zero(void **states)
{
    set_instruction(0xA5, 0x14); // LDA $14

    expect_read(0x0014, 0x34);

    cpu_step();

    expected_cpu_state.pc += 2;
    expected_cpu_state.a = 0x34;

    assert_cpu_state();
}


static void test__lda_zerox(void **states)
{
    set_instruction(0xB5, 0x14); // LDA $14, X

    cpu.x = 0x21;

    expect_read(0x0014 + 0x0021, 0x34);

    cpu_step();

    expected_cpu_state.pc += 2;
    expected_cpu_state.a = 0x34;
    expected_cpu_state.x = 0x21;

    assert_cpu_state();
}

static void test__lda_abs(void **states)
{
    set_instruction(0xAD, 0xEF, 0xBE); // LDA $BEEF

    expect_read(0xBEEF, 0x73);

    cpu_step();

    expected_cpu_state.pc += 3;
    expected_cpu_state.a = 0x73;

    assert_cpu_state();
}


static void test__lda_absx(void **states)
{
    set_instruction(0xBD, 0xEF, 0xBE); // LDA $BEEF, X

    cpu.x = 0x07;

    expect_read(0xBEEF + 0x07, 0x45);

    cpu_step();

    expected_cpu_state.pc += 3;
    expected_cpu_state.a = 0x45;
    expected_cpu_state.x = 0x07;

    assert_cpu_state();
}

static void test__lda_absy(void **states)
{
    set_instruction(0xB9, 0xEF, 0xBE); // LDA $BEEF, Y

    cpu.y = 0x15;

    expect_read(0xBEEF + 0x15, 0x56);

    cpu_step();

    expected_cpu_state.pc += 3;
    expected_cpu_state.a = 0x56;
    expected_cpu_state.y = 0x15;

    assert_cpu_state();
}

static void test__lda_indx(void **states)
{
    set_instruction(0xA1, 0xEF); // LDA ($EF, X)

    cpu.x = 0x03;

    expect_read(0x00EF + 0x03, 0x56);

    expect_read(0x00F0 + 0x03, 0x78);

    expect_read(0x7856, 0x11);

    cpu_step();

    expected_cpu_state.pc += 2;
    expected_cpu_state.a = 0x11;
    expected_cpu_state.x = 0x03;


    assert_cpu_state();
}


static void test__lda_indy(void **states)
{
    set_instruction(0xB1, 0xEF); // LDA ($EF), Y

    cpu.y = 0x04;

    expect_read(0x00EF, 0x56);

    expect_read(0x00F0, 0x78);

    expect_read(0x7856 + 0x04, 0x12);

    cpu_step();

    expected_cpu_state.pc += 2;
    expected_cpu_state.a = 0x12;
    expected_cpu_state.y = 0x04;


    assert_cpu_state();
}

static void test__lda_imm_flag_zero(void **states)
{
    set_instruction(0xA9, 0x00); // LDA #$00

    cpu_step();

    expected_cpu_state.pc += 2;
    expected_cpu_state.a = 0x00;
    expected_cpu_state.status = FLAG_CONSTANT | FLAG_ZERO;

    assert_cpu_state();
}

static void test__lda_imm_flag_neg(void **states)
{
    set_instruction(0xA9, -7); // LDA #-7

    cpu_step();

    expected_cpu_state.pc += 2;
    expected_cpu_state.a = -7;
    expected_cpu_state.status = FLAG_CONSTANT | FLAG_NEGATIVE;


    assert_cpu_state();
}

static void test__lda_indx_wrap(void **states)
{
    set_instruction(0xA1, 0xFE); // LDA ($FE, X)

    cpu.x = 0x01;

    expect_read(0x00FF, 0x56);

    expect_read(0x0000, 0x78);

    expect_read(0x7856, 0x11);

    cpu_step();

    expected_cpu_state.pc += 2;
    expected_cpu_state.a = 0x11;
    expected_cpu_state.x = 0x01;


    assert_cpu_state();
}

static void test__lda_indy_wrap(void **states)
{
    set_instruction(0xB1, 0xFF); // LDA ($FF), Y

    cpu.y = 0x04;

    expect_read(0x00FF, 0x56);

    expect_read(0x0000, 0x78);

    expect_read(0x7856 + 0x04, 0x12);

    cpu_step();

    expected_cpu_state.pc += 2;
    expected_cpu_state.a = 0x12;
    expected_cpu_state.y = 0x04;


    assert_cpu_state();
}


/**
 * This is run once before all group tests
 */
static int setup_group(void** state)
{
    return 0;
}

/**
 * This is run once after all group tests
 */
static int teardown_group(void** state)
{
    return 0;
}

/**
 * This is run once before one given test
 */
static int setup_test(void** state)
{
    cpu.pc = DEFAULT_PC;
    cpu.a = DEFAULT_A;
    cpu.x = DEFAULT_X;
    cpu.y = DEFAULT_Y;
    cpu.status = DEFAULT_STATUS;
    cpu.sp = DEFAULT_SP;
    cpu.irq_pending = 0;
    cpu.clock = 0;

    expected_cpu_state.pc = DEFAULT_PC;
    expected_cpu_state.a = DEFAULT_A;
    expected_cpu_state.x = DEFAULT_X;
    expected_cpu_state.y = DEFAULT_Y;
    expected_cpu_state.status = DEFAULT_STATUS;
    expected_cpu_state.sp = DEFAULT_SP;
    expected_cpu_state.irq_pending = 0;
    expected_cpu_state.clock = 0xBAD;

    return 0;
}

static void test__lda_zerox_wrap(void **states)
{
    set_instruction(0xB5, 0xFF); // LDA $FF, X

    cpu.x = 0x02;

    expect_read(0x0001, 0x34);

    cpu_step();

    expected_cpu_state.pc += 2;
    expected_cpu_state.a = 0x34;
    expected_cpu_state.x = 0x02;

    assert_cpu_state();
}



/* LDX */

static void test__ldx_imm(void **states)
{
    set_instruction(0xA2, 0x12); // LDX #$12

    cpu_step();

    expected_cpu_state.pc += 2;
    expected_cpu_state.x = 0x12;

    assert_cpu_state();
}

static void test__ldx_zero(void **states)
{
    set_instruction(0xA6, 0x14); // LDX $14

    expect_read(0x0014, 0x34);

    cpu_step();

    expected_cpu_state.pc += 2;
    expected_cpu_state.x = 0x34;

    assert_cpu_state();
}


static void test__ldx_zeroy(void **states)
{
    set_instruction(0xB6, 0x14); // LDX $14, Y
    cpu.y = 0x0021;

    expect_read(0x0014 + 0x0021, 0x34);

    cpu_step();

    expected_cpu_state.pc += 2;
    expected_cpu_state.x = 0x34;
    expected_cpu_state.y = 0x21;

    assert_cpu_state();
}

static void test__ldx_abs(void **states)
{
    set_instruction(0xAE, 0xEF, 0xBE); // LDX $BEEF

    expect_read(0xBEEF, 0x73);

    cpu_step();

    expected_cpu_state.pc += 3;
    expected_cpu_state.x = 0x73;

    assert_cpu_state();
}


static void test__ldx_absy(void **states)
{
    set_instruction(0xBE, 0xEF, 0xBE); // LDX $BEEF, Y

    cpu.y = 0x15;

    expect_read(0xBEEF + 0x15, 0x56);

    cpu_step();

    expected_cpu_state.pc += 3;
    expected_cpu_state.x = 0x56;
    expected_cpu_state.y = 0x15;

    assert_cpu_state();
}



/* LDY */

static void test__ldy_imm(void **states)
{
    set_instruction(0xA0, 0x12); // LDY #$12

    cpu_step();

    expected_cpu_state.pc += 2;
    expected_cpu_state.y = 0x12;

    assert_cpu_state();
}

static void test__ldy_zero(void **states)
{
    set_instruction(0xA4, 0x14); // LDY $14

    expect_read(0x0014, 0x34);

    cpu_step();

    expected_cpu_state.pc += 2;
    expected_cpu_state.y = 0x34;

    assert_cpu_state();
}


static void test__ldy_zerox(void **states)
{
    set_instruction(0xB4, 0x14); // LDY $14, Y
    cpu.x = 0x0021;

    expect_read(0x0014 + 0x0021, 0x34);

    cpu_step();

    expected_cpu_state.pc += 2;
    expected_cpu_state.x = 0x21;
    expected_cpu_state.y = 0x34;

    assert_cpu_state();
}

static void test__ldy_abs(void **states)
{
    set_instruction(0xAC, 0xEF, 0xBE); // LDY $BEEF

    expect_read(0xBEEF, 0x73);

    cpu_step();

    expected_cpu_state.pc += 3;
    expected_cpu_state.y = 0x73;

    assert_cpu_state();
}


static void test__ldy_absx(void **states)
{
    set_instruction(0xBC, 0xEF, 0xBE); // LDY $BEEF, Y

    cpu.x = 0x15;

    expect_read(0xBEEF + 0x15, 0x56);

    cpu_step();

    expected_cpu_state.pc += 3;
    expected_cpu_state.x = 0x15;
    expected_cpu_state.y = 0x56;

    assert_cpu_state();
}



/* STA */

static void test__sta_zero(void **states)
{
    set_instruction(0x85, 0x3F); // STA $3F
    cpu.a = 0x77;

    expect_write(0x003F, 0x77);

    cpu_step();

    expected_cpu_state.pc += 2;
    expected_cpu_state.a = 0x77;

    assert_cpu_state();
}

static void test__sta_zerox(void **states)
{
    set_instruction(0x95, 0x3F); // STA $3F, X
    cpu.a = 0x77;
    cpu.x = 0x03;

    expect_write(0x003F + 0x03, 0x77);

    cpu_step();

    expected_cpu_state.pc += 2;
    expected_cpu_state.a = 0x77;
    expected_cpu_state.x = 0x03;

    assert_cpu_state();
}

static void test__sta_abs(void **states)
{
    set_instruction(0x8D, 0xEF, 0xBE); // STA $BEEF
    cpu.a = 0x77;

    expect_write(0xBEEF, 0x77);

    cpu_step();

    expected_cpu_state.pc += 3;
    expected_cpu_state.a = 0x77;

    assert_cpu_state();
}

static void test__sta_absx(void **states)
{
    set_instruction(0x9D, 0xEF, 0xBE); // STA $BEEF, X
    cpu.a = 0x77;
    cpu.x = 0xFF;

    expect_write(0xBEEF + 0xFF, 0x77);

    cpu_step();

    expected_cpu_state.pc += 3;
    expected_cpu_state.a = 0x77;
    expected_cpu_state.x = 0xFF;

    assert_cpu_state();
}

static void test__sta_absy(void **states)
{
    set_instruction(0x99, 0xEF, 0xBE); // STA $BEEF, Y
    cpu.a = 0x77;
    cpu.y = 0xFF;

    expect_write(0xBEEF + 0xFF, 0x77);

    cpu_step();

    expected_cpu_state.pc += 3;
    expected_cpu_state.a = 0x77;
    expected_cpu_state.y = 0xFF;

    assert_cpu_state();
}

static void test__sta_indx(void **states)
{
    set_instruction(0x81, 0x3F); // STA ($3F, X)
    cpu.a = 0x77;
    cpu.x = 0x03;

    expect_read(0x003F + 0x03, 0xEF);
    expect_read(0x003F + 0x03 + 0x01, 0xBE);

    expect_write(0xBEEF, 0x77);

    cpu_step();

    expected_cpu_state.pc += 2;
    expected_cpu_state.a = 0x77;
    expected_cpu_state.x = 0x03;

    assert_cpu_state();
}

static void test__sta_indy(void **states)
{
    set_instruction(0x91, 0x3F); // STA ($3F), Y
    cpu.a = 0x77;
    cpu.y = 0x03;

    expect_read(0x003F, 0xEF);
    expect_read(0x003F + 0x01, 0xBE);

    expect_write(0xBEEF + 0x03, 0x77);

    cpu_step();

    expected_cpu_state.pc += 2;
    expected_cpu_state.a = 0x77;
    expected_cpu_state.y = 0x03;

    assert_cpu_state();
}


/* TAX */

static void test__tax(void **states)
{
    set_instruction(0xAA); // TAX
    cpu.a = 0x77;
    cpu.x = 0x03;

    cpu_step();

    expected_cpu_state.pc += 1;
    expected_cpu_state.a = 0x77;
    expected_cpu_state.x = 0x77;

    assert_cpu_state();
}

static void test__tax_flag_zero(void **states)
{
    set_instruction(0xAA); // TAX
    cpu.a = 0x00;
    cpu.x = 0x03;

    cpu_step();

    expected_cpu_state.pc += 1;
    expected_cpu_state.a = 0x00;
    expected_cpu_state.x = 0x00;
    expected_cpu_state.status = FLAG_CONSTANT | FLAG_ZERO;

    assert_cpu_state();
}

static void test__tax_flag_neg(void **states)
{
    set_instruction(0xAA); // TAX
    cpu.a = 0xF0;
    cpu.x = 0x03;

    cpu_step();

    expected_cpu_state.pc += 1;
    expected_cpu_state.a = 0xF0;
    expected_cpu_state.x = 0xF0;
    expected_cpu_state.status = FLAG_CONSTANT | FLAG_NEGATIVE;

    assert_cpu_state();
}



/**
 * This is run once after one given test
 */
static int teardown_test(void** state)
{
    return 0;
}



const struct CMUnitTest all_tests[] = {
    cmocka_unit_test_setup_teardown(test__lda_imm, setup_test, teardown_test),
    cmocka_unit_test_setup_teardown(test__lda_zero, setup_test, teardown_test),
    cmocka_unit_test_setup_teardown(test__lda_zerox, setup_test, teardown_test),
    cmocka_unit_test_setup_teardown(test__lda_abs, setup_test, teardown_test),
    cmocka_unit_test_setup_teardown(test__lda_absx, setup_test, teardown_test),
    cmocka_unit_test_setup_teardown(test__lda_absy, setup_test, teardown_test),
    cmocka_unit_test_setup_teardown(test__lda_indx, setup_test, teardown_test),
    cmocka_unit_test_setup_teardown(test__lda_indy, setup_test, teardown_test),

    cmocka_unit_test_setup_teardown(test__lda_imm_flag_zero, setup_test, teardown_test),
    cmocka_unit_test_setup_teardown(test__lda_imm_flag_neg, setup_test, teardown_test),

    cmocka_unit_test_setup_teardown(test__lda_indx_wrap, setup_test, teardown_test),
    cmocka_unit_test_setup_teardown(test__lda_indy_wrap, setup_test, teardown_test),
    cmocka_unit_test_setup_teardown(test__lda_zerox_wrap, setup_test, teardown_test),

    cmocka_unit_test_setup_teardown(test__ldx_imm, setup_test, teardown_test),
    cmocka_unit_test_setup_teardown(test__ldx_zero, setup_test, teardown_test),
    cmocka_unit_test_setup_teardown(test__ldx_zeroy, setup_test, teardown_test),
    cmocka_unit_test_setup_teardown(test__ldx_abs, setup_test, teardown_test),
    cmocka_unit_test_setup_teardown(test__ldx_absy, setup_test, teardown_test),

    cmocka_unit_test_setup_teardown(test__ldy_imm, setup_test, teardown_test),
    cmocka_unit_test_setup_teardown(test__ldy_zero, setup_test, teardown_test),
    cmocka_unit_test_setup_teardown(test__ldy_zerox, setup_test, teardown_test),
    cmocka_unit_test_setup_teardown(test__ldy_abs, setup_test, teardown_test),
    cmocka_unit_test_setup_teardown(test__ldy_absx, setup_test, teardown_test),


    cmocka_unit_test_setup_teardown(test__sta_zero, setup_test, teardown_test),
    cmocka_unit_test_setup_teardown(test__sta_zerox, setup_test, teardown_test),
    cmocka_unit_test_setup_teardown(test__sta_abs, setup_test, teardown_test),
    cmocka_unit_test_setup_teardown(test__sta_absx, setup_test, teardown_test),
    cmocka_unit_test_setup_teardown(test__sta_absy, setup_test, teardown_test),
    cmocka_unit_test_setup_teardown(test__sta_indx, setup_test, teardown_test),
    cmocka_unit_test_setup_teardown(test__sta_indy, setup_test, teardown_test),

    cmocka_unit_test_setup_teardown(test__tax, setup_test, teardown_test),
    cmocka_unit_test_setup_teardown(test__tax_flag_zero, setup_test, teardown_test),
    cmocka_unit_test_setup_teardown(test__tax_flag_neg, setup_test, teardown_test),

};

int main(void)
{
    int fails = 0;
    fails += cmocka_run_group_tests(all_tests, setup_group, teardown_group);

    return fails;
}
