**练习1** 在start.S中，通过mpidr_el1获得cpuid，如果是0则是primary CPU，跳转到primary处，否则是secondary CPU。Secondary CPU首先通过spin lock，等待clear_bss_flag被设为1。然后根据cpu id设置sp。然后spin lock，等待对应自己的secondary_boot_flag被设为1。之后进入secondary_init_c，完成boot。  
**练习3** 应该是正确的。因为每个CPU的boot栈和内核栈是根据cpu id不同的。激活CPU时，调用的函数为secondary_init_c，secondary_cpu_boot，他们访问的数据不会重叠，所以不会有data race。但是enable_smp_core应该要等所有CPU都激活了才退出，所以顺序激活比较简便。  
