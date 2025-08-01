#ifndef ECALC_JIT_H_32_PRIVATE
#define ECALC_JIT_H_32_PRIVATE

#include "ecalc.h"
#include "ecalc_jit_32.h"
#include <stdint.h>
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/mman.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

// private
static size_t ecalc_get_jit_tree_size( ECALC_JIT_TREE *tree, struct ECALC_TOKEN *token );

static void ecalc_bin_printer( ECALC_JIT_TREE *tree, struct ECALC_TOKEN *token );
static void ecalc_bin_printer_opening( ECALC_JIT_TREE *tree );
static void ecalc_bin_printer_closing( ECALC_JIT_TREE *tree );
static void ecalc_bin_printer_tree( ECALC_JIT_TREE *tree, struct ECALC_TOKEN *token );

// アセンブラに対応するレベルの関数
static void ecalc_bin_printer_load_arg_ptr_to_eax( ECALC_JIT_TREE *tree, int8_t val );
static void ecalc_bin_printer_load_var_ptr_to_eax( ECALC_JIT_TREE *tree, int8_t val );
static void ecalc_bin_printer_load_eax_pointed_to_eax( ECALC_JIT_TREE *tree, int8_t val );
static void ecalc_bin_printer_load_exp_ans_ptr_to_eax( ECALC_JIT_TREE *tree );
static void ecalc_bin_printer_load_exp_var_ptr_to_eax( ECALC_JIT_TREE *tree, int index );
static void ecalc_bin_printer_load_function_ptr_to_eax(ECALC_JIT_TREE *tree, void (*func)( void ) );
static void ecalc_bin_printer_load_val_to_edx( ECALC_JIT_TREE *tree, int32_t val );
static void ecalc_bin_printer_add_eax_i8( ECALC_JIT_TREE *tree, int8_t val );
static void ecalc_bin_printer_push_dword_val( ECALC_JIT_TREE *tree, uint32_t val );
static void ecalc_bin_printer_store_dword_val( ECALC_JIT_TREE *tree, int8_t pos, uint32_t val );
static void ecalc_bin_printer_store_double_val( ECALC_JIT_TREE *tree, int8_t pos, double val );
static void ecalc_bin_printer_push_dword( ECALC_JIT_TREE *tree, int8_t val );
static void ecalc_bin_printer_push_qword( ECALC_JIT_TREE *tree, int8_t val );
static void ecalc_bin_printer_pop_dword( ECALC_JIT_TREE *tree, int8_t val );
static void ecalc_bin_printer_pop_qword( ECALC_JIT_TREE *tree, int8_t val );
static void ecalc_bin_printer_fld( ECALC_JIT_TREE *tree, int8_t val );
static void ecalc_bin_printer_fstp( ECALC_JIT_TREE *tree, int8_t val );
static void ecalc_bin_printer_fstp_st0( ECALC_JIT_TREE *tree );
static void ecalc_bin_printer_fldz( ECALC_JIT_TREE *tree );
static void ecalc_bin_printer_fld1( ECALC_JIT_TREE *tree );
static void ecalc_bin_printer_fldpi( ECALC_JIT_TREE *tree );
static void ecalc_bin_printer_faddp( ECALC_JIT_TREE *tree );
static void ecalc_bin_printer_fadd(ECALC_JIT_TREE *tree , uint8_t st);
static void ecalc_bin_printer_fsubp( ECALC_JIT_TREE *tree );
static void ecalc_bin_printer_fmulp( ECALC_JIT_TREE *tree );
static void ecalc_bin_printer_fdivp( ECALC_JIT_TREE *tree );
static void ecalc_bin_printer_fabs( ECALC_JIT_TREE *tree );
static void ecalc_bin_printer_fsin( ECALC_JIT_TREE *tree );
static void ecalc_bin_printer_fsqrt( ECALC_JIT_TREE *tree );
static void ecalc_bin_printer_fcos( ECALC_JIT_TREE *tree );
static void ecalc_bin_printer_fxch_st1( ECALC_JIT_TREE *tree );
static void ecalc_bin_printer_fchs( ECALC_JIT_TREE *tree );
static void ecalc_bin_printer_fcomi( ECALC_JIT_TREE *tree, uint8_t st );
static void ecalc_bin_printer_call( ECALC_JIT_TREE *tree );
static size_t ecalc_bin_printer_je( ECALC_JIT_TREE *tree, int32_t val );
static size_t ecalc_bin_printer_jb( ECALC_JIT_TREE *tree, int32_t val );
static size_t ecalc_bin_printer_jmp( ECALC_JIT_TREE *tree, int32_t val );
static void ecalc_bin_printer_add_esp_i8( ECALC_JIT_TREE *tree, int8_t val );

#ifdef __cplusplus
}
#endif

#endif // End of ECALC_JIT_H_32_PRIVATE
