#ifndef ECALC_H_INCLUDED
#define ECALC_H_INCLUDED

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <math.h>
#include <float.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define ECALC_MEMMAN_TOKENS     512                         /* メモリマネージャーで使うトークン数 */

#define ECALC_FUNC_COUNT        10                          /* 関数の数 */
#define ECALC_MAX_TOKEN_LENGTH  64                          /* 最大トークン文字数 */
#define ECALC_VAR_COUNT         26                          /* 最大変数数 */

enum ecalc_error {
    ECALC_ERROR_TOO_LONG,                                   /* 長すぎ */
    ECALC_ERROR_BAD_CHAR                                    /* ありえない文字 */
};

enum ecalc_connector {                                      /* 接続子 ( 演算子 ) */
    ECALC_OPE_ADD,                                          /* 足し算 */
    ECALC_OPE_SUB,                                          /* 引き算 */
    ECALC_OPE_MUL,                                          /* 掛け算 */
    ECALC_OPE_DIV,                                          /* 割り算 */
    ECALC_OPE_MOD,
    ECALC_OPE_STI,                                          /* 代入 */
    ECALC_OPE_SEPA,                                         /* 区切り */
    ECALC_OPE_LOOP,                                         /* 繰り返し */
    ECALC_OPE_LBIG,                                         /* > 左が大きい */
    ECALC_OPE_RBIG,                                         /* < 右が大きい */
    ECALC_OPE_EQU,											/* == 等号 */
    ECALC_OPE_ERR,                                          /* 文法エラー */

    ECALC_FUNC_PI,                                          /* π */
    ECALC_FUNC_EPS0,                                        /* ε0 */
    ECALC_FUNC_ANS,                                         /* 前回の答え */
    ECALC_FUNC_POW,                                         /* 指数 */
    ECALC_FUNC_SQRT,                                        /* ルート */
    ECALC_FUNC_LOG10,                                       /* 対数10 */
    ECALC_FUNC_LOGN,                                        /* 対数自然 */
    ECALC_FUNC_RAD,                                         /* 度をラジアンに */
    ECALC_FUNC_DEG,                                         /* ラジアンを度に */
    ECALC_FUNC_SIN,                                         /* sin */
    ECALC_FUNC_COS,                                         /* cos */
    ECALC_FUNC_TAN,                                         /* tan */
    ECALC_FUNC_ASIN,                                        /* asin */
    ECALC_FUNC_ACOS,                                        /* acos */
    ECALC_FUNC_ATAN,                                        /* atan */
    ECALC_FUNC_ATAN2,                                       /* atan */
    ECALC_FUNC_IF                                           /* if文 */
};

enum ecalc_token_type {                                     /* トークンの種類 */
    ECALC_TOKEN_LITE,                                       /* 即値 */
    ECALC_TOKEN_OPE,                                        /* 演算子 */
    ECALC_TOKEN_LBRA,                                       /* 左かっこ */
    ECALC_TOKEN_RBRA,                                       /* 右かっこ */
    ECALC_TOKEN_VAR,                                        /* 変数 */
    ECALC_TOKEN_END,                                        /* 終了 */
    ECALC_TOKEN_TOP,                                        /* 先頭 */
    ECALC_TOKEN_EXP,                                        /* 式 */
    ECALC_TOKEN_ERROR,                                      /* エラー */
    ECALC_TOKEN_FUNC_NON,                                   /* 定数関数 */
    ECALC_TOKEN_FUNC_ONE,                                   /* 単項演算関数 */
    ECALC_TOKEN_FUNC_TWO,                                   /* 2項演算関数 */
    ECALC_TOKEN_ALLOC,                                      /* 確保された */
    ECALC_TOKEN_FREE                                        /* 何でもない */
};

struct ECALC_TOKEN {                                        /* トークン構造体 */
    int type;                                               /* トークンの種類 */
    double value;                                           /* トークンの値 */
    char str[ECALC_MAX_TOKEN_LENGTH];                       /* トークン文字列 */

    struct ECALC_TOKEN *left;                               /* EXP用左辺値 */
    struct ECALC_TOKEN *right;                              /* 右辺値 */
    struct ECALC_TOKEN *next;                               /* 次のトークン */
};

struct ECALC_CONTAINER {                                    /* コンテナ */
    double *vars[ECALC_VAR_COUNT];                          /* 変数のポインタ配列 */
    double ans;                                             /* 前回の答え */

    struct ECALC_TOKEN *token;                              /* トークン */
};

struct ECALC_RESERVED_CONTAINER {
    char *text;                                             /* 文字列 */
    int type;                                               /* 種類 */
    int value;                                              /* 機能 */
    void (*func)( void );									/* 関数 */
};

#ifdef __cplusplus
extern "C" {
#endif

// メモリーマネージャ
void ecalc_memman_init( void );                                                         /* メモリマネージャ初期化 */
struct ECALC_TOKEN *ecalc_memman_malloc( void );                                        /* malloc */
void ecalc_memman_free( struct ECALC_TOKEN *tok );                                      /* free */

// public
struct ECALC_TOKEN *ecalc_make_token( char *text );                                     /* トークン連結リストを作る */
struct ECALC_TOKEN *ecalc_make_tree( struct ECALC_TOKEN *token );                       /* トークンをツリーに変換 */
double ecalc_get_tree_value( struct ECALC_TOKEN *token, double **vars, double ans );    /* ツリーから値を得る */
void ecalc_free_token( struct ECALC_TOKEN *token );                                     /* トークン連結リスト解放 */

// static
struct ECALC_TOKEN *ecalc_malloc_token( void );                                         /* トークンを一個作る */
void ecalc_make_exp( struct ECALC_TOKEN *left );                                        /* 左辺と右辺を結合 */
int ecalc_token_is_value( struct ECALC_TOKEN *token );                                  /* 与えられたトークンが値を持つものかチェック */
int ecalc_strncmp( char *str1, char *str2, int n );                                     /* 大文字小文字を区別しないstrncmp */
void ( *ecalc_get_func_addr( enum ecalc_connector func ) )( void );						/* 関数のポインタを取得する */
// 上の宣言は
// ecalc... is function( enum... ) returning pointer to function( void ) retruning void
// です

#ifdef __cplusplus
}
#endif

#endif

