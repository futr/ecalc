#ifndef ECALC_JIT_H_AARCH64
#define ECALC_JIT_H_AARCH64

#include "ecalc.h"
#include "ecalc_jit.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// public
ECALC_JIT_TREE *ecalc_create_jit_tree_aarch64( struct ECALC_TOKEN *token );
void ecalc_free_jit_tree_aarch64( ECALC_JIT_TREE *tree );
double ecalc_get_jit_tree_value_aarch64( ECALC_JIT_TREE *tree, double **vars, double ans );

#ifdef __cplusplus
}
#endif

#endif // End of ECALC_JIT_H_AARCH64
