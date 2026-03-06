/*
 * Copyright © 2024-2026 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 */

#ifndef _MACROS_H
#define _MACROS_H

#include <ctime>
#include <cstdint>
#include <cstdlib>

// Include shared macros from common directory
#include "shared_macros.h"

// Common constants
#define SHIFT                         (0.1)
#define REF_COMBO_FREQ                19.2
#define REF_CLK_FREQ                  38.4
#define REF_CLK_FREQ_KHZ              (REF_CLK_FREQ * 1000)
#define ONE_SEC_IN_NS                 (1000 * 1000 * 1000)

// Time conversion macros
#define TV_NSEC(t)                    ((long)((t * 1000000) % ONE_SEC_IN_NS))
#define TV_SEC(t)                     ((time_t)((t * 1000000) / ONE_SEC_IN_NS))
#define TIME_IN_USEC(sec, usec)       (uint64_t)(1000000 * (uint64_t)sec + usec)

// Bit manipulation macros
#define BIT(nr)                       (1UL << (nr))
#define REG_BIT8                      BIT
#define BITS_PER_LONG                 64
#define GENMASK(h, l) \
	(((~0UL) - (1UL << (l)) + 1) & (~0UL >> (BITS_PER_LONG - 1 - (h))))
#define REG_GENMASK(h, l)             ((uint32_t)GENMASK(h, l))
#define REG_GENMASK8(h, l)            ((uint8_t)GENMASK(h, l))
#define GETBITS_VAL(val, h, l)        ((val & GENMASK(h, l)) >> l)
#define SETBIT(val, bit)              ((val) | (1UL << (bit)))
#define SETBIT_INPLACE(val, bit)      ((val) |= (1UL << (bit)))
#define SETBIT_TO_INPLACE(val, bit, to) \
    do { \
        if (to) \
            (val) |= (1UL << (bit)); \
        else \
            (val) &= ~(1UL << (bit)); \
    } while (0)

// Array and pick macros
#define ARRAY_SIZE(a)                 (int)(sizeof(a)/sizeof(a[0]))
#define _PICK_EVEN(__index, __a, __b) ((__a) + (__index) * ((__b) - (__a)))
#define _PICK(__index, ...)           (((const uint32_t []){ __VA_ARGS__ })[__index])

// Synchronization calculation macro
/*
 * td - The time difference in between the two systems in ms.
 * s  - The percentage shift that we need to make in our vsyncs.
 *
 * Note: Calling abs to ensure a positive 'step' value result in case if
 * shift (s) is negative.
 */
#define CALC_STEPS_TO_SYNC(td, s)     ((s) == 0 ? 0 : abs((int)((td * 100) / (s))))

// Register helper macro
#define REG(a)                        {a, 0, 0}

// Hardware register addresses - Per-pipe DDI Function Control
#define TRANS_DDI_FUNC_CTL_A          0x60400
#define TRANS_DDI_FUNC_CTL_B          0x61400
#define TRANS_DDI_FUNC_CTL_C          0x62400
#define TRANS_DDI_FUNC_CTL_D          0x63400
#define TRANS_DDI_FUNC_CTL_EDP        0x6F400
#define TRANS_DDI_FUNC_CTL_DSI0       0x6b400
#define TRANS_DDI_FUNC_CTL_DSI1       0x6bc00
#define DPCLKA_CFGCR0                 0x164280
#define DPCLKA_CFGCR1                 0x1642BC

#endif // _MACROS_H
