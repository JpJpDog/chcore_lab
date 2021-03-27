/*
 * Copyright (c) 2020 Institute of Parallel And Distributed Systems (IPADS), Shanghai Jiao Tong University (SJTU)
 * OS-Lab-2020 (i.e., ChCore) is licensed under the Mulan PSL v1.
 * You can use this software according to the terms and conditions of the Mulan PSL v1.
 * You may obtain a copy of Mulan PSL v1 at:
 *   http://license.coscl.org.cn/MulanPSL
 *   THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 *   IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
 *   PURPOSE.
 *   See the Mulan PSL v1 for more details.
 */

#ifdef CHCORE
#include <common/util.h>
#include <common/kmalloc.h>
#endif
#include <common/vars.h>
#include <common/macro.h>
#include <common/types.h>
#include <common/printk.h>
#include <common/mm.h>
#include <common/mmu.h>

#include <common/errno.h>

#include "page_table.h"

/* Page_table.c: Use simple impl for debugging now. */

extern void set_ttbr0_el1(paddr_t);
extern void flush_tlb(void);

void set_page_table(paddr_t pgtbl)
{
	set_ttbr0_el1(pgtbl);
}

/*
 * the 3rd arg means the kind of PTE.
 */
static int set_pte_flags(pte_t * entry, vmr_prop_t flags)
{
	int lv;
	lv = (flags & HUGE_PAGE) ? ((flags & VERY_HUGE) ? 1 : 2) : 3;

	if (lv == 3) {
		if (flags & VMR_WRITE)
			entry->l3_page.AP = AARCH64_PTE_AP_HIGH_RW_EL0_RW;
		else
			entry->l3_page.AP = AARCH64_PTE_AP_HIGH_RO_EL0_RO;
		if (flags & VMR_EXEC)
			entry->l3_page.UXN = AARCH64_PTE_UX;
		else
			entry->l3_page.UXN = AARCH64_PTE_UXN;
		if (!(flags & KERNEL_PT))
			entry->l3_page.PXN = AARCH64_PTE_PXN;
		entry->l3_page.AF = AARCH64_PTE_AF_ACCESSED;
		entry->l3_page.SH = INNER_SHAREABLE;
		entry->l3_page.attr_index = NORMAL_MEMORY;
		entry->l3_page.is_page = 1;
		entry->l3_page.is_valid = 1;
	} else if (lv ==2) {
		if (flags & VMR_WRITE)
			entry->l2_block.AP = AARCH64_PTE_AP_HIGH_RW_EL0_RW;
		else
			entry->l2_block.AP = AARCH64_PTE_AP_HIGH_RO_EL0_RO;
		if (flags & VMR_EXEC)
			entry->l2_block.UXN = AARCH64_PTE_UX;
		else
			entry->l2_block.UXN = AARCH64_PTE_UXN;
		if (!(flags & KERNEL_PT))
			entry->l2_block.PXN = AARCH64_PTE_PXN;
		entry->l2_block.AF = AARCH64_PTE_AF_ACCESSED;
		entry->l2_block.SH = INNER_SHAREABLE;
		entry->l2_block.attr_index = NORMAL_MEMORY;
		entry->l2_block.is_table = 0;
		entry->l2_block.is_valid = 1;
	} else {
		if (flags & VMR_WRITE)
			entry->l1_block.AP = AARCH64_PTE_AP_HIGH_RW_EL0_RW;
		else
			entry->l1_block.AP = AARCH64_PTE_AP_HIGH_RO_EL0_RO;
		if (flags & VMR_EXEC)
			entry->l1_block.UXN = AARCH64_PTE_UX;
		else
			entry->l1_block.UXN = AARCH64_PTE_UXN;
		if (!(flags & KERNEL_PT))
			entry->l1_block.PXN = AARCH64_PTE_PXN;
		entry->l1_block.AF = AARCH64_PTE_AF_ACCESSED;
		entry->l1_block.SH = INNER_SHAREABLE;
		entry->l1_block.attr_index = NORMAL_MEMORY;
		entry->l1_block.is_table = 0;
		entry->l1_block.is_valid = 1;
	}

	return 0;
}

#define GET_PADDR_IN_PTE(entry) \
	(((u64)entry->table.next_table_addr) << PAGE_SHIFT)
#define GET_NEXT_PTP(entry) phys_to_virt(GET_PADDR_IN_PTE(entry))

#define NORMAL_PTP (0)
#define BLOCK_PTP  (1)

/*
 * Find next page table page for the "va".
 *
 * cur_ptp: current page table page
 * level:   current ptp level
 *
 * next_ptp: returns "next_ptp"
 * pte     : returns "pte" (points to next_ptp) in "cur_ptp"
 *
 * alloc: if true, allocate a ptp when missing
 *
 */
static int get_next_ptp(ptp_t * cur_ptp, u32 level, vaddr_t va,
			ptp_t ** next_ptp, pte_t ** pte, bool alloc)
{
	u32 index = 0;
	pte_t *entry;

	if (cur_ptp == NULL)
		return -ENOMAPPING;

	switch (level) {
	case 0:
		index = GET_L0_INDEX(va);
		break;
	case 1:
		index = GET_L1_INDEX(va);
		break;
	case 2:
		index = GET_L2_INDEX(va);
		break;
	case 3:
		index = GET_L3_INDEX(va);
		break;
	default:
		BUG_ON(1);
	}

	entry = &(cur_ptp->ent[index]);
	if (IS_PTE_INVALID(entry->pte)) {
		if (alloc == false) {
			return -ENOMAPPING;
		} else {
			/* alloc a new page table page */
			ptp_t *new_ptp;
			paddr_t new_ptp_paddr;
			pte_t new_pte_val;

			/* alloc a single physical page as a new page table page */
			new_ptp = get_pages(0);
			BUG_ON(new_ptp == NULL);
			memset((void *)new_ptp, 0, PAGE_SIZE);
			new_ptp_paddr = virt_to_phys((vaddr_t) new_ptp);

			new_pte_val.pte = 0;
			new_pte_val.table.is_valid = 1;
			new_pte_val.table.is_table = 1;
			new_pte_val.table.next_table_addr
			    = new_ptp_paddr >> PAGE_SHIFT;

			/* same effect as: cur_ptp->ent[index] = new_pte_val; */
			entry->pte = new_pte_val.pte;
		}
	}
	*next_ptp = (ptp_t *) GET_NEXT_PTP(entry);
	*pte = entry;
	if (IS_PTE_TABLE(entry->pte))
		return NORMAL_PTP;
	else
		return BLOCK_PTP;
}

/*
 * Translate a va to pa, and get its pte for the flags
 */
/*
 * query_in_pgtbl: translate virtual address to physical 
 * address and return the corresponding page table entry
 * 
 * pgtbl @ ptr for the first level page table(pgd) virtual address
 * va @ query virtual address
 * pa @ return physical address
 * entry @ return page table entry
 * 
 * Hint: check the return value of get_next_ptp, if ret == BLOCK_PTP
 * return the pa and block entry immediately
 */

static int find_in_pgtbl(ptp_t *cur_ptp, vaddr_t va, bool alloc,
		int *level, paddr_t *pa, pte_t **pte, int *pte_i)
{
	ptp_t *next_ptp;
	int ret, cur_lv, lv;
	lv = (*level) % 4;
	if (lv == 0) lv = 3;
	for (cur_lv = 0; cur_lv < lv; cur_lv++) {
		ret = get_next_ptp(cur_ptp, cur_lv, va, &cur_ptp, pte, alloc);
		if (ret < 0) return ret;
		if (ret == BLOCK_PTP) {
			if (*level != 0) return -EPERM;
			else goto GOTTEN;
		}
	}
	ret = get_next_ptp(cur_ptp, cur_lv, va, &next_ptp, pte, alloc);
	if (ret < 0) return ret;
GOTTEN:
	*level = cur_lv;
	*pte_i = *pte - cur_ptp->ent;
	switch (cur_lv) {
		case 1: *pa = ((*pte)->l1_block.pfn << L1_INDEX_SHIFT) + GET_VA_OFFSET_L1(va); break;
		case 2: *pa = ((*pte)->l2_block.pfn << L2_INDEX_SHIFT) + GET_VA_OFFSET_L2(va); break;
		case 3: *pa = ((*pte)->l3_page.pfn << L3_INDEX_SHIFT) + GET_VA_OFFSET_L3(va); break;
		default: BUG_ON(1);
	}
	return 0;

}

int query_in_pgtbl(vaddr_t * pgtbl, vaddr_t va, paddr_t * pa, pte_t ** entry)
{
	int level, ret, pte_i;

	level = 0;
	ret = find_in_pgtbl((ptp_t *)pgtbl, va, false, &level, pa, entry, &pte_i);
	if (ret < 0) return ret;
	return 0;
}

/*
 * map_range_in_pgtbl: map the virtual address [va:va+size] to 
 * physical address[pa:pa+size] in given pgtbl
 *
 * pgtbl @ ptr for the first level page table(pgd) virtual address
 * va @ start virtual address
 * pa @ start physical address
 * len @ mapping size
 * flags @ corresponding attribution bit
 *
 * Hint: In this function you should first invoke the get_next_ptp()
 * to get the each level page table entries. Read type pte_t carefully
 * and it is convenient for you to call set_pte_flags to set the page
 * permission bit. Don't forget to call flush_tlb at the end of this function 
 */
int map_range_in_pgtbl(vaddr_t * pgtbl, vaddr_t va, paddr_t pa,
		       size_t len, vmr_prop_t flags)
{
	pte_t *pte, *cur_pte;
	int ret, level, pte_i;
	paddr_t pa1;
	size_t cur_len;
	u64 mask, pg_size;

	if (!(flags & HUGE_PAGE)) {
		level = 3;
		mask = ~L3_PAGE_MASK;
		pg_size = PAGE_SIZE;
	} else if (!(flags & VERY_HUGE)) {
		level = 2;
		mask = ~L2_BLOCK_MASK;
		pg_size = PAGE_SIZE * PTP_ENTRIES;
	} else {
		level = 1;
		mask = ~L1_BLOCK_MASK;
		pg_size = PAGE_SIZE * PTP_ENTRIES * PTP_ENTRIES;
	}
	pa &= mask;
	len &= mask;
	cur_len = 0;
	while (cur_len != len) {
		ret = find_in_pgtbl((ptp_t *)pgtbl, va + cur_len, true, &level, &pa1, &pte, &pte_i);
		if (ret < 0) return ret;
		for (cur_pte = pte; (cur_pte - pte < PTP_ENTRIES - pte_i) && cur_len != len; cur_pte++, cur_len += pg_size) {
			set_pte_flags(cur_pte, flags);
			switch (level) {
				case 1: cur_pte->l1_block.pfn = (pa + cur_len) >> L1_INDEX_SHIFT; break;
				case 2: cur_pte->l2_block.pfn = (pa + cur_len) >> L2_INDEX_SHIFT; break;
				case 3: cur_pte->l3_page.pfn = (pa + cur_len) >> L3_INDEX_SHIFT; break;
				default: BUG_ON(1);
			}
		}
	}
	flush_tlb();
	return 0;
}


/*
 * unmap_range_in_pgtble: unmap the virtual address [va:va+len]
 * 
 * pgtbl @ ptr for the first level page table(pgd) virtual address
 * va @ start virtual address
 * len @ unmapping size
 * 
 * Hint: invoke get_next_ptp to get each level page table, don't 
 * forget the corner case that the virtual address is not mapped.
 * call flush_tlb() at the end of function
 * 
 */
int unmap_range_in_pgtbl(vaddr_t * pgtbl, vaddr_t va, size_t len)
{
	int level, pte_i, cur_len, ret;
	paddr_t pa;
	pte_t *pte, *cur_pte;
	u64 mask, pg_size;

	level = 0;
	cur_len = 0;
	ret = find_in_pgtbl((ptp_t *)pgtbl, va, false, &level, &pa, &pte, &pte_i);
	if (ret < 0) return ret;
	switch (level) {
		case 1: mask = ~L1_BLOCK_MASK; pg_size = PAGE_SIZE * PTP_ENTRIES * PTP_ENTRIES; break;
		case 2: mask = ~L2_BLOCK_MASK; pg_size = PAGE_SIZE * PTP_ENTRIES; break;
		case 3: mask = ~L3_PAGE_MASK; pg_size = PAGE_SIZE; break;
		default: BUG_ON(1);
	}
	va &= mask;
	len &= mask;
	while (true) {
		for (cur_pte = pte; (cur_pte - pte < PTP_ENTRIES - pte_i) && cur_len != len; cur_pte++, cur_len += pg_size) {
			pte->pte &= ~AARCH64_PTE_INVALID_MASK;
		}
		if (cur_len == len) break;
		ret = find_in_pgtbl((ptp_t *)pgtbl, va + cur_len, false, &level, &pa, &pte, &pte_i);
	}
	flush_tlb();
	return 0;
}

// TODO: add hugepage support for user space.
