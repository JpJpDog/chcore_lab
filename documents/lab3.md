<!-- 
1. call **process_create_root** with param *TEXT* to create root process
   1. call **process_create**
      1. malloc struct *object* and *process*
      2. call **process_init** with params *process* and *size* (BASE_OBJECT_NUM, 32)
         1. call **slot_table_init** to init *slot_table* in *process* with params *size*
            1. set *slot_table.slot_size* as 32
            2. set *slot_table.slots* by malloc 32 elements
            3. set *slot_table.slots_bmp* by malloc 1 element
            4. set *slot_table.full_slots_bmp* by malloc 1 element -->




