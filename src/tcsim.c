#include <stdio.h>
#include <string.h>
#include <stdint.h>


struct Tunable
{
    // Branch predictor initial guess: 0 or 1
    uint8_t bp_init_guess;
    // Branch predictor tolorance before reversing decision: 0..+Inf
    uint32_t bp_wrong_tol;
};


int simulate(int32_t mem[], int32_t bsize, struct Tunable *pt)
{
    // Maximum string length
    const uint32_t lsmax = 256;
    // Word length
    const uint8_t ws = 4;
    // Number of registers
    const uint8_t nreg = 16;

    // Cycle count
    int ncyc = 0;

    // Registers
    int32_t r[nreg] = {0};

    // OS: free memory start point saved to last register
    r[nreg-1] = bsize;

    // Code (eadian-independent)
    char *bs = (char*)mem;
    int bsp = 0;
    
    // Branch predictor
    uint8_t bp_guess = pt->bp_init_guess;
    uint32_t bp_wrong_count = 0;

    // Intrepretation
    while (bsp < bsize)
    {
        int stall = 0;
        #ifdef __VERBOSE__
        char first[lsmax];
        #endif
        
        // Coarse-grained decoding
        uint8_t b0 = bs[bsp], b1 = bs[bsp+1], b2 = bs[bsp+2], b3 = bs[bsp+3];
        uint16_t riz = b1 & 0b1111;
        uint16_t rix = (b2 >> 4) & 0b1111;
        uint16_t riy = b2 & 0b1111;
        int16_t imm = (b2 << 8) | b3;
        uint8_t itype = bs[bsp] & 0b11;
        uint8_t take_branch = 0;
        uint8_t bp_active = 0;

        // Fine-grained decoding
        if (itype == 0)
        {
            // tt = 00            
            switch (b0)
            {
                case 0b00000000:
                    #ifdef __VERBOSE__
                    strcpy(first, "nop");
                    #endif
                    break;
                case 0b00000100:
                    #ifdef __VERBOSE__
                    strcpy(first, "add");
                    #endif
                    r[riz] = r[rix] + r[riy];
                    break;
                case 0b00001000:
                    #ifdef __VERBOSE__
                    strcpy(first, "sub");
                    #endif
                    r[riz] = r[rix] - r[riy];
                    break;
                case 0b00001100:
                    #ifdef __VERBOSE__
                    strcpy(first, "mul");
                    #endif
                    r[riz] = r[rix] * r[riy];
                    break;
                case 0b00010000:
                    #ifdef __VERBOSE__
                    strcpy(first, "div");
                    #endif
                    // Skip instruction if register riy is 0
                    if (r[riy] != 0)
                    {
                        r[riz] = r[rix] / r[riy];
                    }
                    break;
                case 0b00100000:
                    #ifdef __VERBOSE__
                    strcpy(first, "load");
                    #endif
                    r[riz] = mem[r[rix] + r[riy]];
                    break;
                case 0b00100100:
                    #ifdef __VERBOSE__
                    strcpy(first, "store");
                    #endif
                    mem[r[rix] + r[riy]] = r[riz];
                    break;
                default:
                    #ifdef __VERBOSE__
                    strcpy(first, "unknown");
                    #endif
            }
        }
        else if (itype == 1)
        {
            switch (b0)
            {
                case 0b00100001:
                    #ifdef __VERBOSE__
                    strcpy(first, "jz");
                    #endif
                    bp_active = 1;
                    take_branch = (r[riz] == 0);
                    break;
                case 0b00110001:
                    #ifdef __VERBOSE__
                    strcpy(first, "jp");
                    #endif
                    bp_active = 1;
                    take_branch = (r[riz] > 0);
                    break;
                case 0b00000101:
                    #ifdef __VERBOSE__
                    strcpy(first, "li");
                    #endif
                    r[riz] = (int32_t)imm;
                    break;
                default:
                    #ifdef __VERBOSE__
                    strcpy(first, "unknown");
                    #endif
            }
        }

        // Branch predictor
        int bp_stall = 0;
        if (bp_active)
        {
            if (bp_guess == take_branch)
            {
                bp_wrong_count = 0;
                // printf("Right: %d\n", bp_wrong_count);
            }
            else
            {
                bp_wrong_count++;
                // printf("Wrong: %d\n", bp_wrong_count);
                bp_stall = 10;
            }
            if (bp_wrong_count >= pt->bp_wrong_tol)
            {
                bp_guess = !bp_guess;
                bp_wrong_count = 0;
            }
        }
        bsp = bsp + ws * (take_branch ? imm : 1);
        
        stall = bp_stall;
        ncyc += (1 + stall);
        
        #ifdef __VERBOSE__
        printf("%u: %s\t[%2hu %2hu %2hu] %6hd B%hhu (%d)\n", bsp, first, riz, rix, riy, imm, take_branch, ncyc);
        #endif
    }

    #ifdef __VERBOSE__
    for (int i = 0; i < nreg; i++)
    {
        printf ("$%d = %d\n", i, r[i]);
    }
    #endif

    return ncyc;
}


int main(int argc, char *argv[]) {
    // Maximum code size
    const uint32_t maxcs = 4096;
    // Number of memory slots
    const uint32_t nms = 32768;
    // Main memory
    int32_t mem[nms];

    // Tunable parameters (hard-coded)
    struct Tunable tunable;
    tunable.bp_init_guess = 0;
    tunable.bp_wrong_tol = 4;

    if (argc < 2) {
        printf("Usage: %s filename\n", argv[0]);
        return 1;
    }

    FILE *fp = fopen(argv[1], "rb");
    if (fp == NULL) {
        printf("Error: could not open file %s\n", argv[1]);
        return 1;
    }
    int32_t bsize = fread(mem, sizeof(uint8_t), maxcs, fp);
    fclose(fp);

    // Run simulation
    int ncyc = simulate(mem, bsize, &tunable);
    printf("%d\n", ncyc);
    return 0;
}
