#ifndef ECALC_JIT_H_64_PRIVATE
#define ECALC_JIT_H_64_PRIVATE

#include "ecalc.h"
#include "ecalc_jit_64.h"
#include <stdint.h>
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/mman.h>
#endif

/*
 * on_rax : わかりにくいですが、「raxが指す場所に」の意味です。
 */

#ifdef __cplusplus
extern "C" {
#endif

// static
static size_t ecalc_get_jit_tree_size( ECALC_JIT_TREE *tree, struct ECALC_TOKEN *token );

static void ecalc_bin_printer( ECALC_JIT_TREE *tree, struct ECALC_TOKEN *token );
static void ecalc_bin_printer_opening_amd64( ECALC_JIT_TREE *tree );
static void ecalc_bin_printer_closing_amd64( ECALC_JIT_TREE *tree );
static void ecalc_bin_printer_tree_64( ECALC_JIT_TREE *tree, struct ECALC_TOKEN *token );

// アセンブラに対応するレベルの関数
static void ecalc_bin_printer_load_var_ptr_to_rax( ECALC_JIT_TREE *tree, int8_t val );
static void ecalc_bin_printer_clear_xmm0( ECALC_JIT_TREE *tree );
static void ecalc_bin_printer_clear_rcx( ECALC_JIT_TREE *tree );
static void ecalc_bin_printer_inc_rcx( ECALC_JIT_TREE *tree );

static void ecalc_bin_printer_push_rcx( ECALC_JIT_TREE *tree );
static void ecalc_bin_printer_push_rdx( ECALC_JIT_TREE *tree );
static void ecalc_bin_printer_pop_rcx( ECALC_JIT_TREE *tree );
static void ecalc_bin_printer_pop_rdx( ECALC_JIT_TREE *tree );

static void ecalc_bin_printer_cmp_rcx_rdx( ECALC_JIT_TREE *tree );
static void ecalc_bin_printer_ucomisd_xmm0_xmm1( ECALC_JIT_TREE *tree );
static void ecalc_bin_printer_addsd_xmm0_xmm1( ECALC_JIT_TREE *tree );
static void ecalc_bin_printer_subsd_xmm0_xmm1( ECALC_JIT_TREE *tree );
static void ecalc_bin_printer_mulsd_xmm0_xmm1( ECALC_JIT_TREE *tree );
static void ecalc_bin_printer_divsd_xmm0_xmm1( ECALC_JIT_TREE *tree );
static void ecalc_bin_printer_sqrtsd_xmm0_xmm1( ECALC_JIT_TREE *tree );

static void ecalc_bin_printer_load_double_val_on_rax_to_xmm0( ECALC_JIT_TREE *tree, int8_t pos );
static void ecalc_bin_printer_load_double_val_on_rax_to_xmm1( ECALC_JIT_TREE *tree, int8_t pos );
static void ecalc_bin_printer_store_double_val_xmm0_on_rax( ECALC_JIT_TREE *tree, int8_t pos );
static void ecalc_bin_printer_store_double_val_xmm1_on_rax( ECALC_JIT_TREE *tree, int8_t pos );
static void ecalc_bin_printer_store_double_val_on_rax( ECALC_JIT_TREE *tree, int8_t pos, double val );
static void ecalc_bin_printer_store_u64_val_to_rdx( ECALC_JIT_TREE *tree, uint64_t val );
static void ecalc_bin_printer_store_u64_val_to_rax( ECALC_JIT_TREE *tree, uint64_t val );
static void ecalc_bin_printer_store_rdx_on_rax( ECALC_JIT_TREE *tree, int8_t pos );
static void ecalc_bin_printer_load_function_ptr_to_rax( ECALC_JIT_TREE *tree, void ( *func )( void ) );
static void ecalc_bin_printer_call_on_rax( ECALC_JIT_TREE *tree );
static void ecalc_bin_printer_load_exp_var_ptr_to_rax( ECALC_JIT_TREE *tree, int index );
static void ecalc_bin_printer_load_exp_ans_ptr_to_rax(ECALC_JIT_TREE *tree);
static void ecalc_bin_printer_load_arg_ptr_to_rax( ECALC_JIT_TREE *tree, int8_t val );
static void ecalc_bin_printer_load_val_to_rdx( ECALC_JIT_TREE *tree, int32_t val );

static void ecalc_bin_printer_mov_double_xmm1_to_xmm0( ECALC_JIT_TREE *tree );
static void ecalc_bin_printer_convert_double_xmm0_to_int_rdx( ECALC_JIT_TREE *tree );
static void ecalc_bin_printer_load_rax_pointed_to_rax( ECALC_JIT_TREE *tree, int8_t val );
static void ecalc_bin_printer_add_rsp_i8(ECALC_JIT_TREE *tree, int8_t val);

static size_t ecalc_bin_printer_je( ECALC_JIT_TREE *tree, int32_t val );
static size_t ecalc_bin_printer_jb( ECALC_JIT_TREE *tree, int32_t val );
static size_t ecalc_bin_printer_jmp( ECALC_JIT_TREE *tree, int32_t val );

#ifdef __cplusplus
}
#endif

#endif // End of ECALC_JIT_H_64_PRIVATE
