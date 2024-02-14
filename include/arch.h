/**
 * Copyright (c) Riven Zheng (zhengheiot@gmail.com).
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#ifndef _ARCH_H_
#define _ARCH_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "configuration.h"

typedef enum IRQn {
    /******  Cortex-Mx Processor Exceptions Numbers
    ***************************************************/
    NonMaskableInt_IRQn = -14,   /*!< 2 Non Maskable Interrupt                         */
    HardFault_IRQn = -13,        /*!<   3  Hard Fault, all classes of Fault        */
    MemoryManagement_IRQn = -12, /*!< 4 Cortex-Mx Memory Management Interrupt          */
    BusFault_IRQn = -11,         /*!< 5 Cortex-Mx Bus Fault Interrupt                  */
    UsageFault_IRQn = -10,       /*!< 6 Cortex-Mx Usage Fault Interrupt                */
    SVCall_IRQn = -5,            /*!< 11 Cortex-Mx SV Call Interrupt                   */
    DebugMonitor_IRQn = -4,      /*!< 12 Cortex-Mx Debug Monitor Interrupt             */
    PendSV_IRQn = -2,            /*!< 14 Cortex-Mx Pend SV Interrupt                   */
    SysTick_IRQn = -1,           /*!< 15 Cortex-Mx System Tick Interrupt               */
} IRQn_Type;

#if ( (defined ( __CC_ARM ) && defined ( __TARGET_FPU_VFP ))                      \
   || (defined ( __CLANG_ARM ) && defined ( __VFP_FP__ ) && !defined(__SOFTFP__)) \
   || (defined ( __ICCARM__ ) && defined ( __ARMVFP__ ))                          \
   || (defined ( __GNUC__ ) && defined ( __VFP_FP__ ) && !defined(__SOFTFP__)) )
    #define __FPU_PRESENT   1
#else
    #define __FPU_PRESENT   0
#endif

/* Default Setting */
#if defined ( ARCH_MPU_PRESENT )
    #define __MPU_PRESENT               1
#endif

#if !defined ( __MPU_PRESENT )
    #define __MPU_PRESENT               1
#endif

#if !defined  ( __NVIC_PRIO_BITS )
    #define __NVIC_PRIO_BITS            8
#endif

#if !defined  ( __Vendor_SysTickConfig )
    #define __Vendor_SysTickConfig      0
#endif

#if !defined  ( __VTOR_PRESENT )
    #define __VTOR_PRESENT              1
#endif

#if !defined  (  __FPU_PRESENT )
    #define __FPU_PRESENT               0
    #undef __FPU_USED
#endif

#if !defined  (  __DSP_PRESENT )
    #define __DSP_PRESENT               0
#endif

#if defined ( ARCH_ARM_CORTEX_CM0 )
    #include "../arch/arch32/arm/cmsis/include/core_cm0.h"

#elif defined ( ARCH_ARM_CORTEX_CM0plus )
    #include "../arch/arch32/arm/cmsis/include/core_cm0plus.h"

#elif defined ( ARCH_ARM_CORTEX_CM3 )
    #include "../arch/arch32/arm/cmsis/include/core_cm3.h"

#elif defined ( ARCH_ARM_CORTEX_CM4 )
    #include "../arch/arch32/arm/cmsis/include/core_cm4.h"

#elif defined ( ARCH_ARM_CORTEX_CM23 )
    #include "../arch/arch32/arm/cmsis/include/core_cm23.h"

#elif defined ( ARCH_ARM_CORTEX_CM33 )
    // #define __CM33_REV                0x0000U   /*!< Core revision r0p1 */
    // #define __SAUREGION_PRESENT       1U        /*!< SAU regions present */
    // #define __MPU_PRESENT             1U        /*!< MPU present */
    // #define __VTOR_PRESENT            1U        /*!< VTOR present */
    // #define __NVIC_PRIO_BITS          4U        /*!< Number of Bits used for Priority Levels */
    // #define __Vendor_SysTickConfig    0U        /*!< Set to 1 if different SysTick Config is used */
    // #define __FPU_PRESENT             1U        /*!< FPU present */
    // #define __DSP_PRESENT             1U        /*!< DSP extension present */

    #include "../arch/arch32/arm/cmsis/include/core_cm33.h"

#elif defined ARCH_ARM_CORTEX_CM7
    #include "../arch/arch32/arm/cmsis/include/core_cm7.h"

#elif defined ARCH_NATIVE_GCC
    // Nothing to do
#else
    #error "No ARM Arch is defined"
#endif

#if !defined ARCH_NATIVE_GCC
    #define ARCH_ENTER_CRITICAL_SECTION()   vu32_t PRIMASK_Bit = __get_PRIMASK();        \
                                              __disable_irq();                           \
                                              __DSB();                                   \
                                              __ISB();

    #define ARCH_EXIT_CRITICAL_SECTION()    __set_PRIMASK(PRIMASK_Bit);                  \
                                              __DSB();                                   \
                                              __ISB();

#else
    #define ARCH_ENTER_CRITICAL_SECTION()
    #define ARCH_EXIT_CRITICAL_SECTION()
#endif

#ifdef __cplusplus
}
#endif

#endif /* _ARCH_H_ */
