#include <ipc/ipc.h>
#include <common/kmalloc.h>

u64 sys_ipc_recv() {
    struct thread *cur_thread = current_thread;

    cur_thread->recv_value = kzalloc(sizeof(struct recv_value));

    if (cur_thread == NULL ||
    cur_thread->thread_ctx == NULL ||
    cur_thread->thread_ctx->sc == NULL ||
    cur_thread->recv_value == NULL ||
    (cur_thread->thread_ctx->type != TYPE_ROOT && 
    cur_thread->thread_ctx->type != TYPE_USER)) {
        return -EINVAL;
    }
    //wait for send
    cur_thread->thread_ctx->sc->budget = 0;
    cur_thread->thread_ctx->state = TS_WAITING;
    sched();
    eret_to_thread(switch_context());
}

u64 sys_ipc_recv_result() {
    u64 ret = current_thread->recv_value->value;
    kfree(current_thread->recv_value);
    return ret;
}