
#include "mem.h"
#include <pthread.h>
#include <stdio.h>
#include "stdlib.h"
#include "string.h"

static BYTE _ram[RAM_SIZE];

static struct {
        uint32_t proc; // ID of process currently uses this page
        int index;     // Index of the page in the list of pages allocated
                       // to the process.
        int next;      // The next page in the list. -1 if it is the last
                       // page.
} _mem_stat[NUM_PAGES];

static pthread_mutex_t mem_lock;

void init_mem(void) {
        memset(_mem_stat, 0, sizeof(*_mem_stat) * NUM_PAGES);
        memset(_ram, 0, sizeof(BYTE) * RAM_SIZE);
        pthread_mutex_init(&mem_lock, NULL);
}

/* get offset of the virtual address */
static addr_t get_offset(addr_t addr) { return addr & ~((~0U) << OFFSET_LEN); }

/* get the first layer index */
static addr_t get_first_lv(addr_t addr) {
        return addr >> (OFFSET_LEN + PAGE_LEN);
}

/* get the second layer index */
static addr_t get_second_lv(addr_t addr) {
        return (addr >> OFFSET_LEN) - (get_first_lv(addr) << PAGE_LEN);
}

/* Search for page table table from the a segment table */
static struct page_table_t*
get_page_table(addr_t index,                    // Segment level index
               struct seg_table_t* seg_table) { // first level table

        /*
         * TODO: Given the Segment index [index], you must go through each
         * row of the segment table [seg_table] and check if the v_index
         * field of the row is equal to the index
         *
         * */

        int i;
        for (i = 0; i < seg_table->size; i++) {
#ifdef DEBUGGING
                // printf("SEG %d\n", i);
#endif
                if (seg_table->table[i].v_index == index)
                        return seg_table->table[i].pages;
        }
        return NULL;
}

/* Translate virtual address to physical address. If [virtual_addr] is valid,
 * return 1 and write its physical counterpart to [physical_addr].
 * Otherwise, return 0 */
static int translate(addr_t virtual_addr,   // Given virtual address
                     addr_t* physical_addr, // Physical address to be returned
                     struct pcb_t* proc) { // Process uses given virtual address

        /* Offset of the virtual address */
        addr_t offset = get_offset(virtual_addr);
        /* The first layer index */
        addr_t first_lv = get_first_lv(virtual_addr);
        /* The second layer index */
        addr_t second_lv = get_second_lv(virtual_addr);


        /* Search in the first level */
        struct page_table_t* page_table = NULL;
        page_table = get_page_table(first_lv, proc->seg_table);
        if (page_table == NULL) {
                return 0;
        }

        int i;
        for (i = 0; i < page_table->size; i++) {
#ifdef DEBUGGING
                // printf("PG %d v_index: %x\n", i,
                // page_table->table[i].v_index); printf("\tp_index: %d\n",
                // page_table->table[i].p_index);
#endif
                if (page_table->table[i].v_index == second_lv) {
                        /* TODO: Concatenate the offset of the virtual addess
                         * to [p_index] field of page_table->table[i] to
                         * produce the correct physical address and save it to
                         * [*physical_addr]  */
                        *physical_addr =
                            (page_table->table[i].p_index << OFFSET_LEN) |
                            offset;

#ifdef DEBUGGING
                        printf("TRANSLATING: %x -> %x %x %x -> %x\n",
                               virtual_addr, first_lv, second_lv, offset,
                               *physical_addr);
#endif
                        return 1;
                }
        }
        return 0;
}


typedef struct {
        addr_t v_index;
        struct page_table_t* pages;
} segment_table;

typedef struct {
        addr_t v_index;
        addr_t p_index;
} page_table;

struct {
        addr_t v_index;
        struct page_table_t* pages;
} * get_segment(addr_t first_lv, struct pcb_t* proc) {
        // get segment in process
        struct {
                addr_t v_index;
                struct page_table_t* pages;
        }* segment = NULL;
        int idx = 0;
        for (idx = 0; idx < proc->seg_table->size; idx++) {
                // find the seg_table with v_index = first_level
                if (first_lv == proc->seg_table->table[idx].v_index)
                        break;
        }

        if (idx == proc->seg_table->size) {
                proc->seg_table->size++;
        }

        segment = &(proc->seg_table->table[idx]);
        segment->v_index = first_lv;

        if (segment->pages == NULL) {
                segment->pages =
                    (struct page_table_t*)malloc(sizeof(struct page_table_t));
        }

        return segment;
}

addr_t alloc_mem(uint32_t size, struct pcb_t* proc) {
        pthread_mutex_lock(&mem_lock);
        addr_t ret_mem = 0;
        /* TODO: Allocate [size] byte in the memory for the
         * process [proc] and save the address of the first
         * byte in the allocated memory region to [ret_mem].
         * */

        uint32_t num_pages =
            (size % PAGE_SIZE)
                ? size / PAGE_SIZE
                : size / PAGE_SIZE + 1; // Number of pages we will use
        int mem_avail = 0; // We could allocate new memory region or not?

        /* First we must check if the amount of free memory in
         * virtual address space and physical address space is
         * large enough to represent the amount of required
         * memory. If so, set 1 to [mem_avail].
         * Hint: check [proc] bit in each page of _mem_stat
         * to know whether this page has been used by a process.
         * For virtual memory space, check bp (break pointer).
         * */

        int free_space = 0;
        for (int i = 0; i < NUM_PAGES; i++) {
                if (_mem_stat[i].proc == 0)
                        free_space++;
                if (free_space == num_pages) {
                        mem_avail = 1;
                        break;
                }
        }

        // if break pointer is out of bound
        if (proc->bp + num_pages * PAGE_SIZE <= (1 << ADDRESS_SIZE)) {
                mem_avail = 1;
        }

        if (!mem_avail || free_space == 0) {
                pthread_mutex_unlock(&mem_lock);
                return ret_mem;
        }

        /* We could allocate new memory region to the process */
        ret_mem = proc->bp;
        proc->bp += num_pages * PAGE_SIZE;
        /* Update status of physical pages which will be allocated
         * to [proc] in _mem_stat. Tasks to do:
         * 	- Update [proc], [index], and [next] field
         * 	- Add entries to segment table page tables of [proc]
         * 	  to ensure accesses to allocated memory slot is
         * 	  valid. */

        int prev = 0;
        free_space = 0;

        addr_t page_anchor = ret_mem;
        addr_t first_lv = get_first_lv(page_anchor);
        addr_t second_lv = get_second_lv(page_anchor);
        addr_t new_first_lv = 0;

        const int max_page = 1 << SEGMENT_LEN;
        int* page_size = NULL;
        struct {
                addr_t v_index;
                addr_t p_index;
        }* page_table = NULL;

        struct {
                addr_t v_index;
                struct page_table_t* pages;
        }* segment = NULL;

#ifdef DEBUGGING
        printf("GIVE: %x - %x - %x: %d\n", ret_mem, first_lv, second_lv,
               num_pages);
#endif

        segment = get_segment(first_lv, proc);
        page_table = segment->pages->table;
        page_size = &(segment->pages->size);

        if (*page_size == max_page) {
                // well, when this happens,
                // means we need to go to the next segment
                // but we won't get to this
                // because of the first_lv above
        }

        for (int mem_page = 0; mem_page < NUM_PAGES; mem_page++) {
                if (_mem_stat[mem_page].proc != 0)
                        continue;

                // update _mem_stat
                _mem_stat[mem_page].proc = proc->pid;
                _mem_stat[mem_page].index = free_space++;
                if (free_space != 1)
                        _mem_stat[prev].next = mem_page;
                if (free_space == num_pages)
                        _mem_stat[mem_page].next = -1;
                prev = mem_page;

                // update in page_table
                page_table[*page_size].v_index = second_lv;
                page_table[*page_size].p_index = mem_page;
                (*page_size)++;

                if (free_space == num_pages)
                        break;

                // move to next page
                page_anchor += PAGE_SIZE;
                new_first_lv = get_first_lv(page_anchor);
                second_lv = get_second_lv(page_anchor);

                if (new_first_lv != first_lv) {
                        // get a new segment people
                        first_lv = new_first_lv;
                        segment = get_segment(first_lv, proc);
                        page_table = segment->pages->table;
                        page_size = &(segment->pages->size);
                }
        }
        pthread_mutex_unlock(&mem_lock);
        // printf("After Give %d pages to %d\n", num_pages, proc->pid);
        // dump();
        // printf("---\n");
        return ret_mem;
}

int free_mem(addr_t address, struct pcb_t* proc) {
        /*TODO: Release memory region allocated by [proc]. The first byte of
         * this region is indicated by [address]. Task to do:
         * 	- Set flag [proc] of physical page use by the memory block
         * 	  back to zero to indicate that it is free.
         * 	- Remove unused entries in segment table and page tables of
         * 	  the process [proc].
         * 	- Remember to use lock to protect the memory from other
         * 	  processes.  */
        addr_t physical_addr;
        if (translate(address, &physical_addr, proc)) {
                int i = physical_addr >> OFFSET_LEN;
                int pages_free = 0;
#ifdef DEBUGGING
                printf("FREE: %5x - I: %d\n", physical_addr, i);
#endif
                pthread_mutex_lock(&mem_lock);
                while (i != -1) {
                        _mem_stat[i].proc = 0;
                        i = _mem_stat[i].next;
                        pages_free++;
                }
                pthread_mutex_unlock(&mem_lock);
                // printf("After Free %d pages to %d from %d\n", pages_free, proc->pid, physical_addr >> OFFSET_LEN);
                // dump();
                // printf("---\n");
                return 0;
        } else {
                return 1;
        }
        return 0;
}

int read_mem(addr_t address, struct pcb_t* proc, BYTE* data) {
        addr_t physical_addr;
        if (translate(address, &physical_addr, proc)) {
                *data = _ram[physical_addr];
                return 0;
        } else {
                return 1;
        }
}

int write_mem(addr_t address, struct pcb_t* proc, BYTE data) {
        addr_t physical_addr;
        if (translate(address, &physical_addr, proc)) {
                _ram[physical_addr] = data;
                return 0;
        } else {
                return 1;
        }
}

void dump(void) {
        int i;
        for (i = 0; i < NUM_PAGES; i++) {
                if (_mem_stat[i].proc != 0) {
                        printf("%03d: ", i);
                        printf("%05x-%05x - PID: %02d (idx %03d, nxt: %03d)\n",
                               i << OFFSET_LEN, ((i + 1) << OFFSET_LEN) - 1,
                               _mem_stat[i].proc, _mem_stat[i].index,
                               _mem_stat[i].next);
                        int j;
                        for (j = i << OFFSET_LEN;
                             j < ((i + 1) << OFFSET_LEN) - 1; j++) {
                                if (_ram[j] != 0) {
                                        printf("\t%05x: %02x\n", j, _ram[j]);
                                }
                        }
                }
        }
}
