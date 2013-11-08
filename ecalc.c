#include "ecalc.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <math.h>
#include <float.h>

/* 簡単電卓Cファイル ecalc ver1.3 */
/* malloc有りバージョン */
/* 2011年更新版 */

/* IF文は、左辺が真の場合は右辺の式を評価し、式自体の値は左辺となる */
/* 角度モードはCのライブラリに依存 ( 多分ラジアン ) */
/* 計算は全てdoubleで、円周率の値はM_PI */

/* 演算子優先度風
 1 | ()		| カッコ	| ->
 2 | 関数	| 関数		| ->
 3 | +,-	| 単項-.+	| ->
 4 | *,/,%	| 乗除算	| ->
 5 | +,-	| 加減算	| ->
 6 | <,>	| 比較		| ->
 7 | @		| ループ	| <-
 8 | =		| 代入		| <-
 9 | ,		| 区切り	| ->
*/

/* メモリマネージャー用トークン */
static struct ECALC_TOKEN ecalc_tokens[ECALC_MEMMAN_TOKENS];

/* 予約語リスト */
static struct ECALC_RESERVED_CONTAINER ecalc_reserved[] = {
	{ "PI",		ECALC_TOKEN_FUNC_NON,	ECALC_FUNC_PI },
	{ "EPS0",	ECALC_TOKEN_FUNC_NON,	ECALC_FUNC_EPS0 },
	{ "ANS",	ECALC_TOKEN_FUNC_NON,	ECALC_FUNC_ANS },
	{ "SIN",	ECALC_TOKEN_FUNC_ONE,	ECALC_FUNC_SIN },
	{ "COS",	ECALC_TOKEN_FUNC_ONE,	ECALC_FUNC_COS },
	{ "TAN",	ECALC_TOKEN_FUNC_ONE,	ECALC_FUNC_TAN },
	{ "ASIN",	ECALC_TOKEN_FUNC_ONE,	ECALC_FUNC_ASIN },
	{ "ACOS",	ECALC_TOKEN_FUNC_ONE,	ECALC_FUNC_ACOS },
	{ "ATAN",	ECALC_TOKEN_FUNC_ONE,	ECALC_FUNC_ATAN },
	{ "LN",		ECALC_TOKEN_FUNC_ONE,	ECALC_FUNC_LOGN },
	{ "LOG",	ECALC_TOKEN_FUNC_ONE,	ECALC_FUNC_LOG10 },
	{ "ROOT",	ECALC_TOKEN_FUNC_ONE,	ECALC_FUNC_SQRT },
	{ "RAD",	ECALC_TOKEN_FUNC_ONE,	ECALC_FUNC_RAD },
	{ "DEG",	ECALC_TOKEN_FUNC_ONE,	ECALC_FUNC_DEG },
	{ "POW",	ECALC_TOKEN_FUNC_TWO,	ECALC_FUNC_POW },
	{ "POWER",	ECALC_TOKEN_FUNC_TWO,	ECALC_FUNC_POW },
	{ "IF",		ECALC_TOKEN_FUNC_TWO,	ECALC_FUNC_IF }
};

/* ------------- メモリマネージャ ------------- */

void ecalc_memman_init( void )
{
	/* メモリマネージャ初期化 */

	/* malloc版ではなにもしない */
	return;
}

struct ECALC_TOKEN *ecalc_memman_malloc( void )
{
	/* 一個トークンをもらう */
	struct ECALC_TOKEN *ret;
	
	ret = malloc( sizeof(struct ECALC_TOKEN) );
	
	return ret;
}

void ecalc_memman_free( struct ECALC_TOKEN *tok )
{
	/* トークン一個開放 */
	free( tok );
}

/* ------------- ユーザー用関数 ------------- */

double ecalc_container_get_value( struct ECALC_CONTAINER *cont )
{
	/* 値を得る */
	return ecalc_get_tree_value( cont->token, cont->vars, cont->ans );
}

int ecalc_container_recog( struct ECALC_CONTAINER *cont, char *text )
{
	/* 文字列を木に */
	ecalc_free_token( cont->token );
	
	cont->token = ecalc_make_token( text );
	cont->token = ecalc_make_tree( cont->token );
	
	if ( cont->token == NULL ) {
		/* 失敗 */
		return 0;
	} else {
		/* 成功 */
		return 1;
	}
}

void ecalc_container_init( struct ECALC_CONTAINER *cont )
{
	/* コンテナを初期化 */
	cont->token = NULL;
}

void ecalc_container_free( struct ECALC_CONTAINER *cont )
{
	/* コンテナを開放 */
	ecalc_free_token( cont->token );
	cont->token = NULL;
}

/* ------------- 計算機関数 ------------- */

struct ECALC_TOKEN *ecalc_make_token( char *text )
{
	/* テキストをトークンに分解 ( レキシカルアナライザーもどき ) */
	struct ECALC_TOKEN *ret;
	char *point;
	char buf[ECALC_MAX_TOKEN_LENGTH];
	int i;
	int resvs;
	
	/* 関数の数 */
	resvs = sizeof(ecalc_reserved) / sizeof(ecalc_reserved[0]);
	
	/* トークンひとつ確保 */
	ret = ecalc_malloc_token();

L_ECALC_ANSYS:
	
	/* 値を一時保存 */
	point = text;
	
	/* 先頭は？ */
	if ( isdigit( *text ) ) {
		/* 数字だった */
		
		/* 0xの表記も許す ( このままだとオーバーランしそうだけど、最悪最後でも\0のはずなので問題ない？ ) */
		if ( ( *text == '0' ) && ( *( text + 1 ) == 'x' || *( text + 1 ) == 'X' ) ) {
			/* 0x飛ばし */
			text += 2;
			
			/* 数字の間は繰り返し */
			do {
				text++;
			} while ( isxdigit( *text ) || *text == '.' );
		} else {
			/* 0xではなかった */

			/* 数字の間は繰り返し */
			do {
				text++;
			} while ( isdigit( *text ) || *text == '.' );
		}

		/* 文字列が最大トークン文字数をオーバーしていないか？ */
		if ( ( text - point ) > ( ECALC_MAX_TOKEN_LENGTH - 1 ) ) {
			/* エラートークンに変更 */
			ret->type   = ECALC_TOKEN_ERROR;
			ret->value  = ECALC_ERROR_TOO_LONG;
			
			strncpy( ret->str, point, ECALC_MAX_TOKEN_LENGTH - 1 );
			ret->str[ECALC_MAX_TOKEN_LENGTH - 1] = '\0';
		} else {		
			/* 問題ないのでトークンをバッファへ */
			memcpy( buf, point, text - point );
			buf[text - point] = '\0';
			
			/* 格納 */
			ret->type  = ECALC_TOKEN_LITE;
			ret->value = strtod( buf, NULL );
			strcpy( ret->str, buf );
		}
	} else if ( isalpha( *text ) ) {
		/* アルファベットだった */
		
		/* 予約語検索 */
		for ( i = 0; i < resvs; i++ ) {
			if ( ecalc_strncmp( text, ecalc_reserved[i].text, strlen( ecalc_reserved[i].text ) ) == 0 ) {
				/* 予約語発見 */
				ret->type  = ecalc_reserved[i].type;
				ret->value = ecalc_reserved[i].value;
				strcpy( ret->str, ecalc_reserved[i].text );
				
				/* 文字列分すすめる */
				text += strlen( ecalc_reserved[i].text );
				
				break;
			}
		}
		
		/* 関数に一致しなかったので変数として処理 */
		if ( i >= resvs ) {
			ret->type  = ECALC_TOKEN_VAR;
			ret->value = toupper( *text ) - 0x41;
			
			*(ret->str)     = *text;
			*(ret->str + 1) = '\0';
			
			/* 一個進む */
			text++;
		}
	} else if ( *text == '(' ) {
		/* 左括弧 */
		
		/* 格納 */
		ret->type  = ECALC_TOKEN_LBRA;
		ret->value = 0;
		
		*(ret->str)     = *text;
		*(ret->str + 1) = '\0';
		
		/* 一個進む */
		text++;
	} else if ( *text == ')' ) {
		/* 右括弧 */
		
		/* 格納 */
		ret->type  = ECALC_TOKEN_RBRA;
		ret->value = 0;
		
		*(ret->str)     = *text;
		*(ret->str + 1) = '\0';
		
		/* 一個進む */
		text++;
	} else if ( *text == '*' ) {
		/* 乗算 */
		
		/* 格納 */
		ret->type  = ECALC_TOKEN_OPE;
		ret->value = ECALC_OPE_MUL;
		
		*(ret->str)     = *text;
		*(ret->str + 1) = '\0';
		
		/* 一個進む */
		text++;
	} else if ( *text == '/' ) {
		/* 除算 */
		
		/* 格納 */
		ret->type  = ECALC_TOKEN_OPE;
		ret->value = ECALC_OPE_DIV;
		
		*(ret->str)     = *text;
		*(ret->str + 1) = '\0';
		
		/* 一個進む */
		text++;
		
	} else if ( *text == '%' ) {
		/* + */
		
		/* 格納 */
		ret->type  = ECALC_TOKEN_OPE;
		ret->value = ECALC_OPE_MOD;
		
		*(ret->str)     = *text;
		*(ret->str + 1) = '\0';
		
		/* 一個進む */
		text++;
	} else if ( *text == '+' ) {
		/* + */
		
		/* 格納 */
		ret->type  = ECALC_TOKEN_OPE;
		ret->value = ECALC_OPE_ADD;
		
		*(ret->str)     = *text;
		*(ret->str + 1) = '\0';
		
		/* 一個進む */
		text++;
	} else if ( *text == '-' ) {
		/* - */
		
		/* 格納 */
		ret->type  = ECALC_TOKEN_OPE;
		ret->value = ECALC_OPE_SUB;
		
		*(ret->str)     = *text;
		*(ret->str + 1) = '\0';
		
		/* 一個進む */
		text++;
	} else if ( *text == '>' ) {
		/* 左大きい */
		
		/* 格納 */
		ret->type  = ECALC_TOKEN_OPE;
		ret->value = ECALC_OPE_LBIG;
		
		*(ret->str)     = *text;
		*(ret->str + 1) = '\0';
		
		/* 一個進む */
		text++;
	} else if ( *text == '<' ) {
		/* 右大きい */
		
		/* 格納 */
		ret->type  = ECALC_TOKEN_OPE;
		ret->value = ECALC_OPE_RBIG;
		
		*(ret->str)     = *text;
		*(ret->str + 1) = '\0';
		
		/* 一個進む */
		text++;
	} else if ( *text == '=' ) {
		/* 代入 */
		
		ret->type  = ECALC_TOKEN_OPE;
		ret->value = ECALC_OPE_STI;
		
		*(ret->str)     = *text;
		*(ret->str + 1) = '\0';
		
		/* 一個進む */
		text++;
	} else if ( *text == ',' ) {
		/* 区切り */
		
		ret->type  = ECALC_TOKEN_OPE;
		ret->value = ECALC_OPE_SEPA;
		
		*(ret->str)     = *text;
		*(ret->str + 1) = '\0';
		
		/* 一個進む */
		text++;
	} else if ( *text == '@' ) {
		/* ループ */
		
		ret->type  = ECALC_TOKEN_OPE;
		ret->value = ECALC_OPE_LOOP;
		
		*(ret->str)     = *text;
		*(ret->str + 1) = '\0';
		
		/* 一個進む */
		text++;
	} else if ( isspace( *text ) ) {
		/* 空白文字 */
		
		/* 空白の間は繰り返し */
		do {
			text++;
		} while ( isspace( *text ) );
		
		goto L_ECALC_ANSYS;
		
	} else if ( *text == '\0' ) {
		/* 終端コード */
		
		ret->type  = ECALC_TOKEN_END;
		ret->value = 0;
		ret->next  = NULL;

		*(ret->str) = '\0';
		
		/* 終わり */
		return ret;
	} else {
		/* その他 ( エラー ) */
		
		ret->type  = ECALC_TOKEN_ERROR;
		ret->value = ECALC_ERROR_BAD_CHAR;
		/* ret->next  = NULL; */ 					/* エラートークンで終了ならコメントを消す */

		*(ret->str)     = *text;
		*(ret->str + 1) = '\0';
		
		/* 終わり エラートークンで終了ならコメントを消す */
		/* return ret; */
		
		text++;										/* エラートークンで終了ならコメントアウト */
	}
	
	/* トークンをもらって、自分に繋ぐ */
	ret->next = ecalc_make_token( text );
	
	return ret;
}

struct ECALC_TOKEN *ecalc_make_tree( struct ECALC_TOKEN *token )
{
	/* トークン連結リストからツリーを作る ( パーサーもどき ) */
	int i;
	int count;
	int err_count;
	int c_lbra;

	struct ECALC_TOKEN b_left;
	struct ECALC_TOKEN *top;
	struct ECALC_TOKEN *next;
	struct ECALC_TOKEN *forw;
	struct ECALC_TOKEN *b_tok;
	struct ECALC_TOKEN *b_rbra;
	struct ECALC_TOKEN *b_rbra_next;
	struct ECALC_TOKEN *b_ret;
	struct ECALC_TOKEN *ret;

	/* 最上位のトークンを作る ( 主に単項マイナス演算用 ) */
	top = ecalc_malloc_token();
	top->next  = token;
	top->type  = ECALC_TOKEN_TOP;
	top->value = 0;
	
	/* 値初期化 */
	err_count = 0;
	
	/* カッコの処理 優先度1 */
	for ( next = top; next != NULL; next = next->next ) {
		/* 左括弧発見 */
		if ( next->type == ECALC_TOKEN_LBRA ) {
			/* 右括弧を探る ( 最後まで ) */
			b_rbra = NULL;
			c_lbra = 1;
			
			/* カッコの開き数を数える ( ( 4 - 34 * ( 200 ) ) -34 ) みたいなのに対応するため */
			for ( forw = next->next; ( forw != NULL ) && ( c_lbra > 0 ); forw = forw->next ) {
				/* かっこ発見 */
				if ( forw->type == ECALC_TOKEN_LBRA ) {
					c_lbra++;										/* 左括弧がさらに開いた */
				} else if ( forw->type == ECALC_TOKEN_RBRA ) {
					b_rbra = forw;
					c_lbra--;										/* 右括弧が閉じた */
				}
			}
			
			/* 右括弧があった */
			if ( b_rbra != NULL ) {
				/* 括弧内のポインタを保持 */
				b_tok = next->next;
				
				/* )の次を保存 */
				b_rbra_next = b_rbra->next;
				
				/* 左括弧を式に */
				next->type = ECALC_TOKEN_EXP;
				
				/* 右括弧を終端記号に変換 */
				b_rbra->type = ECALC_TOKEN_END;
				b_rbra->next = NULL;
				
				/* 括弧内を処理してもらう */
				b_ret = ecalc_make_tree( b_tok );
				
				/* エラーが起きた ( ここでreturnしてもいいはずだが、エラーの数を数えてみる ) */
				if ( b_ret == NULL ) {
					next->type = ECALC_TOKEN_ERROR;
					err_count++;
				} else {
					/* 成功したので(をEXPに変更 */
					*next = *b_ret;

					/* ()内の木のトップの入れ物は必要ないので解放 */
					ecalc_memman_free( b_ret );
				}
				
				/* 再接続 */
				next->next = b_rbra_next;
			}
		}
	}
	
L_ECALC_RECHECK_FUNC:

	/* 関数呼び出し 優先度2 */
	for ( next = top; next != NULL; next = next->next ) {
		if ( next->type == ECALC_TOKEN_FUNC_NON ) {				/* 定数関数 */
			/* 何も格納せずexpに変更 */
			next-> type = ECALC_TOKEN_EXP;
			next->left  = NULL;
			next->right = NULL;
			
			/* 再チェック */
			goto L_ECALC_RECHECK_FUNC;
		} else if ( next->type == ECALC_TOKEN_FUNC_ONE ) {		/* 単項演算関数 */
			if ( next->next != NULL ) {
				if ( ecalc_token_is_value( next->next ) ) {
					/* 結合可能なので、結合 */
					next->right = next->next;					/* 引数は右辺値に与える */
					next->left  = NULL;							/* 単項なので左辺はいらない */
					
					/* 値設定 */
					next->type = ECALC_TOKEN_EXP;
					
					/* 再接続 */
					next->next = next->next->next;
					
					/* ダブらないように設定 */
					next->right->next = NULL;
					
					/* 再チェック */
					goto L_ECALC_RECHECK_FUNC;
				}
			}
		} else if ( ecalc_token_is_value( next ) ) {			/* ２項演算関数 */
			if ( next->next != NULL ) {
				if ( next->next->type == ECALC_TOKEN_FUNC_TWO ) {
					if ( next->next->next != NULL ) {
						if ( ecalc_token_is_value( next->next->next ) ) {
							/* 右辺と左辺結合可能なので結合 */
							ecalc_make_exp( next );
							
							/* 再チェック */
							goto L_ECALC_RECHECK_FUNC;
						}
					}
				}
			}
		}
	}
	
L_ECALC_RECHECK_SINGLE_MP:
	
	/* 単項演算子(-,+)を処理 優先度3 結合方向<- */
	for ( next = top, b_tok = NULL; next != NULL; next = next->next ) {
		if ( !ecalc_token_is_value( next ) ) {					/* 値を持たない、演算子 */
			if ( next->next != NULL ) {
				if ( ( next->next->type == ECALC_TOKEN_OPE ) &&
					 ( ( next->next->value == ECALC_OPE_SUB ) || ( next->next->value == ECALC_OPE_ADD ) ) ) {
					if ( next->next->next != NULL ) {
						if ( ecalc_token_is_value( next->next->next ) ) {
							/* 右辺に値をもつ単項演算子を発見 */
							
							/* 場所を記憶 */
							b_tok = next->next;
						}
					}
				}
			}
		}
	}
	
	if ( b_tok != NULL ) {
		/* 単項演算発見 */

		/* 式に変換 */
		b_tok->type = ECALC_TOKEN_EXP;
		
		/* 左辺値、右辺値設定 */
		b_tok->left  = ecalc_malloc_token();
		b_tok->right = b_tok->next;
		
		/* 0を作る */
		b_tok->left->value = 0;
		b_tok->left->type  = ECALC_TOKEN_LITE;
		
		/* 再接続 */
		b_tok->next = b_tok->next->next;
		
		/* ダブらないように */
		b_tok->right->next = NULL;
		
		/* 再チェック */
		goto L_ECALC_RECHECK_SINGLE_MP;
	}
	
L_ECALC_RECHECK_MUL:
	
	/* 乗算、除算の処理 優先度4 */
	for ( next = top; next != NULL; next = next->next ) {
		/* 左辺値に成りうるものを発見 */
		if ( ecalc_token_is_value( next ) ) {
			/* 次がある？ */
			if ( next->next != NULL ) {
				if ( ( next->next->type == ECALC_TOKEN_OPE && next->next->value == ECALC_OPE_MUL ) ||
				     ( next->next->type == ECALC_TOKEN_OPE && next->next->value == ECALC_OPE_DIV ) ||
				     ( next->next->type == ECALC_TOKEN_OPE && next->next->value == ECALC_OPE_MOD ) ) {
					/* 演算子あり乗除算 */
					if ( next->next->next != NULL ) {
						/* 次に来るのが右辺値になり得る？ */
						if ( ecalc_token_is_value( next->next->next ) ) {
							/* 計算可能！ */
							
							/* 左辺と右辺を演算子で結合 */
							ecalc_make_exp( next );
							
							/* 処理したが、自分自身をもう一度評価して欲しいので、forに入り直す */
							goto L_ECALC_RECHECK_MUL;
						}
					}
				} else if ( ( next->next->type == ECALC_TOKEN_VAR ) || ( next->next->type == ECALC_TOKEN_EXP ) ) {
					/* 演算子なし乗算 */
					
					/* 一個確保 */
					b_tok = ecalc_malloc_token();
					
					/* 左辺値コピー */
					*b_tok = *next;
					
					/* 式に変換 */
					next->type  = ECALC_TOKEN_EXP;
					next->value = ECALC_OPE_MUL;
					
					/* 左辺値、右辺値設定 */
					next->left  = b_tok;
					next->right = next->next;
					
					/* 再接続 */
					next->next = next->next->next;
					
					/* ダブらないように */
					next->right->next = NULL;
					next->left->next  = NULL;
					
					/* 自分自身も再評価したいのでforに入りなおす */
					goto L_ECALC_RECHECK_MUL;
				}
			}
		}
	}

L_ECALC_RECHECK_ADD:

	/* +-の処理 優先度5 */
	for ( next = top; next != NULL; next = next->next ) {
		if ( ecalc_token_is_value( next ) ) {
			/* 次がある */
			if ( next->next != NULL ) {
				/* つぎが+-だった */
				if ( ( next->next->type == ECALC_TOKEN_OPE && next->next->value == ECALC_OPE_SUB ) ||
				     ( next->next->type == ECALC_TOKEN_OPE && next->next->value == ECALC_OPE_ADD ) ) {
					/* 右辺値が存在する */
					if ( next->next->next != NULL ) {
						/* 処理可能 */
						if ( ecalc_token_is_value( next->next->next ) ) {
							/* +-である限り、次がないことはないので、強制接続 */

							/* 演算 */
							ecalc_make_exp( next );
							
							/* 処理したが、自分自身をもう一度評価して欲しいので、forに入り直す */
							goto L_ECALC_RECHECK_ADD;
						}
					}
				}
			}
		}
	}
	
L_ECALC_RECHECK_BIG:

	/* 比較演算子 優先度6 */
	for ( next = top; next != NULL; next = next->next ) {
		if ( ecalc_token_is_value( next ) ) {
			/* 次がある */
			if ( next->next != NULL ) {
				/* つぎが+-だった */
				if ( ( next->next->type == ECALC_TOKEN_OPE ) &&
				     ( ( next->next->value == ECALC_OPE_RBIG ) || ( next->next->value == ECALC_OPE_LBIG ) ) ) {
					/* 右辺値が存在する */
					if ( next->next->next != NULL ) {
						/* 処理可能 */
						if ( ecalc_token_is_value( next->next->next ) ) {
							/* 演算 */
							ecalc_make_exp( next );
							
							/* 処理したが、自分自身をもう一度評価して欲しいので、forに入り直す */
							goto L_ECALC_RECHECK_BIG;
						}
					}
				}
			}
		}
	}
	
L_ECALC_RECHECK_LOOP:
	
	/* ループ 結合方向 <- 優先度7 */
	for ( next = top, b_tok = NULL; next != NULL; next = next->next ) {
		/* ループ記号を一番右まで調査 */
		if ( ecalc_token_is_value( next ) ) {
			/* 次がある */
			if ( next->next != NULL ) {
				/* つぎが代入記号だった */
				if ( ( next->next->type == ECALC_TOKEN_OPE ) && ( next->next->value == ECALC_OPE_LOOP ) ) {
					/* 右辺値が存在する */
					if ( next->next->next != NULL ) {
						/* 右辺値は式以外許されない */
						if ( next->next->next->type == ECALC_TOKEN_EXP ) {
							/* 位置を覚えておく */
							b_tok = next;
						}
					}
				}
			}
		}
	}
	
	/* ループ記号が見つかったか？ */
	if ( b_tok != NULL ) {
		/* 演算 */
		ecalc_make_exp( b_tok );
		
		/* 再チェックしてもらう */
		goto L_ECALC_RECHECK_LOOP;
	}
	
L_ECALC_RECHECK_STI:
	
	/* 代入 結合方向 <- 優先度8 */
	for ( next = top, b_tok = NULL; next != NULL; next = next->next ) {
		/* =を一番右まで調査 */
		if ( next->type == ECALC_TOKEN_VAR ) {
			/* 次がある */
			if ( next->next != NULL ) {
				/* つぎが代入記号だった */
				if ( ( next->next->type == ECALC_TOKEN_OPE ) && ( next->next->value == ECALC_OPE_STI ) ) {
					/* 右辺値が存在する */
					if ( next->next->next != NULL ) {
						/* 処理可能 */
						if ( ecalc_token_is_value( next->next->next ) ) {
							/* 位置を覚えておく */
							b_tok = next;
						}
					}
				}
			}
		}
	}
	
	/* 代入記号が見つかったか？ */
	if ( b_tok != NULL ) {
		/* 演算 */
		ecalc_make_exp( b_tok );
		
		/* 再チェックしてもらう */
		goto L_ECALC_RECHECK_STI;
	}
	
L_ECALC_RECHECK_SEPA:
	
	/* 式区切り 優先度9 */
	for ( next = top; next != NULL; next = next->next ) {
		if ( ecalc_token_is_value( next ) ) {
			/* 次がある */
			if ( next->next != NULL ) {
				/* つぎが区切り文字だった */
				if ( ( next->next->type == ECALC_TOKEN_OPE ) && ( next->next->value == ECALC_OPE_SEPA ) ) {
					/* 右辺値が存在する */
					if ( next->next->next != NULL ) {
						/* 処理可能 */
						if ( ecalc_token_is_value( next->next->next ) ) {
							/* 接続する */

							/* 演算 */
							ecalc_make_exp( next );
							
							/* 自分自身を再チェック */
							goto L_ECALC_RECHECK_SEPA;
						}
					}
				}
			}
		}
	}
	
	/* ツリー作成終了、結果を調べる */
	
	/* 残ったトークンを数える */
	for ( count = 0, next = top; next != NULL; count++ ) {
		next = next->next;
	}
	
	/* 解析結果をチェック ( tokenにNULLが与えられていた場合もcount = 1なので問題ない ) */
	if ( err_count ) {
		/* エラー発生 */
		ret = NULL;
		ecalc_free_token( top );
	} else if ( count == 3 ) {
		/* 成功 */
		
		/* EXPオンリー? */
		if ( top->next->type == ECALC_TOKEN_EXP ) {
			/* そのまま帰す */
			ret = top->next;
			
			/* いらないものを開放 */
			top->next = top->next->next;
			ecalc_free_token( top );
			ret->next = NULL;
		} else if ( ( top->next->type == ECALC_TOKEN_VAR ) || ( top->next->type == ECALC_TOKEN_LITE ) ) {
			/* 即値 */
			
			/* トークン確保 */
			b_tok = ecalc_malloc_token();
			ret   = ecalc_malloc_token();
			
			/* トップのEXPを作成 */
			ret->value = ECALC_OPE_ADD;
			ret->left  = b_tok;
			ret->right = top->next;
			ret->type  = ECALC_TOKEN_EXP;
			
			/* 0を作る */
			b_tok->value = 0;
			b_tok->type  = ECALC_TOKEN_LITE;

			/* tokenを開放する */
			top->next = top->next->next;
			ret->right->next = NULL;
			ecalc_free_token( top );
		} else {
			/* 失敗 */
			ret = NULL;
			ecalc_free_token( top );
		}
	} else {
		/* 完全に失敗 */
		ret = NULL;
		ecalc_free_token( top );
	}
	
	return ret;
}

int ecalc_token_is_value( struct ECALC_TOKEN *token )
{
	/* 与えられたトークンが値を持つかチェック */
	if ( ( token->type == ECALC_TOKEN_LITE ) ||
	     ( token->type == ECALC_TOKEN_VAR )   ||
	     ( token->type == ECALC_TOKEN_EXP ) ) {
		return 1;
	} else {
		return 0;
	}
}

void ecalc_make_exp( struct ECALC_TOKEN *left )
{
	/* 左辺と右辺を結合して式を作る */
	struct ECALC_TOKEN b_left;
	struct ECALC_TOKEN *b_tok;
	
	/* 左辺を左辺値に、右辺を右辺値に接続 ( 左辺値を演算子と交換 ) */
	left->next->left  = left->next;
	left->next->right = left->next->next;

	/* 演算子を式へ変換 */
	left->next->type = ECALC_TOKEN_EXP;
	
	/* 演算子だったTOKENを左辺値と入れ替え */
	b_left   = *left;							/* 左辺値バッファリング */
	b_tok    = left->next;						/* 演算子の位置を記憶 */
	*left    = *(left->next);					/* 演算子を左辺値にもって行く */	
	*(b_tok) = b_left;							/* 左辺値を演算子のいたところへ */
	
	/* 再接続 */
	left->next = left->right->next;
	
	/* 値がダブらないようにしておく */
	left->left->next  = NULL;
	left->right->next = NULL;
}

double ecalc_get_tree_value( struct ECALC_TOKEN *token, double **vars, double ans )
{
	/* ツリーから値を得る */
	int i;
	double left  = 0;			/* 初期値は0なので例えば0@(3)は0となる */
	double right = 0;
	double *left_var;

	/* NULLなら何もしない */
	if ( token == NULL ) {
		return 0;
	}

	/* EXP以外を与えられると強制的に0 */
	if ( token->type != ECALC_TOKEN_EXP ) {
		return 0;
	}

	/* 左辺取得 */
	if ( token->left != NULL ) {
		if ( token->left->type == ECALC_TOKEN_LITE ) {
			left = token->left->value;
		} else if ( token->left->type == ECALC_TOKEN_VAR ) {
			left     = *vars[(int)token->left->value];
			left_var = vars[(int)token->left->value];
		} else if ( token->left->type == ECALC_TOKEN_EXP ) {
			left = ecalc_get_tree_value( token->left, vars, ans );
		} else {
			left = 0;
		}
	}
	
	/* 右辺取得 */
	if ( token->right != NULL ) {
		if ( token->right->type == ECALC_TOKEN_LITE ) {
			right = token->right->value;
		} else if ( token->right->type == ECALC_TOKEN_VAR ) {
			right = *vars[(int)token->right->value];
		} else if ( token->right->type == ECALC_TOKEN_EXP ) {
			/* 右辺を式として扱う場合、ここで処理 */
			if ( token->value == ECALC_OPE_LOOP ) {						/* ループ */
				for ( i = 0; i < (int)left; i++ ) {
					right = ecalc_get_tree_value( token->right, vars, ans );
				}
			} else if ( token->value == ECALC_FUNC_IF ) {				/* IF */
				if ( left ) {
					ecalc_get_tree_value( token->right, vars, ans );
				}
				
				right = left;
			} else {													/* 普通 */
				right = ecalc_get_tree_value( token->right, vars, ans );
			}
		} else {
			right = 0;
		}
	}
	
	/* 計算 */
	switch ( (int)token->value ) {
		case ECALC_OPE_ADD:					/* 足し算 */
			return left + right;
		case ECALC_OPE_SUB:					/* 引き算 */
			return left - right;
		case ECALC_OPE_MUL:					/* 掛け算 */
			return left * right;
		case ECALC_OPE_DIV:					/* 割り算 */
			/* / 0はエラーを発生させない */
			if ( right == 0 ) {
				return 0;
			} else {
				return left / right;
			}
		case ECALC_OPE_MOD:					/* 余り */
			/* % 0はエラーを発生させない */
			if ( right == 0 ) {
				return 0;
			} else {
				return ( (int)left % (int)right );
			}
		case ECALC_OPE_STI:					/* 代入 */
			return ( *left_var = right );
		case ECALC_OPE_SEPA:				/* 区切り */
			return right;
		case ECALC_OPE_LOOP:				/* 繰り返し */
			return right;
		case ECALC_OPE_LBIG:				/* 左が大きい */
			return ( left > right );
		case ECALC_OPE_RBIG:				/* 右が大きい */
			return ( left < right );
		case ECALC_FUNC_SIN:				/* sin */
			return sin( right );
		case ECALC_FUNC_COS:				/* cos */
			return cos( right );
		case ECALC_FUNC_TAN:				/* tan */
			return tan( right );
		case ECALC_FUNC_ASIN:				/* asin */
			return asin( right );
		case ECALC_FUNC_ACOS:				/* acos */
			return acos( right );
		case ECALC_FUNC_ATAN:				/* atan */
			return atan( right );
		case ECALC_FUNC_LOG10:				/* log10 */
			return log10( right );
		case ECALC_FUNC_LOGN:				/* logn */
			return log( right );
		case ECALC_FUNC_SQRT:				/* root */
			return sqrt( right );
		case ECALC_FUNC_POW:				/* power */
			return pow( left, right );
		case ECALC_FUNC_RAD:				/* rad */
			return ( ( right / 180 ) * M_PI );
		case ECALC_FUNC_DEG:				/* deg */
			return ( ( right / M_PI ) * 180.0 );
		case ECALC_FUNC_PI:					/* π */
			return M_PI;
		case ECALC_FUNC_EPS0:				/* ε0 */
			return 8.85418782e-12;
		case ECALC_FUNC_ANS:				/* ans */
			return ans;
		case ECALC_FUNC_IF:					/* if文 */
			return right;
		default:
			return 0;
	}
}

void ecalc_free_token( struct ECALC_TOKEN *token )
{
	/* トークンを開放 */

	/* NULLじゃなかった */
	if ( token != NULL ) {
		/* leftがあれば開放 */
		if ( token->left != NULL ) {
			ecalc_free_token( token->left );
		}

		/* rightがあれば開放 */
		if ( token->right != NULL ) {
			ecalc_free_token( token->right );
		}

		/* nextがあれば解放 */
		if ( token->next != NULL ) {
			ecalc_free_token( token->next );
		}
	}

	/* 最初のを開放 */
	ecalc_memman_free( token );
}

struct ECALC_TOKEN *ecalc_malloc_token( void )
{
	/* トークンを一個作る */
	struct ECALC_TOKEN *ret;
	
	ret = ecalc_memman_malloc();
	
	ret->value  = 0;
	ret->next   = NULL;
	ret->left   = NULL;
	ret->right  = NULL;
	ret->str[0] = '\0';
	ret->type   = ECALC_TOKEN_ALLOC;
	
	return ret;
}

int ecalc_strncmp( char *str1, char *str2, int n )
{
	/* 大文字小文字を区別しないstrncmp */
	int i, ret;
	
	ret = 0;
	
	for ( i = 0; i < n; i++ ) {
		/* \0に出くわすと失敗する ( 本物ではそこで処理を止める = break ) */
		if ( str1[i] == '\0' || str2[i] == '\0' ) {
			return 1;
		}
		
		/* 違ってたら0以外を返す */
		if ( toupper( str1[i] ) != toupper( str2[i] ) ) {
			ret = 1;
		}
	}
	
	return ret;
}
