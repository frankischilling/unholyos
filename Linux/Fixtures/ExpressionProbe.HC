#define EXPR_BASE 40
#if (EXPR_BASE + 2 == 42) && defined(if)
EXPR_BASE + 2;
#else
99;
#endif
#if 0
  #if 1 / 0
  99;
  #endif
#else
3 * 7;
#endif
#if 1
#if 1
17;
#endif
#endif
#assert 8 == 2 * 2 << 1
#assert 0
1 + 2 * 3;
2 * 3 << 1;
3 + 2 & 1;
18 - 5 - 3;
-17 / 4;
-17 % 4;
0xFFFFFFFFFFFFFFFF / 2;
-16 >> 2;
0x8000000000000000 >> 2;
1 << 70;
~5;
1 < 2 < 3;
3 < 2 < 1;
10 < 20 == 20;
(10 < 20) == 1;
expr_i == 0 && (1 / expr_divisor);
expr_i != 0 || (1 / expr_divisor);
3 ^^ 0;
3 ^^ 8;
'AZ' + 1;
!1.25;
!0.0;
!(-0.0);
-0.0 == 0.0;
2.5 % 1.5;
2.5 + 3 * 2.0;
2.5 >> 1;
1.0 << 3;
-2.5 >> 0.1;
2.5 << 0.1;
expr_f << 0.1;
1.25;
2`3`2;
-2`2;
(-2)`3;
expr_i * 7 + expr_u % 10;
expr_f * expr_i + 7.5;
3.5 < expr_i < 120;
expr_i < 0 || expr_f > 2.0;
