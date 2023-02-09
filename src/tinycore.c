#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

// Tuneable parameters
// Three cache parameters:
//     SETID_WIDTH: 1--4
//     LINE_WIDTH: 1--4
//     N_WAYS: 1--16

#define SETID_WIDTH 4
#define LINE_WIDTH 2
#define N_WAYS 3
// One discrete parameter:
// if BRANCH_PREDICT_JUMP is defined: branch predictor will always predict 'jump'
// #define BRANCH_PREDICT_JUMP
// One ordinal parameter: see 'PERMUTATION' in execute()

// Fixed parameters
#define ADDR_WIDTH 16
#define CACHE_SIZE_PRICE_COEF 6
#define CACHE_WAYS_PRICE_COEF 12

#define MAX_CODE_LEN 5000

// Data types
typedef int32_t memcell_t;
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
    memcell_t *mc;

    // Cache
    memaddr_t *line_tag;
    bool *line_valid;
    memcell_t *cc;

    // Status
    bool cache_hit;
};


typedef struct Code Code;
struct Code
{
    uint32_t n_inst;
    uint8_t opcode[MAX_CODE_LEN];
    memaddr_t arg0[MAX_CODE_LEN];
    memaddr_t arg1[MAX_CODE_LEN];
};


int memory_init(Memory *m, int addr_w, int setid_w, int line_w, int n_ways)
{
    m->addr_w = addr_w;
    m->setid_w = setid_w;
    m->line_w = line_w;
    m->n_ways = n_ways;

    m->mem_sz = 1 << addr_w;
    m->n_sets = 1 << setid_w;
    m->line_sz = 1 << line_w;

    // Initialise main memory
    m->mc = calloc(m->mem_sz, sizeof(memcell_t));

    // Initialise cache
    m->line_tag = calloc((m->n_sets) * (m->n_ways), sizeof(memaddr_t));
    m->line_valid = calloc((m->n_sets) * (m->n_ways), sizeof(bool));

    return 0;
}


int cache_locate(Memory *m, memaddr_t addr)
{
    memaddr_t tag = (addr >> (m->line_w + m->setid_w));
    memaddr_t setid = (addr >> m->line_w) & (m->n_sets - 1);
    memaddr_t offset = addr & (m->line_sz - 1);

    m->last_addr = addr;

    #ifdef VERBOSE
    printf("A: %u T: %u S: %u O: %u", addr, tag, setid, offset);
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
        #ifdef VERBOSE
        printf(" Hit\n");
        #endif
        m->cache_hit = 1;
        return 1;
    }
    else
    {
        // Miss: replace least used line
        #ifdef VERBOSE
        printf(" Miss\n");
        #endif
        m->cache_hit = 0;
        
        // Select least used line
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

memcell_t memory_read(Memory *m, memaddr_t addr)
{
    cache_locate(m, addr);
    if (addr < m->mem_sz)
        return m->mc[addr];

    return 0;
}


bool memory_write(Memory *m, memaddr_t addr, memcell_t val)
{
    cache_locate(m, addr);
    if (addr < m->mem_sz)
    {
        m->mc[addr] = val;
        return 0;
    }

    return 1;
}


void parse(char* filename, Code *c)
{
    const int MAX_STRLEN = 256;
    char buf[MAX_STRLEN];
    char optxt[MAX_STRLEN];
    char arg0txt[MAX_STRLEN];
    char arg1txt[MAX_STRLEN];

    int n_labels = 0;
    char labeltext[MAX_CODE_LEN][MAX_STRLEN];
    int32_t labelpos[MAX_CODE_LEN];

    FILE *input;
    input = fopen(filename, "r");
    c->n_inst = 0;

    while(fgets(buf, MAX_STRLEN, input))
    {
        if (buf[0] == '@')
        {
            sscanf(buf+1, "%s", labeltext[n_labels]);
            labelpos[n_labels] = c->n_inst;
            n_labels++;
        }
        else
        {
            c->n_inst ++;
        }
    }

    rewind(input);
    c->n_inst = 0;
    while(fgets(buf, MAX_STRLEN, input))
    {
        if (buf[0] != '@')
        {
            sscanf(buf, "%s%s%s", optxt, arg0txt, arg1txt);
            sscanf(arg0txt, "%u", &(c->arg0[c->n_inst]));
            if (!strcmp(optxt, "bnz"))
            {
                c->opcode[c->n_inst] = 1;
                c->arg1[c->n_inst] = c->n_inst + 1;
                for (int i = 0; i < n_labels; i++)
                {
                    if (!strcmp(labeltext[i], arg1txt))
                    {
                        c->arg1[c->n_inst] = labelpos[i];
                    }
                }
            }
            else
            {
                sscanf(arg1txt, "%u", &(c->arg1[c->n_inst]));
                if (!strcmp(optxt, "set"))
                {
                    c->opcode[c->n_inst] = 2;
                }
                else if (!strcmp(optxt, "sub"))
                {
                    c->opcode[c->n_inst] = 3;
                }
            }
            c->n_inst ++;
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
        printf("%d: %u %u %d\n", i, c->opcode[i], c->arg0[i], c->arg1[i]);
    }
    #endif
}


int execute(Memory *m, Code *c)
{
    int pc = 0;
    int n_cycles = 0;
    while (pc < c->n_inst)
    {
        // printf("PC: %d\n", pc);
        uint8_t opcode = c->opcode[pc];
        memaddr_t arg0 = c->arg0[pc];
        memaddr_t arg1 = c->arg1[pc];

        if (opcode == 1)
        {
            // bnz
            memcell_t cell0 = memory_read(m, arg0);
            // printf("arg0: %u ", arg0);
            // printf("BNZ cell0: %d %d\n", cell0, m->mc[arg0]);
            if (cell0 != 0)
            {
                pc = arg1 - 1;
                #ifdef BRANCH_PREDICT_JUMP
                n_cycles += 1;
                #else
                n_cycles += 3;
                #endif
            }
            else
            {
                #ifdef BRANCH_PREDICT_JUMP
                n_cycles += 3;
                #else
                n_cycles += 1;
                #endif
            }
            if (m->cache_hit)
            {
                n_cycles += 1;
            }
            else
            {
                n_cycles += 3;
            }
        }
        else if (opcode == 2)
        {
            // set
            memory_write(m, arg0, (memcell_t)arg1);
            if (m->cache_hit)
            {
                n_cycles += 1;
            }
            else
            {
                n_cycles += 3;
            }
        }
        else if (opcode == 3)
        {
            // sub
            // PERMUTATION: order of code blocks 0 and 1 can be swapped

            // code block 0
            memcell_t cell0 = memory_read(m, arg0);
            bool hit0 = m->cache_hit;

            // code block 1
            memcell_t cell1 = memory_read(m, arg1);
            bool hit1 = m->cache_hit;
            
            if (m->last_addr == arg1)
                n_cycles += 1;

            cell0 -= cell1;
            memory_write(m, arg0, cell0);
            // printf("val: %d\n", m->mc[cell0]);
            bool hitw = m->cache_hit;

            if (hit0 && hit1 && hitw)
            {
                n_cycles += 1;
            }
            else
            {
                n_cycles += 5;
            }
        }
        pc++;
    }

    return n_cycles;
}


int main(int argc, char *argv[])
{
    srand(43);
    
    Memory mem;
    memory_init(&mem, ADDR_WIDTH, SETID_WIDTH, LINE_WIDTH, N_WAYS);
    Code co;
    parse(argv[1], &co);
    
    int n_cycles = execute(&mem, &co);
    int n_cycles_adj = n_cycles;

    // for (memaddr_t i = 0; i < 1000; i++)
    // {
    //     memory_write(&mem, i, (memcell_t)(i*2));
    // }

    // printf("Fetching:\n");
    // printf("val: %d\n", memory_read(&mem, 445));
    // printf("val: %d\n", memory_read(&mem, 445));
    // printf("val: %d\n", memory_read(&mem, 446));
    // printf("val: %d\n", memory_read(&mem, 447));
    // printf("val: %d\n", memory_read(&mem, 448));
    // printf("val: %d\n", memory_read(&mem, 449));
    // printf("val: %d\n", memory_read(&mem, 988));
    // printf("val: %d\n", memory_read(&mem, 989));
    // printf("val: %d\n", memory_read(&mem, 894));
    // printf("val: %d\n", memory_read(&mem, 446));
    
    // cache_inspect(&mem);
    printf("Adjusted total number of cycles: %d", n_cycles_adj);

    return 0;
}
