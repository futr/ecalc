ecalc
=====

C言語で記述した単純な電卓です．
C言語の練習用に記述したもので，実装方法やアルゴリズムは非常に未熟だと思います．
もし何かの参考になれば幸いです．

### 特徴
一般的な電卓パーサーの実装とは異なり、優先度順にスキャンし続けるという方法をとっています。
決して良い方法ではないと思うのですが、一応動いています。

32bit Windowsで動作するJITがついています。

### 使い方
```c
struct ECALC_TOKEN *tok;
ECALC_JIT_TREE *jit;
double var[ECALC_VAR_COUNT];
double *vars[ECALC_VAR_COUNT];
double ans = 0;
```
:

# License
This project is licensed under the 2-clause BSD license.
