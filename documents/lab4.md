**练习1** 在start.S中，通过mpidr_el1获得cpuid，如果是0则是primary CPU，跳转到primary处，否则是secondary CPU。Secondary CPU首先通过spin lock，等待clear_bss_flag被设为1。然后根据cpu id设置sp。然后spin lock，等待对应自己的secondary_boot_flag被设为1。之后进入secondary_init_c，完成boot。  
**练习3** 应该是正确的。因为每个CPU的boot栈和内核栈是根据cpu id不同的。激活CPU时，调用的函数为secondary_init_c，secondary_cpu_boot，他们访问的数据不会重叠，所以不会有data race。但是enable_smp_core应该要等所有CPU都激活了才退出，所以顺序激活比较简便。  
**练习6** 因为调用unlock_kernel后，直接调用exception_exit退出内核态，不需要用到前面的寄存器了，所以不用保存。
**练习8** 因为虽然idle thread在内核态执行，但是执行前会放锁。所以对idle thread错误处理的时候要先拿大内核锁。

**附加练习**
测试的用户程序直接在 *ipc_reg.c* 和 *ipc_reg_server.c* 后面加了一段，分别作为 *ipc_send* 和 *ipc_recv*。ipc_send等待info_page中的ready为0，说明ipc_recv已经准备好了（可能有data_race）。然后打印ipc_send传来的64位数。  
recv调用时，会在struct thread中分配一个recv_value（直接用的kmalloc），用于放传送的值。然后将线程设为waiting状态，然后把budget设为0，因为不是ready状态所以该线程不会被调度到。
send时，会通过process_cap和thread_cap，把recv的线程找到，把值写到recv_data中，最后把state设置为ready。
recv线程被调度到时会到上一次系统调用的下一行，然后再用一个专门读recv_data的系统调用把值读出来。