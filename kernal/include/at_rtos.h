/**
 * Copyright (c) Riven Zheng (zhengheiot@gmail.com).
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#ifndef _AT_RTOS_H_
#define _AT_RTOS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "basic.h"
#include "unique.h"
#include "configuration.h"

/* The interface of the thread stack allocation. */
#define ATOS_STACK_DEFINE(name, size)           static u32_t name[((u32_t)(size) / sizeof(u32_t))] = {0x0u}; \
S_ASSERT(((size) >= STACK_SIZE_MINIMUM) , "The thread stack size must be higher than STACK_SIZE_MINIMUM")

/* The interface of the thread priority allocation. */
#define ATOS_PRIORITY_DEFINE(name, priority)    u8_t name = (priority); \
S_ASSERT((((priority) >= OS_PRIORITY_USER_THREAD_HIGHEST_LEVEL) && ((priority) <= OS_PRIORITY_USER_THREAD_LOWEST_LEVEL)) , "The thread priority is out of the system design")

#include "thread.h"

/**
 * @brief Initialize a thread, and put it to pending list that are ready to run.
 *
 * @param pEntryFun The pointer of the thread entry function. Thread function must be designed to never return.
 * @param pStackAddr The pointer of the thread stack address. The stack memory must be predefined and allocated in your system.
 * @param stackSize The size of the the stack is base on your specified variable.
 * @param priority The thread priority specified the thread's priority when the AtOS do kernal schedule.
 * @param pName The thread name.
 *
 * @return The value of thread unique id.
 **
 * demo usage:
 *
 * ATOS_STACK_DEFINE(sample_stk, 512);
 *
 * void thread_sample_function(void)
 * {
 *     while(1)
 *     {
 *
 *     }
 * }
 *
 * int main(void)
 * {
 *     ATOS_PRIORITY_DEFINE(sample_pri, 5);
 *     os_thread_id_t sample_id = thread_init(thread_sample_function, sample_stk, 512, sample_pri, "SAMPLE");
 *     if (os_id_is_invalid(sample_id))
 *     {
 *         printf("Thread %d init failed\n", sample_id.val);
 *     }
 *     ...
 * }
 *
 */
static inline os_thread_id_t thread_init(pThread_entryFunc_t pEntryFun, u32_t *pStackAddr, u32_t stackSize, u8_t priority, const char_t *pName)
{
    os_thread_id_t id = {0u};

    id.val = _impl_thread_init(pEntryFun, pStackAddr, stackSize, priority, pName);
    id.number = _impl_thread_os_id_to_number(id.val);
    id.pName = pName;

    return id;
}

/**
 * @brief Put the current running thread into sleep mode with timeout condition.
 *
 * @param timeout_ms The time user defined.
 *
 * @return The result of thread sleep operation.
 **
 * demo usage:
 *
 * void thread_sample_function(void)
 * {
 *     while(1)
 *     {
 *          thread_sleep(1000); // Put the thread to sleep mode 1 sec.
 *     }
 * }
 */
static inline u32p_t thread_sleep(u32_t ms)
{
    return (u32p_t)_impl_thread_sleep(ms);
}

/**
 * @brief Resume a thread to run.
 *
 * @param id The thread unique id.
 *
 * @return The result of thread resume operation.
 **
 * demo usage:
 *
 *    thread_resume(sample_id); // The variable sample_id created in above thread init demo.
 */
static inline u32p_t thread_resume(os_thread_id_t id)
{
    return (u32p_t)_impl_thread_resume(id.val);
}

/**
 * @brief Suspend a thread to permit another to run.
 *
 * @param id The thread unique id.
 *
 * @return The result of thread suspend operation.
 **
 * demo usage:
 *
 *    thread_suspend(sample_id); // The variable sample_id created in above thread init demo.
 */
static inline u32p_t thread_suspend(os_thread_id_t id)
{
    return (u32p_t)_impl_thread_suspend(id.val);
}

/**
 * @brief Yield current thread to allow other thread to run.
 *
 * @param id The thread unique id.
 *
 * @return The result of thread yield operation.
 **
 * demo usage:
 *
 * void thread_sample_function(void)
 * {
 *     while(1)
 *     {
 *          thread_yield(); // Put current thread to sleep mode manually.
 *     }
 * }
 */
static inline u32p_t thread_yield(void)
{
    return (u32p_t)_impl_thread_yield();
}

/**
 * @brief Delete a sleep mode thread, and erase the stack data, and that can't be recovered.
 *
 * @param id The thread unique id.
 *
 * @return The result of thread delete operation.
 **
 * demo usage:
 *
 *    thread_delete(sample_id); // The variable sample_id created in above thread init demo.
 */
static inline u32p_t thread_delete(os_thread_id_t id)
{
    return (u32p_t)_impl_thread_delete(id.val);
}

#include "timer.h"
static inline os_timer_id_t timer_init(pTimer_callbackFunc_t pEntryFun, b_t isCycle, u32_t timeout_ms, const char_t *pName)
{
    os_timer_id_t id = {0u};

    id.val = _impl_timer_init(pEntryFun, isCycle, timeout_ms, pName);
    id.number = _impl_timer_os_id_to_number(id.val);
    id.pName = pName;

    return id;
}

static inline u32p_t timer_start(os_timer_id_t id, b_t isCycle, u32_t timeout_ms)
{
    return (u32p_t)_impl_timer_start(id.val, isCycle, timeout_ms);
}

static inline u32p_t timer_stop(os_timer_id_t id)
{
    return (u32p_t)_impl_timer_stop(id.val);
}

static inline u32p_t timer_isBusy(os_timer_id_t id)
{
    return (u32p_t)_impl_timer_status_isBusy(id.val);
}

static inline u32_t timer_system_total_ms(void)
{
    return (u32_t)_impl_timer_total_system_get();
}

#include "semaphore.h"
static inline os_semaphore_id_t semaphore_init(u8_t availableCount, u8_t limitationCount, const char_t *pName)
{
    os_semaphore_id_t id = {0u};

    id.val = _impl_semaphore_init(availableCount, limitationCount, pName);
    id.number = _impl_semaphore_os_id_to_number(id.val);
    id.pName = pName;

    return id;
}

static inline u32p_t semaphore_take(os_semaphore_id_t id, u32_t timeout_ms)
{
    return (u32p_t)_impl_semaphore_take(id.val, timeout_ms);
}

static inline u32p_t semaphore_give(os_semaphore_id_t id)
{
    return (u32p_t)_impl_semaphore_give(id.val);
}

static inline u32p_t semaphore_flush(os_semaphore_id_t id)
{
    return (u32p_t)_impl_semaphore_flush(id.val);
}

#include "mutex.h"
static inline os_mutex_id_t mutex_init(const char_t *pName)
{
    os_mutex_id_t id = {0u};

    id.val = _impl_mutex_init(pName);
    id.number = _impl_mutex_os_id_to_number(id.val);
    id.pName = pName;

    return id;
}

static inline u32p_t mutex_lock(os_mutex_id_t id)
{
    return (u32p_t)_impl_mutex_lock(id.val);
}

static inline u32p_t mutex_unlock(os_mutex_id_t id)
{
    return (u32p_t)_impl_mutex_unlock(id.val);
}

#include "event.h"
static inline os_event_id_t event_init(u32_t edge, pEvent_callbackFunc_t pCallFun, const char_t *pName)
{
    os_queue_id_t id = {0u};

    id.val = _impl_event_init(edge, pCallFun, pName);
    id.number = _impl_event_os_id_to_number(id.val);
    id.pName = pName;

    return id;
}

static inline u32p_t event_set(os_event_id_t id, u32_t event)
{
    return (u32p_t)_impl_event_set(id.val, event);
}

static inline u32p_t event_wait(os_event_id_t id, u32_t *pEvent, u32_t trigger, u32_t listen, u32_t timeout_ms)
{
    return (u32p_t)_impl_event_wait(id.val, pEvent, trigger, listen, timeout_ms);
}

#include "queue.h"
static inline os_queue_id_t queue_init(const void *pQueueBufferAddr, u16_t elementLen, u16_t elementNum, const char_t *pName)
{
    os_queue_id_t id = {0u};

    id.val = _impl_queue_init(pQueueBufferAddr, elementLen, elementNum, pName);
    id.number = _impl_queue_os_id_to_number(id.val);
    id.pName = pName;

    return id;
}

static inline u32p_t queue_send(os_queue_id_t id, const u8_t *pUserBuffer, u16_t bufferSize, u32_t timeout_ms)
{
    return (u32p_t)_impl_queue_send(id.val, pUserBuffer, bufferSize, timeout_ms);
}

static inline u32p_t queue_receive(os_queue_id_t id, const u8_t *pUserBuffer, u16_t bufferSize, u32_t timeout_ms)
{
    return (u32p_t)_impl_queue_receive(id.val, pUserBuffer, bufferSize, timeout_ms);
}

static inline b_t os_id_is_invalid(struct os_id id)
{
    return (b_t)_impl_kernal_os_id_is_invalid(id);
}

typedef struct
{
    os_thread_id_t (*thread_init)(pThread_entryFunc_t, u32_t *, u32_t, u8_t, const char_t *);
    u32p_t         (*thread_sleep)(u32_t);
    u32p_t         (*thread_resume)(os_thread_id_t);
    u32p_t         (*thread_suspend)(os_thread_id_t);
    u32p_t         (*thread_yield)(void);
    u32p_t         (*thread_delete)(os_thread_id_t);

    os_timer_id_t  (*timer_init)(pTimer_callbackFunc_t, b_t, u32_t, const char_t *);
    u32p_t         (*timer_start)(os_timer_id_t, b_t, u32_t);
    u32p_t         (*timer_stop)(os_timer_id_t);
    u32p_t         (*timer_isBusy)(os_timer_id_t);
    u32_t          (*timer_system_total_ms)(void);

    os_semaphore_id_t (*semaphore_init)(u8_t, u8_t, const char_t *);
    u32p_t            (*semaphore_take)(os_semaphore_id_t, u32_t);
    u32p_t            (*semaphore_give)(os_semaphore_id_t);
    u32p_t            (*semaphore_flush)(os_semaphore_id_t);

    os_mutex_id_t (*mutex_init)(const char_t *);
    u32p_t        (*mutex_lock)(os_mutex_id_t);
    u32p_t        (*mutex_unlock)(os_mutex_id_t);

    os_event_id_t (*event_init)(u32_t, pEvent_callbackFunc_t, const char_t *);
    u32p_t        (*event_set)(os_event_id_t, u32_t);
    u32p_t        (*event_wait)(os_event_id_t, u32_t *, u32_t, u32_t, u32_t);

    os_queue_id_t (*queue_init)(const void *, u16_t, u16_t, const char_t *);
    u32p_t        (*queue_send)(os_queue_id_t, const u8_t *, u16_t, u32_t);
    u32p_t        (*queue_receive)(os_queue_id_t, const u8_t *, u16_t, u32_t);

    b_t           (*os_id_is_invalid)(struct os_id);
    u32p_t        (*kernal_atos_run)(void);
}at_rtos_api_t;

extern const at_rtos_api_t AtOS;

#ifdef __cplusplus
}
#endif

#endif /* _AT_RTOS_H_ */

