#include <stdio.h>
#include <stdlib.h>
#include "vm.h"
#include "API.h"
#include "list.h"

struct Node* fifo_list;
struct Node* lru_list;
struct Node* clock_list;
struct Node* clock_hand;
int page_index;

int fifo()
{
    if (fifo_list != NULL) {
        int PFN = fifo_list->data;
        fifo_list = list_remove_head(fifo_list);
        return PFN;
    }
    return -1;
}

int lru()
{
    if (lru_list != NULL) {
        int PFN = lru_list->data;
        lru_list = list_remove_head(lru_list);
        return PFN;
    }
    return -1;
}

void init_clock()
{
    for (int i = 0; i < MAX_PFN; i++) {
        clock_list = list_insert_tail(clock_list, i);
    }
    clock_hand = clock_list;
    page_index = 0;
}

int clock(int VPN)
{
    if (clock_list == NULL) {
        init_clock();
    }

    while (true) {
        if (clock_hand->second_chance == true) {
            clock_hand->second_chance = false;
            page_index = (page_index + 1) % MAX_PFN;
            clock_hand = clock_hand->next;
            if (clock_hand == NULL)
                clock_hand = clock_list;
        }
        else {
            clock_hand->data = VPN;
            clock_hand->second_chance = true;
            return page_index;
        }
    }

    return 0;
}

void fifo_insert(int PFN)
{
    if (fifo_list == NULL)
        fifo_list = list_new_node(PFN);
    else
         fifo_list = list_insert_tail(fifo_list, PFN);
}

void lru_insert(int PFN)
{
    if (lru_list == NULL)
        lru_list = list_new_node(PFN);
    else
        lru_list = list_insert_tail(lru_list, PFN);
}

void lru_hit(int PFN)
{
    lru_list = list_remove(lru_list, PFN);
    lru_insert(PFN);
}

void clock_hit(int VPN)
{
    if (clock_list == NULL)
        init_clock();
    struct Node *ret = search_list(clock_list, VPN);
    if (ret != NULL)
        ret->second_chance = true;
}
void do_swap_out(int PFN)
{
    IPTE ipte = read_IPTE(PFN);
    PTE pte = read_PTE(ipte.pid, ipte.VPN);
    pte.valid = false;
    write_PTE(ipte.pid, ipte.VPN, pte);
    if (pte.dirty == true)
        swap_out(ipte.pid, ipte.VPN, PFN);
}
/*========================================================================*/

int find_replacement(int VPN)
{
		int PFN;
		if(replacementPolicy == ZERO)  PFN = 0;
		else if(replacementPolicy == FIFO)  PFN = fifo();
		else if(replacementPolicy == LRU) PFN = lru();
		else if(replacementPolicy == CLOCK) PFN = clock(VPN);

		return PFN;
}

int pagefault_handler(int pid, int VPN, char type)
{
		int PFN;

		// find a free PFN.
		PFN = get_freeframe();


		// no free frame available. find a victim using page replacement. ;
		if(PFN < 0) {
				PFN = find_replacement(VPN);
				/* ---- */
                do_swap_out(PFN);
		}

		/* ---- */
        PTE pte = read_PTE(pid, VPN);
        //debug("\npid: %d VPN: %d PFN: %d", pid, VPN, PFN);
        swap_in(pid, VPN, PFN);
        IPTE ipte = read_IPTE(PFN);
        ipte.pid = pid;
        ipte.VPN = VPN;
        write_IPTE(PFN, ipte);
        //debug("\npid: %d VPN: %d PFN: %d\n", pid, VPN, PFN);
        //struct PTE* pte = (struct PTE*)malloc(sizeof(struct PTE));
        pte.PFN = PFN;
        pte.valid = true;
        if (type == 'W') pte.dirty = true;
        else pte.dirty = false;
        write_PTE(pid, VPN, pte);
        if (replacementPolicy == FIFO) fifo_insert(PFN);
        else if (replacementPolicy == LRU) lru_insert(PFN);
		return PFN;
}

int is_page_hit(int pid, int VPN, char type)
{
		/* Read page table entry for (pid, VPN) */
		PTE pte = read_PTE(pid, VPN);

		/* if PTE is valid, it is a page hit. Return physical frame number (PFN) */
		if(pte.valid) {
		/* Mark the page dirty, if it is a write request */
				if(type == 'W') {
						pte.dirty = true;
						write_PTE(pid, VPN, pte);
				}
		/* Need to take care of a page replacement data structure (LRU, CLOCK) for the page hit*/
		/* ---- */
                if (replacementPolicy == LRU)
                    lru_hit(pte.PFN);
                else if (replacementPolicy == CLOCK)
                    clock_hit(VPN);
				return pte.PFN;
		}

		/* PageFault, if the PTE is invalid. Return -1 */
		return -1;
}

int MMU(int pid, int VPN, char type, bool *hit)
{
		int PFN;

		// hit
		PFN = is_page_hit(pid, VPN, type);
		if(PFN >= 0) {
				stats.hitCount++;
				*hit = true;
				return PFN;
		}

		stats.missCount++;
		*hit = false;

		// miss -> pagefault
		PFN = pagefault_handler(pid, VPN, type);

		return PFN;
}
