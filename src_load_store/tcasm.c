#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "common.h"

#define MAX_N_LABELS 512
#define MAX_STRLEN 128

code_t *parse(char* filename, int *p_ninst)
{
    code_t *c;
    char buf[MAX_STRLEN];
    char optxt[MAX_STRLEN];
    char arg0txt[MAX_STRLEN];
    char arg1txt[MAX_STRLEN];
    char arg2txt[MAX_STRLEN];

    int n_labels = 0;
    char labeltext[MAX_N_LABELS][MAX_STRLEN];
    uint32_t labelpos[MAX_N_LABELS];

    FILE *input;
    input = fopen(filename, "r");
    assert(input);
    int n_inst = 0;

    while(fgets(buf, MAX_STRLEN, input))
    {
        if (buf[0] == '@')
        {
            sscanf(buf+1, "%s", labeltext[n_labels]);
            labelpos[n_labels] = n_inst;
            n_labels++;
        }
        else
        {
            n_inst ++;
        }
    }

    // printf("%d\n", n_inst);
    c = (code_t*)malloc(sizeof(code_t) * n_inst);

    rewind(input);
    int icnt = 0;
    
    while(fgets(buf, MAX_STRLEN, input))
    {
        int i;
        if (buf[0] != '@')
        {
            sscanf(buf, "%s", optxt);
            for (i = 0; i < N_INSTRUCTIONS; i++)
            {
                if (!strcmp(optxt, ilut[i].opstring))
                {
                    break;
                }
            }
            if (i == N_INSTRUCTIONS)
            {
                printf("Unknown instruction: %s", buf);
            }
            int itype = (ilut[i].opcode >> 3) & 0b111;
            uint8_t opcode = ilut[i].opcode;
            printf("%s op:%hhu t:%d", optxt, opcode, itype);
            
            uint8_t rd = 0, ra = 0, rb = 0;
            uint32_t imm = 0;
            code_t cur = 0;
            if (itype == 0)
            {
                sscanf(buf+strlen(optxt), "%hhu %u", &rd, &imm);
                printf(" rd:%hhu imm:%u", rd, imm);
                cur = (imm << 8) | opcode;
            }
            else if (itype == 1)
            {
                sscanf(buf+strlen(optxt), "%hhu %hhu %hhu", &rd, &ra, &rb);
                // printf(" r:%hhu %hhu %hhu", rd, ra, rb);
                cur = (rb<<18) | (ra<<13) | (rd<<8) | opcode;
                printf(" r:%u %u %u", (cur>>8)&0b11111, (cur>>13)&0b11111, (cur>>18)&0b11111);
            }
            else if (itype == 2)
            {
                char l[MAX_STRLEN];
                sscanf(buf+strlen(optxt), "%s", l);
                for (uint32_t j = 0; j < n_labels; j++)
                {
                    if (!strcmp(l, labeltext[j]))
                    {
                        imm = labelpos[j];
                        break;
                    }
                }
                // printf(" label:%s imm:%u", l, imm);
                cur = (imm<<13) | (rd<<8) | opcode;
                printf(" label:%s imm:%u", l, (cur>>13));
            }
            printf("\n");
            c[icnt] = cur;
            icnt++;                
        }
    }

    fclose(input);
 
    #ifdef VERBOSE
    printf("Labels:\n");
    for (int i = 0; i < n_labels; i++)
    {
        printf("- %s %d\n", labeltext[i], labelpos[i]);
    }
    printf("\n");
    printf("Instructions:\n");
    for (int i = 0; i < c->n_inst; i++)
    {
        printf("%d: %u %u %d\n", i, c->opcode_t[i], c->arg0[i], c->arg1[i]);
    }
    #endif

    *p_ninst = n_inst;
    return c;
}

void dump(char *filename, code_t *pc, int n_inst)
{
    FILE *output;
    output = fopen(filename, "wt");
    for (int i = 0; i < n_inst; i++)
    {
        fprintf(output, "%u\n", pc[i]);
    }

    fclose(output);
}

int main(int argc, char *argv[])
{
    int n_inst;
    code_t *pc;
    if (argc > 2)
    {
        printf("%s\n", argv[1]);
        pc = parse(argv[1], &n_inst);
        dump(argv[2], pc, n_inst);
    }
    printf("Number of instructions: %d\n", n_inst);
    return 0;
}
