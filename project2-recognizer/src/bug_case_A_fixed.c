/*
 * ============================================================================
 *  任务四 · 问题 A：AI 候选校验代码的"特征下标错位"
 *  复现 → 定位 → 修正 → 回归测试（一条完整的证据链）
 *
 *  说明：
 *    第 1 段 validate_buggy() 是**教师提供的教学错误案例原文**
 *    （examples/bug_case_A.c），此处一字不改地保留，用于复现异常现象；
 *    第 2 段 validate_fixed() 是修正后的实现；
 *    第 3 段 main() 用同一批用例跑"修正前 vs 修正后"，形成回归测试证据。
 *
 *  安全边界：本文件只使用人工构造的静态特征数值，不涉及任何真实恶意代码。
 * ============================================================================
 */

#define _CRT_SECURE_NO_WARNINGS

#include <math.h>
#include <stdio.h>

typedef struct {
    double features[5];
    int    label;
} Sample;

/* ---------------------------------------------------------------------------
 * 1) AI 候选实现（教学错误案例原文，含缺陷）
 *    特征下标约定：0=file_size 1=string_count 2=entropy
 *                  3=byte_00_ratio 4=byte_ff_ratio
 * ------------------------------------------------------------------------- */
int validate_buggy(const Sample *s)
{
    if (s->label != 0 && s->label != 1) return 2;

    /* BUG：features[2] 是 entropy（合法区间是 [0,8]，不是 [0,1]），
     *      而真正的 byte_ff_ratio（features[4]）根本没有被检查。 */
    if (s->features[2] < 0 || s->features[2] > 1 ||
        s->features[3] < 0 || s->features[3] > 1) return 3;

    return 0;
}

/* ---------------------------------------------------------------------------
 * 2) 修正后的实现
 *    改动点：
 *      ① entropy 用 [0,8] 判断（不再是 [0,1]）
 *      ② byte_ff_ratio 检查 features[4]（不再是漏检）
 *      ③ 补上 file_size >= 0 与 string_count 非负整数两项
 * ------------------------------------------------------------------------- */
int validate_fixed(const Sample *s)
{
    if (s->label != 0 && s->label != 1) return 2;                         /* label 只能 0/1 */

    if (s->features[0] < 0.0) return 3;                                   /* file_size >= 0 */
    if (s->features[1] < 0.0 ||
        s->features[1] != floor(s->features[1])) return 3;                 /* string_count 非负整数 */
    if (s->features[2] < 0.0 || s->features[2] > 8.0) return 3;            /* entropy ∈ [0,8] */
    if (s->features[3] < 0.0 || s->features[3] > 1.0) return 3;            /* byte_00_ratio ∈ [0,1] */
    if (s->features[4] < 0.0 || s->features[4] > 1.0) return 3;            /* byte_ff_ratio ∈ [0,1] */

    return 0;
}

/* ---------------------------------------------------------------------------
 * 3) 回归测试：同一批用例分别喂给"修正前 / 修正后"
 * ------------------------------------------------------------------------- */
typedef struct {
    const char *name;
    double      f[5];
    int         label;
    int         expect;   /* 期望返回码：0 = 接受，2 = 非法标签，3 = 特征越界 */
} Case;

int main(void)
{
    Case cases[] = {
        { "① 案例原始输入 byte_ff_ratio = 1.3",        { 8192, 42,   0.5,  0.2,  1.3  }, 1, 3 },
        { "② T04 复现 byte_00_ratio = 1.30",          { 8192, 42,   3.80, 1.30, 0.02 }, 0, 3 },
        { "③ T05 复现 byte_ff_ratio = -0.20",         { 8192, 42,   3.80, 0.05, -0.2 }, 0, 3 },
        { "④ T12 复现 entropy = 8.50",                { 8192, 42,   8.50, 0.05, 0.02 }, 0, 3 },
        { "⑤ T14 复现 string_count = 42.5",           { 8192, 42.5, 3.80, 0.05, 0.02 }, 0, 3 },
        { "⑥ T08 复现 file_size = -8192",             { -8192, 42,  3.80, 0.05, 0.02 }, 0, 3 },
        { "⑦ T03 复现 label = 2",                     { 8192, 42,   3.80, 0.05, 0.02 }, 2, 2 },
        { "⑧ T17 合法边界 0/0/0/0/1（应接受）",        { 0,    0,    0,    0,    1    }, 0, 0 },
        { "⑨ T17 合法边界 100/3/8/1/0（应接受）",      { 100,  3,    8,    1,    0    }, 1, 0 },
        { "⑩ train.csv 首行（应接受）",                { 44032, 176, 5.92, 0.0470, 0.0752 }, 0, 0 }
    };
    int n = (int)(sizeof cases / sizeof cases[0]);
    int i;
    int buggyOk = 0, fixedOk = 0, buggyMiss = 0;

    printf("任务四 · 问题 A「特征下标错位」修正前后回归测试\n");
    printf("特征下标约定：0=file_size 1=string_count 2=entropy 3=byte_00_ratio 4=byte_ff_ratio\n\n");
    printf("%-40s %6s %8s %8s %s\n", "用例", "期望", "修正前", "修正后", "结论");
    printf("--------------------------------------------------------------------------------------\n");

    for (i = 0; i < n; ++i) {
        Sample s;
        int j;
        int rb, rf, okB, okF;

        /* 把用例的特征与标签先装进一个真正的 Sample 再送入被检查的函数：
         * 不能直接把 Case* 传进去 —— 两个结构体布局不同，会读到错位的数据。
         * （MSVC 的 C4133 警告正是在提示这种"指针类型不兼容"。） */
        for (j = 0; j < 5; ++j) s.features[j] = cases[i].f[j];
        s.label = cases[i].label;

        rb = validate_buggy(&s);
        rf = validate_fixed(&s);
        okB = (rb == cases[i].expect);
        okF = (rf == cases[i].expect);
        if (okB) ++buggyOk; else ++buggyMiss;
        if (okF) ++fixedOk;

        printf("%-40s %6d %8d %8d %s\n",
               cases[i].name, cases[i].expect, rb, rf,
               okF ? (okB ? "两者都对" : "已修好") : "仍然错误");
    }

    printf("--------------------------------------------------------------------------------------\n");
    printf("修正前：%d/%d 正确，%d 个用例被错误接受\n", buggyOk, n, buggyMiss);
    printf("修正后：%d/%d 正确\n", fixedOk, n);
    printf("\n结论：AI 候选实现把 entropy 当成比例检查（区间用错），并漏检 byte_ff_ratio；\n");
    printf("      修正下标与区间后，全部异常输入被拒绝、全部合法边界被接受。\n");
    return (fixedOk == n) ? 0 : 1;
}
