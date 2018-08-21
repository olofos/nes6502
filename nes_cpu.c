#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "nes_cpu.h"

#define FLAG_CARRY     0x01
#define FLAG_ZERO      0x02
#define FLAG_INTERRUPT 0x04
#define FLAG_DECIMAL   0x08
#define FLAG_BREAK     0x10
#define FLAG_CONSTANT  0x20
#define FLAG_OVERFLOW  0x40
#define FLAG_NEGATIVE  0x80

#define STACK_BASE     0x100

struct nes_cpu cpu = {.status = FLAG_CONSTANT};



#define GET_ADDR_ABS() do {                                             \
        const uint8_t d1 = read_mem(cpu.pc++);                          \
        const uint8_t d2 = read_mem(cpu.pc++);                          \
        addr = ((uint16_t)d2 << 8) + d1;                                \
        sprintf(addr_data_string, "%02X %02X ", d1, d2);                \
        if((op == 0x4C) || (op == 0x20)) {                              \
            sprintf(addr_string, "$%04X", ((uint16_t)d2 << 8) + d1);    \
        } else {                                                        \
            sprintf(addr_string, "$%04X = %02X", ((uint16_t)d2 << 8) + d1, log_read_mem(addr)); \
        }                                                               \
    } while(0)

#define GET_ADDR_ABSX() do {                                            \
        const uint8_t d1 = read_mem(cpu.pc++);                          \
        const uint8_t d2 = read_mem(cpu.pc++);                          \
        addr = ((uint16_t)d2 << 8) + d1 + cpu.x;                        \
        sprintf(addr_data_string, "%02X %02X ", d1, d2);                \
        sprintf(addr_string, "$%04X,X @ %04X = %02X", ((uint16_t)d2 << 8) + d1, addr, log_read_mem(addr)); \
    } while(0)

#define GET_ADDR_ABSY() do {                                            \
        const uint8_t d1 = read_mem(cpu.pc++);                          \
        const uint8_t d2 = read_mem(cpu.pc++);                          \
        addr = ((uint16_t)d2 << 8) + d1 + cpu.y;                        \
        sprintf(addr_data_string, "%02X %02X ", d1, d2);                \
        sprintf(addr_string, "$%04X,Y @ %04X = %02X", ((uint16_t)d2 << 8) + d1, addr, log_read_mem(addr)); \
    } while(0)

#define GET_ADDR_ZERO() do {                                            \
        const uint8_t a = read_mem(cpu.pc++);                           \
        addr = a;                                                       \
        sprintf(addr_data_string, "%02X    ", a);                       \
        sprintf(addr_string, "$%02X = %02X", addr & 0x0FF, log_read_mem(addr)); \
    } while(0)

#define GET_ADDR_ZEROX() do {                                           \
        const uint8_t a = read_mem(cpu.pc++);                           \
        addr = (a + cpu.x) & 0x00FF;                                    \
        sprintf(addr_data_string, "%02X    ", a);                       \
        sprintf(addr_string, "$%02X,X @ %02X = %02X", a, addr, log_read_mem(addr)); \
    } while(0)

#define GET_ADDR_ZEROY() do {                                           \
        const uint8_t a = read_mem(cpu.pc++);                           \
        addr = (a + cpu.y) & 0x00FF;                                    \
        sprintf(addr_data_string, "%02X    ", a);                       \
        sprintf(addr_string, "$%02X,Y @ %02X = %02X", a, addr, log_read_mem(addr)); \
    } while(0)

#define GET_ADDR_INDX() do {                                            \
        const uint8_t d1 = read_mem(cpu.pc++);                          \
        const uint8_t a = d1 + cpu.x;                                   \
        const uint8_t d3 = read_mem(a);                                 \
        const uint8_t d4 = read_mem((a+1) & 0xFF);                      \
        addr = ((uint16_t)d4 << 8) | (d3 & 0x00FF);                     \
        sprintf(addr_data_string, "%02X    ", d1);                      \
        sprintf(addr_string, "($%02X,X) @ %02X = %04X = %02X", d1, a, addr, log_read_mem(addr)); \
    } while(0)

#define GET_ADDR_INDY() do {                                            \
        const uint8_t d1 = read_mem(cpu.pc++);                          \
        const uint16_t a1 = d1;                                         \
        const uint8_t d3 = read_mem(a1);                                \
        const uint8_t d4 = read_mem((a1+1) & 0x00FF);                   \
        const uint16_t a2 = (((uint16_t)d4 << 8) | (d3 & 0x00FF));      \
        addr = a2 + cpu.y;                                              \
        sprintf(addr_data_string, "%02X    ", d1);                      \
        sprintf(addr_string, "($%02X),Y = %04X @ %04X = %02X", a1, a2, addr, log_read_mem(addr)); \
    } while(0)

#define GET_ADDR_IND() do {                                             \
        const uint8_t d1 = read_mem(cpu.pc++);                          \
        const uint8_t d2 = read_mem(cpu.pc++);                          \
        const uint16_t a = ((uint16_t)d2 << 8) + d1;                    \
        const uint8_t d3 = read_mem(a);                                 \
        const uint8_t d4 = read_mem(((uint16_t)d2 << 8) + ((d1+1) & 0xff)); \
        addr = ((uint16_t)d4 << 8) | (d3 & 0x00FF);                     \
        sprintf(addr_data_string, "%02X %02X ", d1, d2);                \
        sprintf(addr_string, "($%04X) = %04X", a, addr);                \
    } while(0)

#define GET_ADDR_IMM() do {                                             \
        addr = cpu.pc++;                                                \
        sprintf(addr_data_string, "%02X    ", log_read_mem(addr));      \
        sprintf(addr_string, "#$%02X", log_read_mem(addr));             \
    } while(0)

#define GET_ADDR_REL() do {                                             \
        const int8_t offset = read_mem(cpu.pc++);                       \
        addr  = cpu.pc + offset;                                        \
        sprintf(addr_data_string, "%02X    ", (uint8_t) offset);        \
        sprintf(addr_string, "$%04X", cpu.pc + offset);                 \
    } while(0)

#define GET_ADDR_IMPL() do {                    \
        sprintf(addr_data_string, "      ");    \
    } while(0);

#define GET_ADDR_A() do {                       \
        sprintf(addr_data_string, "      ");    \
        sprintf(addr_string, "A");              \
    } while(0);

#define ARITHM_OP(op) /**/                      \
case op: GET_ADDR_INDX();                       \
    goto perform_##op;                          \
case op+0x10: GET_ADDR_INDY();                  \
    goto perform_##op;                          \
case op+0x04: GET_ADDR_ZERO();                  \
    goto perform_##op;                          \
case op+0x14: GET_ADDR_ZEROX();                 \
    goto perform_##op;                          \
case op+0x08: GET_ADDR_IMM();                   \
    goto perform_##op;                          \
case op+0x18: GET_ADDR_ABSY();                  \
    goto perform_##op;                          \
case op+0x0C: GET_ADDR_ABS();                   \
    goto perform_##op;                          \
case op+0x1C: GET_ADDR_ABSX();                  \
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

#define BRANCH_IF_SET(flag) do { if(cpu.status & (flag)) cpu.pc = addr; } while(0)
#define BRANCH_IF_CLEAR(flag) do { if(!(cpu.status & (flag))) cpu.pc = addr; } while(0)

#define PRINT_OP(name) printf("%04X  %02X %s %s %-28sA:%02X X:%02X Y:%02X P:%02X SP:%02X\n", addr_op, op, addr_data_string, name, addr_string, old_a, old_x, old_y, old_status, old_sp)
#define PRINT_UNDOCUMENTED_OP(name) printf("%04X  %02X %s%s %-28sA:%02X X:%02X Y:%02X P:%02X SP:%02X\n", addr_op, op, addr_data_string, name, addr_string, old_a, old_x, old_y, old_status, old_sp)



int cpu_step(void)
{
    uint16_t addr_op = cpu.pc;
    uint8_t op = read_mem(cpu.pc++);
    uint16_t addr;

    char addr_string[32] = "";
    char addr_data_string[32] = "";

    uint8_t old_a = cpu.a, old_x = cpu.x, old_y = cpu.y, old_sp = cpu.sp, old_status = cpu.status;

    switch(op)
    {
    ARITHM_OP(0x01):
        cpu.a |= read_mem(addr);
        AFFECTED_FLAGS(FLAG_ZERO | FLAG_NEGATIVE);
        CHECK_ZERO(cpu.a);
        CHECK_NEG(cpu.a);

        PRINT_OP("ORA");
        break;

    ARITHM_OP(0x21):
        cpu.a &= read_mem(addr);
        AFFECTED_FLAGS(FLAG_ZERO | FLAG_NEGATIVE);
        CHECK_ZERO(cpu.a);
        CHECK_NEG(cpu.a);
        PRINT_OP("AND");
        break;

    ARITHM_OP(0x41):
        cpu.a ^= read_mem(addr);
        AFFECTED_FLAGS(FLAG_ZERO | FLAG_NEGATIVE);
        CHECK_ZERO(cpu.a);
        CHECK_NEG(cpu.a);
        PRINT_OP("EOR");
        break;

    ARITHM_OP(0x61):
        {
            const uint8_t a = cpu.a;
            const uint8_t b = read_mem(addr);
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
            const uint8_t b = ~read_mem(addr);
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
            const uint8_t b = read_mem(addr);
            const uint8_t result = a - b;
            AFFECTED_FLAGS(FLAG_CARRY | FLAG_ZERO | FLAG_NEGATIVE);
            CHECK_CARRY(a >= b);
            CHECK_ZERO(result);
            CHECK_NEG(result);

            PRINT_OP("CMP");
        }
        break;


    case 0xE0: // CPX imm
        GET_ADDR_IMM();
        goto perform_CPX;

    case 0xE4: // CPX zero
        GET_ADDR_ZERO();
        goto perform_CPX;

    case 0xEC: // CPX abs
        GET_ADDR_ABS();
        goto perform_CPX;

    perform_CPX:
        {
            const uint8_t a = cpu.x;
            const uint8_t b = read_mem(addr);
            const uint8_t result = a - b;
            AFFECTED_FLAGS(FLAG_CARRY | FLAG_ZERO | FLAG_NEGATIVE);
            CHECK_CARRY(a >= b);
            CHECK_ZERO(result);
            CHECK_NEG(result);

            PRINT_OP("CPX");
        }
        break;


    case 0xC0: // CPY imm
        GET_ADDR_IMM();
        goto perform_CPY;

    case 0xC4: // CPY zero
        GET_ADDR_ZERO();
        goto perform_CPY;

    case 0xCC: // CPY abs
        GET_ADDR_ABS();
        goto perform_CPY;

    perform_CPY:
        {
            const uint8_t a = cpu.y;
            const uint8_t b = read_mem(addr);
            const uint8_t result = a - b;
            AFFECTED_FLAGS(FLAG_CARRY | FLAG_ZERO | FLAG_NEGATIVE);
            CHECK_CARRY(a >= b);
            CHECK_ZERO(result);
            CHECK_NEG(result);

            PRINT_OP("CPY");
        }
        break;



    case 0x24: // BIT zero
        GET_ADDR_ZERO();
        goto perform_BIT;

    case 0x2C: // BIT abs;
        GET_ADDR_ABS();
        goto perform_BIT;

    perform_BIT:
        {
            const uint8_t b = read_mem(addr);
            const uint8_t result = cpu.a & b;

            AFFECTED_FLAGS(FLAG_ZERO | FLAG_OVERFLOW | FLAG_NEGATIVE);
            CHECK_ZERO(result);
            CHECK_OVERFLOW(b & 0x40);
            CHECK_NEG(b & 0x80);
            PRINT_OP("BIT");
        }
        break;


    case 0xCA: // DEX impl
        GET_ADDR_IMPL();
        cpu.x--;
        AFFECTED_FLAGS(FLAG_ZERO | FLAG_NEGATIVE);
        CHECK_ZERO(cpu.x);
        CHECK_NEG(cpu.x);
        PRINT_OP("DEX");
        break;

    case 0x88: // DEY impl
        GET_ADDR_IMPL();
        cpu.y--;
        AFFECTED_FLAGS(FLAG_ZERO | FLAG_NEGATIVE);
        CHECK_ZERO(cpu.y);
        CHECK_NEG(cpu.y);
        PRINT_OP("DEY");
        break;

    case 0xE8: // INX impl
        GET_ADDR_IMPL();
        cpu.x++;
        AFFECTED_FLAGS(FLAG_ZERO | FLAG_NEGATIVE);
        CHECK_ZERO(cpu.x);
        CHECK_NEG(cpu.x);
        PRINT_OP("INX");
        break;

    case 0xC8: // INY impl
        GET_ADDR_IMPL();
        cpu.y++;
        AFFECTED_FLAGS(FLAG_ZERO | FLAG_NEGATIVE);
        CHECK_ZERO(cpu.y);
        CHECK_NEG(cpu.y);
        PRINT_OP("INY");
        break;

    case 0xE6: // INC zero
        GET_ADDR_ZERO();
        goto perform_INC;

    case 0xF6: // INC zero, X
        GET_ADDR_ZEROX();
        goto perform_INC;

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
        goto perform_DEC;

    case 0xD6: // DEC zero, X
        GET_ADDR_ZEROX();
        goto perform_DEC;

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
        const uint8_t carry = cpu.a & 0x80;
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
        goto perform_ASL;

    case 0x16:
        GET_ADDR_ZEROX();
        goto perform_ASL;

    case 0x0E:
        GET_ADDR_ABS();
        goto perform_ASL;

    case 0x1E:
        GET_ADDR_ABSX();
        goto perform_ASL;

    perform_ASL:
        {
            const uint8_t val = read_mem(addr);
            const uint8_t carry = cpu.a & 0x80;
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
        const uint8_t carry = cpu.a & 0x01;
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
        goto perform_LSR;

    case 0x56: // LSR zero, X
        GET_ADDR_ZEROX();
        goto perform_LSR;

    case 0x4E: // LSR abs
        GET_ADDR_ABS();
        goto perform_LSR;

    case 0x5E: // LSR abs, X
        GET_ADDR_ABSX();
        goto perform_LSR;

    perform_LSR:
        {
            const uint8_t val = read_mem(addr);
            const uint8_t carry = cpu.a & 0x01;
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
        const uint8_t carry = cpu.a & 0x01;
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
        goto perform_ROR;

    case 0x76: // ROR zero, X
        GET_ADDR_ZEROX();
        goto perform_ROR;

    case 0x6E: // ROR abs
        GET_ADDR_ABS();
        goto perform_ROR;

    case 0x7E: // ROR abs, X
        GET_ADDR_ABSX();
        goto perform_ROR;

    perform_ROR:
        {
            const uint8_t val = read_mem(addr);
            const uint8_t carry = cpu.a & 0x01;
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
        const uint8_t carry = cpu.a & 0x80;
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
        goto perform_ROL;

    case 0x36: // ROL zero, X
        GET_ADDR_ZEROX();
        goto perform_ROL;

    case 0x2E: // ROL abs
        GET_ADDR_ABS();
        goto perform_ROL;

    case 0x3E: // ROL zero, X
        GET_ADDR_ABSX();
        goto perform_ROL;

    perform_ROL:
        {
            const uint8_t val = read_mem(addr);
            const uint8_t carry = cpu.a & 0x80;
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
        GET_ADDR_IMM();
        goto perform_LDA;

    case 0xA5: // LDA zero
        GET_ADDR_ZERO();
        goto perform_LDA;

    case 0xB5: // LDA zero, X
        GET_ADDR_ZEROX();
        goto perform_LDA;

    case 0xAD: // LDA abs
        GET_ADDR_ABS();
        goto perform_LDA;

    case 0xBD: // LDA abs, X
        GET_ADDR_ABSX();
        goto perform_LDA;

    case 0xB9: // LDA abs, Y
        GET_ADDR_ABSY();
        goto perform_LDA;

    case 0xA1: // LDA (ind, X)
        GET_ADDR_INDX();
        goto perform_LDA;

    case 0xB1: // LDA (ind), Y
        GET_ADDR_INDY();
        goto perform_LDA;

    perform_LDA:
        cpu.a = read_mem(addr);
        AFFECTED_FLAGS(FLAG_ZERO | FLAG_NEGATIVE);
        CHECK_ZERO(cpu.a);
        CHECK_NEG(cpu.a);
        PRINT_OP("LDA");
        break;



    case 0xA2: // LDX imm
        GET_ADDR_IMM();
        goto perform_LDX;

    case 0xA6: // LDX zero
        GET_ADDR_ZERO();
        goto perform_LDX;

    case 0xB6: // LDX zero, Y
        GET_ADDR_ZEROY();
        goto perform_LDX;

    case 0xAE: // LDX abs
        GET_ADDR_ABS();
        goto perform_LDX;

    case 0xBE: // LDX abs, Y
        GET_ADDR_ABSY();
        goto perform_LDX;

    perform_LDX:
        cpu.x = read_mem(addr);
        AFFECTED_FLAGS(FLAG_ZERO | FLAG_NEGATIVE);
        CHECK_ZERO(cpu.x);
        CHECK_NEG(cpu.x);
        PRINT_OP("LDX");
        break;



    case 0xA0: // LDY imm
        GET_ADDR_IMM();
        goto perform_LDY;

    case 0xA4: // LDY zero
        GET_ADDR_ZERO();
        goto perform_LDY;

    case 0xB4: // LDY zero, X
        GET_ADDR_ZEROX();
        goto perform_LDY;

    case 0xAC: // LDY abs
        GET_ADDR_ABS();
        goto perform_LDY;

    case 0xBC: // LDY abs, X
        GET_ADDR_ABSX();
        goto perform_LDY;

    perform_LDY:
        cpu.y = read_mem(addr);
        AFFECTED_FLAGS(FLAG_ZERO | FLAG_NEGATIVE);
        CHECK_ZERO(cpu.y);
        CHECK_NEG(cpu.y);
        PRINT_OP("LDY");
        break;




    case 0x85: // STA zero
        GET_ADDR_ZERO();
        goto perform_STA;

    case 0x95: // STA zero, X
        GET_ADDR_ZEROX();
        goto perform_STA;

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
        goto perform_STX;

    case 0x96: // STX zero, Y
        GET_ADDR_ZEROY();
        goto perform_STX;

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
        goto perform_STY;

    case 0x94: // STY zero,X
        GET_ADDR_ZEROX();
        goto perform_STY;

    case 0x8C: // STY abs
        GET_ADDR_ABS();
        goto perform_STY;

    perform_STY:
        write_mem(addr, cpu.y);
        NO_FLAGS();
        PRINT_OP("STY");
        break;








    case 0x48: // PHA impl
        GET_ADDR_IMPL();
        STACK_PUSH(cpu.a);
        NO_FLAGS();
        PRINT_OP("PHA");
        break;

    case 0x08: // PHP impl
        GET_ADDR_IMPL();
        STACK_PUSH(cpu.status | FLAG_BREAK);
        NO_FLAGS();
        PRINT_OP("PHP");
        break;

    case 0x68: // PLA impl
        GET_ADDR_IMPL();
        cpu.a = STACK_POP();
        AFFECTED_FLAGS(FLAG_ZERO | FLAG_NEGATIVE);
        CHECK_ZERO(cpu.a);
        CHECK_NEG(cpu.a);
        PRINT_OP("PLA");
        break;

    case 0x28: // PLP impl
        GET_ADDR_IMPL();
        cpu.status = (STACK_POP() & ~FLAG_BREAK) | FLAG_CONSTANT;
        PRINT_OP("PLP");
        break;

    case 0xA8: // TAY impl
        GET_ADDR_IMPL();
        cpu.y = cpu.a;
        AFFECTED_FLAGS(FLAG_ZERO | FLAG_NEGATIVE);
        CHECK_ZERO(cpu.y);
        CHECK_NEG(cpu.y);
        PRINT_OP("TAY");
        break;

    case 0xAA: // TAX impl
        GET_ADDR_IMPL();
        cpu.x = cpu.a;
        AFFECTED_FLAGS(FLAG_ZERO | FLAG_NEGATIVE);
        CHECK_ZERO(cpu.x);
        CHECK_NEG(cpu.x);
        PRINT_OP("TAX");
        break;

    case 0xBA: // TSX impl
        GET_ADDR_IMPL();
        cpu.x = cpu.sp;
        AFFECTED_FLAGS(FLAG_ZERO | FLAG_NEGATIVE);
        CHECK_ZERO(cpu.x);
        CHECK_NEG(cpu.x);
        PRINT_OP("TSX");
        break;

    case 0x8A: // TXA impl
        GET_ADDR_IMPL();
        cpu.a = cpu.x;
        AFFECTED_FLAGS(FLAG_ZERO | FLAG_NEGATIVE);
        CHECK_ZERO(cpu.a);
        CHECK_NEG(cpu.a);
        PRINT_OP("TXA");
        break;

    case 0x9A: // TXS impl
        GET_ADDR_IMPL();
        cpu.sp = cpu.x;
        NO_FLAGS();
        PRINT_OP("TXS");
        break;

    case 0x98: // TYA impl
        GET_ADDR_IMPL();
        cpu.a = cpu.y;
        AFFECTED_FLAGS(FLAG_ZERO | FLAG_NEGATIVE);
        CHECK_ZERO(cpu.a);
        CHECK_NEG(cpu.a);
        PRINT_OP("TYA");
        break;


    case 0x18: // CLC impl
        GET_ADDR_IMPL();
        cpu.status &= ~FLAG_CARRY;
        PRINT_OP("CLC");
        break;

    case 0xD8: // CLD impl
        GET_ADDR_IMPL();
        cpu.status &= ~FLAG_DECIMAL;
        PRINT_OP("CLD");
        break;

    case 0x58: // CLI impl
        GET_ADDR_IMPL();
        cpu.status &= ~FLAG_INTERRUPT;
        PRINT_OP("CLI");
        break;

    case 0xB8: // CLV impl
        GET_ADDR_IMPL();
        cpu.status &= ~FLAG_OVERFLOW;
        PRINT_OP("CLV");
        break;

    case 0x38: // SEC impl
        GET_ADDR_IMPL();
        cpu.status |= FLAG_CARRY;
        PRINT_OP("SEC");
        break;

    case 0xF8: // SEC impl
        GET_ADDR_IMPL();
        cpu.status |= FLAG_DECIMAL;
        PRINT_OP("SED");
        break;

    case 0x78: // SEI impl
        GET_ADDR_IMPL();
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
        GET_ADDR_IMPL();
        const uint8_t lo = STACK_POP();
        const uint8_t hi = STACK_POP();
        cpu.pc = (((uint16_t) hi << 8) | lo) + 1;
        NO_FLAGS();
        PRINT_OP("RTS");
    }
    break;

    case 0x40: // RTI impl
    {
        GET_ADDR_IMPL();
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
        GET_ADDR_IMPL();
        NO_FLAGS();
        PRINT_OP("NOP");
        break;

    case 0x00: // BRK impl
        GET_ADDR_IMPL();
        PRINT_OP("BRK");
        return -2;

    default:
        PRINT_OP("Unkown opcode");
        return -1;
    }

    return 0;
}
