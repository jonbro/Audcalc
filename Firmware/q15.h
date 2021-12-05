// adapted from https://github.com/ARM-software/CMSIS_5
// TODO: copy license into repo

#ifndef _FIXED_POINT_Q15_H_
#define _FIXED_POINT_Q15_H_

#ifdef   __cplusplus
extern "C"
{
#endif

#ifndef   __STATIC_FORCEINLINE
#define __STATIC_INLINE static inline
#endif

#include <stdint.h>

/**
* @brief 8-bit fractional data type in 1.7 format.
*/
typedef int8_t q7_t;

/**
	* @brief 16-bit fractional data type in 1.15 format.
	*/
typedef int16_t q15_t;

/**
 * @brief 32-bit fractional data type in 1.31 format.
 */
typedef int32_t q31_t;



__STATIC_INLINE int32_t __SSAT(int32_t val, uint32_t sat)
{
    if ((sat >= 1U) && (sat <= 32U))
    {
        const int32_t max = (int32_t)((1U << (sat - 1U)) - 1U);
        const int32_t min = -1 - max;
        if (val > max)
        {
            return max;
        }
        else if (val < min)
        {
            return min;
        }
    }
    return val;
}


#define Q31_MAX   ((q31_t)(0x7FFFFFFFL))
#define Q15_MAX   ((q15_t)(0x7FFF))
#define Q7_MAX    ((q7_t)(0x7F))
#define Q31_MIN   ((q31_t)(0x80000000L))
#define Q15_MIN   ((q15_t)(0x8000))
#define Q7_MIN    ((q7_t)(0x80))

#define Q31_ABSMAX   ((q31_t)(0x7FFFFFFFL))
#define Q15_ABSMAX   ((q15_t)(0x7FFF))
#define Q7_ABSMAX    ((q7_t)(0x7F))
#define Q31_ABSMIN   ((q31_t)0)
#define Q15_ABSMIN   ((q15_t)0)
#define Q7_ABSMIN    ((q7_t)0)


q15_t add_q15(q15_t srcA, q15_t srcB)
{
	return (q15_t)__SSAT(((q31_t)srcA + srcB), 16);
}
q15_t mult_q15(q15_t srcA, q15_t srcB)
{
    q31_t mul = (q31_t)((q15_t)(srcA) * (q15_t)(srcB));
    return (q15_t)__SSAT(mul >> 15, 16);
}
q15_t sub_q15(q15_t srcA, q15_t srcB)
{
    return (q15_t)__SSAT(((q31_t)srcA - srcB), 16);
}
q15_t f32_to_q15(float in)
{
    return (q15_t)(in * 32767.0f);
}
float q15_to_f32(q15_t in)
{
    return ((float)in / 32767.0f);
}

// https://www.nullhardware.com/blog/fixed-point-sine-and-cosine-for-embedded-systems/
/*
Implements the 5-order polynomial approximation to sin(x).
@param i   angle (with 2^15 units/circle)
@return    16 bit fixed point Sine value (4.12) (ie: +4096 = +1 & -4096 = -1)
maxval * num * 1/44100
The result is accurate to within +- 1 count. ie: +/-2.44e-4.
*/
q15_t sin_q15(q15_t i)
{
    /* Convert (signed) input to a value between 0 and 8192. (8192 is pi/2, which is the region of the curve fit). */
    /* ------------------------------------------------------------------- */
    i <<= 1;
    uint8_t c = i < 0; //set carry for output pos/neg

    if (i == (i | 0x4000)) // flip input value to corresponding value in range [0..8192)
        i = (1 << 15) - i;
    i = (i & 0x7FFF) >> 1;
    /* ------------------------------------------------------------------- */

    /* The following section implements the formula:
     = y * 2^-n * ( A1 - 2^(q-p)* y * 2^-n * y * 2^-n * [B1 - 2^-r * y * 2^-n * C1 * y]) * 2^(a-q)
    Where the constants are defined as follows:
    */
    enum { A1 = 3370945099UL, B1 = 2746362156UL, C1 = 292421UL };
    enum { n = 13, p = 32, q = 31, r = 3, a = 12 };

    uint32_t y = (C1 * ((uint32_t)i)) >> n;
    y = B1 - (((uint32_t)i * y) >> r);
    y = (uint32_t)i * (y >> n);
    y = (uint32_t)i * (y >> n);
    y = A1 - (y >> (p - q));
    y = (uint32_t)i * (y >> n);
    y = (y + (1UL << (q - a - 1))) >> (q - a); // Rounding
    q15_t out = y<<3;
    return c ? -out : out;
}
#define cos_q15(i) sin_q15((int16_t)(((uint16_t)(i)) + 8192U))

#ifdef   __cplusplus
}
#endif

#endif // _FIXED_POINT_Q15_H_