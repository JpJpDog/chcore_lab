#include <ipc/ipc.h>

u64 sys_ipc_send(u32 process_cap, u32 target_thread_cap, u64 value){
    struct thread *target_thread = NULL;
    struct recv_value *recv_value = NULL;

    target_thread = obj_get(current_process, target_thread_cap, TYPE_THREAD);
    if (!target_thread) {
        return -ECAPBILITY;
    }
    if (target_thread->thread_ctx == NULL || target_thread->thread_ctx->state != TS_WAITING) {
        return -EINVAL;
    }
    recv_value = target_thread->recv_value;
    recv_value->value = value;
    target_thread->thread_ctx->state = TS_READY;
    return 0;
}
