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
#define PC_EOR PC_IER(PC_OS_CMPT_EVENT_7)

/**
 * @brief Check if the event unique id if is's invalid.
 *
 * @param id The provided unique id.
 *
 * @return The true is invalid, otherwise is valid.
 */
static b_t _event_context_isInvalid(event_context_t *pCurEvt)
{
    u32_t start, end;
    INIT_SECTION_FIRST(INIT_SECTION_OS_EVENT_LIST, start);
    INIT_SECTION_LAST(INIT_SECTION_OS_EVENT_LIST, end);

    return ((u32_t)pCurEvt < start || (u32_t)pCurEvt >= end) ? true : false;
}

/**
 * @brief Check if the event object if is's initialized.
 *
 * @param id The provided unique id.
 *
 * @return The true is initialized, otherwise is uninitialized.
 */
static b_t _event_context_isInit(event_context_t *pCurEvt)
{
    return ((pCurEvt) ? (((pCurEvt->head.cs) ? (true) : (false))) : false);
}

/**
 * @brief The event schedule routine execute the the pendsv context.
 *
 * @param id The unique id of the entry thread.
 */
static void _event_schedule(void *pTask)
{
    struct schedule_task *pCurTask = (struct schedule_task *)pTask;

    timeout_remove(&pCurTask->expire, true);

    u32_t changed, any, edge, level, trigger = 0u;
    event_context_t *pCurEvent = (event_context_t *)pCurTask->pPendCtx;
    event_sch_t *pEvt_sche = (event_sch_t *)pCurTask->pPendData;
    if (!pEvt_sche) {
        return;
    }
    changed = pEvt_sche->pEvtVal->value ^ pCurEvent->value;
    if (changed) {
        // Any position
        any = pCurEvent->anyMask;

        // Changings trigger.
        trigger = any & changed;

        // Edge position
        edge = pCurEvent->modeMask;
        edge &= ~pCurEvent->anyMask;

        // Edge rise trigger.
        trigger |= edge & pCurEvent->value & pCurEvent->dirMask & changed;

        // Edge fall trigger.
        trigger |= edge & ~pCurEvent->value & ~pCurEvent->dirMask & changed;

        // Level position
        level = ~pCurEvent->modeMask;
        level &= ~pCurEvent->anyMask;

        // Level high trigger.
        trigger |= level & pCurEvent->value & pCurEvent->dirMask & changed;

        // Level low trigger.
        trigger |= level & ~pCurEvent->value & ~pCurEvent->dirMask & changed;
    }
    // Triggered bits
    trigger |= pCurEvent->triggered;

    pEvt_sche->pEvtVal->value = pCurEvent->value;
    u32_t report = trigger & pEvt_sche->listen;
    if (report) {
        pEvt_sche->pEvtVal->trigger = trigger;
        pCurEvent->triggered &= ~report;
    }

    pCurTask->exec.entry.result = 0;
}

/**
 * @brief It's sub-routine running at privilege mode.
 *
 * @param pArgs The function argument packages.
 *
 * @return The result of privilege routine.
 */
static u32_t _event_init_privilege_routine(arguments_t *pArgs)
{
    ENTER_CRITICAL_SECTION();

    u32_t anyMask = (u32_t)(pArgs[0].u32_val);
    u32_t modeMask = (u32_t)(pArgs[1].u32_val);
    u32_t dirMask = (u32_t)(pArgs[2].u32_val);
    u32_t init = (u32_t)(pArgs[3].u32_val);
    const char_t *pName = (const char_t *)(pArgs[4].pch_val);

    INIT_SECTION_FOREACH(INIT_SECTION_OS_EVENT_LIST, event_context_t, pCurEvent)
    {
        if (_event_context_isInvalid(pCurEvent)) {
            break;
        }

        if (_event_context_isInit(pCurEvent)) {
            continue;
        }

        os_memset((char_t *)pCurEvent, 0x0u, sizeof(event_context_t));
        pCurEvent->head.cs = CS_INITED;
        pCurEvent->head.pName = pName;

        pCurEvent->value = init;
        pCurEvent->triggered = 0u;
        pCurEvent->anyMask = anyMask;
        pCurEvent->modeMask = modeMask;
        pCurEvent->dirMask = dirMask;
        pCurEvent->call.pEvtCallEntry = NULL;

        EXIT_CRITICAL_SECTION();
        return (u32_t)pCurEvent;
    };

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
static i32p_t _event_value_get_privilege_routine(arguments_t *pArgs)
{
    ENTER_CRITICAL_SECTION();

    event_context_t *pCurEvent = (event_context_t *)pArgs[0].u32_val;
    u32_t *pValue = (u32_t *)pArgs[1].pv_val;
    *pValue = pCurEvent->value;

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
static i32p_t _event_set_privilege_routine(arguments_t *pArgs)
{
    ENTER_CRITICAL_SECTION();

    event_context_t *pCurEvent = (event_context_t *)pArgs[0].u32_val;
    u32_t set = (u32_t)pArgs[1].u32_val;
    u32_t clear = (u32_t)pArgs[2].u32_val;
    u32_t toggle = (u32_t)pArgs[3].u32_val;

    u32_t val = pCurEvent->value;
    u32_t changed, any, edge, level, trigger = 0u;
    i32p_t postcode = 0;

    /// Clear bits
    val &= ~clear;
    // Set bits
    val |= set;
    // Toggle bits
    val ^= toggle;

    changed = val ^ pCurEvent->value;

    // Any position
    any = pCurEvent->anyMask;

    // Changings trigger.
    trigger = any & changed;

    // Edge position
    edge = pCurEvent->modeMask;
    edge &= ~pCurEvent->anyMask;

    // Edge rise trigger.
    trigger |= edge & val & pCurEvent->dirMask & changed;

    // Edge fall trigger.
    trigger |= edge & ~val & ~pCurEvent->dirMask & changed;

    // Level position
    level = ~pCurEvent->modeMask;
    level &= ~pCurEvent->anyMask;

    // Level high trigger.
    trigger |= level & val & pCurEvent->dirMask & changed;

    // Level low trigger.
    trigger |= level & ~val & ~pCurEvent->dirMask & changed;

    // Triggered bits
    trigger |= pCurEvent->triggered;

    u32_t report, reported = 0u;
    list_iterator_t it = {0u};
    list_t *pList = (list_t *)&pCurEvent->q_list;
    list_iterator_init(&it, pList);
    struct schedule_task *pCurTask = (struct schedule_task *)list_iterator_next(&it);
    while (pCurTask) {
        event_sch_t *pEvt_sche = (event_sch_t *)pCurTask->pPendData;
        if (!pEvt_sche) {
            return PC_EOR;
        }
        report = trigger & pEvt_sche->listen;
        if (report) {
            reported |= report;
            pEvt_sche->pEvtVal->trigger = trigger;
            pEvt_sche->pEvtVal->value = val;
            postcode = schedule_entry_trigger(pCurTask, _event_schedule, 0u);
            PC_IF(postcode, PC_ERROR)
            {
                break;
            }
        }
        pCurTask = (struct schedule_task *)list_iterator_next(&it);
    }
    pCurEvent->triggered = (~reported) & trigger;
    pCurEvent->value = val;

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
static i32p_t _event_wait_privilege_routine(arguments_t *pArgs)
{
    ENTER_CRITICAL_SECTION();

    event_context_t *pCurEvent = (event_context_t *)pArgs[0].u32_val;
    event_sch_t *pEvt_sch = (event_sch_t *)pArgs[1].pv_val;
    u32_t timeout_ms = (u32_t)pArgs[2].u32_val;
    i32p_t postcode = 0;

    thread_context_t *pCurThread = kernel_thread_runContextGet();
    struct evt_val *pEvtData = pEvt_sch->pEvtVal;
    u32_t changed, any, edge, level, trigger = 0u;

    changed = pEvtData->value ^ pCurEvent->value;
    if (changed) {
        // Any position
        any = pCurEvent->anyMask;

        // Changings trigger.
        trigger = any & changed;

        // Edge position
        edge = pCurEvent->modeMask;
        edge &= ~pCurEvent->anyMask;

        // Edge rise trigger.
        trigger |= edge & pCurEvent->value & pCurEvent->dirMask & changed;

        // Edge fall trigger.
        trigger |= edge & ~pCurEvent->value & ~pCurEvent->dirMask & changed;

        // Level position
        level = ~pCurEvent->modeMask;
        level &= ~pCurEvent->anyMask;

        // Level high trigger.
        trigger |= level & pCurEvent->value & pCurEvent->dirMask & changed;

        // Level low trigger.
        trigger |= level & ~pCurEvent->value & ~pCurEvent->dirMask & changed;
    }

    // Triggered bits
    trigger |= pCurEvent->triggered;

    pEvtData->value = pCurEvent->value;
    u32_t report = trigger & pEvt_sch->listen;
    if (report) {
        pEvtData->trigger = trigger;
        pCurEvent->triggered &= ~report;

        EXIT_CRITICAL_SECTION();
        return postcode;
    }
    postcode = schedule_exit_trigger(&pCurThread->task, pCurEvent, pEvt_sch, &pCurEvent->q_list, timeout_ms, true);
    PC_IF(postcode, PC_PASS)
    {
        postcode = PC_OS_WAIT_UNAVAILABLE;
    }

    EXIT_CRITICAL_SECTION();
    return postcode;
}

/**
 * @brief Initialize a new event.
 *
 * @param anyMask: Changed bits always trigger = 1. otherwise, see dirMask below = 0.
 * @param modeMask: Level trigger = 0, Edge trigger = 1.
 * @param dirMask: Fall or Low trigger = 0, Rise or high trigger = 1.
 * @param init: The init signal value.
 * @param pName: The event name.
 *
 * @return The event unique id.
 */
u32_t _impl_event_init(u32_t anyMask, u32_t modeMask, u32_t dirMask, u32_t init, const char_t *pName)
{
    arguments_t arguments[] = {
        [0] = {.u32_val = (u32_t)anyMask}, [1] = {.u32_val = (u32_t)modeMask},       [2] = {.u32_val = (u32_t)dirMask},
        [3] = {.u32_val = (u32_t)init},    [4] = {.pch_val = (const char_t *)pName},
    };

    return kernel_privilege_invoke((const void *)_event_init_privilege_routine, arguments);
}

/**
 * @brief Read or write the event signal value.
 *
 * @param id: Event unique id.
 * @param pValue: The pointer of the private event value.
 *
 * @return The result of the operation.
 */
i32p_t _impl_event_value_get(u32_t ctx, u32_t *pValue)
{
    event_context_t *pCtx = (event_context_t *)ctx;
    if (_event_context_isInvalid(pCtx)) {
        return PC_EOR;
    }

    if (!_event_context_isInit(pCtx)) {
        return PC_EOR;
    }

    arguments_t arguments[] = {
        [0] = {.u32_val = (u32_t)ctx},
        [1] = {.pv_val = (void *)pValue},
    };

    return kernel_privilege_invoke((const void *)_event_value_get_privilege_routine, arguments);
}

/**
 * @brief Set/clear/toggle event signal bits.
 *
 * @param id: Event unique id.
 * @param set: Event value bits set.
 * @param clear: Event value bits clear.
 * @param toggle: Event value bits toggle.
 *
 * @return The result of the operation.
 */
i32p_t _impl_event_set(u32_t ctx, u32_t set, u32_t clear, u32_t toggle)
{
    event_context_t *pCtx = (event_context_t *)ctx;
    if (_event_context_isInvalid(pCtx)) {
        return PC_EOR;
    }

    if (!_event_context_isInit(pCtx)) {
        return PC_EOR;
    }

    arguments_t arguments[] = {
        [0] = {.u32_val = (u32_t)ctx},
        [1] = {.u32_val = (u32_t)set},
        [2] = {.u32_val = (u32_t)clear},
        [3] = {.u32_val = (u32_t)toggle},
    };

    return kernel_privilege_invoke((const void *)_event_set_privilege_routine, arguments);
}

/**
 * @brief Wait a trigger event.
 *
 * @param id The event unique id.
 * @param pEvtData The pointer of event value.
 * @param listen_mask Current thread listen which bits in the event.
 * @param timeout_ms The event wait timeout setting.
 *
 * @return The result of the operation.
 */
i32p_t _impl_event_wait(u32_t ctx, struct evt_val *pEvtData, u32_t listen_mask, u32_t timeout_ms)
{
    event_context_t *pCtx = (event_context_t *)ctx;
    if (_event_context_isInvalid(pCtx)) {
        return PC_EOR;
    }

    if (!_event_context_isInit(pCtx)) {
        return PC_EOR;
    }

    if (!pEvtData) {
        return PC_EOR;
    }

    if (!timeout_ms) {
        return PC_EOR;
    }

    if (!kernel_isInThreadMode()) {
        return PC_EOR;
    }

    event_sch_t evt_sch = {
        .listen = listen_mask,
        .pEvtVal = pEvtData,
    };
    arguments_t arguments[] = {
        [0] = {.u32_val = (u32_t)ctx},
        [1] = {.pv_val = (void *)&evt_sch},
        [2] = {.u32_val = (u32_t)timeout_ms},
    };

    i32p_t postcode = kernel_privilege_invoke((const void *)_event_wait_privilege_routine, arguments);

    ENTER_CRITICAL_SECTION();

    if (postcode == PC_OS_WAIT_UNAVAILABLE) {
        postcode = kernel_schedule_result_take();
    }

    PC_IF(postcode, PC_PASS_INFO)
    {
        if (postcode != PC_OS_WAIT_TIMEOUT) {
            postcode = 0;
        }
    }

    EXIT_CRITICAL_SECTION();
    return postcode;
}
