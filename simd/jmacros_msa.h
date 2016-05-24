/*
 * MIPS MSA optimizations for libjpeg-turbo
 *
 * Copyright (C) 2016-2017, Imagination Technologies.
 * All rights reserved.
 * Authors: Vikram Dattu (vikram.dattu@imgtec.com)
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 */

#ifndef __JMACROS_MSA_H__
#define __JMACROS_MSA_H__

#include <stdint.h>
#include <msa.h>

#ifdef CLANG_BUILD
  #define MSA_SLLI_W(a, b)   __msa_slli_w((v4i32) a, b)
#else
  #define MSA_SLLI_W(a, b)   (a << b)
#endif

#define LD_H(RTYPE, psrc) *((RTYPE *)(psrc))
#define LD_SH(...) LD_H(v8i16, __VA_ARGS__)

#if (__mips_isa_rev >= 6)
  #define SW(val, pdst) {                                 \
    unsigned char *pdst_sw_m = (unsigned char *) (pdst);  \
    unsigned int val_m = (val);                           \
                                                          \
    asm volatile (                                        \
        "sw  %[val_m],  %[pdst_sw_m]  \n\t"               \
                                                          \
        : [pdst_sw_m] "=m" (*pdst_sw_m)                   \
        : [val_m] "r" (val_m)                             \
    );                                                    \
  }

  #define SD(val, pdst) {                                 \
    unsigned char *pdst_sd_m = (unsigned char *) (pdst);  \
    unsigned long long val_m = (val);                     \
                                                          \
    asm volatile (                                        \
        "sd  %[val_m],  %[pdst_sd_m]  \n\t"               \
                                                          \
        : [pdst_sd_m] "=m" (*pdst_sd_m)                   \
        : [val_m] "r" (val_m)                             \
    );                                                    \
  }


#else  // !(__mips_isa_rev >= 6)
  #define SW(val, pdst) {                                 \
    unsigned char *pdst_sw_m = (unsigned char *) (pdst);  \
    unsigned int val_m = (val);                           \
                                                          \
    asm volatile (                                        \
        "usw  %[val_m],  %[pdst_sw_m]  \n\t"              \
                                                          \
        : [pdst_sw_m] "=m" (*pdst_sw_m)                   \
        : [val_m] "r" (val_m)                             \
    );                                                    \
  }

  #define SD(val, pdst) {                                          \
    unsigned char *pdst_sd_m = (unsigned char *) (pdst);           \
    unsigned int val0_m, val1_m;                                   \
                                                                   \
    val0_m = (unsigned int) ((val) & 0x00000000FFFFFFFF);          \
    val1_m = (unsigned int) (((val) >> 32) & 0x00000000FFFFFFFF);  \
                                                                   \
    SW(val0_m, pdst_sd_m);                                         \
    SW(val1_m, pdst_sd_m + 4);                                     \
  }
#endif  // (__mips_isa_rev >= 6)

/* Description : Load vectors with 8 halfword elements with stride
   Arguments   : Inputs  - psrc, stride
                 Outputs - out0, out1
   Details     : Load 8 halfword elements in 'out0' from (psrc)
                 Load 8 halfword elements in 'out1' from (psrc + stride)
*/
#define LD_H2(RTYPE, psrc, stride, out0, out1) {  \
  out0 = LD_H(RTYPE, (psrc));                     \
  out1 = LD_H(RTYPE, (psrc) + (stride));          \
}

#define LD_H4(RTYPE, psrc, stride, out0, out1, out2, out3) {  \
  LD_H2(RTYPE, (psrc), stride, out0, out1);                   \
  LD_H2(RTYPE, (psrc) + 2 * stride, stride, out2, out3);      \
}

#define LD_H6(RTYPE, psrc, stride, out0, out1, out2, out3, out4, out5) {  \
  LD_H4(RTYPE, (psrc), stride, out0, out1, out2, out3);                   \
  LD_H2(RTYPE, (psrc) + 4 * stride, stride, out4, out5);                  \
}
#define LD_SH6(...) LD_H6(v8i16, __VA_ARGS__)

#define LD_H8(RTYPE, psrc, stride,                                    \
              out0, out1, out2, out3, out4, out5, out6, out7) {       \
  LD_H4(RTYPE, (psrc), stride, out0, out1, out2, out3);               \
  LD_H4(RTYPE, (psrc) + 4 * stride, stride, out4, out5, out6, out7);  \
}
#define LD_SH8(...) LD_H8(v8i16, __VA_ARGS__)

/* Description : Store 8x1 byte block to destination memory from input vector
   Arguments   : Inputs - in, pdst
   Details     : Index 0 double word element from 'in' vector is copied to the
                 GP register and stored to (pdst)
*/
#define ST8x1_UB(in, pdst) {               \
  unsigned long long out0_m;               \
  out0_m = __msa_copy_u_d((v2i64) in, 0);  \
  SD(out0_m, pdst);                        \
}

/* Description : Immediate number of elements to slide with zero
   Arguments   : Inputs  - in0, in1, slide_val
                 Outputs - out0, out1
                 Return Type - as per RTYPE
   Details     : Byte elements from 'zero_m' vector are slid into 'in0' by
                 value specified in the 'slide_val'
*/
#define SLDI_B2_0(RTYPE, in0, in1, out0, out1, slide_val) {             \
  v16i8 zero_m = { 0 };                                                 \
  out0 = (RTYPE) __msa_sldi_b((v16i8) zero_m, (v16i8) in0, slide_val);  \
  out1 = (RTYPE) __msa_sldi_b((v16i8) zero_m, (v16i8) in1, slide_val);  \
}

/* Description : Clips all signed halfword elements of input vector
                 between 0 & 255
   Arguments   : Input  - in
                 Output - out_m
                 Return Type - signed halfword
*/
#define CLIP_SH_0_255(in) ( {                           \
  v8i16 max_m = __msa_ldi_h(255);                       \
  v8i16 out_m;                                          \
                                                        \
  out_m = __msa_maxi_s_h((v8i16) in, 0);                \
  out_m = __msa_min_s_h((v8i16) max_m, (v8i16) out_m);  \
  out_m;                                                \
} )

/* Description : Interleave right half of byte elements from vectors
   Arguments   : Inputs  - in0, in1, in2, in3
                 Outputs - out0, out1
                 Return Type - as per RTYPE
   Details     : Right half of byte elements of 'in0' and 'in1' are interleaved
                 and written to out0.
*/
#define ILVR_B2(RTYPE, in0, in1, in2, in3, out0, out1) {  \
  out0 = (RTYPE) __msa_ilvr_b((v16i8) in0, (v16i8) in1);  \
  out1 = (RTYPE) __msa_ilvr_b((v16i8) in2, (v16i8) in3);  \
}
#define ILVR_B4(RTYPE, in0, in1, in2, in3, in4, in5, in6, in7,  \
                out0, out1, out2, out3) {                       \
  ILVR_B2(RTYPE, in0, in1, in2, in3, out0, out1);               \
  ILVR_B2(RTYPE, in4, in5, in6, in7, out2, out3);               \
}
#define ILVR_B4_SB(...) ILVR_B4(v16i8, __VA_ARGS__)

/* Description : Interleave both left and right half of input vectors
   Arguments   : Inputs  - in0, in1
                 Outputs - out0, out1
                 Return Type - as per RTYPE
   Details     : Right half of byte elements from 'in0' and 'in1' are
                 interleaved and written to 'out0'
*/
#define ILVRL_B2(RTYPE, in0, in1, out0, out1) {           \
  out0 = (RTYPE) __msa_ilvr_b((v16i8) in0, (v16i8) in1);  \
  out1 = (RTYPE) __msa_ilvl_b((v16i8) in0, (v16i8) in1);  \
}
#define ILVRL_B2_SB(...) ILVRL_B2(v16i8, __VA_ARGS__)

#define ILVRL_H2(RTYPE, in0, in1, out0, out1) {           \
  out0 = (RTYPE) __msa_ilvr_h((v8i16) in0, (v8i16) in1);  \
  out1 = (RTYPE) __msa_ilvl_h((v8i16) in0, (v8i16) in1);  \
}
#define ILVRL_H2_SW(...) ILVRL_H2(v4i32, __VA_ARGS__)

#define ILVRL_W2(RTYPE, in0, in1, out0, out1) {           \
  out0 = (RTYPE) __msa_ilvr_w((v4i32) in0, (v4i32) in1);  \
  out1 = (RTYPE) __msa_ilvl_w((v4i32) in0, (v4i32) in1);  \
}
#define ILVRL_W2_SW(...) ILVRL_W2(v4i32, __VA_ARGS__)

/* Description : Indexed word element values are replicated to all
                 elements in output vector
   Arguments   : Inputs  - in, stidx
                 Outputs - out0, out1
                 Return Type - as per RTYPE
   Details     : 'stidx' element value from 'in' vector is replicated to all
                 elements in 'out0' vector
                 'stidx + 1' element value from 'in' vector is replicated to all
                 elements in 'out1' vector
                 Valid index range for word operation is 0-3
*/
#define SPLATI_W2(RTYPE, in, stidx, out0, out1) {        \
  out0 = (RTYPE) __msa_splati_w((v4i32) in, stidx);      \
  out1 = (RTYPE) __msa_splati_w((v4i32) in, (stidx+1));  \
}
#define SPLATI_W2_SW(...) SPLATI_W2(v4i32, __VA_ARGS__)

#define SPLATI_W4(RTYPE, in, out0, out1, out2, out3) {  \
  SPLATI_W2(RTYPE, in, 0, out0, out1);                  \
  SPLATI_W2(RTYPE, in, 2, out2, out3);                  \
}
#define SPLATI_W4_SW(...) SPLATI_W4(v4i32, __VA_ARGS__)

/* Description : Shift left all elements of word vector
   Arguments   : Inputs  - in0, in1, in2, in3, shift
                 Outputs - in place operation
                 Return Type - as per input vector RTYPE
   Details     : Each element of vector 'in0' is left shifted by 'shift' and
                 the result is written in-place.
*/
#define SLLI_W4(RTYPE, in0, in1, in2, in3, shift_val) {  \
  in0 = (RTYPE) MSA_SLLI_W(in0, shift_val);              \
  in1 = (RTYPE) MSA_SLLI_W(in1, shift_val);              \
  in2 = (RTYPE) MSA_SLLI_W(in2, shift_val);              \
  in3 = (RTYPE) MSA_SLLI_W(in3, shift_val);              \
}
#define SLLI_W4_SW(...) SLLI_W4(v4i32, __VA_ARGS__)

/* Description : Shift right arithmetic rounded (immediate)
   Arguments   : Inputs  - in0, in1, shift
                 Outputs - in place operation
                 Return Type - as per RTYPE
   Details     : Each element of vector 'in0' is shifted right arithmetically by
                 the value in 'shift'. The last discarded bit is added to the
                 shifted value for rounding and the result is written in-place.
                 'shift' is an immediate value.
*/
#define SRARI_W2(RTYPE, in0, in1, shift) {          \
  in0 = (RTYPE) __msa_srari_w((v4i32) in0, shift);  \
  in1 = (RTYPE) __msa_srari_w((v4i32) in1, shift);  \
}
#define SRARI_W2_SW(...) SRARI_W2(v4i32, __VA_ARGS__)

/* Description : Multiplication of pairs of vectors
   Arguments   : Inputs  - in0, in1, in2, in3
                 Outputs - out0, out1
   Details     : Each element from 'in0' is multiplied with elements from 'in1'
                 and the result is written to 'out0'
*/
#define MUL2(in0, in1, in2, in3, out0, out1) {  \
  out0 = in0 * in1;                             \
  out1 = in2 * in3;                             \
}
#define MUL4(in0, in1, in2, in3, in4, in5, in6, in7,  \
             out0, out1, out2, out3) {                \
  MUL2(in0, in1, in2, in3, out0, out1);               \
  MUL2(in4, in5, in6, in7, out2, out3);               \
}

/* Description : Addition of 2 pairs of vectors
   Arguments   : Inputs  - in0, in1, in2, in3
                 Outputs - out0, out1
   Details     : Each element in 'in0' is added to 'in1' and result is written
                 to 'out0'.
*/
#define ADD2(in0, in1, in2, in3, out0, out1) {  \
  out0 = in0 + in1;                             \
  out1 = in2 + in3;                             \
}
#define ADD4(in0, in1, in2, in3, in4, in5, in6, in7,  \
             out0, out1, out2, out3) {                \
  ADD2(in0, in1, in2, in3, out0, out1);               \
  ADD2(in4, in5, in6, in7, out2, out3);               \
}

/* Description : Subtraction of 2 pairs of vectors
   Arguments   : Inputs  - in0, in1, in2, in3
                 Outputs - out0, out1
   Details     : Each element in 'in1' is subtracted from 'in0' and result is
                 written to 'out0'.
*/
#define SUB2(in0, in1, in2, in3, out0, out1) {  \
  out0 = in0 - in1;                             \
  out1 = in2 - in3;                             \
}

/* Description : Sign extend halfword elements from input vector and return
                 the result in pair of vectors
   Arguments   : Input   - in            (halfword vector)
                 Outputs - out0, out1   (sign extended word vectors)
                 Return Type - signed word
   Details     : Sign bit of halfword elements from input vector 'in' is
                 extracted and interleaved right with same vector 'in0' to
                 generate 4 signed word elements in 'out0'
                 Then interleaved left with same vector 'in0' to
                 generate 4 signed word elements in 'out1'
*/
#define UNPCK_SH_SW(in, out0, out1) {     \
  v8i16 tmp_m;                            \
                                          \
  tmp_m = __msa_clti_s_h((v8i16) in, 0);  \
  ILVRL_H2_SW(tmp_m, in, out0, out1);     \
}

/* Description : Butterfly of 4 input vectors
   Arguments   : Inputs  - in0, in1, in2, in3
                 Outputs - out0, out1, out2, out3
   Details     : Butterfly operation
*/
#define BUTTERFLY_4(in0, in1, in2, in3, out0, out1, out2, out3) {  \
  out0 = in0 + in3;                                                \
  out1 = in1 + in2;                                                \
                                                                   \
  out2 = in1 - in2;                                                \
  out3 = in0 - in3;                                                \
}

/* Description : Butterfly of 8 input vectors
   Arguments   : Inputs  - in0 ...  in7
                 Outputs - out0 .. out7
   Details     : Butterfly operation
*/
#define BUTTERFLY_8(in0, in1, in2, in3, in4, in5, in6, in7,            \
                    out0, out1, out2, out3, out4, out5, out6, out7) {  \
  out0 = in0 + in7;                                                    \
  out1 = in1 + in6;                                                    \
  out2 = in2 + in5;                                                    \
  out3 = in3 + in4;                                                    \
                                                                       \
  out4 = in3 - in4;                                                    \
  out5 = in2 - in5;                                                    \
  out6 = in1 - in6;                                                    \
  out7 = in0 - in7;                                                    \
}

/* Description : Transpose input 8x8 byte block
   Arguments   : Inputs  - in0, in1, in2, in3, in4, in5, in6, in7
                 Outputs - out0, out1, out2, out3, out4, out5, out6, out7
                 Return Type - as per RTYPE
*/
#define TRANSPOSE8x8_UB(RTYPE, in0, in1, in2, in3, in4, in5, in6, in7,     \
                        out0, out1, out2, out3, out4, out5, out6, out7) {  \
  v16i8 tmp0_m, tmp1_m, tmp2_m, tmp3_m;                                    \
  v16i8 tmp4_m, tmp5_m, tmp6_m, tmp7_m;                                    \
                                                                           \
  ILVR_B4_SB(in2, in0, in3, in1, in6, in4, in7, in5,                       \
             tmp0_m, tmp1_m, tmp2_m, tmp3_m);                              \
  ILVRL_B2_SB(tmp1_m, tmp0_m, tmp4_m, tmp5_m);                             \
  ILVRL_B2_SB(tmp3_m, tmp2_m, tmp6_m, tmp7_m);                             \
  ILVRL_W2(RTYPE, tmp6_m, tmp4_m, out0, out2);                             \
  ILVRL_W2(RTYPE, tmp7_m, tmp5_m, out4, out6);                             \
  SLDI_B2_0(RTYPE, out0, out2, out1, out3, 8);                             \
  SLDI_B2_0(RTYPE, out4, out6, out5, out7, 8);                             \
}
#define TRANSPOSE8x8_UB_UB(...) TRANSPOSE8x8_UB(v16u8, __VA_ARGS__)

/* Description : Transpose 4x4 block with word elements in vectors
   Arguments   : Inputs  - in0, in1, in2, in3
                 Outputs - out0, out1, out2, out3
                 Return Type - as per RTYPE
*/
#define TRANSPOSE4x4_W(RTYPE, in0, in1, in2, in3, out0, out1, out2, out3) {  \
  v4i32 s0_m, s1_m, s2_m, s3_m;                                              \
                                                                             \
  ILVRL_W2_SW(in1, in0, s0_m, s1_m);                                         \
  ILVRL_W2_SW(in3, in2, s2_m, s3_m);                                         \
                                                                             \
  out0 = (RTYPE) __msa_ilvr_d((v2i64) s2_m, (v2i64) s0_m);                   \
  out1 = (RTYPE) __msa_ilvl_d((v2i64) s2_m, (v2i64) s0_m);                   \
  out2 = (RTYPE) __msa_ilvr_d((v2i64) s3_m, (v2i64) s1_m);                   \
  out3 = (RTYPE) __msa_ilvl_d((v2i64) s3_m, (v2i64) s1_m);                   \
}
#define TRANSPOSE4x4_SW_SW(...) TRANSPOSE4x4_W(v4i32, __VA_ARGS__)

#endif  /* __JMACROS_MSA_H__ */
