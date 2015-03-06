#include "ecalc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main( int argc, char **argv )
{
	/* 計算機 */
	char data[256];
	int get;
	int i;
	struct ECALC_TOKEN *tok;
	double var[ECALC_VAR_COUNT];
	double *vars[ECALC_VAR_COUNT];
	double ans = 0;

	puts( "exit と入力すると終了" );

	/* メモリマネージャ初期化 */
	ecalc_memman_init();

	/* 変数ポインタリストに変数を登録 */
	for ( i = 0; i < ECALC_VAR_COUNT; i++ ) {
		vars[i] = &var[i];
		var[i] = 24;
	}

	/* 引数チェック */
	if ( argc > 1 ) {
		strcpy( data, argv[1] );
		goto ECALC_CALC;
	}

ECALC_RECALC:

	/* 式読み込み */
	printf( "式 > " );

	for ( i = 0, data[255] = '\0'; i < 255; i++ ) {
		get = getc( stdin );

		if ( get != '\n' ) {
			data[i] = get;
		} else {
			data[i] = '\0';
			break;
		}
	}

	/* 255以上あったので終わるまで飛ばす */
	if ( i >= 255 ) {
		while ( getchar() != '\n' );
	}

	/* 終了？ */
	if( !strcmp( data, "exit" ) ) {
		goto ECALC_END;
	}

ECALC_CALC:

	/* 分割 */
	tok = ecalc_make_token( data );

	/* ツリーに変更 */
	tok = ecalc_make_tree( tok );

	/* 変更結果を調べる */
	if ( tok != NULL ) {
		ans = ecalc_get_tree_value( tok, vars, ans );

		printf( "\t%lf\n", ans );
		printf( "\t0x%X\n", (int)ans );
		printf( "\t%e\n", ans );
	} else {
		puts( "FAILED!" );
	}

	/* 解放 */
	ecalc_free_token( tok );

	/* 繰り返し */
	goto ECALC_RECALC;

ECALC_END:
	return 0;
}


