#ifndef ECALC_JIT_H
#define ECALC_JIT_H

#include "ecalc.h"
#include <stdint.h>

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

// private
// メモリー関数
void *ecalc_allocate_jit_memory( size_t size );
void ecalc_free_jit_memory( void *data, size_t size );

// バイト列書き込み補助
void ecalc_bin_printer_print(ECALC_JIT_TREE *tree, unsigned char *buf, size_t size );
size_t ecalc_bin_printer_get_pos( ECALC_JIT_TREE *tree );
void ecalc_bin_printer_reset_tree( ECALC_JIT_TREE *tree );
void ecalc_bin_printer_set_address( ECALC_JIT_TREE *tree, size_t pos, int32_t address );

#ifdef __cplusplus
}
#endif

#endif // End of ECALC_JIT_H
