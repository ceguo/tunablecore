#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>

#define nreg 16
#define lsmax 16
#define maxstrlen 1024
#define maxcycles 100000

//#define __VERBOSE__

struct Tunable
{
    // Division algorithm: [0] long division [1] restoring [2] SRT [3] Newton–Raphson 
    uint8_t div_algo;
    // Branch predictor initial guess: 0..1
    uint8_t bp_init_guess;
    // Branch predictor tolerance before reversing decision: 0..16
    uint32_t bp_wrong_tol;
    // Cache: set ID width: 1..5
    uint8_t cache_setid_width;
    // Cache: line width: 1..6
    uint8_t cache_line_width;
    // Cache: number of ways: 1..16
    uint8_t cache_n_ways;
};


// Data types
typedef int32_t cell_t;
typedef uint32_t memaddr_t;
typedef char bool;

typedef struct Memory Memory;
struct Memory
{
    // Raw parameters
    int addr_w;
    int setid_w;
    int line_w;
    int n_ways;

    // Sizes
    int mem_sz;
    int n_sets;
    int line_sz;

    // History
    memaddr_t last_addr;

    // Main memory
    cell_t *mc;

    // Cache
    memaddr_t *line_tag;
    bool *line_valid;
    cell_t *cc;

    // Status
    bool cache_hit;
};


int memory_init(Memory *m, cell_t *content, int addr_w, int setid_w, int line_w, int n_ways)
{
    m->addr_w = addr_w;
    m->setid_w = setid_w;
    m->line_w = line_w;
    m->n_ways = n_ways;

    m->mem_sz = 1 << addr_w;
    m->n_sets = 1 << setid_w;
    m->line_sz = 1 << line_w;

    // Initialise main memory
    m->mc = content;

    // Initialise cache
    m->line_tag = calloc((m->n_sets) * (m->n_ways), sizeof(memaddr_t));
    m->line_valid = calloc((m->n_sets) * (m->n_ways), sizeof(bool));

    return 0;
}


int cache_locate(Memory *m, memaddr_t addr)
{
    memaddr_t tag = (addr >> (m->line_w + m->setid_w));
    memaddr_t setid = (addr >> m->line_w) & (m->n_sets - 1);
    
    m->last_addr = addr;

    #ifdef __VERBOSE__
    memaddr_t offset = addr & (m->line_sz - 1);
    printf("A: %u Tag: %u Set: %u Offset: %u", addr, tag, setid, offset);
    #endif

    // Match tag in specific tag
    memaddr_t line_match = m->n_ways;
    for (memaddr_t line_sel = 0; line_sel < m->n_ways; line_sel++)
        if (m->line_valid[setid * m->n_ways + line_sel])
            if (m->line_tag[setid * m->n_ways + line_sel] == tag)
                line_match = line_sel;
    
    // Determine cache status
    if (line_match < m->n_ways)
    {
        // Hit: increment line frequency
        #ifdef __VERBOSE__
        printf(" Hit\n");
        #endif
        m->cache_hit = 1;
        return 1;
    }
    else
    {
        // Miss: random replacement
        #ifdef __VERBOSE__
        printf(" Miss\n");
        #endif
        m->cache_hit = 0;

        memaddr_t line_least = rand()%(m->n_ways);
        
        m->line_tag[setid * m->n_ways + line_least] = tag;
        m->line_valid[setid * m->n_ways + line_least] = 1;
        return 0;
    }

    return -1;
}


void cache_inspect(Memory *m){
    for (memaddr_t setid = 0; setid < m->n_sets; setid++)
    {
        printf("Set %u:", setid);
        for (memaddr_t line_sel = 0; line_sel < m->n_ways; line_sel++)
        {
            printf(" %u", m->line_tag[setid * m->n_ways + line_sel]);
        }
        printf("\n");
    }
}

cell_t memory_read(Memory *m, memaddr_t addr)
{
    cache_locate(m, addr);
    if (addr < m->mem_sz)
        return m->mc[addr];

    return 0;
}


bool memory_write(Memory *m, memaddr_t addr, cell_t val)
{
    cache_locate(m, addr);
    if (addr < m->mem_sz)
    {
        m->mc[addr] = val;
        return 0;
    }

    return 1;
}


int simulate(Memory *pmem, int32_t bsize, struct Tunable *pt, cell_t *r_init)
{
    // Word length
    const uint8_t ws = 4;

    // Cycle count
    int ncyc = 0;

    // Registers
    cell_t r[nreg];
    if (r_init == NULL)
        for (int i = 0; i < nreg; i++)
            r[i] = 0;
    else
        for (int i = 0; i < nreg; i++)
            r[i] = r_init[i];

    // OS: free memory start point saved to last register
    r[nreg-1] = bsize / sizeof(uint32_t);

    // Code (eadian-independent)
    char *bs = (char*)(pmem->mc);
    int bsp = 0;
    
    // Branch predictor
    uint8_t bp_guess = pt->bp_init_guess;
    uint32_t bp_wrong_count = 0;
    int bp_stall = 0;
    int div_stall = 0;
    int cache_stall = 0;

    // Division
    int div_stall_list[] = {5, 3, 1, 0};

    // Intrepretation
    while (bsp < bsize && ncyc < maxcycles)
    {
        bp_stall = 0;
        div_stall = 0;
        cache_stall = 0;

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
                case 0b00001000:
                    #ifdef __VERBOSE__
                    strcpy(first, "shl");
                    #endif
                    r[riz] = r[rix] << r[riy];
                    break;
                case 0b00001100:
                    #ifdef __VERBOSE__
                    strcpy(first, "shr");
                    #endif
                    r[riz] = r[rix] >> r[riy];
                    break;
                case 0b00010000:
                    #ifdef __VERBOSE__
                    strcpy(first, "bor");
                    #endif
                    r[riz] = r[rix] | r[riy];
                    break;
                case 0b00010100:
                    #ifdef __VERBOSE__
                    strcpy(first, "band");
                    #endif
                    r[riz] = r[rix] & r[riy];
                    break;
                case 0b00100000:
                    #ifdef __VERBOSE__
                    strcpy(first, "add");
                    #endif
                    r[riz] = r[rix] + r[riy];
                    break;
                case 0b00100100:
                    #ifdef __VERBOSE__
                    strcpy(first, "sub");
                    #endif
                    r[riz] = r[rix] - r[riy];
                    break;
                case 0b00110000:
                    #ifdef __VERBOSE__
                    strcpy(first, "mul");
                    #endif
                    r[riz] = r[rix] * r[riy];
                    break;
                case 0b01000000:
                    #ifdef __VERBOSE__
                    strcpy(first, "div");
                    #endif
                    // Skip instruction if register riy is 0
                    if (r[riy] != 0)
                    {
                        r[riz] = r[rix] / r[riy];
                    }
                    div_stall = div_stall_list[pt->div_algo];
                    break;
                case 0b10000000:
                    #ifdef __VERBOSE__
                    strcpy(first, "load");
                    #endif
                    // r[riz] = pmem->mc[r[rix] + r[riy]];
                    r[riz] = memory_read(pmem, r[rix] + r[riy]);
                    if (pmem->cache_hit == 0)
                    {
                        cache_stall = 4;
                    }
                    else
                    {
                        cache_stall = 0;
                    }
                    
                    break;
                case 0b10010000:
                    #ifdef __VERBOSE__
                    strcpy(first, "store");
                    #endif
                    // pmem->mc[r[rix] + r[riy]] = r[riz];
                    memory_write(pmem, r[rix] + r[riy], r[riz]);
                    if (pmem->cache_hit == 0)
                    {
                        cache_stall = 4;
                    }
                    else
                    {
                        cache_stall = 0;
                    }

                    break;
                default:
                    #ifdef __VERBOSE__
                    strcpy(first, "unknown");
                    #endif
                    break;
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
                    break;
            }
        }

        // Branch predictor
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
        ncyc += 1 + bp_stall + div_stall + cache_stall;
        
        #ifdef __VERBOSE__
        printf("%u: %s\t[%2hu %2hu %2hu] %6hd B%hhu (%d)\n", bsp, first, riz, rix, riy, imm, take_branch, ncyc);
        #endif
    }

    for (int i = 0; i < nreg; i++)
    {
        printf ("Register %d %d\n", i, r[i]);
    }

    return ncyc;
}


int estimate_cost(struct Tunable* pt)
{
    int div_cost[] = {10, 20, 50, 100};
    int base_cost = div_cost[pt->div_algo] + pt->bp_wrong_tol;

    int n_sets = 1 << pt->cache_setid_width;
    int line_sz = 1 << pt->cache_line_width;
    int cache_sz = n_sets * line_sz * (pt->cache_n_ways);
    int cache_cost = cache_sz * 5;

    return base_cost + cache_cost;
}


int main(int argc, char *argv[]) {
    // Tunable parameters (hard-coded)
    struct Tunable tunable;
    tunable.div_algo = 2;
    tunable.bp_init_guess = 0;
    tunable.bp_wrong_tol = 4;
    tunable.cache_setid_width = 4;
    tunable.cache_line_width = 2;
    tunable.cache_n_ways = 3;

    cell_t *r_init = NULL;

    int dump_flag = 0;
    char dump_file[maxstrlen];

    const uint32_t addr_w = 14;
    // Maximum code size
    const uint32_t maxcs = 4096;
    // Number of memory slots
    const uint32_t nms = 1 << addr_w;
    // Memory content
    cell_t *mc;
    mc = calloc(nms, sizeof(cell_t));

    if (argc < 2) {
        printf("Usage: %s filename\n", argv[0]);
        return 1;
    }

    FILE *fp = fopen(argv[1], "rb");
    if (fp == NULL) {
        printf("Error: could not open file %s\n", argv[1]);
        return 1;
    }
    int32_t bsize = fread(mc, sizeof(uint8_t), maxcs, fp);
    fclose(fp);


    // Process optional command-line arguments
    for (int i = 2; i < argc; i++)
    {
        if (!strcmp(argv[i], "-c"))
        {
            if (argc < i + 2) {
                printf("Error: configuration file not specified\n");
                return 1;
            }
            FILE *cfg = fopen(argv[i+1], "r");
            if (cfg == NULL) {
                printf("Error: could not open configuration file %s\n", argv[i+1]);
                return 1;
            }
            assert(fscanf(cfg, "%hhu", &tunable.div_algo) == 1);
            assert(fscanf(cfg, "%hhu", &tunable.bp_init_guess) == 1);
            assert(fscanf(cfg, "%u", &tunable.bp_wrong_tol) == 1);
            assert(fscanf(cfg, "%hhu", &tunable.cache_setid_width) == 1);
            assert(fscanf(cfg, "%hhu", &tunable.cache_line_width) == 1);
            assert(fscanf(cfg, "%hhu", &tunable.cache_n_ways) == 1);
            fclose(cfg);
        }

        if (!strcmp(argv[i], "-r"))
        {
            if (argc < i + 2) {
                printf("Error: register file not specified\n");
                return 1;
            }
            FILE *mi = fopen(argv[i+1], "r");
            if (mi == NULL) {
                printf("Error: could not open register file %s\n", dump_file);
                return 1;
            }
            char buf[maxstrlen];
            r_init = calloc(nreg, sizeof(cell_t));
            while (fgets(buf, maxstrlen, mi))
            {
                int add, val;
                assert(sscanf(buf, "%d%d", &add, &val)==2);
                // printf("Register %d: %d\n", add, val);
                r_init[add] = val;
            }
            fclose(mi);
        }

        if (!strcmp(argv[i], "-i"))
        {
            if (argc < i + 2) {
                printf("Error: dump file not specified\n");
                return 1;
            }
            FILE *mi = fopen(argv[i+1], "r");
            if (mi == NULL) {
                printf("Error: could not open memory dump file %s\n", dump_file);
                return 1;
            }
            char buf[maxstrlen];
            while (fgets(buf, maxstrlen, mi))
            {
                int add, val;
                assert(sscanf(buf, "%d%d", &add, &val)==2);
                printf("%d %d\n", add, val);
                mc[add] = val;
            }
            fclose(mi);
        }

        if (!strcmp(argv[i], "-o"))
        {
            if (argc < i + 2) {
                printf("Error: dump file not specified\n");
                return 1;
            }
            dump_flag = 1;
            strcpy(dump_file, argv[i+1]);
        }
    }


    // Cached data memory
    Memory mem;
    memory_init(&mem, mc, addr_w, tunable.cache_setid_width, tunable.cache_line_width, tunable.cache_n_ways);
    srand(42);

    // Run simulation
    int ncyc = simulate(&mem, bsize, &tunable, r_init);
    int hwcost = estimate_cost(&tunable);
    #ifdef __VERBOSE__
    printf("%d\n%d\n", ncyc, hwcost);
    #endif
    printf("%d\n", ncyc + hwcost);

    if (dump_flag == 1)
    {
        FILE *md = fopen(dump_file, "wt");
        if (md == NULL) {
            printf("Error: could not open memory dump file %s\n", dump_file);
            return 1;
        }
        for (int i = 0; i < nms; i++)
        {
            if (mc[i] != 0)
            {
                fprintf(md, "%d %d\n", i, mc[i]);
            }
        }
        fclose(md);
    }
    if (r_init != NULL)
        free(r_init);
    free(mc);
    return 0;
}
