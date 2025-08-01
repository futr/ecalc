#include "ecalc_jit.h"

#include "ecalc_jit_64.h"
#include "ecalc_jit_32.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/mman.h>
#endif

ECALC_JIT_TREE *ecalc_create_jit_tree( struct ECALC_TOKEN *token )
{
    // JITエンジン作成
#ifdef _WIN32
    #ifdef _WIN64
    return ecalc_create_jit_tree_amd64( token );
    #else
    return ecalc_create_jit_tree_i386( token );
    #endif
#else
    #ifdef __x86_64__
    return ecalc_create_jit_tree_amd64( token );
    #else
    return ecalc_create_jit_tree_i386( token );
    #endif
#endif
}

void ecalc_free_jit_tree( ECALC_JIT_TREE *tree )
{
    // JITエンジン破棄
#ifdef _WIN32
    #ifdef _WIN64
    return ecalc_free_jit_tree_amd64( tree );
    #else
    return ecalc_free_jit_tree_i386( tree );
    #endif
#else
    #ifdef __x86_64__
    return ecalc_free_jit_tree_amd64( tree );
    #else
    return ecalc_free_jit_tree_i386( tree );
    #endif
#endif
}

double ecalc_get_jit_tree_value( ECALC_JIT_TREE *tree, double **vars, double ans )
{
    // JIT木の値を取得
#ifdef _WIN32
    #ifdef _WIN64
    return ecalc_get_jit_tree_value_amd64( tree, vars, ans );
    #else
    return ecalc_get_jit_tree_value_i386( tree, vars, ans );
    #endif
#else
    #ifdef __x86_64__
    return ecalc_get_jit_tree_value_amd64( tree, vars, ans );
    #else
    return ecalc_get_jit_tree_value_i386( tree, vars, ans );
    #endif
#endif
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
