/**
 * Copyright (c) Riven Zheng (zhengheiot@gmail.com).
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 **/
#include "kernel.h"
#include "timer.h"
#include "postcode.h"
#include "trace.h"
#include "init.h"

/**
 * Local unique postcode.
 */
#define PC_EOR PC_IER(PC_OS_CMPT_MUTEX_5)

/**
 * @brief Check if the mutex unique id if is's invalid.
 *
 * @param id The provided unique id.
 *
 * @return The true is invalid, otherwise is valid.
 */
static b_t _mutex_context_isInvalid(mutex_context_t *pCurMutex)
{
    u32_t start, end;
    INIT_SECTION_FIRST(INIT_SECTION_OS_MUTEX_LIST, start);
    INIT_SECTION_LAST(INIT_SECTION_OS_MUTEX_LIST, end);

    return ((u32_t)pCurMutex < start || (u32_t)pCurMutex >= end) ? true : false;
}

/**
 * @brief Check if the mutex object if is's initialized.
 *
 * @param id The provided unique id.
 *
 * @return The true is initialized, otherwise is uninitialized.
 */
static b_t _mutex_context_isInit(mutex_context_t *pCurMutex)
{
    return ((pCurMutex) ? (((pCurMutex->head.cs) ? (true) : (false))) : false);
}

/**
 * @brief It's sub-routine running at privilege mode.
 *
 * @param pArgs The function argument packages.
 *
 * @return The result of privilege routine.
 */
static u32_t _mutex_init_privilege_routine(arguments_t *pArgs)
{
    ENTER_CRITICAL_SECTION();

    const char_t *pName = (const char_t *)(pArgs[0].pch_val);

    INIT_SECTION_FOREACH(INIT_SECTION_OS_MUTEX_LIST, mutex_context_t, pCurMutex)
    {
        if (_mutex_context_isInvalid(pCurMutex)) {
            break;
        }

        if (_mutex_context_isInit(pCurMutex)) {
            continue;
        }

        os_memset((char_t *)pCurMutex, 0x0u, sizeof(mutex_context_t));
        pCurMutex->head.cs = CS_INITED;
        pCurMutex->head.pName = pName;

        pCurMutex->locked = false;
        pCurMutex->pHoldTask = NULL;
        pCurMutex->originalPriority = OS_PRIOTITY_INVALID_LEVEL;

        EXIT_CRITICAL_SECTION();
        return (u32_t)pCurMutex;
    }

    EXIT_CRITICAL_SECTION();
    return 0u;
}

/**
 * @brief It's sub-routine running at privilege mode.
 *
 * @param pArgs The function argument packages.
 *
 * @return The result of privilege routine.
 */
static i32p_t _mutex_lock_privilege_routine(arguments_t *pArgs)
{
    ENTER_CRITICAL_SECTION();

    mutex_context_t *pCurMutex = (mutex_context_t *)pArgs[0].u32_val;
    thread_context_t *pCurThread = NULL;
    i32p_t postcode = 0;

    pCurThread = kernel_thread_runContextGet();
    if (pCurMutex->locked == true) {
        struct schedule_task *pLockTask = pCurMutex->pHoldTask;
        if (pCurThread->task.prior < pLockTask->prior) {
            pLockTask->prior = pCurThread->task.prior;
        }
        postcode = schedule_exit_trigger(&pCurThread->task, pCurMutex, NULL, &pCurMutex->q_list, 0u, true);

        EXIT_CRITICAL_SECTION();
        return postcode;
    }

    /* Highest priority inheritance */
    pCurMutex->pHoldTask = &pCurThread->task;
    pCurMutex->originalPriority = pCurThread->task.prior;
    pCurMutex->locked = true;

    EXIT_CRITICAL_SECTION();
    return postcode;
}

/**
 * @brief It's sub-routine running at privilege mode.
 *
 * @param pArgs The function argument packages.
 *
 * @return The result of privilege routine.
 */
static i32p_t _mutex_unlock_privilege_routine(arguments_t *pArgs)
{
    ENTER_CRITICAL_SECTION();

    mutex_context_t *pCurMutex = (mutex_context_t *)pArgs[0].u32_val;
    i32p_t postcode = 0;

    struct schedule_task *pCurTask = (struct schedule_task *)list_head(&pCurMutex->q_list);
    struct schedule_task *pLockTask = pCurMutex->pHoldTask;
    /* priority recovery */
    pLockTask->prior = pCurMutex->originalPriority;
    if (!pCurTask) {
        // no blocking thread
        pCurMutex->originalPriority = OS_PRIOTITY_INVALID_LEVEL;
        pCurMutex->pHoldTask = NULL;
        pCurMutex->locked = false;
    } else {
        /* The next thread take the ticket */
        pCurMutex->pHoldTask = pCurTask;
        pCurMutex->originalPriority = pCurTask->prior;
        postcode = schedule_entry_trigger(pCurTask, NULL, 0u);
    }

    EXIT_CRITICAL_SECTION();
    return postcode;
}

/**
 * @brief Initialize a new mutex.
 *
 * @param pName The mutex name.
 *
 * @return The mutex unique id.
 */
u32_t _impl_mutex_init(const char_t *pName)
{
    arguments_t arguments[] = {
        [0] = {.pch_val = (const char_t *)pName},
    };

    return kernel_privilege_invoke((const void *)_mutex_init_privilege_routine, arguments);
}

/**
 * @brief Mutex lock to avoid another thread access this resource.
 *
 * @param id The mutex unique id.
 *
 * @return The result of the operation.
 */
i32p_t _impl_mutex_lock(u32_t ctx)
{
    mutex_context_t *pCtx = (mutex_context_t *)ctx;
    if (_mutex_context_isInvalid(pCtx)) {
        return PC_EOR;
    }

    if (!_mutex_context_isInit(pCtx)) {
        return PC_EOR;
    }

    if (!kernel_isInThreadMode()) {
        return PC_EOR;
    }

    arguments_t arguments[] = {
        [0] = {.u32_val = (u32_t)ctx},
    };

    return kernel_privilege_invoke((const void *)_mutex_lock_privilege_routine, arguments);
}

/**
 * @brief Mutex unlock to allow another access the resource.
 *
 * @param id The mutex unique id.
 *
 * @return The result of the operation.
 */
i32p_t _impl_mutex_unlock(u32_t ctx)
{
    mutex_context_t *pCtx = (mutex_context_t *)ctx;
    if (_mutex_context_isInvalid(pCtx)) {
        return PC_EOR;
    }

    if (!_mutex_context_isInit(pCtx)) {
        return PC_EOR;
    }

    arguments_t arguments[] = {
        [0] = {.u32_val = (u32_t)ctx},
    };

    return kernel_privilege_invoke((const void *)_mutex_unlock_privilege_routine, arguments);
}
