ecalc
=====

C言語で記述した単純な電卓です．
C言語の練習用に記述したもので，実装方法やアルゴリズムは非常に未熟だと思います．
もし何かの参考になれば幸いです．

## 特徴
一般的な電卓パーサーの実装とは異なり、優先度順にスキャンし続けるという方法をとっています。
決して良い方法ではないと思うのですが、一応動いています。

32bit Linux/Windowsで動作するJITがついています。

## 使い方
ヘッダをインクルードします。
JITを使用しない場合は`ecalc_jit.h`は不要です。
```C
#include "ecalc.h"
#include "ecalc_jit.h"
#include <stdio.h>
```
構造体へのポインタ、変数配列、変数ポインタ配列、答えバッファを用意します。
`ECALC_JIT_TREE`はJITを使用しないのであれば不要です。
```C
struct ECALC_TOKEN *tok;
ECALC_JIT_TREE *jit;
double var[ECALC_VAR_COUNT];
double *vars[ECALC_VAR_COUNT];
double ans = 0;
int i;

/* 変数ポインタリストに変数を登録 */
for ( i = 0; i < ECALC_VAR_COUNT; i++ ) {
	vars[i] = &var[i];
	var[i] = 0;
}
```
入力文字列をレキシカルアナライズ、パース、実行します。
答えはansに入ります。

JITを使用しない場合はツリーに変更したあと`ans = ecalc_get_tree_value( tok, vars, ans )`
を実行すれば結果が得られます。
```C
/* 分割 */
tok = ecalc_make_token( "5*2+3-(42*sin(pi/4))" );

/* ツリーに変更 */
tok = ecalc_make_tree( tok );

/* 変更結果を調べる */
if ( tok != NULL ) {
	// JITコンパイル
	jit = ecalc_create_jit_tree( tok );

	// JIT実行
	ans = ecalc_get_jit_tree_value( jit, vars, ans );

	// JIT破棄
	ecalc_free_jit_tree( jit );

	// JITを使わない場合はこの行だけで良い
	// ans = ecalc_get_tree_value( tok, vars, ans );

	printf( "%f\n", ans );
} else {
	puts( "FAILED!" );
}

/* 解放 */
ecalc_free_token( tok );
```

## 演算子優先度
```
  1 | ()    | カッコ  | ->
  2 | 関数  | 関数    | ->
  3 | +,-   | 単項-.+ | ->
  4 | *,/,% | 乗除算  | ->
  5 | +,-   | 加減算  | ->
  6 | <,>   | 比較    | ->
  7 | ==    | 同じ    | ->
  8 | @     | ループ  | <-
  9 | =     | 代入    | <-
 10 | ,     | 区切り  | ->
```

## 関数
関数には１引数関数、２引数関数、定数関数があります。
三角関数はラジアンです。
* sin(a), cos(a), tan(a), asin(a), acos(a), atan(a)
* log(a) : log10
* ln(a) : 自然対数のlog
* rad(a), deg(a) : aをラジアン、度に変換
* sqrt(a) : ルート
* (a)pow(b) : aのb乗
* (y)atan2(x) : atan2( y, x )
* PI, EPS0

## 文法
非常に扱いづらいですが文法要素が少しあります。
* 変数はaからzまで
* a*bはabと書ける
* a=3でaに3を代入

### if文
`a`が`0`以外の時に`b=2`が実行され、式の値は`a`になります。
```
(a)if(b=2)
```
条件式を入れることもできます
```
(a>2)if(b=3)
```

### ループ文
`a`回`b=b+1`を実行します。
式の値は最後に実行された`b=b+1`です。
```
(a)@(b=b+1)
```

# License
This project is licensed under the 2-clause BSD license.
