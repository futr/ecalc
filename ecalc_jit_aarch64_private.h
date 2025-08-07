#ifndef ECALC_JIT_H_AARCH64_PRIVATE
#define ECALC_JIT_H_AARCH64_PRIVATE

#include "ecalc.h"
#include "ecalc_jit_aarch64.h"
#include <stdint.h>
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/mman.h>
#endif

#ifdef __APPLE__
#include <libkern/OSCacheControl.h>
#endif

typedef enum ECALC_ARM64_XREG_tag {
    A64_X0 = 0, A64_X1, A64_X2, A64_X3,
    A64_X4, A64_X5, A64_X6, A64_X7,
    A64_XR  = 8,
    A64_X9, A64_X10, A64_X11, A64_X12,
    A64_X13, A64_X14, A64_X15,
    A64_IP0 = 16,
    A64_IP1 = 17,
    A64_PR  = 18,
    A64_FP  = 29,
    A64_LR  = 30,
    A64_SP  = 31,
    A64_XZR = 31,
} ECALC_ARM64_XREG;

typedef enum ECALC_ARM64_DFPREG_tag {
    A64_D0 = 0,
    A64_D1,
    A64_D2,
} ECALC_ARM64_DFPREG;

typedef enum ECALC_ARM64_HW_tag {
    A64_HW_0  = 0,
    A64_HW_16 = 1,
    A64_HW_32 = 2,
    A64_HW_48 = 3,
} ECALC_ARM64_HW;

typedef enum ECALC_ARM64_SHIFT_tag {
    A64_SH_LSL = 0,
    A64_SH_LSR = 1,
    A64_SH_ASL = 2,
} ECALC_ARM64_SHIFT;

typedef enum ECALC_ARM64_COND_tag {
    A64_COND_EQ =  0,
    A64_COND_NE =  1,
    A64_COND_GE = 10,
    A64_COND_LT = 11,
    A64_COND_GT = 12,
    A64_COND_LE = 13,
} ECALC_ARM64_COND;

#ifdef __cplusplus
extern "C" {
#endif

// static
static size_t ecalc_get_jit_tree_size( ECALC_JIT_TREE *tree, struct ECALC_TOKEN *token );

static void ecalc_bin_printer( ECALC_JIT_TREE *tree, struct ECALC_TOKEN *token );
static void ecalc_bin_printer_opening( ECALC_JIT_TREE *tree );
static void ecalc_bin_printer_closing( ECALC_JIT_TREE *tree );
static void ecalc_bin_printer_tree( ECALC_JIT_TREE *tree, struct ECALC_TOKEN *token );

// アセンブラに対応するレベルの関数
static void ecalc_bin_printer_add_sp_uimm12(ECALC_JIT_TREE *tree, uint16_t val);
static void ecalc_bin_printer_sub_sp_uimm12(ECALC_JIT_TREE *tree, uint16_t val);
static void ecalc_bin_printer_add_Xreg_Xreg_uimm12( ECALC_JIT_TREE *tree, uint8_t rd, uint8_t rn, uint16_t val, uint8_t shift12 );
static void ecalc_bin_printer_sub_Xreg_Xreg_uimm12( ECALC_JIT_TREE *tree, uint8_t rd, uint8_t rn, uint16_t val, uint8_t shift12 );

static void ecalc_bin_printer_subs_shifted_Xreg( ECALC_JIT_TREE *tree, uint8_t rd, uint8_t rn, uint8_t rm, uint8_t shift_mode, uint8_t shift );
static void ecalc_bin_printer_cmp( ECALC_JIT_TREE *tree, uint8_t rn, uint8_t rm );
static void ecalc_bin_printer_fcmp_Dreg( ECALC_JIT_TREE *tree, uint8_t dn, uint8_t dm );
static void ecalc_bin_printer_fcmp_Dreg_zero( ECALC_JIT_TREE *tree, uint8_t dn );
static size_t ecalc_bin_printer_b_cond( ECALC_JIT_TREE *tree, uint64_t write_pos, uint8_t cond, int32_t label );
static size_t ecalc_bin_printer_b( ECALC_JIT_TREE *tree, uint64_t write_pos, int32_t label );
static void ecalc_bin_printer_set_b_cond_label( ECALC_JIT_TREE *tree, uint64_t pos, int32_t label );
static void ecalc_bin_printer_set_b_label( ECALC_JIT_TREE *tree, uint64_t pos, int32_t label );
static void ecalc_bin_printer_blr( ECALC_JIT_TREE *tree, uint8_t reg );

static void ecalc_bin_printer_mov_fp_sp(ECALC_JIT_TREE *tree);
static void ecalc_bin_printer_clear_D0(ECALC_JIT_TREE *tree);
static void ecalc_bin_printer_fcvtzs_Xreg( ECALC_JIT_TREE *tree, uint8_t rd, uint8_t rn );
static void ecalc_bin_printer_fadd_Dreg( ECALC_JIT_TREE *tree, uint8_t rd, uint8_t rn, uint8_t rm );
static void ecalc_bin_printer_fsub_Dreg( ECALC_JIT_TREE *tree, uint8_t rd, uint8_t rn, uint8_t rm );
static void ecalc_bin_printer_fmul_Dreg( ECALC_JIT_TREE *tree, uint8_t rd, uint8_t rn, uint8_t rm );
static void ecalc_bin_printer_fdiv_Dreg( ECALC_JIT_TREE *tree, uint8_t rd, uint8_t rn, uint8_t rm );
static void ecalc_bin_printer_fneg_Dreg( ECALC_JIT_TREE *tree, uint8_t rd, uint8_t rn );
static void ecalc_bin_printer_fsqrt_Dreg( ECALC_JIT_TREE *tree, uint8_t rd, uint8_t rn );
static void ecalc_bin_printer_fmov_Dreg( ECALC_JIT_TREE *tree, uint8_t rd, uint8_t rn );
static void ecalc_bin_printer_fmov_Dreg_Xreg( ECALC_JIT_TREE *tree, uint8_t rd, uint8_t rn );
static void ecalc_bin_printer_fmov_Xreg_Dreg( ECALC_JIT_TREE *tree, uint8_t rd, uint8_t rn );

static void ecalc_bin_printer_movz_Xreg_uimm16( ECALC_JIT_TREE *tree, uint8_t reg, uint16_t val, uint8_t hw );
static void ecalc_bin_printer_movk_Xreg_uimm16( ECALC_JIT_TREE *tree, uint8_t reg, uint16_t val, uint8_t hw );

static void ecalc_bin_printer_str_DFPreg_on_Xreg_offset_uimm12( ECALC_JIT_TREE *tree, uint8_t rt, uint8_t rbase, uint16_t offset );
static void ecalc_bin_printer_ldr_DFPreg_on_Xreg_offset_uimm12( ECALC_JIT_TREE *tree, uint8_t rt, uint8_t rbase, uint16_t offset );

static void ecalc_bin_printer_str_Xreg_on_Xreg_offset_uimm12( ECALC_JIT_TREE *tree, uint8_t rt, uint8_t rbase, uint16_t offset );
static void ecalc_bin_printer_ldr_Xreg_on_Xreg_offset_uimm12( ECALC_JIT_TREE *tree, uint8_t rt, uint8_t rbase, uint16_t offset );

static void ecalc_bin_printer_stp_Xreg_on_Xreg_pre_index( ECALC_JIT_TREE *tree, uint8_t rt, uint8_t rt2, uint8_t rbase, int8_t offset );
static void ecalc_bin_printer_ldp_Xreg_on_Xreg_post_index( ECALC_JIT_TREE *tree, uint8_t rt, uint8_t rt2, uint8_t rbase, int8_t offset );

static void ecalc_bin_printer_load_var_ptr_to_Xreg( ECALC_JIT_TREE *tree, uint8_t reg, uint16_t offset );
static void ecalc_bin_printer_store_double_val_on_Xreg( ECALC_JIT_TREE *tree, uint8_t rbuf, uint8_t rbase, uint16_t offset, double val );
static void ecalc_bin_printer_mov_u64_val_to_Xreg( ECALC_JIT_TREE *tree, uint8_t reg, uint64_t val );
static void ecalc_bin_printer_mov_double_val_to_Xreg( ECALC_JIT_TREE *tree, uint8_t reg, double val );
static void ecalc_bin_printer_load_arg_val_to_Xreg( ECALC_JIT_TREE *tree, uint8_t reg, uint16_t offset );
static void ecalc_bin_printer_load_exp_var_ptr_to_Xreg( ECALC_JIT_TREE *tree, uint8_t reg, uint16_t index );
static void ecalc_bin_printer_load_exp_ans_ptr_to_Xreg( ECALC_JIT_TREE *tree, uint8_t reg );

static void ecalc_bin_printer_load_function_ptr_to_Xreg( ECALC_JIT_TREE *tree, uint8_t reg, void ( *func )( void ) );
static void ecalc_bin_printer_call_on_Xreg( ECALC_JIT_TREE *tree, uint8_t reg );

#ifdef __cplusplus
}
#endif

#endif // End of ECALC_JIT_H_AARCH64_PRIVATE
