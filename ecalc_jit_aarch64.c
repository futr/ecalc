#include "ecalc_jit.h"
#include "ecalc_jit_aarch64_private.h"
#include <stdint.h>

// ecalcjit AArch64
// 命令で、符号付きの値を引数をとるものの上位ビットをクリアしていないものがあるかもしれないので確認

ECALC_JIT_TREE *ecalc_create_jit_tree_aarch64( struct ECALC_TOKEN *token )
{
    // JITエンジン作成
    size_t size;
    ECALC_JIT_TREE *tree;

    // tokenがNULLなら何もしない
    if ( token == NULL ) {
        return NULL;
    }

    // 構造体確保
    tree = (ECALC_JIT_TREE *)malloc( sizeof(ECALC_JIT_TREE) );
    ecalc_bin_printer_reset_tree( tree );

    // JIT領域の量
    size = ecalc_get_jit_tree_size( tree, token );

    // 実行可能メモリ空間確保
    tree->size = size;
    tree->pos  = 0;
    tree->data = (unsigned char *)ecalc_allocate_jit_memory( size );

    // 関数バイナリ出力
    ecalc_bin_printer( tree, token );

    return tree;
}

void ecalc_free_jit_tree_aarch64( ECALC_JIT_TREE *tree )
{
    // JITエンジン破棄

    // NULLなら何もしない
    if ( tree == NULL ) {
        return;
    }

    // 実行可能メモリ空間破棄
    ecalc_free_jit_memory( tree->data, tree->size );

    free( tree );
}

double ecalc_get_jit_tree_value_aarch64( ECALC_JIT_TREE *tree, double **vars, double ans )
{
    // JIT木の値を取得
    double ret;
    double ( *func )( double **vars, double ans );

    // NULLなら何もしない
    if ( tree == NULL ) {
        return 0;
    }

#if defined(__aarch64__) && defined(__APPLE__)
    if ( mprotect( tree->data, tree->size, PROT_READ | PROT_EXEC ) != 0 ) {
        return 0;
    }
#endif

#ifdef __aarch64__
    #ifdef __APPLE__
    sys_icache_invalidate( tree->data, tree->size );
    #else
    __builtin___clear_cache( tree->data, tree->data + tree->size );
    #endif
#endif

    // 関数ポインタセット
    func = ( double (*)( double **, double ) )tree->data;

    // 実行
    ret = func( vars, ans );

    return ret;
}

static size_t ecalc_get_jit_tree_size( ECALC_JIT_TREE *tree, struct ECALC_TOKEN *token )
{
    // 必要なメモリ量を(多めに雑に)計算
    size_t size = 0;

    // tree->dataがNULLの状態で印刷することでtree->posからサイズが分かる
    ecalc_bin_printer( tree, token );

    // 関数バイナリ出力位置からサイズを取得
    size = tree->pos;

    return size;
}

static void ecalc_bin_printer( ECALC_JIT_TREE *tree, struct ECALC_TOKEN *token )
{
    // バイナリ出力開始

    // 関数開始
    ecalc_bin_printer_opening( tree );

    // 木をJITに展開
    ecalc_bin_printer_tree( tree, token );

    // 関数終了
    ecalc_bin_printer_closing( tree );
}

static void ecalc_bin_printer_tree( ECALC_JIT_TREE *tree, struct ECALC_TOKEN *token )
{
    const int left  = 0;
    const int right = 16;
    // const int dbuf  = 32;
    int depth = 48;
    size_t pos1, pos2, pos3, pos4, apos1, apos2, apos3;

    /*
     * X9 : スタックや変数アクセス用ポインタ
     * X0 : 色々に使うバッファ
     */

    // EXP以外なら強制ゼロ
    if ( token->type != ECALC_TOKEN_EXP ) {
        ecalc_bin_printer_clear_D0( tree );
        return;
    }

    // ローカル変数をスタックに確保
    ecalc_bin_printer_sub_sp_uimm12( tree, depth );

    // X9に現在のスタックトップの位置を設定
    ecalc_bin_printer_load_var_ptr_to_Xreg( tree, A64_X9, 0 );

    // left, rightを0クリア
    ecalc_bin_printer_store_double_val_on_Xreg( tree, A64_X0, A64_X9, left, 0 );
    ecalc_bin_printer_store_double_val_on_Xreg( tree, A64_X0, A64_X9, right, 0 );

    // 左辺値処理
    if ( token->left != NULL ) {
        if ( token->left->type == ECALC_TOKEN_LITE ) {
            // 左辺値定数リテラルを左にセット
            ecalc_bin_printer_store_double_val_on_Xreg( tree, A64_X0, A64_X9, left, token->left->value );
        } else if ( token->left->type == ECALC_TOKEN_VAR ) {
            // 左辺値変数値を左にセット
            ecalc_bin_printer_load_exp_var_ptr_to_Xreg( tree, A64_X9, token->left->value );
            ecalc_bin_printer_ldr_DFPreg_on_Xreg_offset_uimm12( tree, A64_D0, A64_X9, 0 );

            // X9復帰
            ecalc_bin_printer_load_var_ptr_to_Xreg( tree, A64_X9, 0 );
            ecalc_bin_printer_str_DFPreg_on_Xreg_offset_uimm12( tree, A64_D0, A64_X9, left );
        } else if ( token->left->type == ECALC_TOKEN_EXP ) {
            // 左辺が式だった場合さらにJIT展開し戻り値 D0 を左にセット
            // ecalc_bin_printer_sub_sp_uimm12( tree, depth );
            ecalc_bin_printer_tree( tree, token->left );
            // ecalc_bin_printer_add_sp_uimm12( tree, depth );

            // X9復帰
            ecalc_bin_printer_load_var_ptr_to_Xreg( tree, A64_X9, 0 );
            ecalc_bin_printer_str_DFPreg_on_Xreg_offset_uimm12( tree, A64_D0, A64_X9, left );
        } else {
            // 左を0に
            ecalc_bin_printer_store_double_val_on_Xreg( tree, A64_X0, A64_X9, left, 0 );
        }
    }

    // 右辺取得
    if ( token->right != NULL ) {
        if ( token->right->type == ECALC_TOKEN_LITE ) {
            // 右辺値定数リテラルを右にセット
            ecalc_bin_printer_store_double_val_on_Xreg( tree, A64_X0, A64_X9, right, token->right->value );
        } else if ( token->right->type == ECALC_TOKEN_VAR ) {
            // 右辺値変数値を右にセット
            ecalc_bin_printer_load_exp_var_ptr_to_Xreg( tree, A64_X9, token->right->value );
            ecalc_bin_printer_ldr_DFPreg_on_Xreg_offset_uimm12( tree, A64_D0, A64_X9, 0 );

            // X9復帰
            ecalc_bin_printer_load_var_ptr_to_Xreg( tree, A64_X9, 0 );
            ecalc_bin_printer_str_DFPreg_on_Xreg_offset_uimm12( tree, A64_D0, A64_X9, right );
        } else if ( token->right->type == ECALC_TOKEN_EXP ) {
            // 右辺を式として扱う場合、ここで処理
            if ( token->value == ECALC_OPE_LOOP ) {
                // ループ処理 (@式)
                // 32bit版ではなぜか実数のまま処理しているので、整数に変換して処理する
                // 理由は、スタックが使いやすく、treeの呼び出しでループ変数が破壊されないから
                // X0 : カウンタ(初期値0)
                // X1 : left(ループ回数)
                // として比較実行

                // for ( i = 0; i != left; i++ ) {

                // ループ回数をRDXにロード
                ecalc_bin_printer_ldr_DFPreg_on_Xreg_offset_uimm12( tree, A64_D0, A64_X9, left );
                ecalc_bin_printer_fcvtzs_Xreg( tree, A64_X1, A64_D0 );

                // カウンタクリア
                ecalc_bin_printer_movz_Xreg_uimm16( tree, A64_X0, 0, A64_HW_0 );

                // 現在位置保存
                pos1 = ecalc_bin_printer_get_pos( tree );

                // X0とX1を比較
                ecalc_bin_printer_cmp( tree, A64_X0, A64_X1 );

                // X0==X1ならpos3までジャンプ
                apos1 = ecalc_bin_printer_b_cond( tree, 0, A64_COND_EQ, 0 );

                // 現在位置保存
                pos2 = ecalc_bin_printer_get_pos( tree );

                // 実行 X0とX1が破壊される可能性があるので退避
                ecalc_bin_printer_stp_Xreg_on_Xreg_pre_index( tree, A64_X0, A64_X1, A64_SP, 16 );
                ecalc_bin_printer_tree( tree, token->right );
                ecalc_bin_printer_ldp_Xreg_on_Xreg_post_index( tree, A64_X0, A64_X1, A64_SP, 16 );

                // 戻り値D0をrightに設定
                // X9復帰
                ecalc_bin_printer_load_var_ptr_to_Xreg( tree, A64_X9, 0 );
                ecalc_bin_printer_str_DFPreg_on_Xreg_offset_uimm12( tree, A64_D0, A64_X9, right );

                // インクリメント
                ecalc_bin_printer_add_Xreg_Xreg_uimm12( tree, A64_X0, A64_X0, 1, 0 );

                // pos1の比較まで戻る
                apos2 = ecalc_bin_printer_b( tree, 0, 0 );

                // 現在位置保存
                pos3 = ecalc_bin_printer_get_pos( tree );

                // ジャンプアドレス埋め込み
                ecalc_bin_printer_set_b_cond_label( tree, apos1, pos3 - pos2 );
                ecalc_bin_printer_set_b_label( tree, apos2, pos1 - pos3 );
                // }
            } else if ( token->value == ECALC_FUNC_IF ) {
                // if式
                // D0 : 0
                // D1 : left
                // として比較

                // D0 = 0
                // ecalc_bin_printer_clear_D0( tree );

                // D1 = left
                ecalc_bin_printer_ldr_DFPreg_on_Xreg_offset_uimm12( tree, A64_D1, A64_X9, left );

                // 0とD1を比較 FCMP zero
                ecalc_bin_printer_fcmp_Dreg_zero( tree, A64_D1 );

                // D1 == 0ならpos2までジャンプ
                apos1 = ecalc_bin_printer_b_cond( tree, 0, A64_COND_EQ, 0 );

                // 現在位置保存
                pos1 = ecalc_bin_printer_get_pos( tree );

                // 実行 ( 右の値は使わないので最後のD0は読まなくても良い )
                ecalc_bin_printer_tree( tree, token->right );

                // X9復帰
                ecalc_bin_printer_load_var_ptr_to_Xreg( tree, A64_X9, 0 );

                // 結果を右にセット
                ecalc_bin_printer_str_DFPreg_on_Xreg_offset_uimm12( tree, A64_D0, A64_X9, right );

                // 現在位置保存
                pos2 = ecalc_bin_printer_get_pos( tree );

                // ジャンプアドレス埋め込み
                ecalc_bin_printer_set_b_cond_label( tree, apos1, pos2 - pos1 );

                // right = left
                ecalc_bin_printer_ldr_DFPreg_on_Xreg_offset_uimm12( tree, A64_D0, A64_X9, left );
                ecalc_bin_printer_str_DFPreg_on_Xreg_offset_uimm12( tree, A64_D0, A64_X9, right );
            } else {
                // 右辺の式処理
                ecalc_bin_printer_tree( tree, token->right );

                // RAX復帰
                ecalc_bin_printer_load_var_ptr_to_Xreg( tree, A64_X9, 0 );

                // 結果を右にセット
                ecalc_bin_printer_str_DFPreg_on_Xreg_offset_uimm12( tree, A64_D0, A64_X9, right );
            }
        } else {
            // 右を0に
            ecalc_bin_printer_store_double_val_on_Xreg( tree, A64_X0, A64_X9, right, 0 );
        }
    }

    // 念の為X9復帰
    // ecalc_bin_printer_load_var_ptr_to_Xreg( tree, A64_X9, 0 );

    // D0 = left, D1 = right
    ecalc_bin_printer_ldr_DFPreg_on_Xreg_offset_uimm12( tree, A64_D0, A64_X9, left );
    ecalc_bin_printer_ldr_DFPreg_on_Xreg_offset_uimm12( tree, A64_D1, A64_X9, right );

    // 計算 結果はXMM0に入っているものとする
    switch ( (int)token->value ) {
    case ECALC_OPE_ADD:
        // 足し算
        ecalc_bin_printer_fadd_Dreg( tree, A64_D0, A64_D0, A64_D1 );
        break;
    case ECALC_OPE_SUB:
        // 引き算
        ecalc_bin_printer_fsub_Dreg( tree, A64_D0, A64_D0, A64_D1 );
        break;
    case ECALC_OPE_MUL:
        // 掛け算
        ecalc_bin_printer_fmul_Dreg( tree, A64_D0, A64_D0, A64_D1 );
        break;
    case ECALC_OPE_DIV:
        // 割り算
        // / 0はエラーを発生させない
        // 0とD1 : rightと比較
        ecalc_bin_printer_fcmp_Dreg_zero( tree, A64_D1 );

        // right == 0ならpos2へ飛ぶ
        apos1 = ecalc_bin_printer_b_cond( tree, 0, A64_COND_EQ, 0 );

        // 現在位置保存
        pos1 = ecalc_bin_printer_get_pos( tree );

        // right != 0なので割り算
        ecalc_bin_printer_fdiv_Dreg( tree, A64_D0, A64_D0, A64_D1 );

        // 無条件にpos3の終了までジャンプ
        apos2 = ecalc_bin_printer_b( tree, 0, 0 );

        // 現在位置保存
        pos2 = ecalc_bin_printer_get_pos( tree );

        // right == 0なのでXMM0=0とする
        ecalc_bin_printer_clear_D0( tree );

        // 現在位置保存
        pos3 = ecalc_bin_printer_get_pos( tree );

        // ジャンプアドレス決定
        ecalc_bin_printer_set_b_cond_label( tree, apos1, pos2 - pos1 );
        ecalc_bin_printer_set_b_label( tree,      apos2, pos3 - pos2 );

        break;
    case ECALC_OPE_MOD:
        // 余り
        // % 0はエラーを発生させない
        // 0とD1 : rightと比較
        ecalc_bin_printer_fcmp_Dreg_zero( tree, A64_D1 );

        // right == 0ならpos2へ飛ぶ
        apos1 = ecalc_bin_printer_b_cond( tree, 0, A64_COND_EQ, 0 );

        // 現在位置保存
        pos1 = ecalc_bin_printer_get_pos( tree );

        // right != 0なので余りを計算

        // 引数1 D0 = left
        // 引数2 D1 = right
        // 関数ポインタをX9にロード・コール
        ecalc_bin_printer_load_function_ptr_to_Xreg( tree, A64_X9, ecalc_get_func_addr( token->value ) );
        ecalc_bin_printer_call_on_Xreg( tree, A64_X9 );

        // X9復帰
        ecalc_bin_printer_load_var_ptr_to_Xreg( tree, A64_X9, 0 );

        // 無条件にpos3の終了までジャンプ
        apos2 = ecalc_bin_printer_b( tree, 0, 0 );

        // 現在位置保存
        pos2 = ecalc_bin_printer_get_pos( tree );

        // right == 0なのでD0 = 0とする
        ecalc_bin_printer_clear_D0( tree );

        // 現在位置保存
        pos3 = ecalc_bin_printer_get_pos( tree );

        // ジャンプアドレス決定
        ecalc_bin_printer_set_b_cond_label( tree, apos1, pos2 - pos1 );
        ecalc_bin_printer_set_b_label( tree,      apos2, pos3 - pos2 );

        break;
    case ECALC_OPE_STI:
        // 代入
        // 左辺変数ポインタをX9にロード
        ecalc_bin_printer_load_exp_var_ptr_to_Xreg( tree, A64_X9, token->left->value );

        // [X9] <= D1 : right
        ecalc_bin_printer_str_DFPreg_on_Xreg_offset_uimm12( tree, A64_D1, A64_X9, 0 );

        // D0 <= D1 式の値は右辺
        ecalc_bin_printer_fmov_Dreg( tree, A64_D0, A64_D1 );

        // X9復帰
        ecalc_bin_printer_load_var_ptr_to_Xreg( tree, A64_X9, 0 );

        break;
    case ECALC_OPE_SEPA:
        // 区切り
        // D0 <= D1 式の値は右辺
        ecalc_bin_printer_fmov_Dreg( tree, A64_D0, A64_D1 );
        break;
    case ECALC_OPE_LOOP:
        // 繰り返し
        // D0 <= D1 式の値は右辺
        ecalc_bin_printer_fmov_Dreg( tree, A64_D0, A64_D1 );
        break;
    case ECALC_OPE_LBIG:
        // 左が大きい

        // D0とD1を比較
        ecalc_bin_printer_fcmp_Dreg( tree, A64_D0, A64_D1 );

        // D0==D1ならpos2までジャンプ
        apos1 = ecalc_bin_printer_b_cond( tree, 0, A64_COND_EQ, 0 );

        // 現在位置保存
        pos1 = ecalc_bin_printer_get_pos( tree );

        // D0 < D1ならpos3までジャンプ
        apos2 = ecalc_bin_printer_b_cond( tree, 0, A64_COND_LT, 0 );

        // 現在位置保存
        pos2 = ecalc_bin_printer_get_pos( tree );

        // right < left なのでD0 = 1
        ecalc_bin_printer_mov_double_val_to_Xreg( tree, A64_X0, 1 );
        ecalc_bin_printer_fmov_Dreg_Xreg( tree, A64_D0, A64_X0 );

        // 無条件にpos3の終了までジャンプ
        apos3 = ecalc_bin_printer_b( tree, 0, 0 );

        // 現在位置保存
        pos3 = ecalc_bin_printer_get_pos( tree );

        // left <= right なのでD0 = 0
        ecalc_bin_printer_clear_D0( tree );

        // 現在位置保存
        pos4 = ecalc_bin_printer_get_pos( tree );

        // ジャンプアドレス埋め込み
        ecalc_bin_printer_set_b_cond_label( tree, apos1, pos2 - pos1 );
        ecalc_bin_printer_set_b_cond_label( tree, apos2, pos3 - pos2 );
        ecalc_bin_printer_set_b_label(      tree, apos3, pos4 - pos3 );

        break;
    case ECALC_OPE_RBIG:
        // 右が大きい

        // D0とD1を比較
        ecalc_bin_printer_fcmp_Dreg( tree, A64_D0, A64_D1 );

        // D0==D1ならpos3までジャンプ
        apos1 = ecalc_bin_printer_b_cond( tree, 0, A64_COND_EQ, 0 );

        // 現在位置保存
        pos1 = ecalc_bin_printer_get_pos( tree );

        // D0 < D1ならpos3までジャンプ
        apos2 = ecalc_bin_printer_b_cond( tree, 0, A64_COND_LT, 0 );

        // 現在位置保存
        pos2 = ecalc_bin_printer_get_pos( tree );

        // right < left なのでD0 = 0
        ecalc_bin_printer_clear_D0( tree );

        // 無条件にpos3の終了までジャンプ
        apos3 = ecalc_bin_printer_b( tree, 0, 0 );

        // 現在位置保存
        pos3 = ecalc_bin_printer_get_pos( tree );

        // left <= right なのでD0 = 1
        ecalc_bin_printer_mov_double_val_to_Xreg( tree, A64_X0, 1 );
        ecalc_bin_printer_fmov_Dreg_Xreg( tree, A64_D0, A64_X0 );

        // 現在位置保存
        pos4 = ecalc_bin_printer_get_pos( tree );

        // ジャンプアドレス埋め込み
        ecalc_bin_printer_set_b_cond_label( tree, apos1, pos3 - pos1 );
        ecalc_bin_printer_set_b_cond_label( tree, apos2, pos3 - pos2 );
        ecalc_bin_printer_set_b_label(      tree, apos3, pos4 - pos3 );

        break;
    case ECALC_OPE_EQU:
        // 同じ

        // D0とD1を比較
        ecalc_bin_printer_fcmp_Dreg( tree, A64_D0, A64_D1 );

        // D0==D1ならpos2までジャンプ
        apos1 = ecalc_bin_printer_b_cond( tree, 0, A64_COND_EQ, 0 );

        // 現在位置保存
        pos1 = ecalc_bin_printer_get_pos( tree );

        // right != left なのでD0 = 0
        ecalc_bin_printer_clear_D0( tree );

        // 無条件にpos3の終了までジャンプ
        apos2 = ecalc_bin_printer_b( tree, 0, 0 );

        // 現在位置保存
        pos2 = ecalc_bin_printer_get_pos( tree );

        // right == left なのでなのでXMM0 = 1
        ecalc_bin_printer_mov_double_val_to_Xreg( tree, A64_X0, 1 );
        ecalc_bin_printer_fmov_Dreg_Xreg( tree, A64_D0, A64_X0 );

        // 現在位置保存
        pos3 = ecalc_bin_printer_get_pos( tree );

        // ジャンプアドレス埋め込み
        ecalc_bin_printer_set_b_cond_label( tree, apos1, pos2 - pos1 );
        ecalc_bin_printer_set_b_label(      tree, apos2, pos3 - pos2 );

        break;
    case ECALC_FUNC_SIN:
    case ECALC_FUNC_COS:
    case ECALC_FUNC_TAN:                /* tan */
    case ECALC_FUNC_ASIN:               /* asin */
    case ECALC_FUNC_ACOS:               /* acos */
    case ECALC_FUNC_ATAN:               /* atan */
    case ECALC_FUNC_LOG10:              /* log10 */
    case ECALC_FUNC_LOGN:               /* logn */
        // 引数としてD0 = rightをセット
        ecalc_bin_printer_fmov_Dreg( tree, A64_D0, A64_D1 );

        // 関数ポインタをX9にロード・コール
        ecalc_bin_printer_load_function_ptr_to_Xreg( tree, A64_X9, ecalc_get_func_addr( token->value ) );
        ecalc_bin_printer_call_on_Xreg( tree, A64_X9 );

        // X9復帰
        ecalc_bin_printer_load_var_ptr_to_Xreg( tree, A64_X9, 0 );

        break;
    case ECALC_FUNC_SQRT:
        // sqrt
        ecalc_bin_printer_fsqrt_Dreg( tree, A64_D0, A64_D1 );

        break;
    case ECALC_FUNC_POW:
    case ECALC_FUNC_ATAN2:
        // POWER, ATAN2

        // 引数1 D0 = left
        // 引数2 D1 = right
        // 関数ポインタをX9にロード・コール
        ecalc_bin_printer_load_function_ptr_to_Xreg( tree, A64_X9, ecalc_get_func_addr( token->value ) );
        ecalc_bin_printer_call_on_Xreg( tree, A64_X9 );

        // X9復帰
        ecalc_bin_printer_load_var_ptr_to_Xreg( tree, A64_X9, 0 );

        break;
    case ECALC_FUNC_RAD:
        // rad

        // D0 = D1
        ecalc_bin_printer_fmov_Dreg( tree, A64_D0, A64_D1 );

        // PI / 180
        ecalc_bin_printer_mov_double_val_to_Xreg( tree, A64_X0, M_PI / 180 );
        ecalc_bin_printer_fmov_Dreg_Xreg( tree, A64_D1, A64_X0 );

        // PI / 180かける
        ecalc_bin_printer_fmul_Dreg( tree, A64_D0, A64_D0, A64_D1 );

        break;
    case ECALC_FUNC_DEG:
        // deg

        // D0 = D1
        ecalc_bin_printer_fmov_Dreg( tree, A64_D0, A64_D1 );

        // 180 / PI
        ecalc_bin_printer_mov_double_val_to_Xreg( tree, A64_X0, 180 / M_PI );
        ecalc_bin_printer_fmov_Dreg_Xreg( tree, A64_D1, A64_X0 );

        // 180 / PIかける
        ecalc_bin_printer_fmul_Dreg( tree, A64_D0, A64_D0, A64_D1 );

        break;
    case ECALC_FUNC_PI:
        // π
        ecalc_bin_printer_mov_double_val_to_Xreg( tree, A64_X0, M_PI );
        ecalc_bin_printer_fmov_Dreg_Xreg( tree, A64_D0, A64_X0 );

        break;
    case ECALC_FUNC_EPS0:
        // ε0
        ecalc_bin_printer_mov_double_val_to_Xreg( tree, A64_X0, 8.85418782e-12 );
        ecalc_bin_printer_fmov_Dreg_Xreg( tree, A64_D0, A64_X0 );

        break;
    case ECALC_FUNC_ANS:
        // ans
        ecalc_bin_printer_load_exp_ans_ptr_to_Xreg( tree, A64_X9 );
        ecalc_bin_printer_ldr_DFPreg_on_Xreg_offset_uimm12( tree, A64_D0, A64_X9, 0 );

        // X9復帰
        ecalc_bin_printer_load_var_ptr_to_Xreg( tree, A64_X9, 0 );

        break;
    case ECALC_FUNC_IF:
        // if文

        // 左破棄して右を返す
        ecalc_bin_printer_fmov_Dreg( tree, A64_D0, A64_D1 );

        break;
    default:
        // デフォルトはゼロ
        ecalc_bin_printer_clear_D0( tree );
    }

    // スタックからローカル変数破棄
    ecalc_bin_printer_add_sp_uimm12( tree, depth );

    // 一つのノード出力完了
    // この時点でD0に答えがあるはず
}

static void ecalc_bin_printer_opening( ECALC_JIT_TREE *tree )
{
    // オープニング
    // 引数をつみフレームポインタを設定
    // FPとLRをつむ
    /*
     * sub sp, sp, #0x40       FF 03 01 D1
     * str X0, [sp, #0]        E0 03 00 F9
     * str D0, [sp, #16]       E0 07 00 FD
     * stp fp, lr, [sp, #32]   FD 7B 02 A9
     * mov fp, sp              FD 03 00 91
     */
    uint8_t bin1[] = {0xE0, 0x03, 0x00, 0xF9};
    uint8_t bin2[] = {0xE0, 0x07, 0x00, 0xFD};
    uint8_t bin3[] = {0xFD, 0x7B, 0x02, 0xA9};

    ecalc_bin_printer_sub_sp_uimm12( tree, 0x40 );
    ecalc_bin_printer_print( tree, bin1, sizeof( bin1 ) );
    ecalc_bin_printer_print( tree, bin2, sizeof( bin2 ) );
    ecalc_bin_printer_print( tree, bin3, sizeof( bin3 ) );
    ecalc_bin_printer_mov_fp_sp( tree );
}

static void ecalc_bin_printer_closing( ECALC_JIT_TREE *tree )
{
    // クロージング
    /*
     * ldp FP, LR, [sp, #32]    FD 7B 42 A9
     * add sp, sp, #0x40        FF 03 01 91
     * ret                      C0 03 5F D6
     */
    uint8_t bin1[] = {0xFD, 0x7B, 0x42, 0xA9};
    uint8_t bin2[] = {0xC0, 0x03, 0x5F, 0xD6};

    ecalc_bin_printer_print( tree, bin1, sizeof( bin1 ) );
    ecalc_bin_printer_add_sp_uimm12( tree, 0x40 );
    ecalc_bin_printer_print( tree, bin2, sizeof( bin2 ) );
}

static void ecalc_bin_printer_add_sp_uimm12(ECALC_JIT_TREE *tree, uint16_t val)
{
    // SPに即値を足す
    /*
     * add sp, sp, uimm12   FF v3 vv 91
     */
    /*
    uint8_t bin[] = {0xFF, 0x03, 0x00, 0x91};
    uint32_t buf = 0;

    memcpy( &buf, bin, 4 );
    buf = buf | ( (uint32_t)val << 10 );

    ecalc_bin_printer_print( tree, (uint8_t *)&buf, sizeof( buf ) );
    */
    ecalc_bin_printer_add_Xreg_Xreg_uimm12( tree, A64_SP, A64_SP, val, 0 );
}

static void ecalc_bin_printer_sub_sp_uimm12(ECALC_JIT_TREE *tree, uint16_t val)
{
    // SPから即値をひく
    /*
     * sub sp, sp, uimm12   FF v3 vv D1
     */
    /*
    uint8_t bin[] = {0xFF, 0x03, 0x00, 0xD1};
    uint32_t buf = 0;

    memcpy( &buf, bin, 4 );
    buf = buf | ( (uint32_t)val << 10 );

    ecalc_bin_printer_print( tree, (uint8_t *)&buf, sizeof( buf ) );
    */
    ecalc_bin_printer_sub_Xreg_Xreg_uimm12( tree, A64_SP, A64_SP, val, 0 );
}

static void ecalc_bin_printer_add_reg_uimm12(ECALC_JIT_TREE *tree, uint8_t reg, uint16_t val)
{
    // DEPRECATED 同じ関数を別に作っていた
    // REGに即値を足す
    // reg <= reg + val
    /*
     * add reg, reg, uimm12   RR v(Xxxrr) vv 91
     */
    uint8_t bin[] = {0x00, 0x00, 0x00, 0x91};
    uint32_t buf = 0;

    memcpy( &buf, bin, 4 );
    buf = buf | ( (uint32_t)val << 10 );
    buf = buf | reg;
    buf = buf | ( reg << 5 );

    ecalc_bin_printer_print( tree, (uint8_t *)&buf, sizeof( buf ) );
}

static void ecalc_bin_printer_sub_reg_uimm12(ECALC_JIT_TREE *tree, uint8_t reg, uint16_t val)
{
    // DEPRECATED
    // REGから即値をひく
    /*
     * sub reg, reg, uimm12   RR v(Xxxrr) vv D1
     */
    uint8_t bin[] = {0x00, 0x00, 0x00, 0xD1};
    uint32_t buf = 0;

    memcpy( &buf, bin, 4 );
    buf = buf | ( (uint32_t)val << 10 );
    buf = buf | reg;
    buf = buf | ( reg << 5 );

    ecalc_bin_printer_print( tree, (uint8_t *)&buf, sizeof( buf ) );
}

static void ecalc_bin_printer_subs_shifted_Xreg( ECALC_JIT_TREE *tree, uint8_t rd, uint8_t rn, uint8_t rm, uint8_t shift_mode, uint8_t shift )
{
    // RnからシフトしたRmを引いてRdにセットする
    // Rd = XZRとすることでCMPとなる
    /*
     * subs Xd, Xn, Xm, shift_mode, #shift
     * 1110 1011 ss0m mmmm iiii iiRR RRRr rrrr
     *           sh Rm     uimm6  Rn    Rd
     */
    uint32_t buf = 0xEB000000;

    buf = buf | rd;
    buf = buf | ( (uint32_t)rn         <<  5 );
    buf = buf | ( (uint32_t)shift      << 10 );
    buf = buf | ( (uint32_t)rm         << 16 );
    buf = buf | ( (uint32_t)shift_mode << 22 );

    ecalc_bin_printer_print( tree, (uint8_t *)&buf, sizeof( buf ) );
}

static void ecalc_bin_printer_cmp( ECALC_JIT_TREE *tree, uint8_t rn, uint8_t rm )
{
    // subsを使ってcmp
    ecalc_bin_printer_subs_shifted_Xreg( tree, A64_XZR, rn, rm, A64_SH_LSL, 0 );
}

static void ecalc_bin_printer_fcmp_Dreg_zero( ECALC_JIT_TREE *tree, uint8_t dn )
{
    // 倍精度浮動小数と比較0を比較
    /*
     * fcmp Dn, Dm
     * 0001 1110 011m mmmm 0010 0000 0000 x000
     *              Rm            Rn    opc(01)
     */
    uint32_t buf = 0x1E602008;

    buf = buf | ( (uint32_t)dn <<  5 );

    ecalc_bin_printer_print( tree, (uint8_t *)&buf, sizeof( buf ) );
}

static void ecalc_bin_printer_fcmp_Dreg( ECALC_JIT_TREE *tree, uint8_t dn, uint8_t dm )
{
    // 倍精度浮動小数点比較
    /*
     * fcmp Dn, Dm
     * 0001 1110 011m mmmm 0010 00RR RRR0 x000
     *              Rm            Rn    opc(00)
     */
    uint32_t buf = 0x1E602000;

    buf = buf | ( (uint32_t)dn <<  5 );
    buf = buf | ( (uint32_t)dm << 16 );

    ecalc_bin_printer_print( tree, (uint8_t *)&buf, sizeof( buf ) );
}

static size_t ecalc_bin_printer_b_cond( ECALC_JIT_TREE *tree, uint64_t write_pos, uint8_t cond, int32_t label )
{
    // 条件分岐
    // labelは、実際のアドレスで与える
    // write_pos == 0 現在の位置に書き込み
    // write_pos != 0 write_posに書き込み
    // この機能は使ってないけど一応残しておく
    /*
     * b.cond label
     * 0101 0100 iiii iiii iiii iiii iii0 cccc
     */
    uint32_t buf   = 0x54000000;
    uint32_t mask  = 0x00FFFFE0;
    uint32_t imm19 = ( ( label / 4 ) << 5 ) & mask;

    buf = buf | cond;
    buf = buf | imm19;

    if ( write_pos == 0 ) {
        // 通常の書き込み
        ecalc_bin_printer_print( tree, (uint8_t *)&buf, sizeof( buf ) );

        // 命令の開始位置を返す
        return tree->pos - 4;
    } else {
        // 指定位置に書き込み
        ecalc_bin_printer_print_to( tree, write_pos, (uint8_t *)&buf, sizeof( buf ) );

        return 0;
    }
}

static size_t ecalc_bin_printer_b( ECALC_JIT_TREE *tree, uint64_t write_pos, int32_t label )
{
    // ジャンプ
    // labelは、実際のアドレスで与える
    // write_pos == 0 現在の位置に書き込み
    // write_pos != 0 write_posに書き込み
    /*
     * b label
     * 0001 01ii iiii iiii iiii iiii iiii iiii
     *       simm26
     */
    uint32_t buf   = 0x14000000;
    uint32_t mask  = 0x03FFFFFF;
    uint32_t imm26 = ( label / 4 ) & mask;

    buf = buf | imm26;

    if ( write_pos == 0 ) {
        // 通常の書き込み
        ecalc_bin_printer_print( tree, (uint8_t *)&buf, sizeof( buf ) );

        // 命令の開始位置を返す
        return tree->pos - 4;
    } else {
        // 指定位置に書き込み
        ecalc_bin_printer_print_to( tree, write_pos, (uint8_t *)&buf, sizeof( buf ) );

        return 0;
    }
}

static void ecalc_bin_printer_set_b_cond_label( ECALC_JIT_TREE *tree, uint64_t pos, int32_t label )
{
    // 分岐命令のラベルをあとから書き込む
    uint32_t buf;
    uint32_t mask  = 0x00FFFFE0;
    uint32_t imm19 = ( ( label / 4 ) << 5 ) & mask;

    ecalc_bin_printer_get_data( tree, (uint8_t *)&buf, pos, sizeof( buf ) );

    buf = buf | imm19;
    ecalc_bin_printer_print_to( tree, pos, (uint8_t *)&buf, sizeof( buf ) );
}

static void ecalc_bin_printer_set_b_label( ECALC_JIT_TREE *tree, uint64_t pos, int32_t label )
{
    // ジャンプ命令のラベルをあとから書き込む
    uint32_t buf;
    uint32_t mask  = 0x03FFFFFF;
    uint32_t imm26 = ( label / 4 ) & mask;

    ecalc_bin_printer_get_data( tree, (uint8_t *)&buf, pos, sizeof( buf ) );

    buf = buf | imm26;
    ecalc_bin_printer_print_to( tree, pos, (uint8_t *)&buf, sizeof( buf ) );
}

static void ecalc_bin_printer_blr( ECALC_JIT_TREE *tree, uint8_t reg )
{
    // レジスタの場所をコールし、リンクレジスタに次の命令の場所をセット
    /*
     * blr Xreg
     * 1101 0110 0011 1111 0000 00nn nnn0 0000
     */
    uint32_t buf = 0xD63F0000;

    buf = buf | ( (uint32_t)reg << 5 );

    ecalc_bin_printer_print( tree, (uint8_t *)&buf, sizeof( buf ) );
}

static void ecalc_bin_printer_mov_fp_sp(ECALC_JIT_TREE *tree)
{
    // fp <= sp
    /*
     * mov fp, sp   FD 03 00 91
     */
    uint8_t bin[] = {0xFD, 0x03, 0x00, 0x91};

    ecalc_bin_printer_print( tree, bin, sizeof( bin ) );
}

static void ecalc_bin_printer_clear_D0(ECALC_JIT_TREE *tree)
{
    /*
     * movi D0, #0  00 E4 00 2F
     */
    uint8_t bin[] = {0x00, 0xE4, 0x00, 0x2F};

    ecalc_bin_printer_print( tree, bin, sizeof( bin ) );
}

static void ecalc_bin_printer_fcvtzs_Xreg( ECALC_JIT_TREE *tree, uint8_t rd, uint8_t rn )
{
    // 整数レジスタRdに実数レジスタRnを符号付き整数に切り捨てで変換して代入
    /*
     * fcvtzs Xd, Dn
     * 1001 1110 0111 1000 0000 00RR RRRr rrrr
     *
     */
    uint32_t buf = 0x9E780000;

    buf = buf | rd;
    buf = buf | ( (uint32_t)rn << 5 );

    ecalc_bin_printer_print( tree, (uint8_t *)&buf, sizeof( buf ) );
}

static void ecalc_bin_printer_fadd_Dreg( ECALC_JIT_TREE *tree, uint8_t rd, uint8_t rn, uint8_t rm )
{
/*
     * fadd Rd, Rn, Rm
     * 0001 1110 011m mmmm 0010 10RR RRRr rrrr
     */
    uint32_t buf = 0x1E602800;

    buf = buf | rd;
    buf = buf | ( (uint32_t)rn << 5 );
    buf = buf | ( (uint32_t)rm << 16 );

    ecalc_bin_printer_print( tree, (uint8_t *)&buf, sizeof( buf ) );
}

static void ecalc_bin_printer_fsub_Dreg( ECALC_JIT_TREE *tree, uint8_t rd, uint8_t rn, uint8_t rm )
{
/*
     * fsub Rd, Rn, Rm
     * 0001 1110 011m mmmm 0011 10nn nnnd dddd
     */
    uint32_t buf = 0x1E603800;

    buf = buf | rd;
    buf = buf | ( (uint32_t)rn << 5 );
    buf = buf | ( (uint32_t)rm << 16 );

    ecalc_bin_printer_print( tree, (uint8_t *)&buf, sizeof( buf ) );
}

static void ecalc_bin_printer_fmul_Dreg( ECALC_JIT_TREE *tree, uint8_t rd, uint8_t rn, uint8_t rm )
{
/*
     * fmul Rd, Rn, Rm
     * 0001 1110 011m mmmm 0000 10nn nnnd dddd
     */
    uint32_t buf = 0x1E600800;

    buf = buf | rd;
    buf = buf | ( (uint32_t)rn << 5 );
    buf = buf | ( (uint32_t)rm << 16 );

    ecalc_bin_printer_print( tree, (uint8_t *)&buf, sizeof( buf ) );
}

static void ecalc_bin_printer_fdiv_Dreg( ECALC_JIT_TREE *tree, uint8_t rd, uint8_t rn, uint8_t rm )
{
/*
     * fdiv Rd, Rn, Rm
     * 0001 1110 011m mmmm 0001 10nn nnnd dddd
     */
    uint32_t buf = 0x1E601800;

    buf = buf | rd;
    buf = buf | ( (uint32_t)rn << 5 );
    buf = buf | ( (uint32_t)rm << 16 );

    ecalc_bin_printer_print( tree, (uint8_t *)&buf, sizeof( buf ) );
}

static void ecalc_bin_printer_fneg_Dreg( ECALC_JIT_TREE *tree, uint8_t rd, uint8_t rn )
{
/*
     * fneg Rd, Rn
     * 0001 1110 0110 0001 0100 00nn nnnd dddd
     */
    uint32_t buf = 0x1E614000;

    buf = buf | rd;
    buf = buf | ( (uint32_t)rn << 5 );

    ecalc_bin_printer_print( tree, (uint8_t *)&buf, sizeof( buf ) );
}

static void ecalc_bin_printer_fsqrt_Dreg( ECALC_JIT_TREE *tree, uint8_t rd, uint8_t rn )
{
/*
     * fsqrt Rd, Rn
     * 0001 1110 0110 0001 1100 00nn nnnd dddd
     */
    uint32_t buf = 0x1E61C000;

    buf = buf | rd;
    buf = buf | ( (uint32_t)rn << 5 );

    ecalc_bin_printer_print( tree, (uint8_t *)&buf, sizeof( buf ) );
}

static void ecalc_bin_printer_fmov_Dreg( ECALC_JIT_TREE *tree, uint8_t rd, uint8_t rn )
{
/*
     * fmov Rd, Rn
     * 0001 1110 0110 0000 0100 00nn nnnd dddd
     */
    uint32_t buf = 0x1E604000;

    buf = buf | rd;
    buf = buf | ( (uint32_t)rn << 5 );

    ecalc_bin_printer_print( tree, (uint8_t *)&buf, sizeof( buf ) );
}

static void ecalc_bin_printer_fmov_Dreg_Xreg( ECALC_JIT_TREE *tree, uint8_t rd, uint8_t rn )
{
    /*
     * fmov Dd, Xn
     * 1001 1110 0110 011x 0000 00nn nnnd dddd
     * x = 1 : fmov Dd, Xn
     */
    uint32_t buf = 0x9E670000;

    buf = buf | rd;
    buf = buf | ( (uint32_t)rn << 5 );

    ecalc_bin_printer_print( tree, (uint8_t *)&buf, sizeof( buf ) );
}

static void ecalc_bin_printer_fmov_Xreg_Dreg( ECALC_JIT_TREE *tree, uint8_t rd, uint8_t rn )
{
    /*
     * fmov Xd, Dn
     * 1001 1110 0110 011x 0000 00nn nnnd dddd
     * x = 0 : fmov Xd, Dn
     */
    uint32_t buf = 0x9E660000;

    buf = buf | rd;
    buf = buf | ( (uint32_t)rn << 5 );

    ecalc_bin_printer_print( tree, (uint8_t *)&buf, sizeof( buf ) );
}

static void ecalc_bin_printer_movz_Xreg_uimm16( ECALC_JIT_TREE *tree, uint8_t reg, uint16_t val, uint8_t hw )
{
    // 64bitレジスタに16ビット即値をシフトしてセット、指定されないビットはクリア MOVZ
    /*
     * MSB 1101 0010 1hhv vvvv vvvv vvvv vvvr rrrr LSB
     */
    uint32_t buf = 0xD2800000;

    buf = buf | ( (uint32_t)val << 5 );
    buf = buf | reg;
    buf = buf | ( (uint32_t)hw << 21 );

    ecalc_bin_printer_print( tree, (uint8_t *)&buf, sizeof( buf ) );
}

static void ecalc_bin_printer_add_Xreg_Xreg_uimm12( ECALC_JIT_TREE *tree, uint8_t rd, uint8_t rn, uint16_t val, uint8_t shift12 )
{
    // 64bitレジスタrdに、64bitレジスタrnと即値val（または12bitシフトした即値）を足したものを代入
    /*
     * add Xd, Xn, #val, (lsl #12)
     * MSB 1001 0001 0svv vvvv vvvv vvRR RRRr rrrr LSB
     */
    uint32_t buf = 0x91000000;

    buf = buf | rd;
    buf = buf | ( (uint32_t)rn      <<  5 );
    buf = buf | ( (uint32_t)val     << 10 );
    buf = buf | ( (uint32_t)shift12 << 22 );

    ecalc_bin_printer_print( tree, (uint8_t *)&buf, sizeof( buf ) );
}

static void ecalc_bin_printer_sub_Xreg_Xreg_uimm12( ECALC_JIT_TREE *tree, uint8_t rd, uint8_t rn, uint16_t val, uint8_t shift12 )
{
    // 64bitレジスタrdに、64bitレジスタrnから即値val（または12bitシフトした即値）を引いたものを代入
    /*
     * sub Xd, Xn, #val, (lsl #12)
     * MSB 1101 0001 0svv vvvv vvvv vvRR RRRr rrrr LSB
     */
    uint32_t buf = 0xD1000000;

    buf = buf | rd;
    buf = buf | ( (uint32_t)rn      <<  5 );
    buf = buf | ( (uint32_t)val     << 10 );
    buf = buf | ( (uint32_t)shift12 << 22 );

    ecalc_bin_printer_print( tree, (uint8_t *)&buf, sizeof( buf ) );
}

static void ecalc_bin_printer_movk_Xreg_uimm16( ECALC_JIT_TREE *tree, uint8_t reg, uint16_t val, uint8_t hw )
{
    // 64bitレジスタに16ビット即値をシフトしてセット、指定されないビットは変えない MOVK
    /*
     * MSB (1111 0010 1hhv vvvv vvvv vvvv vvvr rrrr) LSB
     */
    uint32_t buf = 0xF2800000;

    buf = buf | ( (uint32_t)val << 5 );
    buf = buf | reg;
    buf = buf | ( (uint32_t)hw << 21 );

    ecalc_bin_printer_print( tree, (uint8_t *)&buf, sizeof( buf ) );
}

static void ecalc_bin_printer_load_var_ptr_to_Xreg( ECALC_JIT_TREE *tree, uint8_t reg, uint16_t offset )
{
    // regに現在のスタック位置から相対の変数のポインタをセット
    /*
     * mov Xreg, SP            E9 03 00 91 = add Xreg, SP, #0, lsl #0
     * add Xreg, Xreg, offset
     */
    ecalc_bin_printer_add_Xreg_Xreg_uimm12( tree, reg, A64_SP, 0, 0 );
    ecalc_bin_printer_add_Xreg_Xreg_uimm12( tree, reg, reg, offset, 0 );
}

static void ecalc_bin_printer_str_DFPreg_on_Xreg_offset_uimm12( ECALC_JIT_TREE *tree, uint8_t rt, uint8_t rbase, uint16_t offset )
{
    // Dtの値を[base + offset]の位置に書き込む
    // offsetは8の倍数のみ
    /*
     * str Dt, [Xbase, #offset]
     * MSB 1111 1101 00vv vvvv vvvv vvRR RRR r rrrr LSB
     */
    uint32_t buf = 0xFD000000;

    buf = buf | rt;
    buf = buf | ( (uint32_t)rbase << 5 );
    buf = buf | ( (uint32_t)( offset / 8 ) << 10 );

    // offsetは8の倍数でなければならない
    if ( offset % 8 ) ecalc_bin_printer_error( tree, "AArch64", "str Dt, [Xbase, #offset]", "offset is not a multiple of 8" );

    ecalc_bin_printer_print( tree, (uint8_t *)&buf, sizeof( buf ) );
}

static void ecalc_bin_printer_ldr_DFPreg_on_Xreg_offset_uimm12( ECALC_JIT_TREE *tree, uint8_t rt, uint8_t rbase, uint16_t offset )
{
    // [base + offset]の位置の値をDtにロード
    // offsetは8の倍数のみ
    /*
     * ldr Dt, [Xbase, #offset]
     * MSB 1111 1101 01vv vvvv vvvv vvRR RRRr rrrr LSB
     */
    uint32_t buf = 0xFD400000;

    buf = buf | rt;
    buf = buf | ( (uint32_t)rbase << 5 );
    buf = buf | ( (uint32_t)( offset / 8 ) << 10 );

    // offsetは8の倍数でなければならない
    if ( offset % 8 ) ecalc_bin_printer_error( tree, "AArch64", "ldr Dt, [Xbase, #offset]", "offset is not a multiple of 8" );

    ecalc_bin_printer_print( tree, (uint8_t *)&buf, sizeof( buf ) );
}

static void ecalc_bin_printer_str_Xreg_on_Xreg_offset_uimm12( ECALC_JIT_TREE *tree, uint8_t rt, uint8_t rbase, uint16_t offset )
{
    // Rtの値を[base + offset]の位置に書き込む
    // offsetは8の倍数のみ
    /*
     * str Xt, [Xbase, #offset]
     * 1111 1001 00vv vvvv vvvv vvRR RRRr rrrr
     */
    uint32_t buf = 0xF9000000;

    buf = buf | rt;
    buf = buf | ( (uint32_t)rbase << 5 );
    buf = buf | ( (uint32_t)( offset / 8 ) << 10 );

    // offsetは8の倍数でなければならない
    if ( offset % 8 ) ecalc_bin_printer_error( tree, "AArch64", "str Xt, [Xbase, #offset]", "offset is not a multiple of 8" );

    ecalc_bin_printer_print( tree, (uint8_t *)&buf, sizeof( buf ) );
}

static void ecalc_bin_printer_ldr_Xreg_on_Xreg_offset_uimm12( ECALC_JIT_TREE *tree, uint8_t rt, uint8_t rbase, uint16_t offset )
{
    // [base + offset]の位置の値をRtにロード
    // offsetは8の倍数のみ
    /*
     * ldr Xt, [Xbase, #offset]
     * MSB 1111 1001 01vv vvvv vvvv vvRR RRRr rrrr LSB
     */
    uint32_t buf = 0xF9400000;

    buf = buf | rt;
    buf = buf | ( (uint32_t)rbase << 5 );
    buf = buf | ( (uint32_t)( offset / 8 ) << 10 );

    // offsetは8の倍数でなければならない
    if ( offset % 8 ) ecalc_bin_printer_error( tree, "AArch64", "ldr Xt, [Xbase, #offset]", "offset is not a multiple of 8" );

    ecalc_bin_printer_print( tree, (uint8_t *)&buf, sizeof( buf ) );
}

static void ecalc_bin_printer_stp_Xreg_on_Xreg_pre_index( ECALC_JIT_TREE *tree, uint8_t rt, uint8_t rt2, uint8_t rbase, int8_t offset )
{
    // rn = rn + offset, [rn]に２つのレジスタ書き込み
    /*
     * stp x0, x1, [sp, #16]!
     * 1010 1001 10ii iiii i222 22RR RRRr rrrr
     *             simm7    Rt2   Rbase Rt
     */
    uint32_t buf  = 0xA9800000;
    uint32_t mask = 0x003F8000;
    uint32_t simm7 = ( ( (int32_t)offset / 8 ) << 15 ) & mask;

    buf = buf | rt;
    buf = buf | ( (uint32_t)rbase << 5 );
    buf = buf | ( (uint32_t)rt2 << 10 );
    buf = buf | simm7;

    // offsetは8の倍数でなければならない
    if ( offset % 8 ) ecalc_bin_printer_error( tree, "AArch64", "stp Xt, Xt2, [Xbase, #offset]!", "offset is not a multiple of 8" );

    ecalc_bin_printer_print( tree, (uint8_t *)&buf, sizeof( buf ) );
}

static void ecalc_bin_printer_ldp_Xreg_on_Xreg_post_index( ECALC_JIT_TREE *tree, uint8_t rt, uint8_t rt2, uint8_t rbase, int8_t offset )
{
    // [rn]から２つのレジスタに読み込み, rn = rn + offset
    /*
     * ldp x0, x1, [sp], #16
     * 1010 1000 11ii iiii i222 22RR RRRr rrrr
     *             simm7    Rt2   Rbase Rt
     */
    uint32_t buf  = 0xA8C00000;
    uint32_t mask = 0x003F8000;
    uint32_t simm7 = ( ( (int32_t)offset / 8 ) << 15 ) & mask;

    buf = buf | rt;
    buf = buf | ( (uint32_t)rbase << 5 );
    buf = buf | ( (uint32_t)rt2 << 10 );
    buf = buf | simm7;

    // offsetは8の倍数でなければならない
    if ( offset % 8 ) ecalc_bin_printer_error( tree, "AArch64", "ldp Xt, Xt2, [Xbase] #offset", "offset is not a multiple of 8" );

    ecalc_bin_printer_print( tree, (uint8_t *)&buf, sizeof( buf ) );
}

static void ecalc_bin_printer_store_double_val_on_Xreg( ECALC_JIT_TREE *tree, uint8_t rbuf, uint8_t rbase, uint16_t offset, double val )
{
    // Xbase上の相対位置に倍精度実数値を書き込み
    ecalc_bin_printer_mov_double_val_to_Xreg( tree, rbuf, val );
    ecalc_bin_printer_str_Xreg_on_Xreg_offset_uimm12( tree, rbuf, rbase, offset );
}

static void ecalc_bin_printer_mov_u64_val_to_Xreg( ECALC_JIT_TREE *tree, uint8_t reg, uint64_t val )
{
    // Xregに64bit即値をセット
    /*
     * 0xFEDCBA98 76543210
     *
     * // 64bit即値をレジスタにロード
     * movz Xreg, 0x3210, lsl 0
     * movk Xreg, 0x7654, lsl 16
     * movk Xreg, 0xBA98, lsl 32
     * movk Xreg, 0xFEDC, lsl 48
     *
     */
    uint8_t *pv = (uint8_t *)&val;
    uint16_t vals[4];
    memcpy( &vals[0], pv + 0, 2 );
    memcpy( &vals[1], pv + 2, 2 );
    memcpy( &vals[2], pv + 4, 2 );
    memcpy( &vals[3], pv + 6, 2 );

    ecalc_bin_printer_movz_Xreg_uimm16( tree, reg, vals[0], A64_HW_0 );
    ecalc_bin_printer_movk_Xreg_uimm16( tree, reg, vals[1], A64_HW_16 );
    ecalc_bin_printer_movk_Xreg_uimm16( tree, reg, vals[2], A64_HW_32 );
    ecalc_bin_printer_movk_Xreg_uimm16( tree, reg, vals[3], A64_HW_48 );
}

static void ecalc_bin_printer_mov_double_val_to_Xreg( ECALC_JIT_TREE *tree, uint8_t reg, double val )
{
    // Xregに浮動小数点数をセット
    uint64_t buf;

    memcpy( &buf, &val, sizeof( buf ) );

    ecalc_bin_printer_mov_u64_val_to_Xreg( tree, reg, buf );
}

static void ecalc_bin_printer_load_arg_val_to_Xreg( ECALC_JIT_TREE *tree, uint8_t reg, uint16_t offset )
{
    // 引数（jit関数呼び出し時に渡された引数）へのポインタをレジスタにセット
    /*
     * mov x9, fr
     * ldr Xreg, [X9], #offset
     */

    ecalc_bin_printer_add_Xreg_Xreg_uimm12( tree, A64_X9, A64_FP, 0, 0 );
    ecalc_bin_printer_ldr_Xreg_on_Xreg_offset_uimm12( tree, reg, A64_X9, offset );
}

static void ecalc_bin_printer_load_exp_var_ptr_to_Xreg( ECALC_JIT_TREE *tree, uint8_t reg, uint16_t index )
{
    // regに、式で使う変数へのポインタをセットする

    // regにdouble**(変数ポインタ配列の先頭アドレス)をロード
    ecalc_bin_printer_load_arg_val_to_Xreg( tree, reg, 0 );

    // regにdouble*(変数ポインタ)をロード
    ecalc_bin_printer_ldr_Xreg_on_Xreg_offset_uimm12( tree, reg, reg, index * 8 );
}

static void ecalc_bin_printer_load_exp_ans_ptr_to_Xreg( ECALC_JIT_TREE *tree, uint8_t reg )
{
    // regに、答えへのポインタをセットする。
    ecalc_bin_printer_load_arg_val_to_Xreg( tree, reg, 16 );
}

static void ecalc_bin_printer_load_function_ptr_to_Xreg( ECALC_JIT_TREE *tree, uint8_t reg, void ( *func )( void ) )
{
    // 関数ポインタをXレジスタにセット
    ecalc_bin_printer_mov_u64_val_to_Xreg( tree, reg, (uint64_t)func );
}

static void ecalc_bin_printer_call_on_Xreg( ECALC_JIT_TREE *tree, uint8_t reg )
{
    // レジスタの値の場所をコールしリンクレジスタセット
    ecalc_bin_printer_blr( tree, reg );
}
