#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "nes_cpu.h"

#define STACK_BASE     0x100

struct nes_cpu cpu = {.status = FLAG_CONSTANT};

#ifdef DEBUG_INSTRUCTION_LOG
#define DISPRINTF(...) sprintf(__VA_ARGS__)
#else
#define DISPRINTF(...) do { } while(0)
#endif

#define read_mem_zero(addr) read_mem(addr&0x00FF)
#define write_mem_zero(addr,val) write_mem(addr&0x00FF,val)

static const uint8_t clock_table [256] =
{// 0 1 2 3 4 5 6 7 8 9 A B C D E F
    7,6,2,8,3,3,5,5,3,2,2,2,4,4,6,6,// 0
    3,5,2,8,4,4,6,6,2,4,2,7,4,4,7,7,// 1
    6,6,2,8,3,3,5,5,4,2,2,2,4,4,6,6,// 2
    3,5,2,8,4,4,6,6,2,4,2,7,4,4,7,7,// 3
    6,6,2,8,3,3,5,5,3,2,2,2,3,4,6,6,// 4
    3,5,2,8,4,4,6,6,2,4,2,7,4,4,7,7,// 5
    6,6,2,8,3,3,5,5,4,2,2,2,5,4,6,6,// 6
    3,5,2,8,4,4,6,6,2,4,2,7,4,4,7,7,// 7
    2,6,2,6,3,3,3,3,2,2,2,2,4,4,4,4,// 8
    3,6,2,6,4,4,4,4,2,5,2,5,5,5,5,5,// 9
    2,6,2,6,3,3,3,3,2,2,2,2,4,4,4,4,// A
    3,5,2,5,4,4,4,4,2,4,2,4,4,4,4,4,// B
    2,6,2,8,3,3,5,5,2,2,2,2,4,4,6,6,// C
    3,5,2,8,4,4,6,6,2,4,2,7,4,4,7,7,// D
    2,6,2,8,3,3,5,5,2,2,2,2,4,4,6,6,// E
    3,5,0,8,4,4,6,6,2,4,2,7,4,4,7,7 // F
};


#define GET_ADDR_ABS() do {                                             \
        const uint8_t d1 = read_mem(cpu.pc++);                          \
        const uint8_t d2 = read_mem(cpu.pc++);                          \
        addr = ((uint16_t)d2 << 8) + d1;                                \
        DISPRINTF(addr_data_string, "%02X %02X ", d1, d2);              \
        if((op == 0x4C) || (op == 0x20)) {                              \
            DISPRINTF(addr_string, "$%04X", ((uint16_t)d2 << 8) + d1);  \
        } else {                                                        \
            DISPRINTF(addr_string, "$%04X = %02X", ((uint16_t)d2 << 8) + d1, read_mem(addr)); \
        }                                                               \
    } while(0)

#define GET_ADDR_ABSX() do {                                            \
        const uint8_t d1 = read_mem(cpu.pc++);                          \
        const uint8_t d2 = read_mem(cpu.pc++);                          \
        const uint16_t a = ((uint16_t)d2 << 8) + d1;                    \
        addr = a + cpu.x;                                               \
        if((a ^ addr) & 0xFF00) { penalty_clock = 1; };                 \
        DISPRINTF(addr_data_string, "%02X %02X ", d1, d2);              \
        DISPRINTF(addr_string, "$%04X,X @ %04X = %02X", ((uint16_t)d2 << 8) + d1, addr, read_mem(addr)); \
    } while(0)

#define GET_ADDR_ABSY() do {                                            \
        const uint8_t d1 = read_mem(cpu.pc++);                          \
        const uint8_t d2 = read_mem(cpu.pc++);                          \
        const uint16_t a = ((uint16_t)d2 << 8) + d1;                    \
        addr = a + cpu.y;                                               \
        if((a ^ addr) & 0xFF00) { penalty_clock = 1; };                 \
        DISPRINTF(addr_data_string, "%02X %02X ", d1, d2);              \
        DISPRINTF(addr_string, "$%04X,Y @ %04X = %02X", ((uint16_t)d2 << 8) + d1, addr, read_mem(addr)); \
    } while(0)

#define GET_ADDR_ZERO() do {                                            \
        const uint8_t a = read_mem(cpu.pc++);                           \
        addr = a;                                                       \
        DISPRINTF(addr_data_string, "%02X    ", a);                     \
        DISPRINTF(addr_string, "$%02X = %02X", addr & 0x00FF, read_mem(addr)); \
    } while(0)

#define GET_ADDR_ZEROX() do {                                           \
        const uint8_t a = read_mem(cpu.pc++);                           \
        addr = (a + cpu.x) & 0x00FF;                                    \
        DISPRINTF(addr_data_string, "%02X    ", a);                     \
        DISPRINTF(addr_string, "$%02X,X @ %02X = %02X", a, addr, read_mem(addr)); \
    } while(0)

#define GET_ADDR_ZEROY() do {                                           \
        const uint8_t a = read_mem(cpu.pc++);                           \
        addr = (a + cpu.y) & 0x00FF;                                    \
        DISPRINTF(addr_data_string, "%02X    ", a);                     \
        DISPRINTF(addr_string, "$%02X,Y @ %02X = %02X", a, addr, read_mem(addr)); \
    } while(0)

#define GET_ADDR_INDX() do {                                            \
        const uint8_t d1 = read_mem(cpu.pc++);                          \
        const uint8_t a = d1 + cpu.x;                                   \
        const uint8_t d3 = read_mem_zero(a);                            \
        const uint8_t d4 = read_mem_zero((a+1) & 0xFF);                 \
        addr = ((uint16_t)d4 << 8) | (d3 & 0x00FF);                     \
        DISPRINTF(addr_data_string, "%02X    ", d1);                    \
        DISPRINTF(addr_string, "($%02X,X) @ %02X = %04X = %02X", d1, a, addr, read_mem(addr)); \
    } while(0)

#define GET_ADDR_INDY() do {                                            \
        const uint8_t d1 = read_mem(cpu.pc++);                          \
        const uint16_t a1 = d1;                                         \
        const uint8_t d3 = read_mem_zero(a1);                           \
        const uint8_t d4 = read_mem_zero((a1+1) & 0x00FF);              \
        const uint16_t a2 = (((uint16_t)d4 << 8) | (d3 & 0x00FF));      \
        addr = a2 + cpu.y;                                              \
        if((a2 ^ addr) & 0xFF00) { penalty_clock = 1; };                \
        DISPRINTF(addr_data_string, "%02X    ", d1);                    \
        DISPRINTF(addr_string, "($%02X),Y = %04X @ %04X = %02X", a1, a2, addr, read_mem(addr)); \
    } while(0)

#define GET_ADDR_IND() do {                                             \
        const uint8_t d1 = read_mem(cpu.pc++);                          \
        const uint8_t d2 = read_mem(cpu.pc++);                          \
        const uint16_t a = ((uint16_t)d2 << 8) + d1;                    \
        const uint8_t d3 = read_mem(a);                                 \
        const uint8_t d4 = read_mem(((uint16_t)d2 << 8) + ((d1+1) & 0XFF)); \
        addr = ((uint16_t)d4 << 8) | (d3 & 0x00FF);                     \
        DISPRINTF(addr_data_string, "%02X %02X ", d1, d2);              \
        DISPRINTF(addr_string, "($%04X) = %04X", a, addr);              \
    } while(0)


#define GET_ADDR_REL() do {                                             \
        const int8_t offset = read_mem(cpu.pc++);                       \
        addr = cpu.pc + offset;                                         \
        if((addr ^ cpu.pc) & 0xFF00) { penalty_clock = 2; } else { penalty_clock = 1; } \
        DISPRINTF(addr_data_string, "%02X    ", (uint8_t) offset);      \
        DISPRINTF(addr_string, "$%04X", cpu.pc + offset);               \
    } while(0)


#define GET_ADDR_A() do {                       \
        DISPRINTF(addr_string, "A");            \
    } while(0)

#define GET_VALUE_ABS() do { \
        GET_ADDR_ABS();      \
        value = read_mem(addr); \
    } while(0)

#define GET_VALUE_ABSX() do {                   \
        GET_ADDR_ABSX();                        \
        value = read_mem(addr);                 \
        cpu.clock += penalty_clock;             \
    } while(0)

#define GET_VALUE_ABSY() do {                   \
        GET_ADDR_ABSY();                        \
        value = read_mem(addr);                 \
        cpu.clock += penalty_clock;             \
    } while(0)

#define GET_VALUE_ZERO() do {                   \
        GET_ADDR_ZERO();                        \
        value = read_mem_zero(addr);                 \
    } while(0)

#define GET_VALUE_ZEROX() do {                  \
        GET_ADDR_ZEROX();                       \
        value = read_mem_zero(addr);                 \
    } while(0)

#define GET_VALUE_ZEROY() do {                  \
        GET_ADDR_ZEROY();                       \
        value = read_mem_zero(addr);                 \
    } while(0)

#define GET_VALUE_INDX() do {                   \
        GET_ADDR_INDX();                        \
        value = read_mem(addr);                 \
    } while(0)

#define GET_VALUE_INDY() do {                   \
        GET_ADDR_INDY();                        \
        value = read_mem(addr);                 \
        cpu.clock += penalty_clock;             \
    } while(0)

#define GET_VALUE_IMM() do {                                            \
        addr = cpu.pc++;                                                \
        DISPRINTF(addr_data_string, "%02X    ", read_mem(addr));        \
        DISPRINTF(addr_string, "#$%02X", read_mem(addr));               \
        value = read_mem(addr);                                         \
    } while(0)


#define ARITHM_OP(op) /**/                      \
case op: GET_VALUE_INDX();                       \
    goto perform_##op;                          \
case op+0x10: GET_VALUE_INDY();                  \
    goto perform_##op;                          \
case op+0x04: GET_VALUE_ZERO();                  \
    goto perform_##op;                          \
case op+0x14: GET_VALUE_ZEROX();                 \
    goto perform_##op;                          \
case op+0x08: GET_VALUE_IMM();                   \
    goto perform_##op;                          \
case op+0x18: GET_VALUE_ABSY();                  \
    goto perform_##op;                          \
case op+0x0C: GET_VALUE_ABS();                   \
    goto perform_##op;                          \
case op+0x1C: GET_VALUE_ABSX();                  \
    goto perform_##op;                          \
    perform_##op


#define NO_FLAGS()
#define AFFECTED_FLAGS(flags) do { cpu.status &= ~(flags); } while(0)
#define CHECK_ZERO(val) do { if(!val) cpu.status |= FLAG_ZERO; } while(0)
#define CHECK_NEG(val) do { if(val & FLAG_NEGATIVE) cpu.status |= FLAG_NEGATIVE; } while(0)
#define CHECK_CARRY(carry) do { if(carry) cpu.status |= FLAG_CARRY; } while(0)
#define CHECK_OVERFLOW(overflow) do { if(overflow) cpu.status |= FLAG_OVERFLOW; } while(0)

#define STACK_PUSH(val) write_mem(STACK_BASE + cpu.sp--, val)
#define STACK_POP() read_mem(STACK_BASE + ++cpu.sp)

#define BRANCH_IF_SET(flag) do { if(cpu.status & (flag)) { cpu.pc = addr; cpu.clock += penalty_clock; } } while(0)
#define BRANCH_IF_CLEAR(flag) do { if(!(cpu.status & (flag))) { cpu.pc = addr; cpu.clock += penalty_clock; } } while(0)

#ifdef DEBUG_INSTRUCTION_LOG
#ifdef DEBUG_STACK
#define PRINT_OP(name) do {                                             \
        printf("%04X  %02X %s %s %-28sA:%02X X:%02X Y:%02X P:%02X SP:%02X [ ", addr_op, op, addr_data_string, name, addr_string, old_a, old_x, old_y, old_status, old_sp); \
        for(int i = old_sp+1; i <= 0xFF; i++) {                         \
            printf("%02X ", read_mem(0x100 + i));                       \
        }                                                               \
        printf("]\n");                                                  \
    } while(0)
#else
#define PRINT_OP(name) printf("%04X  %02X %s %s %-28sA:%02X X:%02X Y:%02X P:%02X SP:%02X\n", addr_op, op, addr_data_string, name, addr_string, old_a, old_x, old_y, old_status, old_sp)
#endif

#define PRINT_UNDOCUMENTED_OP(name) printf("%04X  %02X %s%s %-28sA:%02X X:%02X Y:%02X P:%02X SP:%02X\n", addr_op, op, addr_data_string, name, addr_string, old_a, old_x, old_y, old_status, old_sp)
#else
#define PRINT_OP(name) do {} while(0)
#define PRINT_UNDOCUMENTED_OP(name) do {} while(0)
#endif



int cpu_step(void)
{
    uint8_t op = read_mem(cpu.pc++);
    uint8_t value;
    uint16_t addr;
    uint8_t penalty_clock = 0;

#ifdef DEBUG_INSTRUCTION_LOG
    char addr_string[32] = "";
    char addr_data_string[32] = "      ";

    const uint16_t addr_op = cpu.pc-1;
    const uint8_t old_a = cpu.a, old_x = cpu.x, old_y = cpu.y, old_sp = cpu.sp, old_status = cpu.status;
#endif

    if(cpu.irq_pending & FLAG_NMI_PENDING) {
        const uint8_t lo = read_mem(0xFFFA);
        const uint8_t hi = read_mem(0xFFFB);
        const uint16_t a = ((uint16_t) hi << 8) | lo;

        STACK_PUSH((cpu.pc & 0xFF00) >> 8);
        STACK_PUSH(cpu.pc & 0x00FF);
        STACK_PUSH(cpu.status);
        cpu.status |= FLAG_INTERRUPT;
        cpu.status &= ~FLAG_BREAK;
        cpu.irq_pending &= ~FLAG_NMI_PENDING;
        cpu.pc = a;
        cpu.clock += 7;

        printf("***NMI***\n");
    } else if(!(cpu.status & FLAG_INTERRUPT) && (cpu.irq_pending & FLAG_IRQ_PENDING)) {
        const uint8_t lo = read_mem(0xFFFE);
        const uint8_t hi = read_mem(0xFFFF);
        const uint16_t a = ((uint16_t) hi << 8) | lo;

        STACK_PUSH((cpu.pc & 0xFF00) >> 8);
        STACK_PUSH(cpu.pc & 0x00FF);
        STACK_PUSH(cpu.status);
        cpu.status |= FLAG_INTERRUPT;
        cpu.status &= ~FLAG_BREAK;
        // The IRQ is level triggered and hence FLAG_IRQ_PENDING should not be reset here
        cpu.pc = a;
        cpu.clock += 7;

        printf("***IRQ***\n");
    } else {
        cpu.clock += clock_table[op];
        switch(op)
        {
        ARITHM_OP(0x01):
            cpu.a |= value;
            AFFECTED_FLAGS(FLAG_ZERO | FLAG_NEGATIVE);
            CHECK_ZERO(cpu.a);
            CHECK_NEG(cpu.a);

            PRINT_OP("ORA");
            break;

        ARITHM_OP(0x21):
            cpu.a &= value;
            AFFECTED_FLAGS(FLAG_ZERO | FLAG_NEGATIVE);
            CHECK_ZERO(cpu.a);
            CHECK_NEG(cpu.a);
            PRINT_OP("AND");
            break;

        ARITHM_OP(0x41):
            cpu.a ^= value;
            AFFECTED_FLAGS(FLAG_ZERO | FLAG_NEGATIVE);
            CHECK_ZERO(cpu.a);
            CHECK_NEG(cpu.a);
            PRINT_OP("EOR");
            break;

        ARITHM_OP(0x61):
            {
                const uint8_t a = cpu.a;
                const uint8_t b = value;
                const uint16_t result = a + b + ((cpu.status & FLAG_CARRY) ? 1 : 0);
                const uint8_t result8 = result & 0xFF;

                AFFECTED_FLAGS(FLAG_CARRY | FLAG_ZERO | FLAG_OVERFLOW | FLAG_NEGATIVE);
                CHECK_CARRY(result & 0x0100);
                CHECK_OVERFLOW((a ^ result8) & (b ^ result8) & 0x80);
                CHECK_ZERO(result8);
                CHECK_NEG(result8);

                cpu.a = result8;

                PRINT_OP("ADC");
            }
            break;

        ARITHM_OP(0xE1):
            {
                const uint8_t a = cpu.a;
                const uint8_t b = ~value;
                const uint16_t result = a + b + ((cpu.status & FLAG_CARRY) ? 1 : 0);
                const uint8_t result8 = result & 0xFF;

                AFFECTED_FLAGS(FLAG_CARRY | FLAG_ZERO | FLAG_OVERFLOW | FLAG_NEGATIVE);
                CHECK_CARRY(result & 0x0100);
                CHECK_OVERFLOW((a ^ result8) & (b ^ result8) & 0x80);
                CHECK_ZERO(result8);
                CHECK_NEG(result8);

                cpu.a = result8;
                PRINT_OP("SBC");
            }
            break;


        ARITHM_OP(0xC1):
            {
                const uint8_t a = cpu.a;
                const uint8_t b = value;
                const uint8_t result = a - b;
                AFFECTED_FLAGS(FLAG_CARRY | FLAG_ZERO | FLAG_NEGATIVE);
                CHECK_CARRY(a >= b);
                CHECK_ZERO(result);
                CHECK_NEG(result);

                PRINT_OP("CMP");
            }
            break;


        case 0xE0: // CPX imm
            GET_VALUE_IMM();
            goto perform_CPX;

        case 0xE4: // CPX zero
            GET_VALUE_ZERO();
            goto perform_CPX;

        case 0xEC: // CPX abs
            GET_VALUE_ABS();
            goto perform_CPX;

        perform_CPX:
            {
                const uint8_t a = cpu.x;
                const uint8_t b = value;
                const uint8_t result = a - b;
                AFFECTED_FLAGS(FLAG_CARRY | FLAG_ZERO | FLAG_NEGATIVE);
                CHECK_CARRY(a >= b);
                CHECK_ZERO(result);
                CHECK_NEG(result);

                PRINT_OP("CPX");
            }
            break;


        case 0xC0: // CPY imm
            GET_VALUE_IMM();
            goto perform_CPY;

        case 0xC4: // CPY zero
            GET_VALUE_ZERO();
            goto perform_CPY;

        case 0xCC: // CPY abs
            GET_VALUE_ABS();
            goto perform_CPY;

        perform_CPY:
            {
                const uint8_t a = cpu.y;
                const uint8_t b = value;
                const uint8_t result = a - b;
                AFFECTED_FLAGS(FLAG_CARRY | FLAG_ZERO | FLAG_NEGATIVE);
                CHECK_CARRY(a >= b);
                CHECK_ZERO(result);
                CHECK_NEG(result);

                PRINT_OP("CPY");
            }
            break;



        case 0x24: // BIT zero
            GET_VALUE_ZERO();
            goto perform_BIT;

        case 0x2C: // BIT abs;
            GET_VALUE_ABS();
            goto perform_BIT;

        perform_BIT:
            {
                const uint8_t b = value;
                const uint8_t result = cpu.a & b;

                AFFECTED_FLAGS(FLAG_ZERO | FLAG_OVERFLOW | FLAG_NEGATIVE);
                CHECK_ZERO(result);
                CHECK_OVERFLOW(b & 0x40);
                CHECK_NEG(b & 0x80);
                PRINT_OP("BIT");
            }
            break;


        case 0xCA: // DEX impl
            cpu.x--;
            AFFECTED_FLAGS(FLAG_ZERO | FLAG_NEGATIVE);
            CHECK_ZERO(cpu.x);
            CHECK_NEG(cpu.x);
            PRINT_OP("DEX");
            break;

        case 0x88: // DEY impl
            cpu.y--;
            AFFECTED_FLAGS(FLAG_ZERO | FLAG_NEGATIVE);
            CHECK_ZERO(cpu.y);
            CHECK_NEG(cpu.y);
            PRINT_OP("DEY");
            break;

        case 0xE8: // INX impl
            cpu.x++;
            AFFECTED_FLAGS(FLAG_ZERO | FLAG_NEGATIVE);
            CHECK_ZERO(cpu.x);
            CHECK_NEG(cpu.x);
            PRINT_OP("INX");
            break;

        case 0xC8: // INY impl
            cpu.y++;
            AFFECTED_FLAGS(FLAG_ZERO | FLAG_NEGATIVE);
            CHECK_ZERO(cpu.y);
            CHECK_NEG(cpu.y);
            PRINT_OP("INY");
            break;


        case 0xE6: // INC zero
            GET_ADDR_ZERO();
            goto perform_INCz;

        case 0xF6: // INC zero, X
            GET_ADDR_ZEROX();
            goto perform_INCz;

        perform_INCz:
            {
                const uint8_t result = read_mem_zero(addr) + 1;
                write_mem_zero(addr, result);
                AFFECTED_FLAGS(FLAG_ZERO | FLAG_NEGATIVE);
                CHECK_ZERO(result);
                CHECK_NEG(result);
                PRINT_OP("INC");
            }
            break;


        case 0xEE: // INC abs
            GET_ADDR_ABS();
            goto perform_INC;

        case 0xFE: // INC abs, X
            GET_ADDR_ABSX();
            goto perform_INC;

        perform_INC:
            {
                const uint8_t result = read_mem(addr) + 1;
                write_mem(addr, result);
                AFFECTED_FLAGS(FLAG_ZERO | FLAG_NEGATIVE);
                CHECK_ZERO(result);
                CHECK_NEG(result);
                PRINT_OP("INC");
            }
            break;



        case 0xC6: // DEC zero
            GET_ADDR_ZERO();
            goto perform_DECz;

        case 0xD6: // DEC zero, X
            GET_ADDR_ZEROX();
            goto perform_DECz;

        perform_DECz:
            {
                const uint8_t result = read_mem_zero(addr) - 1;
                write_mem_zero(addr, result);
                AFFECTED_FLAGS(FLAG_ZERO | FLAG_NEGATIVE);
                CHECK_ZERO(result);
                CHECK_NEG(result);
                PRINT_OP("DEC");
            }
            break;

        case 0xCE: // DEC abs
            GET_ADDR_ABS();
            goto perform_DEC;

        case 0xDE: // DEC abs, X
            GET_ADDR_ABSX();
            goto perform_DEC;

        perform_DEC:
            {
                const uint8_t result = read_mem(addr) - 1;
                write_mem(addr, result);
                AFFECTED_FLAGS(FLAG_ZERO | FLAG_NEGATIVE);
                CHECK_ZERO(result);
                CHECK_NEG(result);
                PRINT_OP("DEC");
            }
            break;


        case 0x0A: // ASL A
        {
            GET_ADDR_A();
            const uint8_t val = cpu.a;
            const uint8_t carry = val & 0x80;
            const uint8_t result = val << 1;

            cpu.a = result;

            AFFECTED_FLAGS(FLAG_ZERO | FLAG_NEGATIVE | FLAG_CARRY);
            CHECK_ZERO(result);
            CHECK_NEG(result);
            CHECK_CARRY(carry);

            PRINT_OP("ASL");
        }
        break;

        case 0x06: // ASL zero
            GET_ADDR_ZERO();
            goto perform_ASLz;

        case 0x16:
            GET_ADDR_ZEROX();
            goto perform_ASLz;

        perform_ASLz:
            {
                const uint8_t val = read_mem_zero(addr);
                const uint8_t carry = val & 0x80;
                const uint8_t result = val << 1;

                write_mem_zero(addr, result);

                AFFECTED_FLAGS(FLAG_ZERO | FLAG_NEGATIVE | FLAG_CARRY);
                CHECK_ZERO(result);
                CHECK_NEG(result);
                CHECK_CARRY(carry);

                PRINT_OP("ASL");
            }
            break;

        case 0x0E:
            GET_ADDR_ABS();
            goto perform_ASL;

        case 0x1E:
            GET_ADDR_ABSX();
            goto perform_ASL;

        perform_ASL:
            {
                const uint8_t val = read_mem(addr);
                const uint8_t carry = val & 0x80;
                const uint8_t result = val << 1;

                write_mem(addr, result);

                AFFECTED_FLAGS(FLAG_ZERO | FLAG_NEGATIVE | FLAG_CARRY);
                CHECK_ZERO(result);
                CHECK_NEG(result);
                CHECK_CARRY(carry);

                PRINT_OP("ASL");
            }
            break;


        case 0x4A: // LSR A
        {
            GET_ADDR_A();
            const uint8_t val = cpu.a;
            const uint8_t carry = val & 0x01;
            const uint8_t result = val >> 1;

            cpu.a = result;

            AFFECTED_FLAGS(FLAG_ZERO | FLAG_NEGATIVE | FLAG_CARRY);
            CHECK_ZERO(result);
            CHECK_NEG(result);
            CHECK_CARRY(carry);

            PRINT_OP("LSR");
        }
        break;

        case 0x46: // LSR zero
            GET_ADDR_ZERO();
            goto perform_LSRz;

        case 0x56: // LSR zero, X
            GET_ADDR_ZEROX();
            goto perform_LSRz;

        perform_LSRz:
            {
                const uint8_t val = read_mem_zero(addr);
                const uint8_t carry = val & 0x01;
                const uint8_t result = val >> 1;

                write_mem_zero(addr, result);

                AFFECTED_FLAGS(FLAG_ZERO | FLAG_NEGATIVE | FLAG_CARRY);
                CHECK_ZERO(result);
                CHECK_NEG(result);
                CHECK_CARRY(carry);

                PRINT_OP("LSR");
            }
            break;

        case 0x4E: // LSR abs
            GET_ADDR_ABS();
            goto perform_LSR;

        case 0x5E: // LSR abs, X
            GET_ADDR_ABSX();
            goto perform_LSR;

        perform_LSR:
            {
                const uint8_t val = read_mem(addr);
                const uint8_t carry = val & 0x01;
                const uint8_t result = val >> 1;

                write_mem(addr, result);

                AFFECTED_FLAGS(FLAG_ZERO | FLAG_NEGATIVE | FLAG_CARRY);
                CHECK_ZERO(result);
                CHECK_NEG(result);
                CHECK_CARRY(carry);

                PRINT_OP("LSR");
            }
            break;





        case 0x6A: // ROR A
        {
            GET_ADDR_A();
            const uint8_t val = cpu.a;
            const uint8_t carry = val & 0x01;
            const uint8_t result = (val >> 1) | ((cpu.status & FLAG_CARRY) ? 0x80 : 0);

            cpu.a = result;

            AFFECTED_FLAGS(FLAG_ZERO | FLAG_NEGATIVE | FLAG_CARRY);
            CHECK_ZERO(result);
            CHECK_NEG(result);
            CHECK_CARRY(carry);

            PRINT_OP("ROR");
        }
        break;

        case 0x66: // ROR zero
            GET_ADDR_ZERO();
            goto perform_RORz;

        case 0x76: // ROR zero, X
            GET_ADDR_ZEROX();
            goto perform_RORz;

        perform_RORz:
            {
                const uint8_t val = read_mem_zero(addr);
                const uint8_t carry = val & 0x01;
                const uint8_t result = (val >> 1) | ((cpu.status & FLAG_CARRY) ? 0x80 : 0);

                write_mem(addr, result);

                AFFECTED_FLAGS(FLAG_ZERO | FLAG_NEGATIVE | FLAG_CARRY);
                CHECK_ZERO(result);
                CHECK_NEG(result);
                CHECK_CARRY(carry);

                PRINT_OP("ROR");
            }
            break;

        case 0x6E: // ROR abs
            GET_ADDR_ABS();
            goto perform_ROR;

        case 0x7E: // ROR abs, X
            GET_ADDR_ABSX();
            goto perform_ROR;

        perform_ROR:
            {
                const uint8_t val = read_mem(addr);
                const uint8_t carry = val & 0x01;
                const uint8_t result = (val >> 1) | ((cpu.status & FLAG_CARRY) ? 0x80 : 0);

                write_mem(addr, result);

                AFFECTED_FLAGS(FLAG_ZERO | FLAG_NEGATIVE | FLAG_CARRY);
                CHECK_ZERO(result);
                CHECK_NEG(result);
                CHECK_CARRY(carry);

                PRINT_OP("ROR");
            }
            break;





        case 0x2A: // ROL A
        {
            GET_ADDR_A();
            const uint8_t val = cpu.a;
            const uint8_t carry = val & 0x80;
            const uint8_t result = (val << 1) | ((cpu.status & FLAG_CARRY) ? 1 : 0);

            cpu.a = result;

            AFFECTED_FLAGS(FLAG_ZERO | FLAG_NEGATIVE | FLAG_CARRY);
            CHECK_ZERO(result);
            CHECK_NEG(result);
            CHECK_CARRY(carry);

            PRINT_OP("ROL");
        }
        break;

        case 0x26: // ROL zero
            GET_ADDR_ZERO();
            goto perform_ROLz;

        case 0x36: // ROL zero, X
            GET_ADDR_ZEROX();
            goto perform_ROLz;

        perform_ROLz:
            {
                const uint8_t val = read_mem_zero(addr);
                const uint8_t carry = val & 0x80;
                const uint8_t result = (val << 1) | ((cpu.status & FLAG_CARRY) ? 1 : 0);

                write_mem_zero(addr, result);

                AFFECTED_FLAGS(FLAG_ZERO | FLAG_NEGATIVE | FLAG_CARRY);
                CHECK_ZERO(result);
                CHECK_NEG(result);
                CHECK_CARRY(carry);

                PRINT_OP("ROL");
            }
            break;


        case 0x2E: // ROL abs
            GET_ADDR_ABS();
            goto perform_ROL;

        case 0x3E: // ROL zero, X
            GET_ADDR_ABSX();
            goto perform_ROL;

        perform_ROL:
            {
                const uint8_t val = read_mem(addr);
                const uint8_t carry = val & 0x80;
                const uint8_t result = (val << 1) | ((cpu.status & FLAG_CARRY) ? 1 : 0);

                write_mem(addr, result);

                AFFECTED_FLAGS(FLAG_ZERO | FLAG_NEGATIVE | FLAG_CARRY);
                CHECK_ZERO(result);
                CHECK_NEG(result);
                CHECK_CARRY(carry);

                PRINT_OP("ROL");
            }
            break;





        case 0xA9: // LDA imm
            GET_VALUE_IMM();
            goto perform_LDA;

        case 0xA5: // LDA zero
            GET_VALUE_ZERO();
            goto perform_LDA;

        case 0xB5: // LDA zero, X
            GET_VALUE_ZEROX();
            goto perform_LDA;

        case 0xAD: // LDA abs
            GET_VALUE_ABS();
            goto perform_LDA;

        case 0xBD: // LDA abs, X
            GET_VALUE_ABSX();
            goto perform_LDA;

        case 0xB9: // LDA abs, Y
            GET_VALUE_ABSY();
            goto perform_LDA;

        case 0xA1: // LDA (ind, X)
            GET_VALUE_INDX();
            goto perform_LDA;

        case 0xB1: // LDA (ind), Y
            GET_VALUE_INDY();
            goto perform_LDA;

        perform_LDA:
            cpu.a = value;
            AFFECTED_FLAGS(FLAG_ZERO | FLAG_NEGATIVE);
            CHECK_ZERO(cpu.a);
            CHECK_NEG(cpu.a);
            PRINT_OP("LDA");
            break;



        case 0xA2: // LDX imm
            GET_VALUE_IMM();
            goto perform_LDX;

        case 0xA6: // LDX zero
            GET_VALUE_ZERO();
            goto perform_LDX;

        case 0xB6: // LDX zero, Y
            GET_VALUE_ZEROY();
            goto perform_LDX;

        case 0xAE: // LDX abs
            GET_VALUE_ABS();
            goto perform_LDX;

        case 0xBE: // LDX abs, Y
            GET_VALUE_ABSY();
            goto perform_LDX;

        perform_LDX:
            cpu.x = value;
            AFFECTED_FLAGS(FLAG_ZERO | FLAG_NEGATIVE);
            CHECK_ZERO(cpu.x);
            CHECK_NEG(cpu.x);
            PRINT_OP("LDX");
            break;



        case 0xA0: // LDY imm
            GET_VALUE_IMM();
            goto perform_LDY;

        case 0xA4: // LDY zero
            GET_VALUE_ZERO();
            goto perform_LDY;

        case 0xB4: // LDY zero, X
            GET_VALUE_ZEROX();
            goto perform_LDY;

        case 0xAC: // LDY abs
            GET_VALUE_ABS();
            goto perform_LDY;

        case 0xBC: // LDY abs, X
            GET_VALUE_ABSX();
            goto perform_LDY;

        perform_LDY:
            cpu.y = value;
            AFFECTED_FLAGS(FLAG_ZERO | FLAG_NEGATIVE);
            CHECK_ZERO(cpu.y);
            CHECK_NEG(cpu.y);
            PRINT_OP("LDY");
            break;




        case 0x85: // STA zero
            GET_ADDR_ZERO();
            goto perform_STAz;

        case 0x95: // STA zero, X
            GET_ADDR_ZEROX();
            goto perform_STAz;

        perform_STAz:
            write_mem_zero(addr, cpu.a);
            NO_FLAGS();
            PRINT_OP("STA");
            break;

        case 0x8D: // STA abs
            GET_ADDR_ABS();
            goto perform_STA;

        case 0x9D: // STA abs, X
            GET_ADDR_ABSX();
            goto perform_STA;

        case 0x99: // STA abs, Y
            GET_ADDR_ABSY();
            goto perform_STA;

        case 0x81: // STA (ind, X)
            GET_ADDR_INDX();
            goto perform_STA;

        case 0x91: // STA (ind), Y
            GET_ADDR_INDY();
            goto perform_STA;

        perform_STA:
            write_mem(addr, cpu.a);
            NO_FLAGS();
            PRINT_OP("STA");
            break;



        case 0x86: // STX zero
            GET_ADDR_ZERO();
            goto perform_STXz;

        case 0x96: // STX zero, Y
            GET_ADDR_ZEROY();
            goto perform_STXz;

        perform_STXz:
            write_mem(addr, cpu.x);
            NO_FLAGS();
            PRINT_OP("STX");
            break;

        case 0x8E: // STX abs
            GET_ADDR_ABS();
            goto perform_STX;

        perform_STX:
            write_mem(addr, cpu.x);
            NO_FLAGS();
            PRINT_OP("STX");
            break;



        case 0x84: // STY zero
            GET_ADDR_ZERO();
            goto perform_STYz;

        case 0x94: // STY zero,X
            GET_ADDR_ZEROX();
            goto perform_STYz;

        perform_STYz:
            write_mem(addr, cpu.y);
            NO_FLAGS();
            PRINT_OP("STY");
            break;

        case 0x8C: // STY abs
            GET_ADDR_ABS();
            goto perform_STY;

        perform_STY:
            write_mem(addr, cpu.y);
            NO_FLAGS();
            PRINT_OP("STY");
            break;








        case 0x48: // PHA impl
            STACK_PUSH(cpu.a);
            NO_FLAGS();
            PRINT_OP("PHA");
            break;

        case 0x08: // PHP impl
            STACK_PUSH(cpu.status | FLAG_BREAK);
            NO_FLAGS();
            PRINT_OP("PHP");
            break;

        case 0x68: // PLA impl
            cpu.a = STACK_POP();
            AFFECTED_FLAGS(FLAG_ZERO | FLAG_NEGATIVE);
            CHECK_ZERO(cpu.a);
            CHECK_NEG(cpu.a);
            PRINT_OP("PLA");
            break;

        case 0x28: // PLP impl
            cpu.status = (STACK_POP() & ~FLAG_BREAK) | FLAG_CONSTANT;
            PRINT_OP("PLP");
            break;

        case 0xA8: // TAY impl
            cpu.y = cpu.a;
            AFFECTED_FLAGS(FLAG_ZERO | FLAG_NEGATIVE);
            CHECK_ZERO(cpu.y);
            CHECK_NEG(cpu.y);
            PRINT_OP("TAY");
            break;

        case 0xAA: // TAX impl
            cpu.x = cpu.a;
            AFFECTED_FLAGS(FLAG_ZERO | FLAG_NEGATIVE);
            CHECK_ZERO(cpu.x);
            CHECK_NEG(cpu.x);
            PRINT_OP("TAX");
            break;

        case 0xBA: // TSX impl
            cpu.x = cpu.sp;
            AFFECTED_FLAGS(FLAG_ZERO | FLAG_NEGATIVE);
            CHECK_ZERO(cpu.x);
            CHECK_NEG(cpu.x);
            PRINT_OP("TSX");
            break;

        case 0x8A: // TXA impl
            cpu.a = cpu.x;
            AFFECTED_FLAGS(FLAG_ZERO | FLAG_NEGATIVE);
            CHECK_ZERO(cpu.a);
            CHECK_NEG(cpu.a);
            PRINT_OP("TXA");
            break;

        case 0x9A: // TXS impl
            cpu.sp = cpu.x;
            NO_FLAGS();
            PRINT_OP("TXS");
            break;

        case 0x98: // TYA impl
            cpu.a = cpu.y;
            AFFECTED_FLAGS(FLAG_ZERO | FLAG_NEGATIVE);
            CHECK_ZERO(cpu.a);
            CHECK_NEG(cpu.a);
            PRINT_OP("TYA");
            break;


        case 0x18: // CLC impl
            cpu.status &= ~FLAG_CARRY;
            PRINT_OP("CLC");
            break;

        case 0xD8: // CLD impl
            cpu.status &= ~FLAG_DECIMAL;
            PRINT_OP("CLD");
            break;

        case 0x58: // CLI impl
            cpu.status &= ~FLAG_INTERRUPT;
            PRINT_OP("CLI");
            break;

        case 0xB8: // CLV impl
            cpu.status &= ~FLAG_OVERFLOW;
            PRINT_OP("CLV");
            break;

        case 0x38: // SEC impl
            cpu.status |= FLAG_CARRY;
            PRINT_OP("SEC");
            break;

        case 0xF8: // SEC impl
            cpu.status |= FLAG_DECIMAL;
            PRINT_OP("SED");
            break;

        case 0x78: // SEI impl
            cpu.status |= FLAG_INTERRUPT;
            PRINT_OP("SEI");
            break;


        case 0x4C: // JMP abs
            GET_ADDR_ABS();
            cpu.pc = addr;
            NO_FLAGS();
            PRINT_OP("JMP");
            break;

        case 0x6C: // JMP ind
            GET_ADDR_IND();
            cpu.pc = addr;
            NO_FLAGS();
            PRINT_OP("JMP");
            break;



        case 0x20: // JSR abs
            GET_ADDR_ABS();
            STACK_PUSH(((cpu.pc-1) & 0xFF00) >> 8);
            STACK_PUSH((cpu.pc-1) & 0x00FF);
            cpu.pc = addr;
            NO_FLAGS();
            PRINT_OP("JSR");
            break;

        case 0x60: // RTS impl
        {
            const uint8_t lo = STACK_POP();
            const uint8_t hi = STACK_POP();
            cpu.pc = (((uint16_t) hi << 8) | lo) + 1;
            NO_FLAGS();
            PRINT_OP("RTS");
        }
        break;

        case 0x40: // RTI impl
        {
            const uint8_t s = STACK_POP();
            const uint8_t lo = STACK_POP();
            const uint8_t hi = STACK_POP();
            cpu.status = s | FLAG_CONSTANT;
            cpu.pc = (((uint16_t) hi << 8) | lo);
            PRINT_OP("RTI");
        }
        break;



        case 0x10: // BPL rel
            GET_ADDR_REL();
            BRANCH_IF_CLEAR(FLAG_NEGATIVE);
            PRINT_OP("BPL");
            break;

        case 0x30: // BMI rel
            GET_ADDR_REL();
            BRANCH_IF_SET(FLAG_NEGATIVE);
            PRINT_OP("BMI");
            break;

        case 0x50: // BVC rel
            GET_ADDR_REL();
            BRANCH_IF_CLEAR(FLAG_OVERFLOW);
            PRINT_OP("BVC");
            break;

        case 0x70: // BVS rel
            GET_ADDR_REL();
            BRANCH_IF_SET(FLAG_OVERFLOW);
            PRINT_OP("BVS");
            break;

        case 0x90: // BCC rel
            GET_ADDR_REL();
            BRANCH_IF_CLEAR(FLAG_CARRY);
            PRINT_OP("BCC");
            break;

        case 0xB0: // BCS rel
            GET_ADDR_REL();
            BRANCH_IF_SET(FLAG_CARRY);
            PRINT_OP("BCS");
            break;

        case 0xD0: // BNE rel
            GET_ADDR_REL();
            BRANCH_IF_CLEAR(FLAG_ZERO);
            PRINT_OP("BNE");
            break;

        case 0xF0: // BEQ rel
            GET_ADDR_REL();
            BRANCH_IF_SET(FLAG_ZERO);
            PRINT_OP("BEQ");
            break;

        case 0xEA: // NOP impl
            NO_FLAGS();
            PRINT_OP("NOP");
            break;

        case 0x00: // BRK impl
        {
            const uint8_t lo = read_mem(0xFFFE);
            const uint8_t hi = read_mem(0xFFFF);
            const uint16_t a = ((uint16_t) hi << 8) | lo;

            STACK_PUSH((cpu.pc & 0xFF00) >> 8);
            STACK_PUSH(cpu.pc & 0x00FF);
            STACK_PUSH(cpu.status | FLAG_BREAK);
            cpu.status |= FLAG_INTERRUPT;
            cpu.pc = a;
            PRINT_OP("BRK");
        }
        break;

// Undocumented instructions

        case 0x1A: // *NOP impl
        case 0x3A:
        case 0x5A:
        case 0x7A:
        case 0xDA:
        case 0xFA:
            goto undocumented_NOP;

        case 0x04: // *NOP zero
        case 0x44:
        case 0x64:
            GET_ADDR_ZERO();
            goto undocumented_NOP;

        case 0x14: // *NOP zero,X
        case 0x34:
        case 0x54:
        case 0x74:
        case 0xD4:
        case 0xF4:
            GET_ADDR_ZEROX();
            goto undocumented_NOP;

        case 0x80: // *NOP imm
        case 0x82:
        case 0x89:
        case 0xC2:
        case 0xE2:
            GET_VALUE_IMM();
            goto undocumented_NOP;

        case 0x0C: // *NOP abs
            GET_ADDR_ABS();
            goto undocumented_NOP;

        case 0x1C: // *NOP abs, X
        case 0x3C:
        case 0x5C:
        case 0x7C:
        case 0xDC:
        case 0xFC:
            GET_ADDR_ABSX();
            goto undocumented_NOP;

        undocumented_NOP:
            NO_FLAGS();
            PRINT_UNDOCUMENTED_OP("*NOP");
            break;


        case 0xA7: // *LAX
            GET_VALUE_ZERO();
            goto undocumented_LAX;

        case 0xB7: // *LAX
            GET_VALUE_ZEROY();
            goto undocumented_LAX;

        case 0xAF: // *LAX
            GET_VALUE_ABS();
            goto undocumented_LAX;

        case 0xBF: // *LAX
            GET_VALUE_ABSY();
            goto undocumented_LAX;

        case 0xA3: // *LAX
            GET_VALUE_INDX();
            goto undocumented_LAX;

        case 0xB3: // *LAX
            GET_VALUE_INDY();
            goto undocumented_LAX;

        undocumented_LAX:
            {
                const uint8_t result = value;
                cpu.x = cpu.a = result;
                AFFECTED_FLAGS(FLAG_ZERO | FLAG_NEGATIVE);
                CHECK_ZERO(result);
                CHECK_NEG(result);
                PRINT_UNDOCUMENTED_OP("*LAX");
            }
            break;



        case 0x87: // *SAX
            GET_ADDR_ZERO();
            goto undocumented_SAX;

        case 0x97: // *SAX
            GET_ADDR_ZEROY();
            goto undocumented_SAX;

        case 0x83: // *SAX
            GET_ADDR_INDX();
            goto undocumented_SAX;

        case 0x8F: // *SAX
            GET_ADDR_ABS();
            goto undocumented_SAX;

        undocumented_SAX:
            {
                uint8_t result = cpu.a & cpu.x;
                write_mem(addr, result);
                NO_FLAGS();
                PRINT_UNDOCUMENTED_OP("*SAX");
            }
            break;


        case 0xEB: // *SBC
        {
            GET_VALUE_IMM();

            const uint8_t a = cpu.a;
            const uint8_t b = ~value;
            const uint16_t result = a + b + ((cpu.status & FLAG_CARRY) ? 1 : 0);
            const uint8_t result8 = result & 0xFF;

            AFFECTED_FLAGS(FLAG_CARRY | FLAG_ZERO | FLAG_OVERFLOW | FLAG_NEGATIVE);
            CHECK_CARRY(result & 0x0100);
            CHECK_OVERFLOW((a ^ result8) & (b ^ result8) & 0x80);
            CHECK_ZERO(result8);
            CHECK_NEG(result8);

            cpu.a = result8;
            PRINT_UNDOCUMENTED_OP("*SBC");
        }
        break;


        case 0xC7: // *DCP
            GET_ADDR_ZERO();
            goto undocumented_DCP;

        case 0xD7: // *DCP
            GET_ADDR_ZEROX();
            goto undocumented_DCP;

        case 0xCF: // *DCP
            GET_ADDR_ABS();
            goto undocumented_DCP;

        case 0xDF: // *DCP
            GET_ADDR_ABSX();
            goto undocumented_DCP;

        case 0xDB: // *DCP
            GET_ADDR_ABSY();
            goto undocumented_DCP;

        case 0xC3: // *DCP
            GET_ADDR_INDX();
            goto undocumented_DCP;

        case 0xD3: // *DCP
            GET_ADDR_INDY();
            goto undocumented_DCP;

        undocumented_DCP:
            {
                const uint8_t val = read_mem(addr) - 1;
                const uint8_t result = cpu.a - val;
                write_mem(addr, val);
                AFFECTED_FLAGS(FLAG_ZERO | FLAG_NEGATIVE | FLAG_CARRY);
                CHECK_ZERO(result);
                CHECK_NEG(result);
                CHECK_CARRY(cpu.a >= val);
                PRINT_UNDOCUMENTED_OP("*DCP");
            }
            break;


        case 0xE7: // *ISB
            GET_ADDR_ZERO();
            goto undocumented_ISB;

        case 0xF7: // *ISB
            GET_ADDR_ZEROX();
            goto undocumented_ISB;

        case 0xEF: // *ISB
            GET_ADDR_ABS();
            goto undocumented_ISB;

        case 0xFF: // *ISB
            GET_ADDR_ABSX();
            goto undocumented_ISB;

        case 0xFB: // *ISB
            GET_ADDR_ABSY();
            goto undocumented_ISB;

        case 0xE3: // *ISB
            GET_ADDR_INDX();
            goto undocumented_ISB;

        case 0xF3: // *ISB
            GET_ADDR_INDY();
            goto undocumented_ISB;

        undocumented_ISB:
            {
                const uint8_t val = read_mem(addr) + 1;
                write_mem(addr, val);
                const uint8_t a = cpu.a;
                const uint8_t b = ~val;
                const uint16_t result = a + b + ((cpu.status & FLAG_CARRY) ? 1 : 0);
                const uint8_t result8 = result & 0xFF;

                AFFECTED_FLAGS(FLAG_CARRY | FLAG_ZERO | FLAG_OVERFLOW | FLAG_NEGATIVE);
                CHECK_CARRY(result & 0x0100);
                CHECK_OVERFLOW((a ^ result8) & (b ^ result8) & 0x80);
                CHECK_ZERO(result8);
                CHECK_NEG(result8);

                cpu.a = result8;
                PRINT_UNDOCUMENTED_OP("*ISB");
            }
            break;


        case 0x07: // *SLO
            GET_ADDR_ZERO();
            goto undocumented_SLO;

        case 0x17: // *SLO
            GET_ADDR_ZEROX();
            goto undocumented_SLO;

        case 0x0F: // *SLO
            GET_ADDR_ABS();
            goto undocumented_SLO;

        case 0x1F: // *SLO
            GET_ADDR_ABSX();
            goto undocumented_SLO;

        case 0x1B: // *SLO
            GET_ADDR_ABSY();
            goto undocumented_SLO;

        case 0x03: // *SLO
            GET_ADDR_INDX();
            goto undocumented_SLO;

        case 0x13: // *SLO
            GET_ADDR_INDY();
            goto undocumented_SLO;

        undocumented_SLO:
            {
                const uint8_t val = read_mem(addr);
                const uint8_t carry = val & 0x80;
                const uint8_t result = val << 1;

                write_mem(addr, result);
                cpu.a |= result;

                AFFECTED_FLAGS(FLAG_ZERO | FLAG_CARRY);
                CHECK_ZERO(result);
                CHECK_CARRY(carry);

                PRINT_UNDOCUMENTED_OP("*SLO");
            }
            break;



        case 0x27: // *RLA
            GET_ADDR_ZERO();
            goto undocumented_RLA;

        case 0x37: // *RLA
            GET_ADDR_ZEROX();
            goto undocumented_RLA;

        case 0x2F: // *RLA
            GET_ADDR_ABS();
            goto undocumented_RLA;

        case 0x3F: // *RLA
            GET_ADDR_ABSX();
            goto undocumented_RLA;

        case 0x3B: // *RLA
            GET_ADDR_ABSY();
            goto undocumented_RLA;

        case 0x23: // *RLA
            GET_ADDR_INDX();
            goto undocumented_RLA;

        case 0x33: // *RLA
            GET_ADDR_INDY();
            goto undocumented_RLA;

        undocumented_RLA:
            {
                const uint8_t val = read_mem(addr);
                const uint8_t carry = val & 0x80;
                const uint8_t result = (val << 1) | ((cpu.status & FLAG_CARRY) ? 1 : 0);

                cpu.a &= result;

                write_mem(addr, result);

                AFFECTED_FLAGS(FLAG_ZERO | FLAG_NEGATIVE | FLAG_CARRY);
                CHECK_ZERO(result);
                CHECK_NEG(result);
                CHECK_CARRY(carry);

                PRINT_UNDOCUMENTED_OP("*RLA");
            }
            break;


        case 0x47: // *SRE
            GET_ADDR_ZERO();
            goto undocumented_SRE;

        case 0x57: // *SRE
            GET_ADDR_ZEROX();
            goto undocumented_SRE;

        case 0x4F: // *SRE
            GET_ADDR_ABS();
            goto undocumented_SRE;

        case 0x5F: // *SRE
            GET_ADDR_ABSX();
            goto undocumented_SRE;

        case 0x5B: // *SRE
            GET_ADDR_ABSY();
            goto undocumented_SRE;

        case 0x43: // *SRE
            GET_ADDR_INDX();
            goto undocumented_SRE;

        case 0x53: // *SRE
            GET_ADDR_INDY();
            goto undocumented_SRE;

        undocumented_SRE:
            {
                const uint8_t val = read_mem(addr);
                const uint8_t carry = val & 0x01;
                const uint8_t result = val >> 1;

                write_mem(addr, result);

                cpu.a ^= result;

                AFFECTED_FLAGS(FLAG_ZERO | FLAG_CARRY);
                CHECK_ZERO(result);
                CHECK_CARRY(carry);

                PRINT_UNDOCUMENTED_OP("*SRE");
            }
            break;



        case 0x67: // *RRA
            GET_ADDR_ZERO();
            goto undocumented_RRA;

        case 0x77: // *RRA
            GET_ADDR_ZEROX();
            goto undocumented_RRA;

        case 0x6F: // *RRA
            GET_ADDR_ABS();
            goto undocumented_RRA;

        case 0x7F: // *RRA
            GET_ADDR_ABSX();
            goto undocumented_RRA;

        case 0x7B: // *RRA
            GET_ADDR_ABSY();
            goto undocumented_RRA;

        case 0x63: // *RRA
            GET_ADDR_INDX();
            goto undocumented_RRA;

        case 0x73: // *RRA
            GET_ADDR_INDY();
            goto undocumented_RRA;

        undocumented_RRA:
            {
                const uint8_t val = read_mem(addr);
                const uint8_t carry = val & 0x01;
                const uint8_t resultR = (val >> 1) | ((cpu.status & FLAG_CARRY) ? 0x80 : 0);

                write_mem(addr, resultR);

                const uint8_t a = cpu.a;
                const uint8_t b = resultR;
                const uint16_t result = a + b + ((carry & FLAG_CARRY) ? 1 : 0);
                const uint8_t result8 = result & 0xFF;

                AFFECTED_FLAGS(FLAG_CARRY | FLAG_ZERO | FLAG_OVERFLOW | FLAG_NEGATIVE);
                CHECK_CARRY(result & 0x0100);
                CHECK_OVERFLOW((a ^ result8) & (b ^ result8) & 0x80);
                CHECK_ZERO(result8);
                CHECK_NEG(result8);

                cpu.a = result8;

                PRINT_UNDOCUMENTED_OP("*RRA");
            }
            break;




        default:
            PRINT_OP("Unkown opcode");
            printf("Unkown instruction %02X\n", op);
            return -1;
        }
    }

    return 0;
}
