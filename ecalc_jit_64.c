#include "ecalc_jit_64_private.h"

// ecalcjit x86_64

ECALC_JIT_TREE *ecalc_create_jit_tree_amd64( struct ECALC_TOKEN *token )
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

void ecalc_free_jit_tree_amd64( ECALC_JIT_TREE *tree )
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

double ecalc_get_jit_tree_value_amd64( ECALC_JIT_TREE *tree, double **vars, double ans )
{
    // JIT木の値を取得
    double ret;
    double ( *func )( double **vars, double ans );

    // NULLなら何もしない
    if ( tree == NULL ) {
        return 0;
    }

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
    ecalc_bin_printer_opening_amd64( tree );

    // 木をJITに展開
    ecalc_bin_printer_tree_64( tree, token );

    // 関数終了
    ecalc_bin_printer_closing_amd64( tree );
}

static void ecalc_bin_printer_tree_64( ECALC_JIT_TREE *tree, struct ECALC_TOKEN *token )
{
    const int left  = -8;
    const int right = -16;
    const int dbuf  = -24;
    int depth = 32;
    size_t pos1, pos2, pos3, pos4, apos1, apos2, apos3;

    // EXP以外なら強制ゼロ
    if ( token->type != ECALC_TOKEN_EXP ) {
        ecalc_bin_printer_clear_xmm0( tree );
        return;
    }

    // RAXに現在のスタックトップの位置を設定
    ecalc_bin_printer_load_var_ptr_to_rax( tree, 0 );

    // left, rightを0クリア
    ecalc_bin_printer_store_double_val_on_rax( tree, left, 0 );
    ecalc_bin_printer_store_double_val_on_rax( tree, right, 0 );

    // 左辺値処理
    if ( token->left != NULL ) {
        if ( token->left->type == ECALC_TOKEN_LITE ) {
            // 左辺値定数リテラルを左にセット
            ecalc_bin_printer_store_double_val_on_rax( tree, left, token->left->value );
        } else if ( token->left->type == ECALC_TOKEN_VAR ) {
            // 左辺値変数値を左にセット
            ecalc_bin_printer_load_exp_var_ptr_to_rax( tree, token->left->value );
            ecalc_bin_printer_load_double_val_on_rax_to_xmm0( tree, 0 );

            // RAX復帰
            ecalc_bin_printer_load_var_ptr_to_rax( tree, 0 );
            ecalc_bin_printer_store_double_val_xmm0_on_rax( tree, left );
        } else if ( token->left->type == ECALC_TOKEN_EXP ) {
            // 左辺が式だった場合さらにJIT展開し戻り値 XMM0 を左にセット
            ecalc_bin_printer_add_rsp_i8( tree, -depth );
            ecalc_bin_printer_tree_64( tree, token->left );
            ecalc_bin_printer_add_rsp_i8( tree, +depth );

            // RAX復帰
            ecalc_bin_printer_load_var_ptr_to_rax( tree, 0 );
            ecalc_bin_printer_store_double_val_xmm0_on_rax( tree, left );
        } else {
            // 左を0に
            ecalc_bin_printer_store_double_val_on_rax( tree, left, 0 );
        }
    }

    // 右辺取得
    if ( token->right != NULL ) {
        if ( token->right->type == ECALC_TOKEN_LITE ) {
            // 右辺値定数リテラルを右にセット
            ecalc_bin_printer_store_double_val_on_rax( tree, right, token->right->value );
        } else if ( token->right->type == ECALC_TOKEN_VAR ) {
            // 右辺値変数値を右にセット
            ecalc_bin_printer_load_exp_var_ptr_to_rax( tree, token->right->value );
            ecalc_bin_printer_load_double_val_on_rax_to_xmm0( tree, 0 );
            // RAX復帰
            ecalc_bin_printer_load_var_ptr_to_rax( tree, 0 );
            ecalc_bin_printer_store_double_val_xmm0_on_rax( tree, right );
        } else if ( token->right->type == ECALC_TOKEN_EXP ) {
            // 右辺を式として扱う場合、ここで処理
            if ( token->value == ECALC_OPE_LOOP ) {
                // ループ処理 (@式)
                // 32bit版ではなぜか実数のまま処理しているので、整数に変換して処理する
                // 理由は、スタックが使いやすく、treeの呼び出しでループ変数が破壊されないから
                // RCX : カウンタ(初期値0)
                // RDX : left(ループ回数)
                // として比較実行

                // for ( i = 0; i != left; i++ ) {

                // ループ回数をRDXにロード
                // ecalc_bin_printer_load_var_ptr_to_rax( tree, 0 );
                ecalc_bin_printer_load_double_val_on_rax_to_xmm0( tree, left );
                ecalc_bin_printer_convert_double_xmm0_to_int_rdx( tree );

                // カウンタクリア
                ecalc_bin_printer_clear_rcx( tree );

                // 現在位置保存
                pos1 = ecalc_bin_printer_get_pos( tree );

                // RCXとRDXを比較
                ecalc_bin_printer_cmp_rcx_rdx( tree );

                // ZF(C3)が1ならRCX==RDXなのでpos3までジャンプ
                apos1 = ecalc_bin_printer_je( tree, 0 );

                // 現在位置保存
                pos2 = ecalc_bin_printer_get_pos( tree );

                // 実行 RCXとRDXが破壊される可能性があるので退避
                ecalc_bin_printer_add_rsp_i8( tree, -depth );
                ecalc_bin_printer_push_rcx( tree );
                ecalc_bin_printer_push_rdx( tree );
                ecalc_bin_printer_tree_64( tree, token->right );
                ecalc_bin_printer_pop_rdx( tree );
                ecalc_bin_printer_pop_rcx( tree );
                ecalc_bin_printer_add_rsp_i8( tree, +depth );

                // 戻り値をrightに設定
                // RAX復帰
                ecalc_bin_printer_load_var_ptr_to_rax( tree, 0 );
                ecalc_bin_printer_store_double_val_xmm0_on_rax( tree, right );

                // インクリメント
                ecalc_bin_printer_inc_rcx( tree );

                // pos1の比較まで戻る
                apos2 = ecalc_bin_printer_jmp( tree, 0 );

                // 現在位置保存
                pos3 = ecalc_bin_printer_get_pos( tree );

                // ジャンプアドレス埋め込み
                ecalc_bin_printer_set_address( tree, apos1, pos3 - pos2 );
                ecalc_bin_printer_set_address( tree, apos2, pos1 - pos3 );
                // }
            } else if ( token->value == ECALC_FUNC_IF ) {
                // if式
                // XMM1 : 0
                // XMM2 : left
                // として比較
                // XMM0, XMM1でもよかったかも

                // XMM0 = 0
                ecalc_bin_printer_clear_xmm0( tree );

                // XMM1 = left
                // ecalc_bin_printer_load_var_ptr_to_rax( tree, 0 );
                ecalc_bin_printer_load_double_val_on_rax_to_xmm1( tree, left );

                // XMM1とXMM2を比較
                ecalc_bin_printer_ucomisd_xmm0_xmm1( tree );

                // ZF(FPUのC3)が1ならXMM0==XMM1またはUnorderedなのでpos2までジャンプ
                apos1 = ecalc_bin_printer_je( tree, 0 );

                // 現在位置保存
                pos1 = ecalc_bin_printer_get_pos( tree );

                // 実行 ( 右の値は使わないので最後のXMM0は読まなくても良い )
                ecalc_bin_printer_add_rsp_i8( tree, -depth );
                ecalc_bin_printer_tree_64( tree, token->right );
                ecalc_bin_printer_add_rsp_i8( tree, +depth );
                // RAX復帰
                ecalc_bin_printer_load_var_ptr_to_rax( tree, 0 );
                ecalc_bin_printer_store_double_val_xmm0_on_rax( tree, right );

                // 現在位置保存
                pos2 = ecalc_bin_printer_get_pos( tree );

                // ジャンプアドレス埋め込み
                ecalc_bin_printer_set_address( tree, apos1, pos2 - pos1 );

                // right = left
                ecalc_bin_printer_load_double_val_on_rax_to_xmm0( tree, left );
                ecalc_bin_printer_store_double_val_xmm0_on_rax( tree, right );
            } else {
                // 右辺の式処理
                ecalc_bin_printer_add_rsp_i8( tree, -depth );
                ecalc_bin_printer_tree_64( tree, token->right );
                ecalc_bin_printer_add_rsp_i8( tree, +depth );
                // RAX復帰
                ecalc_bin_printer_load_var_ptr_to_rax( tree, 0 );
                ecalc_bin_printer_store_double_val_xmm0_on_rax( tree, right );
            }
        } else {
            // 右を0に
            ecalc_bin_printer_store_double_val_on_rax( tree, right, 0 );
        }
    }

    // 念の為RAX復帰
    // ecalc_bin_printer_load_var_ptr_to_rax( tree, 0 );

    // XMM0 = left, XMM1 = right
    ecalc_bin_printer_load_double_val_on_rax_to_xmm0( tree, left );
    ecalc_bin_printer_load_double_val_on_rax_to_xmm1( tree, right );

    // 計算 結果はXMM0に入っているものとする
    switch ( (int)token->value ) {
    case ECALC_OPE_ADD:
        // 足し算
        ecalc_bin_printer_addsd_xmm0_xmm1( tree );
        break;
    case ECALC_OPE_SUB:
        // 引き算
        ecalc_bin_printer_subsd_xmm0_xmm1( tree );
        break;
    case ECALC_OPE_MUL:
        // 掛け算
        ecalc_bin_printer_mulsd_xmm0_xmm1( tree );
        break;
    case ECALC_OPE_DIV:
        // 割り算
        // / 0はエラーを発生させない
        // XMM0 = 0としてXMM1 : rightと比較
        ecalc_bin_printer_clear_xmm0( tree );
        ecalc_bin_printer_ucomisd_xmm0_xmm1( tree );

        // right == 0ならpos2へ飛ぶ
        apos1 = ecalc_bin_printer_je( tree, 0 );

        // 現在位置保存
        pos1 = ecalc_bin_printer_get_pos( tree );

        // right != 0なので割り算
        ecalc_bin_printer_load_double_val_on_rax_to_xmm0( tree, left );
        ecalc_bin_printer_divsd_xmm0_xmm1( tree );

        // 無条件にpos3の終了までジャンプ
        apos2 = ecalc_bin_printer_jmp( tree, 0 );

        // 現在位置保存
        pos2 = ecalc_bin_printer_get_pos( tree );

        // right == 0なのでXMM0=0とする
        ecalc_bin_printer_clear_xmm0( tree );

        // 現在位置保存
        pos3 = ecalc_bin_printer_get_pos( tree );

        // ジャンプアドレス決定
        ecalc_bin_printer_set_address( tree, apos1, pos2 - pos1 );
        ecalc_bin_printer_set_address( tree, apos2, pos3 - pos2 );

        break;
    case ECALC_OPE_MOD:
        // 余り
        // % 0はエラーを発生させない
        // XMM0 = 0としてXMM1 : rightと比較
        ecalc_bin_printer_clear_xmm0( tree );
        ecalc_bin_printer_ucomisd_xmm0_xmm1( tree );

        // right == 0ならpos2へ飛ぶ
        apos1 = ecalc_bin_printer_je( tree, 0 );

        // 現在位置保存
        pos1 = ecalc_bin_printer_get_pos( tree );

        // right != 0なので余りを計算

        // 引数1 XMM0 = left
        // 引数2 XMM1 = right
        ecalc_bin_printer_load_double_val_on_rax_to_xmm0( tree, left );

        // 関数ポインタをraxにロード・コール
        ecalc_bin_printer_load_function_ptr_to_rax( tree, ecalc_get_func_addr( token->value ) );
        ecalc_bin_printer_call_on_rax( tree );

        // RAX復帰
        ecalc_bin_printer_load_var_ptr_to_rax( tree, 0 );

        // 無条件にpos3の終了までジャンプ
        apos2 = ecalc_bin_printer_jmp( tree, 0 );

        // 現在位置保存
        pos2 = ecalc_bin_printer_get_pos( tree );

        // right == 0なのでXMM0 = 0とする
        ecalc_bin_printer_clear_xmm0( tree );

        // 現在位置保存
        pos3 = ecalc_bin_printer_get_pos( tree );

        // ジャンプアドレス決定
        ecalc_bin_printer_set_address( tree, apos1, pos2 - pos1 );
        ecalc_bin_printer_set_address( tree, apos2, pos3 - pos2 );

        break;
    case ECALC_OPE_STI:
        // 代入
        // 左辺変数ポインタをEAXにロード
        ecalc_bin_printer_load_exp_var_ptr_to_rax( tree, token->left->value );

        // [RAX] <= XMM1 : right
        ecalc_bin_printer_store_double_val_xmm1_on_rax( tree, 0 );

        // XMM0 <= XMM1 式の値は右辺
        ecalc_bin_printer_mov_double_xmm1_to_xmm0( tree );

        // RAX復帰
        ecalc_bin_printer_load_var_ptr_to_rax( tree, 0 );

        break;
    case ECALC_OPE_SEPA:
        // 区切り
        // 式の値は右辺
        ecalc_bin_printer_mov_double_xmm1_to_xmm0( tree );
        break;
    case ECALC_OPE_LOOP:
        // 繰り返し
        // 式の値は右辺
        ecalc_bin_printer_mov_double_xmm1_to_xmm0( tree );
        break;
    case ECALC_OPE_LBIG:
        // 左が大きい

        // XMM0とXMM1を比較
        ecalc_bin_printer_ucomisd_xmm0_xmm1( tree );

        // ZF(C3)が1ならXMM0==XMM1なのでpos2までジャンプ
        apos1 = ecalc_bin_printer_je( tree, 0 );

        // 現在位置保存
        pos1 = ecalc_bin_printer_get_pos( tree );

        // CFが1ならXMM0 < XMM1なのでpos3までジャンプ
        apos2 = ecalc_bin_printer_jb( tree, 0 );

        // 現在位置保存
        pos2 = ecalc_bin_printer_get_pos( tree );

        // right < left なのでXMM0 = 1
        // ecalc_bin_printer_load_var_ptr_to_rax( tree, 0 );
        ecalc_bin_printer_store_double_val_on_rax( tree, dbuf, 1 );
        ecalc_bin_printer_load_double_val_on_rax_to_xmm0( tree, dbuf );

        // 無条件にpos3の終了までジャンプ
        apos3 = ecalc_bin_printer_jmp( tree, 0 );

        // 現在位置保存
        pos3 = ecalc_bin_printer_get_pos( tree );

        // left <= right なのでXMM0 = 0
        ecalc_bin_printer_clear_xmm0( tree );

        // 現在位置保存
        pos4 = ecalc_bin_printer_get_pos( tree );

        // ジャンプアドレス埋め込み
        ecalc_bin_printer_set_address( tree, apos1, pos2 - pos1 );
        ecalc_bin_printer_set_address( tree, apos2, pos3 - pos2 );
        ecalc_bin_printer_set_address( tree, apos3, pos4 - pos3 );

        break;
    case ECALC_OPE_RBIG:
        // 右が大きい

        // XMM0とXMM1を比較
        ecalc_bin_printer_ucomisd_xmm0_xmm1( tree );

        // ZF(C3)が1ならXMM0 == XMM1なのでpos3までジャンプ
        apos1 = ecalc_bin_printer_je( tree, 0 );

        // 現在位置保存
        pos1 = ecalc_bin_printer_get_pos( tree );

        // CFが1ならXMM0 < XMM1なのでpos3までジャンプ
        apos2 = ecalc_bin_printer_jb( tree, 0 );

        // 現在位置保存
        pos2 = ecalc_bin_printer_get_pos( tree );

        // right < left なのでXMM0 = 0
        ecalc_bin_printer_clear_xmm0( tree );

        // 無条件にpos3の終了までジャンプ
        apos3 = ecalc_bin_printer_jmp( tree, 0 );

        // 現在位置保存
        pos3 = ecalc_bin_printer_get_pos( tree );

        // left <= right なのでXMM0 = 1
        // ecalc_bin_printer_load_var_ptr_to_rax( tree, 0 );
        ecalc_bin_printer_store_double_val_on_rax( tree, dbuf, 1 );
        ecalc_bin_printer_load_double_val_on_rax_to_xmm0( tree, dbuf );

        // 現在位置保存
        pos4 = ecalc_bin_printer_get_pos( tree );

        // ジャンプアドレス埋め込み
        ecalc_bin_printer_set_address( tree, apos1, pos3 - pos1 );
        ecalc_bin_printer_set_address( tree, apos2, pos3 - pos2 );
        ecalc_bin_printer_set_address( tree, apos3, pos4 - pos3 );

        break;
    case ECALC_OPE_EQU:
        // 同じ

        // XMM0とXMM1を比較
        ecalc_bin_printer_ucomisd_xmm0_xmm1( tree );

        // ZF(C3)が1ならXMM0==XMM1なのでpos2までジャンプ
        apos1 = ecalc_bin_printer_je( tree, 0 );

        // 現在位置保存
        pos1 = ecalc_bin_printer_get_pos( tree );

        // right != left なのでXMM0 = 0
        ecalc_bin_printer_clear_xmm0( tree );

        // 無条件にpos3の終了までジャンプ
        apos2 = ecalc_bin_printer_jmp( tree, 0 );

        // 現在位置保存
        pos2 = ecalc_bin_printer_get_pos( tree );

        // right == left なのでなのでXMM0 = 1
        // ecalc_bin_printer_load_var_ptr_to_rax( tree, 0 );
        ecalc_bin_printer_store_double_val_on_rax( tree, dbuf, 1 );
        ecalc_bin_printer_load_double_val_on_rax_to_xmm0( tree, dbuf );

        // 現在位置保存
        pos3 = ecalc_bin_printer_get_pos( tree );

        // ジャンプアドレス埋め込み
        ecalc_bin_printer_set_address( tree, apos1, pos2 - pos1 );
        ecalc_bin_printer_set_address( tree, apos2, pos3 - pos2 );

        break;
    case ECALC_FUNC_SIN:
    case ECALC_FUNC_COS:
    case ECALC_FUNC_TAN:                /* tan */
    case ECALC_FUNC_ASIN:               /* asin */
    case ECALC_FUNC_ACOS:               /* acos */
    case ECALC_FUNC_ATAN:               /* atan */
    case ECALC_FUNC_LOG10:              /* log10 */
    case ECALC_FUNC_LOGN:               /* logn */
        // 引数としてXMM0 = rightをセット
        // ecalc_bin_printer_load_var_ptr_to_rax( tree, 0 );
        ecalc_bin_printer_load_double_val_on_rax_to_xmm0( tree, right );

        // 関数ポインタをraxにロード・コール
        ecalc_bin_printer_load_function_ptr_to_rax( tree, ecalc_get_func_addr( token->value ) );
        ecalc_bin_printer_add_rsp_i8( tree, -depth );
        ecalc_bin_printer_call_on_rax( tree );
        ecalc_bin_printer_add_rsp_i8( tree, +depth );
        // RAX復帰
        ecalc_bin_printer_load_var_ptr_to_rax( tree, 0 );

        break;
    case ECALC_FUNC_SQRT:
        // sqrt
        ecalc_bin_printer_sqrtsd_xmm0_xmm1( tree );

        break;
    case ECALC_FUNC_POW:
    case ECALC_FUNC_ATAN2:
        // POWER, ATAN2

        // 引数としてleft : XMM0, right : XMM1をセット、されている。
        // 関数ポインタをeaxにロード・コール
        ecalc_bin_printer_load_function_ptr_to_rax( tree, ecalc_get_func_addr( token->value ) );
        ecalc_bin_printer_add_rsp_i8( tree, -depth );
        ecalc_bin_printer_call_on_rax( tree );
        ecalc_bin_printer_add_rsp_i8( tree, +depth );
        // RAX復帰
        ecalc_bin_printer_load_var_ptr_to_rax( tree, 0 );

        break;
    case ECALC_FUNC_RAD:
        // rad
        // RAXを現在のスタック先頭に
        // ecalc_bin_printer_load_var_ptr_to_rax( tree, 0 );

        // XMM0 = XMM1
        ecalc_bin_printer_mov_double_xmm1_to_xmm0( tree );

        // PI / 180
        ecalc_bin_printer_store_double_val_on_rax( tree, dbuf, M_PI / 180 );
        ecalc_bin_printer_load_double_val_on_rax_to_xmm1( tree, dbuf );

        // PI / 180 かける
        ecalc_bin_printer_mulsd_xmm0_xmm1( tree );

        break;
    case ECALC_FUNC_DEG:
        // deg
        // RAXを現在のスタック先頭に
        // ecalc_bin_printer_load_var_ptr_to_rax( tree, 0 );

        // XMM0 = XMM1
        ecalc_bin_printer_mov_double_xmm1_to_xmm0( tree );

        // 180 / PI
        ecalc_bin_printer_store_double_val_on_rax( tree, dbuf, 180 / M_PI );
        ecalc_bin_printer_load_double_val_on_rax_to_xmm1( tree, dbuf );

        // 180 / PIかける
        ecalc_bin_printer_mulsd_xmm0_xmm1( tree );

        break;
    case ECALC_FUNC_PI:
        // π
        // ecalc_bin_printer_load_var_ptr_to_rax( tree, 0 );
        ecalc_bin_printer_store_double_val_on_rax( tree, dbuf, M_PI );
        ecalc_bin_printer_load_double_val_on_rax_to_xmm0( tree, dbuf );

        break;
    case ECALC_FUNC_EPS0:
        // ε0
        // ecalc_bin_printer_load_var_ptr_to_rax( tree, 0 );
        ecalc_bin_printer_store_double_val_on_rax( tree, dbuf, 8.85418782e-12 );
        ecalc_bin_printer_load_double_val_on_rax_to_xmm0( tree, dbuf );

        break;
    case ECALC_FUNC_ANS:
        // ans
        ecalc_bin_printer_load_exp_ans_ptr_to_rax( tree );
        ecalc_bin_printer_load_double_val_on_rax_to_xmm0( tree, 0 );
        // RAX復帰
        ecalc_bin_printer_load_var_ptr_to_rax( tree, 0 );

        break;
    case ECALC_FUNC_IF:
        // if文

        // 左破棄して右を返す
        ecalc_bin_printer_mov_double_xmm1_to_xmm0( tree );

        break;
    default:
        // デフォルトはゼロ
        ecalc_bin_printer_clear_xmm0( tree );
    }

    // 一つのノード出力完了
    // この時点でXMM0に答えがあるべき
}

static void ecalc_bin_printer_opening_amd64( ECALC_JIT_TREE *tree )
{
    // オープニング
    /*
    push rbp            55
    */
    uint8_t bin1[] = {0x55};

    // 引数をスタック上につんでおく
    /*
     * push rcx 51
     * or
     * push rdi 57
     */
#ifdef _WIN32
    uint8_t bin2[] = {0x51};
#else
    uint8_t bin2[] = {0x57};
#endif
    /*
     * movsd [rsp-8], xmm0  F20F 11 44 24 F8
     * add rsp, -8          48 83 C4 F8
     */
    uint8_t bin3[] = {0xF2, 0x0F, 0x11, 0x44, 0x24, 0xF8};

    /*
    mov rbp, rsp        48 89 E5
    */
    uint8_t bin4[] = {0x48, 0x89, 0xE5};

    ecalc_bin_printer_print( tree, bin1, sizeof( bin1 ) );
    ecalc_bin_printer_print( tree, bin2, sizeof( bin2 ) );
    ecalc_bin_printer_print( tree, bin3, sizeof( bin3 ) );
    ecalc_bin_printer_add_rsp_i8( tree, -8 );
    ecalc_bin_printer_print( tree, bin4, sizeof( bin4 ) );
}

static void ecalc_bin_printer_closing_amd64( ECALC_JIT_TREE *tree )
{
    // クロージング
    /*
    add rsp, 16
    pop ebp             5D
    ret                 C3
    */

    uint8_t bin[] = {0x5D, 0xC3};

    // つんでおいた引数分スタックを戻す
    ecalc_bin_printer_add_rsp_i8( tree, 16 );
    ecalc_bin_printer_print( tree, bin, sizeof( bin ) );
}

static void ecalc_bin_printer_load_var_ptr_to_rax( ECALC_JIT_TREE *tree, int8_t val )
{
    // RSPを基準としたポインタをRAXに読み込む
    /*
     * lea rax, [rsp+val]   48 8D4424 vv
     */

    uint8_t bin[] = {0x48, 0x8D, 0x44, 0x24, 0x00};

    bin[4] = val;

    ecalc_bin_printer_print( tree, bin, sizeof( bin ) );
}

static void ecalc_bin_printer_clear_rcx( ECALC_JIT_TREE *tree )
{
    // rcxをクリア
    /*
     * xor rax, rax  48 31 C9
     */

    uint8_t bin[] = {0x48, 0x31, 0xC9};

    ecalc_bin_printer_print( tree, bin, sizeof( bin ) );
}

static void ecalc_bin_printer_inc_rcx( ECALC_JIT_TREE *tree )
{
    // rcxをインクリメント
    /*
     * inc rcx  48 FF C1
     */

    uint8_t bin[] = {0x48, 0xFF, 0xC1};

    ecalc_bin_printer_print( tree, bin, sizeof( bin ) );
}

static void ecalc_bin_printer_push_rcx( ECALC_JIT_TREE *tree )
{
    // push rcx
    /*
     * push rcx 0x51
     */

    uint8_t bin[] = {0x51};

    ecalc_bin_printer_print( tree, bin, sizeof( bin ) );
}

static void ecalc_bin_printer_push_rdx( ECALC_JIT_TREE *tree )
{
    // push rdx
    /*
     * push rdx 0x52
     */

    uint8_t bin[] = {0x52};

    ecalc_bin_printer_print( tree, bin, sizeof( bin ) );
}

static void ecalc_bin_printer_pop_rcx( ECALC_JIT_TREE *tree )
{
    // pop rcx
    /*
     * pop rcx 0x59
     */

    uint8_t bin[] = {0x59};

    ecalc_bin_printer_print( tree, bin, sizeof( bin ) );
}

static void ecalc_bin_printer_pop_rdx( ECALC_JIT_TREE *tree )
{
    // pop rdx
    /*
     * pop rdx 0x5a
     */

    uint8_t bin[] = {0x5a};

    ecalc_bin_printer_print( tree, bin, sizeof( bin ) );
}

static void ecalc_bin_printer_clear_xmm0( ECALC_JIT_TREE *tree )
{
    // XMM0をクリア
    /*
     * pxor xmm0, xmm0  66 0f ef c0
     */

    uint8_t bin[] = {0x66, 0x0f, 0xef, 0xc0};

    ecalc_bin_printer_print( tree, bin, sizeof( bin ) );
}

static void ecalc_bin_printer_convert_double_xmm0_to_int_rdx( ECALC_JIT_TREE *tree )
{
    // XMM0 のdoubleをintに変換してrdxにコピー
    /*
     * cvttsd2si rdx, xmm0 F2 48 0F 2C D0
     */
    uint8_t bin[] = {0xF2, 0x48, 0x0F, 0x2C, 0xD0};

    ecalc_bin_printer_print( tree, bin, sizeof( bin ) );
}

static void ecalc_bin_printer_cmp_rcx_rdx( ECALC_JIT_TREE *tree )
{
    // RCXとRDXを比較
    /*
     * cmp rcx, rdx         48 39D1
     */
    uint8_t bin[] = {0x48, 0x39, 0xD1};

    ecalc_bin_printer_print( tree, bin, sizeof( bin ) );
}

static void ecalc_bin_printer_ucomisd_xmm0_xmm1( ECALC_JIT_TREE *tree )
{
    // xmm0とxmm1を比較
    /*
     * ucomisd xmm0, xmm1   66 0F 2E C1
     */
    uint8_t bin[] = {0x66, 0x0F, 0x2E, 0xC1};

    ecalc_bin_printer_print( tree, bin, sizeof( bin ) );
}

static void ecalc_bin_printer_addsd_xmm0_xmm1( ECALC_JIT_TREE *tree )
{
    // xmm0 <= xmm0 + xmm1
    /*
     * addsd xmm0, xmm1   F2 0F 58 C1
     */
    uint8_t bin[] = {0xF2, 0x0F, 0x58, 0xC1};

    ecalc_bin_printer_print( tree, bin, sizeof( bin ) );
}

static void ecalc_bin_printer_subsd_xmm0_xmm1( ECALC_JIT_TREE *tree )
{
    // xmm0 <= xmm0 - xmm1
    /*
     * subsd xmm0, xmm1   F2 0F 5C C1
     */
    uint8_t bin[] = {0xF2, 0x0F, 0x5C, 0xC1};

    ecalc_bin_printer_print( tree, bin, sizeof( bin ) );
}

static void ecalc_bin_printer_mulsd_xmm0_xmm1( ECALC_JIT_TREE *tree )
{
    // xmm0 <= xmm0 * xmm1
    /*
     * mulsd xmm0, xmm1   F2 0F 59 C1
     */
    uint8_t bin[] = {0xF2, 0x0F, 0x59, 0xC1};

    ecalc_bin_printer_print( tree, bin, sizeof( bin ) );
}

static void ecalc_bin_printer_divsd_xmm0_xmm1( ECALC_JIT_TREE *tree )
{
    // xmm0 <= xmm0 / xmm1
    /*
     * divsd xmm0, xmm1   F2 0F 5E C1
     */
    uint8_t bin[] = {0xF2, 0x0F, 0x5E, 0xC1};

    ecalc_bin_printer_print( tree, bin, sizeof( bin ) );
}

static void ecalc_bin_printer_sqrtsd_xmm0_xmm1( ECALC_JIT_TREE *tree )
{
    // xmm0 <= sqrt( xmm1 )
    /*
     * sqrtsd xmm0, xmm1   F2 0F 51 C1
     */
    uint8_t bin[] = {0xF2, 0x0F, 0x51, 0xC1};

    ecalc_bin_printer_print( tree, bin, sizeof( bin ) );
}

static void ecalc_bin_printer_load_double_val_on_rax_to_xmm1( ECALC_JIT_TREE *tree, int8_t pos )
{
    // XMM1 に [rax+pos]の先のdoubleをコピー
    /*
     * movsd xmm1, [rax+pos] f20f 1048pp
     */
    uint8_t bin[] = {0xF2, 0x0F, 0x10, 0x48, pos};

    ecalc_bin_printer_print( tree, bin, sizeof( bin ) );
}

static void ecalc_bin_printer_load_double_val_on_rax_to_xmm0( ECALC_JIT_TREE *tree, int8_t pos )
{
    // XMM0 に [rax+pos]の先のdoubleをコピー
    /*
     * movsd xmm0, [rax+pos] f20f 1068pp
     */
    uint8_t bin[] = {0xF2, 0x0F, 0x10, 0x40, pos};

    ecalc_bin_printer_print( tree, bin, sizeof( bin ) );
}

static void ecalc_bin_printer_store_double_val_xmm0_on_rax( ECALC_JIT_TREE *tree, int8_t pos )
{
    // [rax+pos]の先にXMM0のdoubleをコピー
    /*
     * movsd [rax+pos], xmm0  f20f 1140pp
     */
    uint8_t bin[] = {0xF2, 0x0F, 0x11, 0x40, pos};

    ecalc_bin_printer_print( tree, bin, sizeof( bin ) );
}

static void ecalc_bin_printer_store_double_val_xmm1_on_rax( ECALC_JIT_TREE *tree, int8_t pos )
{
    // [rax+pos]の先にXMM1のdoubleをコピー
    /*
     * movsd [rax+pos], xmm1  f20f 1148pp
     */
    uint8_t bin[] = {0xF2, 0x0F, 0x11, 0x48, pos};

    ecalc_bin_printer_print( tree, bin, sizeof( bin ) );
}

static void ecalc_bin_printer_store_double_val_on_rax(ECALC_JIT_TREE *tree, int8_t pos, double val)
{
    // doubleをrax+posの指す位置にストア
    ecalc_bin_printer_store_u64_val_to_rdx( tree, *(uint64_t *)(&val) );
    ecalc_bin_printer_store_rdx_on_rax( tree, pos );
}

static void ecalc_bin_printer_store_u64_val_to_rdx(ECALC_JIT_TREE *tree, uint64_t val)
{
    // rdxに64bit即値をセット
    /*
     * movabs rdx, val		48 BA vvvvvvvvvvvvvvvv
     */

    uint8_t bin[] = {0x48, 0xBA, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    memcpy( bin + 2, &val, sizeof(uint64_t) );

    ecalc_bin_printer_print( tree, bin, sizeof( bin ) );
}

static void ecalc_bin_printer_store_u64_val_to_rax(ECALC_JIT_TREE *tree, uint64_t val)
{
    // raxに64bit即値をセット
    /*
     * movabs rax, val		48 B8 vvvvvvvvvvvvvvvv
     */

    uint8_t bin[] = {0x48, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    memcpy( bin + 2, &val, sizeof(uint64_t) );

    ecalc_bin_printer_print( tree, bin, sizeof( bin ) );
}

void ecalc_bin_printer_load_function_ptr_to_rax( ECALC_JIT_TREE *tree, void ( *func )( void ) )
{
    // raxに関数ポインタ（即値）を読み込む
    ecalc_bin_printer_store_u64_val_to_rax( tree, (uint64_t)func );
}

static void ecalc_bin_printer_store_rdx_on_rax( ECALC_JIT_TREE *tree, int8_t pos )
{
    // [rax+p]にRDXをセット
    /*
     * mov QWORD PTR [rax+p], rdx
     */

    uint8_t bin[] = {0x48, 0x89, 0x50, pos};

    ecalc_bin_printer_print( tree, bin, sizeof( bin ) );
}

static void ecalc_bin_printer_call_on_rax( ECALC_JIT_TREE *tree )
{
    // RAXの指す関数をコール
    /*
     * call rax FFD0
     */
    uint8_t bin[] = {0xFF, 0xD0};

    ecalc_bin_printer_print( tree, bin, sizeof( bin ) );
}

static void ecalc_bin_printer_load_exp_var_ptr_to_rax( ECALC_JIT_TREE *tree, int index )
{
    // raxにvar[index]のポインタをセット
    uint8_t bin[] = {0x48, 0x8D, 0x04, 0xD0};

    // RAXにdouble**の場所をロード
    ecalc_bin_printer_load_arg_ptr_to_rax( tree, 8 );

    // RAXにdouble**の値（引数の値）をロード
    ecalc_bin_printer_load_rax_pointed_to_rax( tree, 0 );

    // indexをrdxにロード
    ecalc_bin_printer_load_val_to_rdx( tree, index );

    // 変数のアドレス計算
    // 48 8d04d0    lea rax, [rax+rdx*8]
    ecalc_bin_printer_print( tree, bin, sizeof( bin ) );

    // RAXにdouble*の値をロード
    ecalc_bin_printer_load_rax_pointed_to_rax( tree, 0 );
}

static void ecalc_bin_printer_load_exp_ans_ptr_to_rax(ECALC_JIT_TREE *tree)
{
    // eaxにansのポインタセット
    ecalc_bin_printer_load_arg_ptr_to_rax( tree, 0 );
}

static void ecalc_bin_printer_load_arg_ptr_to_rax( ECALC_JIT_TREE *tree, int8_t val )
{
    // RBPを先頭とする引数のポインタをEAXに読み込む
    /*
     * lea rax, [rbp+val]   48 8D45vv
     */
    uint8_t bin[] = {0x48, 0x8D, 0x45, 0x00};

    bin[3] = val;

    ecalc_bin_printer_print( tree, bin, sizeof( bin ) );
}

static void ecalc_bin_printer_load_val_to_rdx( ECALC_JIT_TREE *tree, int32_t val )
{
    // RDXに32ビット整数valをロード
    /*
     * mov rdx, val 48 C7C2 vvvvvvvv
     */
    uint8_t bin[] = {0x48, 0xC7, 0xC2, 0x00, 0x00, 0x00, 0x00};

    memcpy( bin + 3, &val, sizeof(int32_t) );

    ecalc_bin_printer_print( tree, bin, sizeof( bin ) );
}

static void ecalc_bin_printer_mov_double_xmm1_to_xmm0( ECALC_JIT_TREE *tree )
{
    // XMM0 <- XMM1
    /*
     * movsd xmm0, xmm1 f20f10c1
     */
    uint8_t bin[] = {0xF2, 0x0F, 0x10, 0xC1};

    ecalc_bin_printer_print( tree, bin, sizeof( bin ) );
}

static void ecalc_bin_printer_load_rax_pointed_to_rax( ECALC_JIT_TREE *tree, int8_t val )
{
    // RAXの指すメモリ空間上にある値をRAXにコピー
    /*
     * mov rax, [rax+val]   48 8B40 vv
     */
    uint8_t bin[] = {0x48, 0x8B, 0x40, val};

    ecalc_bin_printer_print( tree, bin, sizeof( bin ) );
}

static void ecalc_bin_printer_add_rsp_i8(ECALC_JIT_TREE *tree, int8_t val)
{
    // RSPにvalをたす
    /*
     * add esp, val 48 83C4vv
     */
    uint8_t bin[] = {0x48, 0x83, 0xC4, val};

    ecalc_bin_printer_print( tree, bin, sizeof( bin ) );
}

static size_t ecalc_bin_printer_je(ECALC_JIT_TREE *tree, int32_t val)
{
    // Z=1なら32bit相対ジャンプ
    // アドレスを後で入力するためにアドレスを書く位置を返す
    /*
     * je 指定先	0F84vvvvvvvv
     */
    unsigned char bin[] = {0x0F, 0x84, 0x00, 0x00, 0x00, 0x00};

    memcpy( bin + 2, &val, sizeof(int32_t) );

    ecalc_bin_printer_print( tree, bin, sizeof( bin ) );

    return tree->pos - 4;
}

static size_t ecalc_bin_printer_jmp(ECALC_JIT_TREE *tree, int32_t val)
{
    // 32bit相対ジャンプ
    // アドレスを後で入力するためにアドレスを書く位置を返す
    /*
     * jmp 指定先	E9vvvvvvvv
     */
    unsigned char bin[] = {0xE9, 0x00, 0x00, 0x00, 0x00};

    memcpy( bin + 1, &val, sizeof(int32_t) );

    ecalc_bin_printer_print( tree, bin, sizeof( bin ) );

    return tree->pos - 4;
}

static size_t ecalc_bin_printer_jb(ECALC_JIT_TREE *tree, int32_t val)
{
    // CF=1なら32bit相対ジャンプ
    // アドレスを後で入力するためにアドレスを書く位置を返す
    /*
     * je 指定先	0F82vvvvvvvv
     */
    unsigned char bin[] = {0x0F, 0x82, 0x00, 0x00, 0x00, 0x00};

    memcpy( bin + 2, &val, sizeof(int32_t) );

    ecalc_bin_printer_print( tree, bin, sizeof( bin ) );

    return tree->pos - 4;
}
