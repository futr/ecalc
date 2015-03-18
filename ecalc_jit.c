#include "ecalc_jit.h"

// ecalcjitエンジン 32bitのWindows用
// 出力するバイナリが86用、呼び出し規約も86用cdeclです
// 命令セットはi686以降（だと思います）

ECALC_JIT_TREE *ecalc_create_jit_tree( struct ECALC_TOKEN *token )
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

void ecalc_free_jit_tree( ECALC_JIT_TREE *tree )
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

void *ecalc_allocate_jit_memory( size_t size )
{
    // JIT用バイナリ空間確保
#ifdef _WIN32
    return VirtualAlloc( NULL, size, MEM_COMMIT, PAGE_EXECUTE_READWRITE );
#else
    void *ptr;
    long pageSize;

    // Detect page size
#ifdef _SC_PAGESIZE
    pageSize = sysconf( _SC_PAGESIZE );
#elif defined(_SC_PAGE_SIZE)
    pageSize = sysconf( _SC_PAGE_SIZE );
#else
    pageSize = 4096;
#endif

    // posix_memaling and mprotect?
    // Allocate jit memory space
    ptr = mmap( NULL, size, PROT_EXEC | PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0 );

    if ( ptr == MAP_FAILED ) {
        return NULL;
    }

    return ptr;
#endif
}

void ecalc_free_jit_memory( void *data , size_t size )
{
    // JIT用バイナリ空間破棄
#ifdef _WIN32
    VirtualFree( data, 0, MEM_RELEASE );
#else
    if ( data != NULL ) {
        munmap( data, size );
    }
#endif
}

double ecalc_get_jit_tree_value( ECALC_JIT_TREE *tree, double **vars, double ans )
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

size_t ecalc_get_jit_tree_size( ECALC_JIT_TREE *tree, struct ECALC_TOKEN *token )
{
    // 必要なメモリ量を(多めに雑に)計算
    size_t size = 0;

    // tree->dataがNULLの状態で印刷することでtree->posからサイズが分かる
    ecalc_bin_printer( tree, token );

    // 関数バイナリ出力位置からサイズを取得
    size = tree->pos;

    return size;
}

void ecalc_bin_printer( ECALC_JIT_TREE *tree, struct ECALC_TOKEN *token )
{
    // バイナリ出力開始

    // 関数開始
    ecalc_bin_printer_opening( tree );

    // 木をJITに展開
    ecalc_bin_printer_tree( tree, token );

    // 関数終了
    ecalc_bin_printer_closing( tree );
}

void ecalc_bin_printer_tree( ECALC_JIT_TREE *tree, struct ECALC_TOKEN *token )
{
    // 木をバイナリへ
    /*
     * eaxはポインタレジスタ、ecxはeaxのバッファ、edxは定数レジスタとした
     * ebxは保存しなければならないので使っていない
     * 戻り値はst(0)に入れる
     * EBPは常にコールされた時点でのESPを指している
     * つまりavrs,ansにはEBP経由でアクセス可能
     * ecalc_bin_printer_treeをネストしてコールする場合cdeclと同様にレジスタを破壊するので
     * 呼び出し側でスタック位置操作、レジスタ退避などが必要
     */
    const int left  = -8;
    const int right = -16;
    const int dbuf  = -24;
    const int arg2  = -32;
    const int arg1  = -40;
    int depth = 40;
    size_t pos1, pos2, pos3, pos4, apos1, apos2, apos3;

    // EAXを現在のスタックトップに
    ecalc_bin_printer_load_var_ptr_to_eax( tree, 0 );

    // EXP以外なら強制ゼロ
    if ( token->type != ECALC_TOKEN_EXP ) {
        ecalc_bin_printer_store_double_val( tree, dbuf, 0 );
        ecalc_bin_printer_fld( tree, dbuf );

        return;
    }

    // left, rightを0クリア
    ecalc_bin_printer_store_double_val( tree, left, 0 );
    ecalc_bin_printer_store_double_val( tree, right, 0 );

    // 左辺値処理
    if ( token->left != NULL ) {
        if ( token->left->type == ECALC_TOKEN_LITE ) {
            // 左辺値定数リテラルを左にセット
            ecalc_bin_printer_store_double_val( tree, left, token->left->value );
        } else if ( token->left->type == ECALC_TOKEN_VAR ) {
            // 左辺値変数値を左にセット
            ecalc_bin_printer_load_exp_var_ptr_to_eax( tree, token->left->value );
            ecalc_bin_printer_fld( tree, 0 );
            ecalc_bin_printer_load_var_ptr_to_eax( tree, 0 );
            ecalc_bin_printer_fstp( tree, left );
        } else if ( token->left->type == ECALC_TOKEN_EXP ) {
            // 左辺が式だった場合さらにJIT展開し戻り地st(0)を左にセット
            ecalc_bin_printer_add_esp_i8( tree, -depth );
            ecalc_bin_printer_tree( tree, token->left );
            ecalc_bin_printer_add_esp_i8( tree, +depth );
            ecalc_bin_printer_load_var_ptr_to_eax( tree, 0 );
            ecalc_bin_printer_fstp( tree, left );
        } else {
            // 左を0に
            ecalc_bin_printer_store_double_val( tree, left, 0 );
        }
    }

    // 右辺取得
    if ( token->right != NULL ) {
        if ( token->right->type == ECALC_TOKEN_LITE ) {
            // 右辺値定数リテラルを右にセット
            ecalc_bin_printer_store_double_val( tree, right, token->right->value );
        } else if ( token->right->type == ECALC_TOKEN_VAR ) {
            // 右辺値変数値を右にセット
            ecalc_bin_printer_load_exp_var_ptr_to_eax( tree, token->right->value );
            ecalc_bin_printer_fld( tree, 0 );
            ecalc_bin_printer_load_var_ptr_to_eax( tree, 0 );
            ecalc_bin_printer_fstp( tree, right );
        } else if ( token->right->type == ECALC_TOKEN_EXP ) {
            // 右辺を式として扱う場合、ここで処理
            if ( token->value == ECALC_OPE_LOOP ) {
                // ループ処理 (@式)
                // st(0) : カウンタ(初期値0)
                // st(1) : 1
                // st(2) : left(ループ回数)
                // として比較実行

                // for ( i = 0; i != left; i++ ) {

                // PUSH
                ecalc_bin_printer_fld( tree, left );
                ecalc_bin_printer_fld1( tree );
                ecalc_bin_printer_fldz( tree );

                // 現在位置保存
                pos1 = ecalc_bin_printer_get_pos( tree );

                // st(0)とst(2)を比較
                ecalc_bin_printer_fcomi( tree, 2 );

                // ZF(C3)が1ならst(0)==st(2)なのでpos3までジャンプ
                apos1 = ecalc_bin_printer_je( tree, 0 );

                // 現在位置保存
                pos2 = ecalc_bin_printer_get_pos( tree );

                // POP
                ecalc_bin_printer_fstp( tree, dbuf );
                ecalc_bin_printer_fstp_st0( tree );
                ecalc_bin_printer_fstp_st0( tree );

                // 実行
                ecalc_bin_printer_add_esp_i8( tree, -depth );
                ecalc_bin_printer_tree( tree, token->right );
                ecalc_bin_printer_add_esp_i8( tree, +depth );
                ecalc_bin_printer_load_var_ptr_to_eax( tree, 0 );
                ecalc_bin_printer_fstp( tree, right );

                // PUSH
                ecalc_bin_printer_fld( tree, left );
                ecalc_bin_printer_fld1( tree );
                ecalc_bin_printer_fld( tree, dbuf );

                // インクリメント
                ecalc_bin_printer_fadd( tree, 1 );

                // pos1の比較まで戻る
                apos2 = ecalc_bin_printer_jmp( tree, 0 );

                // 現在位置保存
                pos3 = ecalc_bin_printer_get_pos( tree );

                // ジャンプアドレス埋め込み
                ecalc_bin_printer_set_address( tree, apos1, pos3 - pos2 );
                ecalc_bin_printer_set_address( tree, apos2, pos1 - pos3 );

                // FPUスタッククリア
                ecalc_bin_printer_fstp_st0( tree );
                ecalc_bin_printer_fstp_st0( tree );
                ecalc_bin_printer_fstp_st0( tree );

                // }
            } else if ( token->value == ECALC_FUNC_IF ) {
                // if式
                // st(0) : 0
                // st(1) : left
                // として比較

                // PUSH
                ecalc_bin_printer_fldz( tree );
                ecalc_bin_printer_fld( tree, left );

                // st(0)とst(1)を比較
                ecalc_bin_printer_fcomi( tree, 1 );

                // ZF(FPUのC3)が1ならst(0)==st(1)なのでpos2までジャンプ
                apos1 = ecalc_bin_printer_je( tree, 0 );

                // 現在位置保存
                pos1 = ecalc_bin_printer_get_pos( tree );

                // POP
                ecalc_bin_printer_fstp_st0( tree );
                ecalc_bin_printer_fstp_st0( tree );

                // 実行 ( 右の値は使わないので最後のstpはただのPOPでもよい )
                ecalc_bin_printer_add_esp_i8( tree, -depth );
                ecalc_bin_printer_tree( tree, token->right );
                ecalc_bin_printer_add_esp_i8( tree, +depth );
                ecalc_bin_printer_load_var_ptr_to_eax( tree, 0 );
                ecalc_bin_printer_fstp( tree, right );

                // ダミーPUSH
                ecalc_bin_printer_fldz( tree );
                ecalc_bin_printer_fldz( tree );

                // 現在位置保存
                pos2 = ecalc_bin_printer_get_pos( tree );

                // ジャンプアドレス埋め込み
                ecalc_bin_printer_set_address( tree, apos1, pos2 - pos1 );

                // FPUスタッククリア
                ecalc_bin_printer_fstp_st0( tree );
                ecalc_bin_printer_fstp_st0( tree );

                // right = left
                ecalc_bin_printer_fld( tree, left );
                ecalc_bin_printer_fstp( tree, right );
            } else {
                // 右辺の式処理
                ecalc_bin_printer_add_esp_i8( tree, -depth );
                ecalc_bin_printer_tree( tree, token->right );
                ecalc_bin_printer_add_esp_i8( tree, +depth );
                ecalc_bin_printer_load_var_ptr_to_eax( tree, 0 );
                ecalc_bin_printer_fstp( tree, right );
            }
        } else {
            // 右を0に
            ecalc_bin_printer_store_double_val( tree, right, 0 );
        }
    }

    // st(1) = left, st(0) = right
    ecalc_bin_printer_fld( tree, left );
    ecalc_bin_printer_fld( tree, right );

    // 計算 結果はスタックレベルが2の状態でst(0)に入っているものとする
    // それぞれ自分のcase内でst(0)に結果を入れてスタックレベルを1にする
    switch ( (int)token->value ) {
    case ECALC_OPE_ADD:
        // 足し算
        ecalc_bin_printer_faddp( tree );
        break;
    case ECALC_OPE_SUB:
        // 引き算
        ecalc_bin_printer_fsubp( tree );
        break;
    case ECALC_OPE_MUL:
        // 掛け算
        ecalc_bin_printer_fmulp( tree );
        break;
    case ECALC_OPE_DIV:
        // 割り算
        // / 0はエラーを発生させない
        // st(0) = 0としてst(1) : rightと比較
        ecalc_bin_printer_fldz( tree );
        ecalc_bin_printer_fcomi( tree, 1 );

        // right == 0ならpos2へ飛ぶ
        apos1 = ecalc_bin_printer_je( tree, 0 );

        // 現在位置保存
        pos1 = ecalc_bin_printer_get_pos( tree );

        // right != 0なので割り算
        ecalc_bin_printer_fstp_st0( tree );
        ecalc_bin_printer_fdivp( tree );

        // 無条件にpos3の終了までジャンプ
        apos2 = ecalc_bin_printer_jmp( tree, 0 );

        // 現在位置保存
        pos2 = ecalc_bin_printer_get_pos( tree );

        // right == 0なのでst(0)=0とする
        ecalc_bin_printer_fstp_st0( tree );
        ecalc_bin_printer_fstp_st0( tree );
        ecalc_bin_printer_fstp_st0( tree );
        ecalc_bin_printer_fldz( tree );

        // 現在位置保存
        pos3 = ecalc_bin_printer_get_pos( tree );

        // ジャンプアドレス決定
        ecalc_bin_printer_set_address( tree, apos1, pos2 - pos1 );
        ecalc_bin_printer_set_address( tree, apos2, pos3 - pos2 );

        break;
    case ECALC_OPE_MOD:
        // 余り
        // % 0はエラーを発生させない
        // st(0) = 0としてst(1) : rightと比較
        ecalc_bin_printer_fldz( tree );
        ecalc_bin_printer_fcomi( tree, 1 );

        // right == 0ならpos2へ飛ぶ
        apos1 = ecalc_bin_printer_je( tree, 0 );

        // 現在位置保存
        pos1 = ecalc_bin_printer_get_pos( tree );

        // right != 0なので余りを計算
        // 引数POP and PUSH
        ecalc_bin_printer_fstp_st0( tree );
        ecalc_bin_printer_fstp( tree, arg2 );
        ecalc_bin_printer_fstp( tree, arg1 );

        // 関数ポインタをeaxにロード・コール
        ecalc_bin_printer_load_function_ptr_to_eax( tree, ecalc_get_func_addr( token->value ) );
        ecalc_bin_printer_add_esp_i8( tree, -depth );
        ecalc_bin_printer_call( tree );
        ecalc_bin_printer_add_esp_i8( tree, +depth );
        ecalc_bin_printer_load_var_ptr_to_eax( tree, 0 );

        // 無条件にpos3の終了までジャンプ
        apos2 = ecalc_bin_printer_jmp( tree, 0 );

        // 現在位置保存
        pos2 = ecalc_bin_printer_get_pos( tree );

        // right == 0なのでst(0)=0, st(1)=rightとする
        ecalc_bin_printer_fstp_st0( tree );
        ecalc_bin_printer_fstp_st0( tree );
        ecalc_bin_printer_fstp_st0( tree );
        ecalc_bin_printer_fldz( tree );

        // 現在位置保存
        pos3 = ecalc_bin_printer_get_pos( tree );

        // ジャンプアドレス決定
        ecalc_bin_printer_set_address( tree, apos1, pos2 - pos1 );
        ecalc_bin_printer_set_address( tree, apos2, pos3 - pos2 );

        break;
    case ECALC_OPE_STI:
        // 代入
        // 左辺変数ポインタをEAXにロード
        ecalc_bin_printer_load_exp_var_ptr_to_eax( tree, token->left->value );

        // 代入
        ecalc_bin_printer_fstp( tree, 0 );
        ecalc_bin_printer_fstp_st0( tree );

        // st(0)セット
        ecalc_bin_printer_fld( tree, 0 );

        // EAX復帰
        ecalc_bin_printer_load_var_ptr_to_eax( tree, 0 );

        break;
    case ECALC_OPE_SEPA:
        // 区切り
        ecalc_bin_printer_fxch_st1( tree );
        ecalc_bin_printer_fstp_st0( tree );
        break;
    case ECALC_OPE_LOOP:
        // 繰り返し
        ecalc_bin_printer_fxch_st1( tree );
        ecalc_bin_printer_fstp_st0( tree );
        break;
    case ECALC_OPE_LBIG:
        // 左が大きい

        // st(0)とst(1)を比較
        ecalc_bin_printer_fcomi( tree, 1 );

        // POP
        ecalc_bin_printer_fstp_st0( tree );
        ecalc_bin_printer_fstp_st0( tree );

        // ZF(C3)が1ならst(0)==st(1)なのでpos2までジャンプ
        apos1 = ecalc_bin_printer_je( tree, 0 );

        // 現在位置保存
        pos1 = ecalc_bin_printer_get_pos( tree );

        // CF(FPUのC0)が1ならst(0) < st(1)なのでpos3までジャンプ
        apos2 = ecalc_bin_printer_jb( tree, 0 );

        // 現在位置保存
        pos2 = ecalc_bin_printer_get_pos( tree );

        // left <= right なので0をPUSH
        ecalc_bin_printer_fldz( tree );

        // 無条件にpos3の終了までジャンプ
        apos3 = ecalc_bin_printer_jmp( tree, 0 );

        // 現在位置保存
        pos3 = ecalc_bin_printer_get_pos( tree );

        // right < left なので1をPUSH
        ecalc_bin_printer_fld1( tree );

        // 現在位置保存
        pos4 = ecalc_bin_printer_get_pos( tree );

        // ジャンプアドレス埋め込み
        ecalc_bin_printer_set_address( tree, apos1, pos2 - pos1 );
        ecalc_bin_printer_set_address( tree, apos2, pos3 - pos2 );
        ecalc_bin_printer_set_address( tree, apos3, pos4 - pos3 );

        break;
    case ECALC_OPE_RBIG:
        // 右が大きい

        // st(0)とst(1)を比較
        ecalc_bin_printer_fcomi( tree, 1 );

        // POP
        ecalc_bin_printer_fstp_st0( tree );
        ecalc_bin_printer_fstp_st0( tree );

        // ZF(C3)が1ならst(0)==st(1)なのでpos3までジャンプ
        apos1 = ecalc_bin_printer_je( tree, 0 );

        // 現在位置保存
        pos1 = ecalc_bin_printer_get_pos( tree );

        // CF(FPUのC0)が1ならst(0) < st(1)なのでpos3までジャンプ
        apos2 = ecalc_bin_printer_jb( tree, 0 );

        // 現在位置保存
        pos2 = ecalc_bin_printer_get_pos( tree );

        // right > left なので1をPUSH
        ecalc_bin_printer_fld1( tree );

        // 無条件にpos3の終了までジャンプ
        apos3 = ecalc_bin_printer_jmp( tree, 0 );

        // 現在位置保存
        pos3 = ecalc_bin_printer_get_pos( tree );

        // right <= left なので0をPUSH
        ecalc_bin_printer_fldz( tree );

        // 現在位置保存
        pos4 = ecalc_bin_printer_get_pos( tree );

        // ジャンプアドレス埋め込み
        ecalc_bin_printer_set_address( tree, apos1, pos3 - pos1 );
        ecalc_bin_printer_set_address( tree, apos2, pos3 - pos2 );
        ecalc_bin_printer_set_address( tree, apos3, pos4 - pos3 );

        break;
    case ECALC_OPE_EQU:
        // 同じ

        // st(0)とst(1)を比較
        ecalc_bin_printer_fcomi( tree, 1 );

        // POP
        ecalc_bin_printer_fstp_st0( tree );
        ecalc_bin_printer_fstp_st0( tree );

        // ZF(C3)が1ならst(0)==st(1)なのでpos2までジャンプ
        apos1 = ecalc_bin_printer_je( tree, 0 );

        // 現在位置保存
        pos1 = ecalc_bin_printer_get_pos( tree );

        // right != left なので0をPUSH
        ecalc_bin_printer_fldz( tree );

        // 無条件にpos3の終了までジャンプ
        apos2 = ecalc_bin_printer_jmp( tree, 0 );

        // 現在位置保存
        pos2 = ecalc_bin_printer_get_pos( tree );

        // right == left なので1をPUSH
        ecalc_bin_printer_fld1( tree );

        // 現在位置保存
        pos3 = ecalc_bin_printer_get_pos( tree );

        // ジャンプアドレス埋め込み
        ecalc_bin_printer_set_address( tree, apos1, pos2 - pos1 );
        ecalc_bin_printer_set_address( tree, apos2, pos3 - pos2 );

        break;
    case ECALC_FUNC_SIN:
        // sin
        ecalc_bin_printer_fsin( tree );
        ecalc_bin_printer_fxch_st1( tree );
        ecalc_bin_printer_fstp_st0( tree );

        break;
    case ECALC_FUNC_COS:
        // cos
        ecalc_bin_printer_fcos( tree );
        ecalc_bin_printer_fxch_st1( tree );
        ecalc_bin_printer_fstp_st0( tree );

        break;
    case ECALC_FUNC_TAN:                /* tan */
    case ECALC_FUNC_ASIN:               /* asin */
    case ECALC_FUNC_ACOS:               /* acos */
    case ECALC_FUNC_ATAN:               /* atan */
    case ECALC_FUNC_LOG10:              /* log10 */
    case ECALC_FUNC_LOGN:               /* logn */
        // 引数としてst(0) = rightをセット
        ecalc_bin_printer_fstp( tree, arg1 );
        ecalc_bin_printer_fstp_st0( tree );

        // 関数ポインタをeaxにロード・コール
        ecalc_bin_printer_load_function_ptr_to_eax( tree, ecalc_get_func_addr( token->value ) );
        ecalc_bin_printer_add_esp_i8( tree, -depth );
        ecalc_bin_printer_call( tree );
        ecalc_bin_printer_add_esp_i8( tree, +depth );
        ecalc_bin_printer_load_var_ptr_to_eax( tree, 0 );

        break;
    case ECALC_FUNC_SQRT:
        // sqrt
        ecalc_bin_printer_fsqrt( tree );
        ecalc_bin_printer_fxch_st1( tree );
        ecalc_bin_printer_fstp_st0( tree );

        break;
    case ECALC_FUNC_POW:
    case ECALC_FUNC_ATAN2:
        // POWER, ATAN2

        // 引数としてleft : arg1, right : arg2をセット
        ecalc_bin_printer_fstp( tree, arg2 );
        ecalc_bin_printer_fstp( tree, arg1 );

        // 関数ポインタをeaxにロード・コール
        ecalc_bin_printer_load_function_ptr_to_eax( tree, ecalc_get_func_addr( token->value ) );
        ecalc_bin_printer_add_esp_i8( tree, -depth );
        ecalc_bin_printer_call( tree );
        ecalc_bin_printer_add_esp_i8( tree, +depth );
        ecalc_bin_printer_load_var_ptr_to_eax( tree, 0 );

        break;
    case ECALC_FUNC_RAD:
        // rad

        // 左破棄
        ecalc_bin_printer_fxch_st1( tree );
        ecalc_bin_printer_fstp_st0( tree );

        // 180わる
        ecalc_bin_printer_store_double_val( tree, dbuf, 180 );
        ecalc_bin_printer_fld( tree, dbuf );
        ecalc_bin_printer_fdivp( tree );

        // PIかける
        ecalc_bin_printer_fldpi( tree );
        ecalc_bin_printer_fmulp( tree );

        break;
    case ECALC_FUNC_DEG:
        // deg

        // 左破棄
        ecalc_bin_printer_fxch_st1( tree );
        ecalc_bin_printer_fstp_st0( tree );

        // PIわる
        ecalc_bin_printer_fldpi( tree );
        ecalc_bin_printer_fdivp( tree );

        // 180かける
        ecalc_bin_printer_store_double_val( tree, dbuf, 180 );
        ecalc_bin_printer_fld( tree, dbuf );
        ecalc_bin_printer_fmulp( tree );

        break;
    case ECALC_FUNC_PI:
        // π
        ecalc_bin_printer_fstp_st0( tree );
        ecalc_bin_printer_fstp_st0( tree );
        ecalc_bin_printer_fldpi( tree );
        // ecalc_bin_printer_store_double_val( tree, dbuf, M_PI );
        // ecalc_bin_printer_fld( tree, dbuf );

        break;
    case ECALC_FUNC_EPS0:
        // ε0
        ecalc_bin_printer_fstp_st0( tree );
        ecalc_bin_printer_fstp_st0( tree );
        ecalc_bin_printer_store_double_val( tree, dbuf, 8.85418782e-12 );
        ecalc_bin_printer_fld( tree, dbuf );

        break;
    case ECALC_FUNC_ANS:
        // ans
        ecalc_bin_printer_fstp_st0( tree );
        ecalc_bin_printer_fstp_st0( tree );
        ecalc_bin_printer_load_exp_ans_ptr_to_eax( tree );
        ecalc_bin_printer_fld( tree, 0 );
        ecalc_bin_printer_load_var_ptr_to_eax( tree, 0 );

        break;
    case ECALC_FUNC_IF:
        // if文

        // 左破棄して右を返す
        ecalc_bin_printer_fxch_st1( tree );
        ecalc_bin_printer_fstp_st0( tree );

        break;
    default:
        // デフォルトはゼロ
        ecalc_bin_printer_fstp_st0( tree );
        ecalc_bin_printer_fstp_st0( tree );
        ecalc_bin_printer_fldz( tree );
    }

    // 一つのノード出力完了
    // この時点でFPUスタックレベルは1でst(0)に答えがあるべき
}

void ecalc_bin_printer_print( ECALC_JIT_TREE *tree, unsigned char *buf, size_t size )
{
    // もらったバイト列を書き込み
    if ( tree->data != NULL ) {
        memcpy( tree->data + tree->pos, buf, size );
    }

    tree->pos += size;
}

size_t ecalc_bin_printer_get_pos( ECALC_JIT_TREE *tree )
{
    // 現在位置取得
    return tree->pos;
}

void ecalc_bin_printer_reset_tree( ECALC_JIT_TREE *tree )
{
    // 構造体をクリアする（サイズ測定などに使う）
    tree->pos  = 0;
    tree->size = 0;
    tree->data = NULL;
    tree->pos  = 0;
}

void ecalc_bin_printer_set_address( ECALC_JIT_TREE *tree, size_t pos, int32_t address )
{
    // アドレスを後で入れるための補助関数
    if ( tree->data != NULL ) {
        memcpy( tree->data + pos, &address, sizeof( address ) );
    }
}

void ecalc_bin_printer_opening( ECALC_JIT_TREE *tree )
{
    // オープニング
    /*
    push ebp			55
    mov ebp, esp		8BEC もしくは 89E5
    */
    unsigned char bin[] = {0x55, 0x89, 0xE5};

    ecalc_bin_printer_print( tree, bin, sizeof( bin ) );
}

void ecalc_bin_printer_closing( ECALC_JIT_TREE *tree )
{
    // クロージング
    /*
    pop ebp				5D
    ret					C3
    */

    unsigned char bin[] = {0x5D, 0xC3};

    ecalc_bin_printer_print( tree, bin, sizeof( bin ) );
}

void ecalc_bin_printer_load_arg_ptr_to_eax( ECALC_JIT_TREE *tree, int8_t val )
{
    // EBPを先頭とする引数のポインタをEAXに読み込む
    /*
     * lea eax, [ebp+val]	8D45vv
     */
    unsigned char bin[] = {0x8D, 0x45, 0x00};

    bin[2] = val;

    ecalc_bin_printer_print( tree, bin, sizeof( bin ) );
}

void ecalc_bin_printer_load_var_ptr_to_eax( ECALC_JIT_TREE *tree, int8_t val )
{
    // ESPを基準としたポインタをEAXに読み込む
    /*
     * lea eax, [esp+val]	8D4424vv
     */

    unsigned char bin[] = {0x8D, 0x44, 0x24, 0x00};

    bin[3] = val;

    ecalc_bin_printer_print( tree, bin, sizeof( bin ) );
}

void ecalc_bin_printer_load_eax_pointed_to_eax( ECALC_JIT_TREE *tree, int8_t val )
{
    // EAXの指すメモリ空間上にある値をEAXにコピー
    /*
     * mov eax, [eax+val]	8B40vv
     */
    unsigned char bin[] = {0x8B, 0x40, 0x00};

    bin[2] = val;

    ecalc_bin_printer_print( tree, bin, sizeof( bin ) );
}

void ecalc_bin_printer_push_dword_val(ECALC_JIT_TREE *tree, uint32_t val)
{
    // DWORD値をスタックに積む
    /*
     * push 0xvvvvvvvv	68vvvvvvvv
     */

    unsigned char bin[] = {0x68, 0x00, 0x00, 0x00, 0x00};

    memcpy( bin + 1, &val, sizeof(uint32_t) );

    ecalc_bin_printer_print( tree, bin, sizeof( bin ) );
}

void ecalc_bin_printer_add_eax_i8(ECALC_JIT_TREE *tree, int8_t val)
{
    // eaxにsigned 8bitをたす
    /*
     * add eax, v	83C0vv
     */

    unsigned char bin[] = {0x83, 0xC0, 0x00};

    bin[2] = val;

    ecalc_bin_printer_print( tree, bin, sizeof( bin ) );
}

void ecalc_bin_printer_store_dword_val(ECALC_JIT_TREE *tree, int8_t pos, uint32_t val)
{
    // eax+posの指す場所にdword値をセット
    /*
     * mov dword ptr [eax+pos], val		C740pp vvvvvvvv
     */

    unsigned char bin[] = {0xC7, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00};

    bin[2] = pos;
    memcpy( bin + 3, &val, sizeof(uint32_t) );

    ecalc_bin_printer_print( tree, bin, sizeof( bin ) );
}

void ecalc_bin_printer_store_double_val(ECALC_JIT_TREE *tree, int8_t pos, double val)
{
    // doubleをeax+posの指す位置にストア
    ecalc_bin_printer_store_dword_val( tree, pos,     *(uint32_t *)( ( (unsigned char *)&val ) + 0 ) );
    ecalc_bin_printer_store_dword_val( tree, pos + 4, *(uint32_t *)( ( (unsigned char *)&val ) + 4 ) );
}

void ecalc_bin_printer_push_dword(ECALC_JIT_TREE *tree, int8_t val)
{
    // eax+valの指す先をスタックにプッシュ
    /*
     * push dword ptr [eax+val]	FF70vv
     */

    unsigned char bin[] = {0xFF, 0x70, 0x00};

    bin[2] = val;

    ecalc_bin_printer_print( tree, bin, sizeof( bin ) );
}

void ecalc_bin_printer_push_qword(ECALC_JIT_TREE *tree, int8_t val)
{
    // eax+valの指す先をスタックに8バイトプッシュ(高いアドレスから)
    ecalc_bin_printer_push_dword( tree, val + 4 );
    ecalc_bin_printer_push_dword( tree, val );
}

void ecalc_bin_printer_pop_dword(ECALC_JIT_TREE *tree, int8_t val)
{
    // eax+valの指す先にスタックからポップ
    /*
     * pop dword ptr [eax+val]	8F40vv
     */

    unsigned char bin[] = {0x8F, 0x40, 0x00};

    bin[2] = val;

    ecalc_bin_printer_print( tree, bin, sizeof( bin ) );
}

void ecalc_bin_printer_pop_qword(ECALC_JIT_TREE *tree, int8_t val)
{
    // eax+valの指す先にスタックから8バイトポップ(PUSHと逆順)
    ecalc_bin_printer_pop_dword( tree, val );
    ecalc_bin_printer_pop_dword( tree, val + 4 );
}

void ecalc_bin_printer_fld(ECALC_JIT_TREE *tree, int8_t val)
{
    // eax+valの指す位置にあるdoubleをFPUにPUSH
    /*
     * fld qword ptr [eax+val]	DD40vv
     */

    unsigned char bin[] = {0xDD, 0x40, 0x00};

    bin[2] = val;

    ecalc_bin_printer_print( tree, bin, sizeof( bin ) );
}

void ecalc_bin_printer_fstp(ECALC_JIT_TREE *tree, int8_t val)
{
    // eax+valにFPUのトップをストアしてポップ
    /*
     * fstp qword ptr [eax+val]	DD58vv
     */

    unsigned char bin[] = {0xDD, 0x58, 0x00};

    bin[2] = val;

    ecalc_bin_printer_print( tree, bin, sizeof( bin ) );
}

void ecalc_bin_printer_fistp( ECALC_JIT_TREE *tree, int8_t val )
{
    // eax+valにFPUのトップを32bit符号付整数としてストアしてポップ
    /*
     * fistp qword ptr [eax+val]	DF78vv
     */

    unsigned char bin[] = {0xDF, 0x78, 0x00};

    bin[2] = val;

    ecalc_bin_printer_print( tree, bin, sizeof( bin ) );
}

void ecalc_bin_printer_fstp_st0( ECALC_JIT_TREE *tree )
{
    // FPUスタックをPOPしてデータ転送なし
    /*
     * fstp st(0)	DDD8
     */
    unsigned char bin[] = {0xDD, 0xD8};

    ecalc_bin_printer_print( tree, bin, sizeof( bin ) );
}

void ecalc_bin_printer_fldz(ECALC_JIT_TREE *tree)
{
    // FPUに0をプッシュ
    /*
     * fldz	D9EE
     */
    unsigned char bin[] = {0xD9, 0xEE};

    ecalc_bin_printer_print( tree, bin, sizeof( bin ) );
}

void ecalc_bin_printer_fld1( ECALC_JIT_TREE *tree )
{
    // FPUに1をプッシュ
    /*
     * fld1 D9E8
     */
    unsigned char bin[] = {0xD9, 0xE8};

    ecalc_bin_printer_print( tree, bin, sizeof( bin ) );
}

void ecalc_bin_printer_fldpi(ECALC_JIT_TREE *tree)
{
    // FPUにpiをプッシュ
    /*
     * fldpi	D9EB
     */
    unsigned char bin[] = {0xD9, 0xEB};

    ecalc_bin_printer_print( tree, bin, sizeof( bin ) );
}

void ecalc_bin_printer_faddp(ECALC_JIT_TREE *tree)
{
    // st(1) <- st(1) + st(0), POP
    /*
     * faddp	DEC1
     */
    unsigned char bin[] = {0xDE, 0xC1};

    ecalc_bin_printer_print( tree, bin, sizeof( bin ) );
}

void ecalc_bin_printer_fadd( ECALC_JIT_TREE *tree, uint8_t st )
{
    // st(0) <- st(1) + st(0)
    /*
     * fadd st(st)	D8Cs
     */
    unsigned char bin[] = {0xD8, 0xC0};

    bin[1] = bin[1] | st;

    ecalc_bin_printer_print( tree, bin, sizeof( bin ) );
}

void ecalc_bin_printer_fsubp(ECALC_JIT_TREE *tree)
{
    // st(1) <- st(1) - st(0)
    /*
     * fsubp	DEE9
     */
    unsigned char bin[] = {0xDE, 0xE9};

    ecalc_bin_printer_print( tree, bin, sizeof( bin ) );
}

void ecalc_bin_printer_fmulp(ECALC_JIT_TREE *tree)
{
    // st(1) <- st(1) * st(0)
    /*
     * fmulp	DEC9
     */
    unsigned char bin[] = {0xDE, 0xC9};

    ecalc_bin_printer_print( tree, bin, sizeof( bin ) );
}

void ecalc_bin_printer_fdivp(ECALC_JIT_TREE *tree)
{
    // st(1) <- st(1) / st(0)
    /*
     * fdivp	DEF9
     */
    unsigned char bin[] = {0xDE, 0xF9};

    ecalc_bin_printer_print( tree, bin, sizeof( bin ) );
}

void ecalc_bin_printer_fabs(ECALC_JIT_TREE *tree)
{
    // st(0) <- abs( st(0) )
    /*
     * fabs	D9E1
     */
    unsigned char bin[] = {0xD9, 0xE1};

    ecalc_bin_printer_print( tree, bin, sizeof( bin ) );
}

void ecalc_bin_printer_fxch_st1(ECALC_JIT_TREE *tree)
{
    // st(0) <> st(1)
    /*
     * fxch st(1)	D9C9
     */
    unsigned char bin[] = {0xD9, 0xC9};

    ecalc_bin_printer_print( tree, bin, sizeof( bin ) );
}

void ecalc_bin_printer_fchs( ECALC_JIT_TREE *tree )
{
    // st(0) <= -st(0)
    /*
     * fchs	D9E0
     */
    unsigned char bin[] = {0xD9, 0xE0};

    ecalc_bin_printer_print( tree, bin, sizeof( bin ) );
}

void ecalc_bin_printer_fsin(ECALC_JIT_TREE *tree)
{
    // st(0) <- sin( st(0) )
    /*
     * fsin	D9FE
     */
    unsigned char bin[] = {0xD9, 0xFE};

    ecalc_bin_printer_print( tree, bin, sizeof( bin ) );
}

void ecalc_bin_printer_fsqrt(ECALC_JIT_TREE *tree)
{
    // st(0) <- sqrt( st(0) )
    /*
     * fsqrt	D9FA
     */
    unsigned char bin[] = {0xD9, 0xFA};

    ecalc_bin_printer_print( tree, bin, sizeof( bin ) );
}

void ecalc_bin_printer_fcos(ECALC_JIT_TREE *tree)
{
    // st(0) <- cos( st(0) )
    /*
     * fsin	D9FF
     */
    unsigned char bin[] = {0xD9, 0xFF};

    ecalc_bin_printer_print( tree, bin, sizeof( bin ) );
}

void ecalc_bin_printer_fcomi(ECALC_JIT_TREE *tree, uint8_t st)
{
    // CPUの比較フラッグをセットさせる st(0)とst(st)の比較
    /*
     * fcomi st(st)	DBFs
     */
    unsigned char bin[] = {0xDB, 0xF0};

    bin[1] = bin[1] | st;

    ecalc_bin_printer_print( tree, bin, sizeof( bin ) );
}

void ecalc_bin_printer_load_exp_ans_ptr_to_eax(ECALC_JIT_TREE *tree)
{
    // eaxにansのポインタセット
    ecalc_bin_printer_load_arg_ptr_to_eax( tree, 12 );
}

void ecalc_bin_printer_load_exp_var_ptr_to_eax( ECALC_JIT_TREE *tree, int index )
{
    // eaxにvar[index]のポインタをセット
    unsigned char bin[] = {0x8D, 0x04, 0x90};

    // EAXにdouble**の場所をロード
    ecalc_bin_printer_load_arg_ptr_to_eax( tree, 8 );

    // EAXにdouble**の値（引数の値）をロード
    ecalc_bin_printer_load_eax_pointed_to_eax( tree, 0 );

    // indexをedxにロード
    ecalc_bin_printer_load_val_to_edx( tree, index );

    // 変数のアドレス計算
    // lea eax, [eax+edx*4]	8D0490
    ecalc_bin_printer_print( tree, bin, sizeof( bin ) );

    // EAXにdouble*の値をロード
    ecalc_bin_printer_load_eax_pointed_to_eax( tree, 0 );
}

void ecalc_bin_printer_load_function_ptr_to_eax( ECALC_JIT_TREE *tree, void ( *func )( void ) )
{
    // eaxに関数ポインタ（即値）を読み込む
    /*
     * mov eax, val	B8vvvvvvvv
     */
    unsigned char bin[] = {0xB8, 0x00, 0x00, 0x00, 0x00};

    memcpy( bin + 1, &func, sizeof( void ( * )( void ) ) );

    ecalc_bin_printer_print( tree, bin, sizeof( bin ) );
}

void ecalc_bin_printer_mov_eax_ecx(ECALC_JIT_TREE *tree)
{
    // ECXをEAXにコピー
    /*
     * mov eax, ecx	89C8
     */
    unsigned char bin[] = {0x89, 0xC8};

    ecalc_bin_printer_print( tree, bin, sizeof( bin ) );
}

void ecalc_bin_printer_mov_ecx_eax(ECALC_JIT_TREE *tree)
{
    // EAXをECXにコピー
    /*
     * mov ecx, eax	89C1
     */
    unsigned char bin[] = {0x89, 0xC1};

    ecalc_bin_printer_print( tree, bin, sizeof( bin ) );
}

void ecalc_bin_printer_mov_eax_edx(ECALC_JIT_TREE *tree)
{
    // EDXをEAXにコピー
    /*
     * mov eax, edx	89D0
     */
    unsigned char bin[] = {0x89, 0xD0};

    ecalc_bin_printer_print( tree, bin, sizeof( bin ) );
}

void ecalc_bin_printer_mov_edx_eax(ECALC_JIT_TREE *tree)
{
    // EAXをEDXにコピー
    /*
     * mov edx, eax	89C2
     */
    unsigned char bin[] = {0x89, 0xC2};

    ecalc_bin_printer_print( tree, bin, sizeof( bin ) );
}

void ecalc_bin_printer_call( ECALC_JIT_TREE *tree )
{
    // EAXの指す関数をコール
    /*
     * call eax	FFD0
     */
    unsigned char bin[] = {0xFF, 0xD0};

    ecalc_bin_printer_print( tree, bin, sizeof( bin ) );
}

void ecalc_bin_printer_load_val_to_edx(ECALC_JIT_TREE *tree, int32_t val)
{
    // EDXに32ビット整数valをロード
    /*
     * mov edx, val	BAvvvvvvvv
     */
    unsigned char bin[] = {0xBA, 0x00, 0x00, 0x00, 0x00};

    memcpy( bin + 1, &val, sizeof(int32_t) );

    ecalc_bin_printer_print( tree, bin, sizeof( bin ) );
}

void ecalc_bin_printer_add_esp_i8(ECALC_JIT_TREE *tree, int8_t val)
{
    // ESPにvalをたす
    /*
     * add esp, val	83C4vv
     */
    unsigned char bin[] = {0x83, 0xC4, 0x00};

    bin[2] = val;

    ecalc_bin_printer_print( tree, bin, sizeof( bin ) );
}

size_t ecalc_bin_printer_je(ECALC_JIT_TREE *tree, int32_t val)
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

size_t ecalc_bin_printer_jmp(ECALC_JIT_TREE *tree, int32_t val)
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

size_t ecalc_bin_printer_jb(ECALC_JIT_TREE *tree, int32_t val)
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
