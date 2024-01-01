/**
 * Copyright (c) Riven Zheng (zhengheiot@gmail.com).
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "basic.h"
#include "kernal.h"
#include "timer.h"
#include "mutex.h"

#ifdef __cplusplus
extern "C" {
#endif

#define _PC_CMPT_FAILED       PC_FAILED(PC_CMPT_MUTEX)

static u32_t _mutex_init_privilege_routine(arguments_t *pArgs);
static u32_t _mutex_lock_privilege_routine(arguments_t *pArgs);
static u32_t _mutex_unlock_privilege_routine(arguments_t *pArgs);

/**
 * @brief Get the mutex context based on provided unique id.
 *
 * Get the mutex context based on provided unique id, and then return the mutex context pointer.
 *
 * @param id The mutex unique id.
 *
 * @retval VALUE The mutex context.
 */
static mutex_context_t* _mutex_object_contextGet(os_id_t id)
{
    return (mutex_context_t*)(_impl_kernal_member_unified_id_toContainerAddress(id));
}

/**
 * @brief Get the mutex locking list head address.
 *
 * Get the mutex locking list head address.
 *
 * @param NONE.
 *
 * @retval VALUE The locking list head address.
 */
static list_t* _mutex_list_lockingHeadGet(void)
{
    return (list_t*)_impl_kernal_member_list_get(KERNAL_MEMBER_MUTEX, KERNAL_MEMBER_LIST_MUTEX_LOCK);
}

/**
 * @brief Get the mutex unlocking list head address.
 *
 * Get the mutex unlocking list head address.
 *
 * @param NONE.
 *
 * @retval VALUE The unlocking list head address.
 */
static list_t* _mutex_list_unlockingHeadGet(void)
{
    return (list_t*)_impl_kernal_member_list_get(KERNAL_MEMBER_MUTEX, KERNAL_MEMBER_LIST_MUTEX_UNLOCK);
}

/**
 * @brief Get the mutex blocking thread list head address.
 *
 * Get the blocking thread list head address.
 *
 * @param NONE.
 *
 * @retval VALUE the blocking thread list head address.
 */
static list_t* _mutex_list_blockingHeadGet(os_id_t id)
{
    mutex_context_t *pCurMutex = (mutex_context_t *)_mutex_object_contextGet(id);

    return (list_t*)((pCurMutex) ? (&pCurMutex->blockingThreadHead) : (NULL));
}

/**
 * @brief Push one mutex context into unlocking list.
 *
 * Push one mutex context into unlocking list.
 *
 * @param pCurHead The pointer of the mutex linker head.
 *
 * @retval NONE .
 */
static void _mutex_list_transfer_toLock(linker_head_t *pCurHead)
{
    ENTER_CRITICAL_SECTION();

    list_t *pToLockingList = (list_t *)_mutex_list_lockingHeadGet();
    linker_list_transaction_common(&pCurHead->linker, pToLockingList, LIST_TAIL);

    EXIT_CRITICAL_SECTION();
}

/**
 * @brief Push one mutex context into locking list.
 *
 * Push one mutex context into locking list.
 *
 * @param pCurHead The pointer of the mutex linker head.
 *
 * @retval NONE .
 */
static void _mutex_list_transfer_toUnlock(linker_head_t *pCurHead)
{
    ENTER_CRITICAL_SECTION();

    list_t *pToUnlockingList = (list_t *)_mutex_list_unlockingHeadGet();
    linker_list_transaction_common(&pCurHead->linker, pToUnlockingList, LIST_TAIL);

    EXIT_CRITICAL_SECTION();
}

/**
 * @brief Pick up a highest priority thread from the mutex blocking list.
 *
 * Pick up a highest priority thread from the mutex blocking list.
 *
 * @param id The mutex context id
 *
 * @retval VALUE The highest thread head.
 */
static linker_head_t* _mutex_linker_head_fromBlocking(os_id_t id)
{
    ENTER_CRITICAL_SECTION();
    list_t *pListBlocking = (list_t *)_mutex_list_blockingHeadGet(id);
    EXIT_CRITICAL_SECTION();

    return (linker_head_t*)(pListBlocking->pHead);
}

/**
 * @brief Check if the mutex unique id if is's invalid.
 *
 * Check if the mutex unique id if is's invalid.
 *
 * @param id The provided unique id.
 *
 * @retval TRUE The id is invalid
 *         FALSE The id is valid
 */
static b_t _mutex_id_isInvalid(i32_t id)
{
    return _impl_kernal_member_unified_id_isInvalid(KERNAL_MEMBER_MUTEX, id);
}

/**
 * @brief Check if the mutex object if is's initialized.
 *
 * Check if the mutex unique id if is's initialization.
 *
 * @param id The provided unique id.
 *
 * @retval TRUE The id is initialized.
 *         FALSE The id isn't initialized.
 */
static b_t _mutex_object_isInit(i32_t id)
{
    mutex_context_t *pCurMutex = (mutex_context_t *)_mutex_object_contextGet(id);

    if (pCurMutex)
    {
        return ((pCurMutex->head.linker.pList) ? (TRUE) : (FALSE));
    }
    else
    {
        return FALSE;
    }
}

/**
 * @brief Convert the internal os id to kernal member number.
 *
 * Convert the internal os id to kernal member number.
 *
 * @param id The provided unique id.
 *
 * @retval VALUE Member number.
 */
u32_t _impl_mutex_os_id_to_number(os_id_t id)
{
    return (u32_t)(_mutex_id_isInvalid(id) ? (0u) : (id - _impl_kernal_member_id_toUnifiedIdStart(KERNAL_MEMBER_MUTEX)) / sizeof(mutex_context_t));
}

/**
 * @brief Initialize a new mutex.
 *
 * Initialize a new mutex.
 *
 * @param pName The mutex name.
 *
 * @retval VALUE The mutex unique id.
 */
os_id_t _impl_mutex_init(const char_t *pName)
{
    arguments_t arguments[] =
    {
        [0] = {(u32_t)pName},
    };

    return _impl_kernal_privilege_invoke(_mutex_init_privilege_routine, arguments);
}

/**
 * @brief Mutex lock to avoid another thread access this resource.
 *
 * Mutex lock to avoid another thread access this resource.
 *
 * @param id The mutex unique id.
 *
 * @retval VALUE The result of the operation.
 */
u32p_t _impl_mutex_lock(os_id_t id)
{
    if (_mutex_id_isInvalid(id))
    {
        return _PC_CMPT_FAILED;
    }

    if (!_mutex_object_isInit(id))
    {
        return _PC_CMPT_FAILED;
    }

    if (!_impl_kernal_isInThreadMode())
    {
        return _PC_CMPT_FAILED;
    }

    arguments_t arguments[] =
    {
        [0] = {(u32_t)id},
    };

    return _impl_kernal_privilege_invoke(_mutex_lock_privilege_routine, arguments);

}

/**
 * @brief Mutex unlock to allow another thread can access this resource.
 *
 * Mutex unlock to allow another thread can access this resource.
 *
 * @param id The mutex unique id.
 *
 * @retval POSTCODE_RTOS_MUTEX_UNLOCK_SUCCESS mutex lock successful.
 *         POSTCODE_RTOS_MUTEX_UNLOCK_FAILED mutex lock failed.
 */
u32p_t _impl_mutex_unlock(os_id_t id)
{
    if (_mutex_id_isInvalid(id))
    {
        return _PC_CMPT_FAILED;
    }

    if (!_mutex_object_isInit(id))
    {
        return _PC_CMPT_FAILED;
    }

    arguments_t arguments[] =
    {
        [0] = {(u32_t)id},
    };

    return _impl_kernal_privilege_invoke(_mutex_unlock_privilege_routine, arguments);
}

/**
 * @brief The privilege routine running at handle mode.
 *
 * The privilege routine running at handle mode.
 *
 * @param args_0 The function argument 1.
 * @param args_1 The function argument 2.
 * @param args_2 The function argument 3.
 * @param args_3 The function argument 4.
 *
 * @retval VALUE The result of privilege routine.
 */
static u32_t _mutex_init_privilege_routine(arguments_t *pArgs)
{
    ENTER_CRITICAL_SECTION();

    const char_t *pName = (const char_t *)(pArgs[0].u32_val);

    mutex_context_t *pCurMutex = (mutex_context_t *)_impl_kernal_member_id_toContainerStartAddress(KERNAL_MEMBER_MUTEX);
    os_id_t id = _impl_kernal_member_id_toUnifiedIdStart(KERNAL_MEMBER_MUTEX);

    do {
        if (!_mutex_object_isInit(id))
        {
            _memset((char_t*)pCurMutex, 0x0u, sizeof(mutex_context_t));

            pCurMutex->head.id = id;
            pCurMutex->head.pName = pName;

            pCurMutex->holdThreadId = OS_INVALID_ID;
            pCurMutex->originalPriority.level = OS_PRIORITY_INVALID;

            _mutex_list_transfer_toUnlock((linker_head_t*)&pCurMutex->head);
            break;
        }

        pCurMutex++;
        id = _impl_kernal_member_containerAddress_toUnifiedid((u32_t)pCurMutex);
    } while ((u32_t)pCurMutex < (u32_t)_impl_kernal_member_id_toContainerEndAddress(KERNAL_MEMBER_MUTEX));

    id = ((!_mutex_id_isInvalid(id)) ? (id) : (OS_INVALID_ID));

    EXIT_CRITICAL_SECTION();

    return id;
}

/**
 * @brief The privilege routine running at handle mode.
 *
 * The privilege routine running at handle mode.
 *
 * @param args_0 The function argument 1.
 * @param args_1 The function argument 2.
 * @param args_2 The function argument 3.
 * @param args_3 The function argument 4.
 *
 * @retval VALUE The result of privilege routine.
 */
static u32_t _mutex_lock_privilege_routine(arguments_t *pArgs)
{
    ENTER_CRITICAL_SECTION();

    os_id_t id = (os_id_t)pArgs[0].u32_val;

    u32p_t postcode = PC_SC_SUCCESS;
    mutex_context_t *pCurMutex = (mutex_context_t *)_mutex_object_contextGet(id);
    thread_context_t *pCurThread = (thread_context_t *)_impl_kernal_thread_runContextGet();

    if (pCurMutex->head.linker.pList == _mutex_list_lockingHeadGet())
    {
        thread_context_t *pLockThread = (thread_context_t*)_impl_kernal_member_unified_id_toContainerAddress(pCurMutex->holdThreadId);

        if (pCurThread->priority.level < pLockThread->priority.level)
        {
            pLockThread->priority = pCurThread->priority;
        }
        postcode = _impl_kernal_thread_exit_trigger(pCurThread->head.id, id, _mutex_list_blockingHeadGet(id), 0u, NULL);

    }
    else
    {
        /* Highest priority inheritance */
        pCurMutex->holdThreadId = pCurThread->head.id;
        pCurMutex->originalPriority = pCurThread->priority;
        _mutex_list_transfer_toLock((linker_head_t*)&pCurMutex->head);
    }

    EXIT_CRITICAL_SECTION();

    return postcode;
}

/**
 * @brief The privilege routine running at handle mode.
 *
 * The privilege routine running at handle mode.
 *
 * @param args_0 The function argument 1.
 * @param args_1 The function argument 2.
 * @param args_2 The function argument 3.
 * @param args_3 The function argument 4.
 *
 * @retval VALUE The result of privilege routine.
 */
static u32_t _mutex_unlock_privilege_routine(arguments_t *pArgs)
{
    ENTER_CRITICAL_SECTION();

    os_id_t id = (os_id_t)pArgs[0].u32_val;

    u32p_t postcode = PC_SC_SUCCESS;
    mutex_context_t *pCurMutex = (mutex_context_t *)_mutex_object_contextGet(id);
    thread_context_t *pMutexHighestBlockingThread = (thread_context_t *)_mutex_linker_head_fromBlocking(id);
    thread_context_t *pLockThread = (thread_context_t*)_impl_kernal_member_unified_id_toContainerAddress(pCurMutex->holdThreadId);

    /* priority recovery */
    pLockThread->priority = pCurMutex->originalPriority;

    if (!pMutexHighestBlockingThread)
    {
        // no blocking thread
        pCurMutex->originalPriority.level = OS_PRIORITY_INVALID;
        pCurMutex->holdThreadId = OS_INVALID_ID;

        _mutex_list_transfer_toUnlock((linker_head_t*)&pCurMutex->head);
    }
    else
    {
        /* The next thread take the ticket */
        pCurMutex->holdThreadId = pMutexHighestBlockingThread->head.id;
        pCurMutex->originalPriority = pMutexHighestBlockingThread->priority;
        postcode = _impl_kernal_thread_entry_trigger(pMutexHighestBlockingThread->head.id, id, PC_SC_SUCCESS, NULL);
    }

    EXIT_CRITICAL_SECTION();

    return postcode;
}

#ifdef __cplusplus
}
#endif
