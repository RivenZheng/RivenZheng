/**
 * Copyright (c) Riven Zheng (zhengheiot@gmail.com).
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 **/
#include "at_rtos.h"
#include "kernel.h"
#include "timer.h"
#include "compiler.h"
#include "clock_tick.h"
#include "ktype.h"
#include "postcode.h"
#include "trace.h"
#include "init.h"

/**
 * Local unique postcode.
 */
#define PC_EOR PC_IER(PC_OS_CMPT_KERNEL_2)

/**
 * Data structure for location timer
 */
typedef struct {
    struct schedule_task *pTask;

    b_t run;

    u32_t pendsv_ms;

    list_t sch_pend_list;

    list_t sch_entry_list;

    list_t sch_exit_list;

    list_t sch_wait_list;
} _kernel_resource_t;

/**
 * Local kernel resource
 */
static _kernel_resource_t g_kernel_rsc = {
    .pTask = NULL,
    .run = false,
    .pendsv_ms = 0u,
};

/**
 * @brief Compare the priority between the current and extract thread.
 *
 * @param pCurNode The pointer of the current thread node.
 * @param pExtractNode The pointer of the extract thread node.
 *
 * @return The false indicates it's a right position and it can kill the loop calling.
 */
static b_t _schedule_priority_node_order_compare_condition(list_node_t *pCurNode, list_node_t *pExtractNode)
{
    struct schedule_task *pCurTask = (struct schedule_task *)pCurNode;
    struct schedule_task *pExtractTask = (struct schedule_task *)pExtractNode;

    if ((!pCurTask) || (!pExtractTask)) {
        /* no available thread */
        return false;
    }

    if (pCurTask->prior <= pExtractTask->prior) {
        /* Find a right position and doesn't has to do schedule */
        return false;
    }

    return true;
}

static void _schedule_transfer_toEntryList(linker_t *pLinker)
{
    ENTER_CRITICAL_SECTION();

    list_t *pToList = (list_t *)&g_kernel_rsc.sch_entry_list;
    linker_list_transaction_common(pLinker, pToList, LIST_TAIL);

    EXIT_CRITICAL_SECTION();
}

static void _schedule_transfer_toNullList(linker_t *pLinker)
{
    ENTER_CRITICAL_SECTION();

    linker_list_transaction_common(pLinker, NULL, LIST_TAIL);

    EXIT_CRITICAL_SECTION();
}

/**
 * @brief Push one thread context into target list.
 *
 * @param pCurHead The pointer of the thread linker head.
 */
static void _schedule_transfer_toTargetList(linker_t *pLinker, list_t *pToList)
{
    ENTER_CRITICAL_SECTION();

    if (pToList) {
        linker_list_transaction_specific(pLinker, pToList, _schedule_priority_node_order_compare_condition);
    }

    EXIT_CRITICAL_SECTION();
}

static void _schedule_transfer_toExitList(linker_t *pLinker)
{
    ENTER_CRITICAL_SECTION();

    list_t *pToList = (list_t *)&g_kernel_rsc.sch_exit_list;
    linker_list_transaction_specific(pLinker, pToList, _schedule_priority_node_order_compare_condition);

    EXIT_CRITICAL_SECTION();
}

static void _schedule_transfer_toPendList(linker_t *pLinker)
{
    ENTER_CRITICAL_SECTION();

    list_t *pToList = (list_t *)&g_kernel_rsc.sch_pend_list;
    linker_list_transaction_specific(pLinker, pToList, _schedule_priority_node_order_compare_condition);

    EXIT_CRITICAL_SECTION();
}

static struct schedule_task *_schedule_nextTaskGet(void)
{
    return (struct schedule_task *)g_kernel_rsc.sch_pend_list.pHead;
}

static void _schedule_time_analyze(struct schedule_task *pFrom, struct schedule_task *pTo, u32_t ms)
{
    pFrom->exec.analyze.last_run_ms = ms - pFrom->exec.analyze.last_active_ms;
    pFrom->exec.analyze.total_run_ms += pFrom->exec.analyze.last_run_ms;
    pTo->exec.analyze.last_active_ms = ms;
}

static void _schedule_exit(u32_t ms)
{
    b_t need = false;
    struct schedule_task *pCurTask = NULL;
    list_iterator_t it = ITERATION_NULL;
    list_t *pList = (list_t *)&g_kernel_rsc.sch_exit_list;

    list_iterator_init(&it, pList);
    while (list_iterator_next_condition(&it, (void *)&pCurTask)) {
        struct call_exit *pExit = &pCurTask->exec.exit;

        if (pExit->timeout_ms) {
            timeout_set(&pCurTask->expire, pExit->timeout_ms, false);
            if (pExit->timeout_ms != OS_TIME_FOREVER_VAL) {
                need = true;
            }
        }
        if (pExit->pToList) {
            _schedule_transfer_toTargetList((linker_t *)&pCurTask->linker, (list_t *)pExit->pToList);
        } else {
            thread_context_t *pDelThread = (thread_context_t *)CONTAINEROF(pCurTask, thread_context_t, task);

            _schedule_transfer_toNullList((linker_t *)&pCurTask->linker);
            os_memset((char_t *)pDelThread->pStackAddr, STACT_UNUSED_DATA, pDelThread->stackSize);
            os_memset((char_t *)pDelThread, 0x0u, sizeof(thread_context_t));
        }

        os_memset(pExit, 0x0, sizeof(struct call_exit));
        pCurTask->exec.entry.result = PC_EOR;
    }

    if (need) {
        timer_schedule();
    }
}

static void _schedule_entry(u32_t ms)
{
    struct schedule_task *pCurTask = NULL;
    list_iterator_t it = ITERATION_NULL;
    list_t *pList = (list_t *)&g_kernel_rsc.sch_entry_list;

    list_iterator_init(&it, pList);
    while (list_iterator_next_condition(&it, (void *)&pCurTask)) {
        struct call_entry *pEntry = &pCurTask->exec.entry;
        if (pEntry->fun) {
            pEntry->fun(pCurTask);
            pEntry->fun = NULL;
        }
        pCurTask->pPendCtx = NULL;
        pCurTask->exec.analyze.last_pend_ms = ms;

        _schedule_transfer_toPendList((linker_t *)&pCurTask->linker);
    }
}

i32p_t schedule_exit_trigger(struct schedule_task *pTask, void *pHoldCtx, void *pHoldData, list_t *pToList, u32_t timeout_ms,
                             b_t immediately)
{
    pTask->pPendCtx = pHoldCtx;
    pTask->pPendData = pHoldData;

    if (immediately) {
        timeout_set(&pTask->expire, timeout_ms, true);
        _schedule_transfer_toTargetList((linker_t *)&pTask->linker, pToList);
    } else {
        pTask->exec.exit.pToList = pToList;
        pTask->exec.exit.timeout_ms = timeout_ms;
        _schedule_transfer_toExitList((linker_t *)&pTask->linker);
    }
    return kernel_thread_schedule_request();
}

i32p_t schedule_entry_trigger(struct schedule_task *pTask, pTask_callbackFunc_t callback, u32_t result)
{
    pTask->exec.entry.result = result;
    pTask->exec.entry.fun = callback;
    _schedule_transfer_toEntryList((linker_t *)&pTask->linker);
    return kernel_thread_schedule_request();
}

void schedule_callback_fromTimeOut(void *pNode)
{
    struct schedule_task *pCurTask = (struct schedule_task *)CONTAINEROF(pNode, struct schedule_task, expire);
    schedule_entry_trigger(pCurTask, NULL, PC_OS_WAIT_TIMEOUT);
}

b_t schedule_hasTwoPendingItem(void)
{
    if (!g_kernel_rsc.sch_pend_list.pHead) {
        return false;
    }

    if (!g_kernel_rsc.sch_pend_list.pHead->pNext) {
        return true;
    }

    return false;
}

void schedule_setPend(struct schedule_task *pTask)
{
    ENTER_CRITICAL_SECTION();

    _schedule_transfer_toPendList((linker_t *)&pTask->linker);

    EXIT_CRITICAL_SECTION();
}

list_t *schedule_waitList(void)
{
    return (list_t *)&g_kernel_rsc.sch_wait_list;
}

b_t _schedule_can_preempt(struct schedule_task *pCurrent)
{
    struct schedule_task *pTmpTask = NULL;
    list_iterator_t it = ITERATION_NULL;
    list_t *pList = (list_t *)&g_kernel_rsc.sch_pend_list;

    list_iterator_init(&it, pList);
    while (list_iterator_next_condition(&it, (void *)&pTmpTask)) {
        if (pTmpTask->prior >= 0) {
            break;
        }

        if (pTmpTask == pCurrent) {
            return false;
        }

        if (pTmpTask->prior == OS_PRIOTITY_HIGHEST_LEVEL) {
            break;
        }
    }

    return true;
}

/**
 * @brief kernel schedule in PendSV interrupt content.
 *
 * @param ppCurThreadPsp The current thread psp address.
 * @param ppNextThreadPSP The next thread psp address.
 */
void kernel_scheduler_inPendSV_c(u32_t **ppCurPsp, u32_t **ppNextPSP)
{
    u32_t ms = timer_total_system_ms_get();

    _schedule_exit(ms);
    _schedule_entry(ms);

    struct schedule_task *pCurrent = g_kernel_rsc.pTask;
    struct schedule_task *pNext = _schedule_nextTaskGet();

    if (_schedule_can_preempt(pCurrent)) {
        *ppCurPsp = (u32_t *)&pCurrent->psp;
        *ppNextPSP = (u32_t *)&pNext->psp;

        _schedule_time_analyze(pCurrent, pNext, ms);
        g_kernel_rsc.pTask = pNext;
        g_kernel_rsc.pendsv_ms = ms;
    } else {
        *ppCurPsp = (u32_t *)&pCurrent->psp;
        *ppNextPSP = (u32_t *)&pCurrent->psp;
    }
}

/**
 * @brief To set a PendSV.
 */
static void _kernel_setPendSV(void)
{
    port_setPendSV();
}

/**
 * @brief Check if kernel is in privilege mode.
 *
 * @return The true indicates the kernel is in privilege mode.
 */
static b_t _kernel_isInPrivilegeMode(void)
{
    return port_isInInterruptContent();
}

/**
 * @brief It's sub-routine running at privilege mode.
 *
 * @param pArgs The function argument packages.
 *
 * @return The result of privilege routine.
 */
static i32p_t _kernel_start_privilege_routine(arguments_t *pArgs)
{
    UNUSED_MSG(pArgs);

    ENTER_CRITICAL_SECTION();

    init_static_thread_list();
    port_interrupt_init();
    clock_time_init(timeout_handler);

    g_kernel_rsc.pTask = _schedule_nextTaskGet();
    g_kernel_rsc.run = true;

    EXIT_CRITICAL_SECTION();

    port_run_theFirstThread(g_kernel_rsc.pTask->psp);

    // Unreachable.
    return PC_EOR;
}

/**
 * @brief To check if the kernel message arrived.
 */
static i32p_t _kernel_message_arrived(void)
{
    return kthread_message_arrived();
}

/**
 * @brief Initialize a thread stack frame.
 *
 * @param pEntryFunction The entry function pointer.
 * @param pAddress The stack address.
 * @param size The stack size.
 *
 * @return The PSP stack address.
 */
u32_t kernel_stack_frame_init(void (*pEntryFunction)(void), u32_t *pAddress, u32_t size)
{
    return (u32_t)port_stack_frame_init(pEntryFunction, pAddress, size);
}

/**
 * @brief Get the current running thread context.
 *
 * @return The context pointer of current running thread.
 */
thread_context_t *kernel_thread_runContextGet(void)
{
    return (thread_context_t *)CONTAINEROF(g_kernel_rsc.pTask, thread_context_t, task);
}

/**
 * @brief Read and clean the current running thread schedule entry result.
 *
 * @param pSchedule The pointer of thread action schedule.
 *
 * @return The result of the thread schedule.
 */
i32p_t kernel_schedule_result_take(void)
{
    thread_context_t *pCurThread = kernel_thread_runContextGet();

    i32p_t ret = (i32p_t)pCurThread->task.exec.entry.result;
    pCurThread->task.exec.entry.result = PC_EOR;

    return ret;
}

/**
 * @brief Check if kernel is in thread mode.
 *
 * @return The true indicates the kernel is in thread mode.
 */
b_t kernel_isInThreadMode(void)
{
    return port_isInThreadMode();
}

/**
 * @brief Request the kernel do thread schedule.
 *
 * @return The result of the request operation.
 */
i32p_t kernel_thread_schedule_request(void)
{
    if (!_kernel_isInPrivilegeMode()) {
        return PC_EOR;
    }

    _kernel_setPendSV();
    return 0;
}

/**
 * @brief To issue a kernel message notification.
 */
void kernel_message_notification(void)
{
    kthread_message_notification();
}

/**
 * @brief The kernel thread only serve for RTOS with highest priority.
 */
void kernel_schedule_thread(void)
{
    while (1) {
        PC_IF(_kernel_message_arrived(), PC_PASS)
        {
            timer_reamining_elapsed_handler();

            extern void _impl_publish_pending_handler(void);
            _impl_publish_pending_handler();
        }
    }
}

/**
 * @brief The idle thread entry function.
 */
void kernel_idle_thread(void)
{
    while (1) {
        kthread_message_idle_loop_fn();
    }
}

/**
 * @brief kernel call privilege function in SVC interrupt content.
 *
 * @param svc_args The function arguments.
 */
void kernel_privilege_call_inSVC_c(u32_t *svc_args)
{
    /*
     * Stack contains:
     * r0, r1, r2, r3, r12, r14, the return address and xPSR
     * First argument (r0) is svc_args[0]
     * Put the result into R0
     */
    u8_t svc_number = ((u8_t *)svc_args[6])[-2];

    if (svc_number == SVC_KERNEL_INVOKE_NUMBER) {
        pPrivilege_callFunc_t pCall = (pPrivilege_callFunc_t)svc_args[0];

        svc_args[0] = (u32_t)pCall((arguments_t *)svc_args[1]);
    }
}

/**
 * @brief kernel privilege function invoke interface.
 *
 * @param pCallFun The privilege function entry pointer.
 * @param pArgs The arguments list pool.
 *
 * @return The result of the privilege function.
 */
i32p_t kernel_privilege_invoke(const void *pCallFun, arguments_t *pArgs)
{
    if (!pCallFun) {
        return PC_EOR;
    }

    if (!_kernel_isInPrivilegeMode()) {
        return (i32p_t)kernel_svc_call((u32_t)pCallFun, (u32_t)pArgs, 0u, 0u);
    }

    ENTER_CRITICAL_SECTION();
    pPrivilege_callFunc_t pCall = (pPrivilege_callFunc_t)pCallFun;
    i32p_t ret = (i32p_t)pCall((arguments_t *)pArgs);
    EXIT_CRITICAL_SECTION();

    return ret;
}

/**
 * @brief To check if the kernel OS is running.
 *
 * return The true indicates the kernel OS is running.
 */
b_t _impl_kernel_rtos_isRun(void)
{
    return (b_t)((g_kernel_rsc.run) ? (true) : (false));
}

/**
 * @brief The kernel OS start to run.
 */
i32p_t _impl_kernel_at_rtos_run(void)
{
    if (_impl_kernel_rtos_isRun()) {
        return 0;
    }

    return kernel_privilege_invoke((const void *)_kernel_start_privilege_routine, NULL);
}
