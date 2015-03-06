#ifndef ECALC_JIT_H
#define ECALC_JIT_H

#include "ecalc.h"
#include <stdint.h>

/*
 * 関数ポインタ取得
 * 関数コール引数1,2
 *
 *
 */

#ifdef __cplusplus
extern "C" {
#endif

// JITエンジン用のデータ格納
typedef struct {
	size_t size;
	size_t pos;
	unsigned char *data;
} ECALC_JIT_TREE;

// public
ECALC_JIT_TREE *ecalc_create_jit_tree( struct ECALC_TOKEN *token );
void ecalc_free_jit_tree( ECALC_JIT_TREE *tree );
double ecalc_get_jit_tree_value( ECALC_JIT_TREE *tree, double **vars, double ans );

// static
void *ecalc_allocate_jit_memory( size_t size );
void ecalc_free_jit_memory( void *data );
size_t ecalc_get_jit_tree_size( ECALC_JIT_TREE *tree, struct ECALC_TOKEN *token );
void ecalc_set_jit_function_pointers( ECALC_JIT_TREE *tree );

void ecalc_bin_printer( ECALC_JIT_TREE *tree, struct ECALC_TOKEN *token );

void ecalc_bin_printer_opening( ECALC_JIT_TREE *tree );
void ecalc_bin_printer_closing( ECALC_JIT_TREE *tree );
void ecalc_bin_printer_tree( ECALC_JIT_TREE *tree, struct ECALC_TOKEN *token );
void ecalc_bin_printer_operator( ECALC_JIT_TREE *tree, struct ECALC_TOKEN *token );
void ecalc_bin_printer_function_call( ECALC_JIT_TREE *tree, struct ECALC_TOKEN *token );

// バイト列書き込み補助
void ecalc_bin_printer_print(ECALC_JIT_TREE *tree, unsigned char *buf, size_t size );
size_t ecalc_bin_printer_get_pos( ECALC_JIT_TREE *tree );
void ecalc_bin_printer_reset_tree( ECALC_JIT_TREE *tree );
void ecalc_bin_printer_set_address( ECALC_JIT_TREE *tree, size_t pos, int32_t address );

// アセンブラに対応するレベルの関数
void ecalc_bin_printer_load_arg_ptr_to_eax( ECALC_JIT_TREE *tree, int8_t val );
void ecalc_bin_printer_load_var_ptr_to_eax( ECALC_JIT_TREE *tree, int8_t val );
void ecalc_bin_printer_load_eax_pointed_to_eax( ECALC_JIT_TREE *tree, int8_t val );
void ecalc_bin_printer_load_exp_ans_ptr_to_eax( ECALC_JIT_TREE *tree );
void ecalc_bin_printer_load_exp_var_ptr_to_eax( ECALC_JIT_TREE *tree, int index );
void ecalc_bin_printer_load_function_ptr_to_eax(ECALC_JIT_TREE *tree, void (*func)( void ) );
void ecalc_bin_printer_load_val_to_ebx( ECALC_JIT_TREE *tree, int32_t val );
void ecalc_bin_printer_add_eax_i8( ECALC_JIT_TREE *tree, int8_t val );
void ecalc_bin_printer_push_dword_val( ECALC_JIT_TREE *tree, uint32_t val );
void ecalc_bin_printer_store_dword_val( ECALC_JIT_TREE *tree, int8_t pos, uint32_t val );
void ecalc_bin_printer_store_double_val( ECALC_JIT_TREE *tree, int8_t pos, double val );
void ecalc_bin_printer_push_dword( ECALC_JIT_TREE *tree, int8_t val );
void ecalc_bin_printer_push_qword( ECALC_JIT_TREE *tree, int8_t val );
void ecalc_bin_printer_pop_dword( ECALC_JIT_TREE *tree, int8_t val );
void ecalc_bin_printer_pop_qword( ECALC_JIT_TREE *tree, int8_t val );
void ecalc_bin_printer_fld( ECALC_JIT_TREE *tree, int8_t val );
void ecalc_bin_printer_fstp( ECALC_JIT_TREE *tree, int8_t val );
void ecalc_bin_printer_fstp_st0( ECALC_JIT_TREE *tree );
void ecalc_bin_printer_fldz( ECALC_JIT_TREE *tree );
void ecalc_bin_printer_fld1( ECALC_JIT_TREE *tree );
void ecalc_bin_printer_fldpi( ECALC_JIT_TREE *tree );
void ecalc_bin_printer_faddp( ECALC_JIT_TREE *tree );
void ecalc_bin_printer_fadd(ECALC_JIT_TREE *tree , uint8_t st);
void ecalc_bin_printer_fsubp( ECALC_JIT_TREE *tree );
void ecalc_bin_printer_fmulp( ECALC_JIT_TREE *tree );
void ecalc_bin_printer_fdivp( ECALC_JIT_TREE *tree );
void ecalc_bin_printer_fabs( ECALC_JIT_TREE *tree );
void ecalc_bin_printer_fsin( ECALC_JIT_TREE *tree );
void ecalc_bin_printer_fsqrt( ECALC_JIT_TREE *tree );
void ecalc_bin_printer_fcos( ECALC_JIT_TREE *tree );
void ecalc_bin_printer_fxch_st1( ECALC_JIT_TREE *tree );
void ecalc_bin_printer_fchs( ECALC_JIT_TREE *tree );
void ecalc_bin_printer_fcomi( ECALC_JIT_TREE *tree, uint8_t st );
void ecalc_bin_printer_mov_eax_ecx( ECALC_JIT_TREE *tree );
void ecalc_bin_printer_mov_ecx_eax( ECALC_JIT_TREE *tree );
void ecalc_bin_printer_mov_eax_edx( ECALC_JIT_TREE *tree );
void ecalc_bin_printer_mov_edx_eax( ECALC_JIT_TREE *tree );
void ecalc_bin_printer_call( ECALC_JIT_TREE *tree );
size_t ecalc_bin_printer_je( ECALC_JIT_TREE *tree, int32_t val );
size_t ecalc_bin_printer_jb( ECALC_JIT_TREE *tree, int32_t val );
size_t ecalc_bin_printer_jmp( ECALC_JIT_TREE *tree, int32_t val );
void ecalc_bin_printer_add_esp_i8( ECALC_JIT_TREE *tree, int8_t val );

#ifdef __cplusplus
}
#endif

#endif // End of ECALC_JIT_H
