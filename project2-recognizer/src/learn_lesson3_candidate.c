#define _CRT_SECURE_NO_WARNINGS

#include <math.h>
#include <stdio.h>

#define FEATURE_COUNT 5
#define FIELD_COUNT   6
#define FIELD_MAX     64

typedef struct {
    double features[FEATURE_COUNT];   /* 5 个静态特征 */
    int    label;                     /* 0 = 良性，1 = 可疑 */
} Sample;

/* 把一行按半角逗号切成 6 段（第 2 课的代码，去掉打印）
 * 返回码：0 = 成功；5 = 字段个数不符 */
int split_fields(char line[], char fields[][FIELD_MAX])
{
    int count = 1, f = 0, k = 0, i;

    for (i = 0; i < FIELD_COUNT; i++) fields[i][0] = '\0';

    for (i = 0; line[i] != '\0'; i++) {          /* ① 去掉行尾换行符 */
        if (line[i] == '\n') { line[i] = '\0'; break; }
    }

    for (i = 0; line[i] != '\0'; i++) {          /* ② 逐字符切分 */
        if (line[i] == ',') {
            if (f < FIELD_COUNT) fields[f][k] = '\0';
            count++;
            f++;
            k = 0;
        }
        else {
            if (f < FIELD_COUNT && k < FIELD_MAX - 1) { fields[f][k] = line[i]; k++; }
        }
    }
    if (f < FIELD_COUNT) fields[f][k] = '\0';    /* ③ 最后一段收尾 */

    if (count != FIELD_COUNT) {                  /* ④ 字段数校验 */
        printf("字段个数不符：应有 %d 个，实际 %d 个\n", FIELD_COUNT, count);
        return 5;
    }
    return 0;
}

/* 把 6 段文本转成数值装进 Sample
 * 返回码：0 = 成功；7 = 非数值 / 非有限值；8 = string_count 非非负整数；9 = 非法标签 */
int parse_sample(char fields[][FIELD_MAX], Sample* s)
{
    double v;
    int i;

    for (i = 0; i < FEATURE_COUNT; i++) {              /* 前 5 段 → features */
        if (sscanf(fields[i], "%lf", &v) != 1) {       /* 第一层：格式校验 */
            printf("第 %d 个字段 \"%s\" 不是合法数值\n", i + 1, fields[i]);
            return 7;
        }
        if (!isfinite(v)) {                            /* 第二层：语义校验（nan/inf） */
            printf("第 %d 个字段 \"%s\" 是非有限值（nan/inf）\n", i + 1, fields[i]);
            return 7;
        }
        s->features[i] = v;
    }

    if (s->features[1] < 0.0 || s->features[1] != floor(s->features[1])) {
        printf("string_count = %g 必须是非负整数\n", s->features[1]);
        return 8;
    }

    if (sscanf(fields[5], "%d", &s->label) != 1) {     /* label */
        printf("label \"%s\" 不是合法数值\n", fields[5]);
        return 9;
    }
    if (s->label != 0 && s->label != 1) {
        printf("label = %d 非法（只允许整数 0 或 1）\n", s->label);
        return 9;
    }
    return 0;
}

void print_sample(const Sample* s)
{
    int i;
    printf("features = [");
    for (i = 0; i < FEATURE_COUNT; i++) {
        printf("%s%g", (i == 0) ? "" : ", ", s->features[i]);
    }
    printf("]  label = %d\n", s->label);
}

int main(void)
{
    char paths[6][128] = {
        "..\\data\\train.csv",
        "..\\tests\\invalid\\T20_extra_whitespace.csv",
        "..\\tests\\invalid\\T06_nonnumeric.csv",
        "..\\tests\\invalid\\T11_nonfinite.csv",
        "..\\tests\\invalid\\T14_fractional_string_count.csv",
        "..\\tests\\invalid\\T18_non_integer_label.csv"
    };
    char fields[FIELD_COUNT][FIELD_MAX];
    char line[1024];
    FILE* fp;
    Sample s;
    int i, rc;

    for (i = 0; i < 6; i++) {
        printf("\n===== %s =====\n", paths[i]);
        fp = fopen(paths[i], "r");
        if (fp == NULL) { printf("打开文件失败\n"); continue; }
        if (fgets(line, sizeof line, fp) == NULL) { fclose(fp); printf("文件里没有内容\n"); continue; }
        if (fgets(line, sizeof line, fp) == NULL) { fclose(fp); printf("没有数据行\n"); continue; }
        fclose(fp);

        rc = split_fields(line, fields);
        if (rc != 0) { printf("split_fields 返回码 = %d\n", rc); continue; }

        rc = parse_sample(fields, &s);
        printf("parse_sample 返回码 = %d\n", rc);
        if (rc == 0) print_sample(&s);
    }
    return 0;
}