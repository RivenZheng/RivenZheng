/**
 * Copyright (c) Riven Zheng (zhengheiot@gmail.com).
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#ifndef _ATOS_CONFIGURATION_H_
#define _ATOS_CONFIGURATION_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * If you are use ARM Cortex M seiral architecture, the Cortex-M Core must be specificed as the following list.
 * ARCH_ARM_CORTEX_CM0
 * ARCH_ARM_CORTEX_CM0plus
 * ARCH_ARM_CORTEX_CM3
 * ARCH_ARM_CORTEX_CM4
 * ARCH_ARM_CORTEX_CM23
 * ARCH_ARM_CORTEX_CM33
 * ARCH_ARM_CORTEX_CM7
 **/
#define ARCH_ARM_CORTEX_CM4

/**
 * If you are use ARM Cortex M seiral architecture and use the system tick as the kernal timer.
 * In most cases, PORTAL_SYSTEM_CORE_CLOCK_MHZ must be set to the frequency of the clock
 * that drives the peripheral used to generate the kernels periodic tick interrupt.
 * The default value is set to 120mhz. Your application will certainly need a different value so set this correctly.
 * This is very often, but not always, equal to the main system clock frequency.
 **/
#define PORTAL_SYSTEM_CORE_CLOCK_MHZ              (120u)

/**
 * If you are use ARM Cortex M seiral architecture and use the system tick as the kernal timer.
 * The kernals periodic tick interrupt scheduler needs a minimum time to handle the kernal time function,
 * The defaule value is set ot 50us when the frequency is 120mhz. Your application will certainly need a different value so set this correctly.
 * This is very often, but not always, according to the main system clock frequency.
 **/
#define PORTAL_SYSTEM_CLOCK_INTERVAL_MIN_US       (50u)

/**
 * This symbol defined the thread instance number that your application is using.
 * The defaule value is set to 3. Your application will certainly need a different value so set this correctly.
 * This is very often, but not always, according to the actual thread instance number that you created.
 **/
#define THREAD_INSTANCE_SUPPORTED_NUMBER          (3u)

/**
 * This symbol defined the semaphore instance number that your application is using.
 * The defaule value is set to 3. Your application will certainly need a different value so set this correctly.
 * This is very often, but not always, according to the actual semaphore instance number that you created.
 **/
#define SEMAPHORE_INSTANCE_SUPPORTED_NUMBER       (3u)

/**
 * This symbol defined the event instance number that your application is using.
 * The defaule value is set to 3. Your application will certainly need a different value so set this correctly.
 * This is very often, but not always, according to the actual event instance number that you created.
 **/
#define EVENT_INSTANCE_SUPPORTED_NUMBER           (3u)

/**
 * This symbol defined the mutex instance number that your application is using.
 * The defaule value is set to 3. Your application will certainly need a different value so set this correctly.
 * This is very often, but not always, according to the actual mutex instance number that you created.
 **/
#define MUTEX_INSTANCE_SUPPORTED_NUMBER           (3u)

/**
 * This symbol defined the queue instance number that your application is using.
 * The defaule value is set to 5. Your application will certainly need a different value so set this correctly.
 * This is very often, but not always, according to the actual queue instance number that you created.
 **/
#define QUEUE_INSTANCE_SUPPORTED_NUMBER           (3u)

/**
 * This symbol defined the timer instance number that your application is using.
 * The defaule value is set to 5. Your application will certainly need a different value so set this correctly.
 * This is very often, but not always, according to the actual timer instance number that you created.
 **/
#define TIMER_INSTANCE_SUPPORTED_NUMBER           (3u)

/**
 * This symbol defined your thread running mode, if the thread runs at the privileged mode.
 * The defaule value is set to 0. Your application will certainly need a different value so set this correctly.
 * This is very often, but not always, according to the security level that you want.
 **/
#define THREAD_PSP_WITH_PRIVILEGED                (3u)

#ifdef __cplusplus
}
#endif

#endif /* _ATOS_CONFIGURATION_H_ */