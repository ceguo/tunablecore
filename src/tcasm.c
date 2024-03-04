#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define hmax 997
#define bmax 65536

const uint32_t lsmax = 256;
uint8_t hash_to_op[hmax];
uint8_t hash_to_op_used[hmax];


uint32_t hash_text(char *str)
{
    const uint32_t base = 31;
    uint32_t h = 0;
    uint32_t p = 1;
    for (int i = 0; str[i] != '\0'; i++)
    {
        h = (h + str[i] * p) % hmax;
        p *= base;
    }
    return h;
}


int register_instruction(char *text, uint8_t op)
{
    uint32_t h = hash_text(text);
    if (hash_to_op_used[h])
    {
        printf("Instruction Hash Collision %u\n", op);
        return 1;
    }
    else
    {
        hash_to_op[h] = op;
        hash_to_op_used[h] = 1;
    }
    return 0;
}


int main(int argc, char *argv[]) {
    FILE *fp;
    FILE *fout;
    char line[lsmax];
    char first[lsmax];
    uint8_t bs[bmax] = {0};
    int bsp = 0;

    // tt = 00
    register_instruction("nop",   0b00000000);
    register_instruction("shl",   0b00001000);
    register_instruction("shr",   0b00001100);
    register_instruction("bor",   0b00010000);
    register_instruction("band",  0b00010100);
    register_instruction("add",   0b00100000);
    register_instruction("sub",   0b00100100);
    register_instruction("mul",   0b00110000);
    register_instruction("div",   0b01000000);
    register_instruction("load",  0b10000000);
    register_instruction("store", 0b10010000);
    
    // tt = 01
    // oott = 0001
    register_instruction("jz",    0b00100001);
    register_instruction("jp",    0b00110001);

    // oott = 0101
    register_instruction("li",    0b00000101);

    if (argc != 3) {
        printf("Usage: %s <filename> <filename>\n", argv[0]);
        return 0;
    }

    fp = fopen(argv[1], "r");
    if (fp == NULL) {
        printf("Unable to open file %s", argv[1]);
        return 0;
    }

    while (fgets(line, sizeof(line), fp) != NULL && strlen(line) > 1) {
        // printf("%s", line);
        sscanf(line, "%s", first);
        char *pia = line + strlen(first);

        uint32_t h = hash_text(first);
        uint8_t op = hash_to_op[h];
        uint8_t itype = op & 0b11;

        uint16_t riz, rix, riy;
        int16_t imm;

        #ifdef __VERBOSE__
        printf("%s   op:%u", line, op);
        #endif

        if (first[0] == '@')
        {
            // Label
        }
        else
        {
            // Instruction
            uint8_t b1 = 0, b2 = 0, b3 = 0;
            if (itype == 0)
            {
                sscanf(pia, "%hu%hu%hu", &riz, &rix, &riy);
                #ifdef __VERBOSE__
                printf(" (R) $%hu $%hu $%hu\n", riz, rix, riy);
                #endif
                b1 = riz & 0b1111;
                b2 = ((rix & 0b1111) << 4) | (riy & 0b1111);
                b3 = 0;
            }
            else if (itype == 1)
            {
                sscanf(pia, "%hu%hd", &riz, &imm);
                #ifdef __VERBOSE__
                printf(" (I) $%hu %hd\n", riz, imm);
                #endif
                b1 = riz & 0b1111;
                b2 = (imm >> 8) & 0b11111111;
                b3 = imm & 0b11111111;
            }

            // Emit instruction
            bs[bsp + 0] = op;
            bs[bsp + 1] = b1;
            bs[bsp + 2] = b2;
            bs[bsp + 3] = b3;
            bsp += 4;
        }
    }
    fclose(fp);

    // Write binary stream file
    fout = fopen(argv[2], "wt");
    if (fout == NULL) {
        printf("Unable to open file %s", argv[2]);
        return 0;
    }
    fwrite(bs, sizeof(uint8_t), bsp, fp);
    fclose(fout);

    return 0;
}
