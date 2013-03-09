#include "checkedthreads.h"
#include <cstdlib>
#include <cstring>

void ctx_for_ind_func(int ind, void* context) {
    ctx_ind_func* f = (ctx_ind_func*)context;
    (*f)(ind);
}

void ctx_for(int n, const ctx_ind_func& f, ct_canceller* c) {
    ct_for(n, ctx_for_ind_func, (void*)&f, c);
}

void ctx_invoke_ind_func(int ind, void* context) {
    ctx_task_func** tasks = (ctx_task_func**)context;
    (*tasks[ind])();
}

void ctx_invoke_(ctx_task_node_* head, ct_canceller* c) {
    const int max_local_tasks = 128;
    ctx_task_func* local_tasks[max_local_tasks]; 
    ctx_task_func** tasks = local_tasks;
    int max_tasks = max_local_tasks;
    
    int n = 0;
    while(head) {
        if(n >= max_tasks) { //Wow. OK...
            int new_size = sizeof(ctx_task_func*)*max_tasks*2;
            if(tasks != local_tasks) {
                tasks = (ctx_task_func**)realloc(tasks, new_size);
            }
            else {
                ctx_task_func** new_tasks = (ctx_task_func**)malloc(new_size);
                memcpy(new_tasks, tasks, sizeof(ctx_task_func*)*max_tasks);
                tasks = new_tasks;
            }
            max_tasks *= 2;
        }
        tasks[n] = head->func;
        head = head->next;
        ++n;
    }

    ct_for(n, ctx_invoke_ind_func, tasks, c);

    if(tasks != local_tasks) {
        free(tasks);
    }
}
