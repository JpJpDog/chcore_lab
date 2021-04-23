### 2 ###
> **process_create_root**先调用**ramdisk_read_file**读取第一个线程用户态的二进制文件,放到*binary*中.  
> 调用**process_create**创建一个进程返回到*root_process*.  
> > **process_create**首先分配进程对象并调用**process_init**初始化.  
> > 然后调用**obj_alloc**分配进程的*vmspace*对象,并用**vmspace_init**初始化。  
> > 调用**cap_alloc**将其连到进程的*slot_table*上.  
> 
> 调用**thread_create_main**创建第一个线程.传入的参数为刚才创建的进程,栈的开始地址和大小,线程类型,读入的二进制文件等.  
> > 拿到*process*中的*vmspace*  
> > 调用**obj_alloc** 和 **pmo_init** 分配并初始化一个pmo对象作为*stack_pmo*. 大小为传入参数.  
> > 调用**cap_alloc**把*stack_pmo*装在*process*的slot中.  
> > 调用**vmspace_map_range**映射stack_pmo到参数中的虚拟地址.  
> > 调用**obj_alloc**分配thread对象.  
> > 调用**load_binary**  
> > > 为binary文件中的每个段分配并初始化pmo,大小为segment的对齐后的大小，起始位置为segment对齐后的起始位置, 并用**cap_alloc**将pmo连到进程process上  
> > > 把segment内容写入到虚拟地址  
> > > 调用**vmspace_map_range**把pmo对象的地址映射到vmspace中  
> >  
> > 调用**prepare_env**初始化user stack,kernel stack.  
> > 调用**thread_init**初始化thread的所属进程,vmspace,上下文等  
> > 调用**cap_alloc**把thread放到process的slot_list中  
> 根据thread_cap找到刚刚的root_thread,并设为current_thread.  

### 7 ###
**START**结束后pc值为0.因为栈底的返回地址是0.