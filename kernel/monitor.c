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

// Simple command-line kernel monitor useful for
// controlling the kernel and exploring the system interactively.

#include <common/printk.h>
#include <common/types.h>

static inline __attribute__ ((always_inline))
u64 *read_fp()
{
	u64 *fp;
	__asm __volatile("mov %0, x29":"=r"(fp));
	return fp;
}

__attribute__ ((optimize("O1")))
int stack_backtrace()
{
	printk("Stack backtrace:\n");

	// Your code here.

	u64 *this_fp = read_fp(), *prev_fp, *lr;
	u64 args[5];

	while ((prev_fp = (u64*) *this_fp) != 0) {
		lr = (u64 *)*(prev_fp + 1);
		int args_i = 0;
		for (u64* p= this_fp + 2; p != prev_fp && args_i <5; p++) {
			args[args_i++] = *p;
		}
		printk("LR %lx FP %lx Args ", lr, prev_fp);
		for (int i = 0; i < args_i; i++) {
			printk("%lx ", args[i]);
		}
		printk("\n");
		this_fp = prev_fp;
	}

	return 0;
}
