#define _CRT_SECURE_NO_WARNINGS

#include <math.h>
#include <stdio.h>
#include <string.h> 

#define FEATURE_COUNT 5
#define FIELD_COUNT   6
#define FIELD_MAX     64
#define MAX_SAMPLES   512

typedef struct {
    double features[FEATURE_COUNT];
    int    label;
} Sample;

typedef struct {
    Sample samples[MAX_SAMPLES];
    int    count;
    int    has_label;
} Dataset;

/* 表头应有的字段名（次序固定） */
static char HEADER[FIELD_COUNT][16] = {
    "file_size", "string_count", "entropy", "byte_00_ratio", "byte_ff_ratio", "label"
};

/* ---------- 第 2 课：按逗号切分 ---------- */
int split_fields(char line[], char fields[][FIELD_MAX])
{
    int count = 1, f = 0, k = 0, i;

    for (i = 0; i < FIELD_COUNT; i++) fields[i][0] = '\0';

    for (i = 0; line[i] != '\0'; i++) {
        if (line[i] == '\n') { line[i] = '\0'; break; }
    }

    for (i = 0; line[i] != '\0'; i++) {
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
    if (f < FIELD_COUNT) fields[f][k] = '\0';

    if (count != FIELD_COUNT) {
        printf("字段个数不符：应有 %d 个，实际 %d 个\n", FIELD_COUNT, count);
        return 5;
    }
    return 0;
}

/* ---------- 第 3 课 + 本课新增的值域校验 ---------- */
int parse_sample(char fields[][FIELD_MAX], Sample* s)
{
    double v;
    double lv;
    int i;

    for (i = 0; i < FEATURE_COUNT; i++) {
        if (sscanf(fields[i], "%lf", &v) != 1) {
            printf("第 %d 个字段 \"%s\" 不是合法数值\n", i + 1, fields[i]);
            return 7;
        }
        if (!isfinite(v)) {
            printf("第 %d 个字段 \"%s\" 是非有限值（nan/inf）\n", i + 1, fields[i]);
            return 7;
        }
        s->features[i] = v;
    }

    /* 本课新增：值域校验 */
    if (s->features[0] < 0.0) {
        printf("file_size = %g 为负数（必须 >= 0）\n", s->features[0]);
        return 8;
    }
    if (s->features[1] < 0.0 || s->features[1] != floor(s->features[1])) {
        printf("string_count = %g 必须是非负整数\n", s->features[1]);
        return 8;
    }
    if (s->features[2] < 0.0 || s->features[2] > 8.0) {
        printf("entropy = %g 超出合法区间 [0, 8]\n", s->features[2]);
        return 8;
    }
    if (s->features[3] < 0.0 || s->features[3] > 1.0) {
        printf("byte_00_ratio = %g 超出合法区间 [0, 1]\n", s->features[3]);
        return 8;
    }
    if (s->features[4] < 0.0 || s->features[4] > 1.0) {
        printf("byte_ff_ratio = %g 超出合法区间 [0, 1]\n", s->features[4]);
        return 8;
    }

    /* label */
    if (sscanf(fields[5], "%lf", &lv) != 1) {
        printf("label \"%s\" 不是合法数值\n", fields[5]);
        return 9;
    }
    if (!isfinite(lv)) {
        printf("label \"%s\" 是非有限值\n", fields[5]);
        return 9;
    }
    if (lv != floor(lv) || (lv != 0.0 && lv != 1.0)) {
        printf("label = %g 非法（只允许整数 0 或 1）\n", lv);
        return 9;
    }
    s->label = (int)lv;

    return 0;
}

/* ---------- 本课新增：读整个文件 ---------- */
int load_csv(const char* path, Dataset* data, int mode)
{
    FILE* fp;
    char line[1024];
    char fields[FIELD_COUNT][FIELD_MAX];
    int  line_no = 0;
    int  i, rc;

    data->count = 0;
    data->has_label = mode;

    fp = fopen(path, "r");
    if (fp == NULL) {
        printf("[错误] 返回码 2 —— 打不开文件 %s\n", path);
        return 2;
    }

    if (fgets(line, sizeof line, fp) == NULL) {
        fclose(fp);
        printf("[错误] 返回码 3 —— 文件是空的，连表头都没有\n");
        return 3;
    }
    line_no = 1;

    /* 这里只检查了"列数" —— 字段名与次序检查了吗？ */
    rc = split_fields(line, fields);
    if (rc != 0) {
        fclose(fp);
        printf("[错误] 返回码 4 —— 表头字段个数不符\n");
        return 4;
    }
    /* 【做法 B】逐列比对字段名与次序 */
    for (i = 0; i < FIELD_COUNT; i++) {
        if (strcmp(fields[i], HEADER[i]) != 0) {
            fclose(fp);
            printf("[错误] 返回码 4 —— 表头第 %d 列应为 \"%s\"，实际是 \"%s\"\n",
                i + 1, HEADER[i], fields[i]);
            return 4;
        }
    }
    while (fgets(line, sizeof line, fp) != NULL) {
        int blank = 1;

        ++line_no;

        for (i = 0; line[i] != '\0'; i++) {
            if (line[i] != ' ' && line[i] != '\t' && line[i] != '\r' && line[i] != '\n') { blank = 0; break; }
        }
        if (blank) {
            fclose(fp);
            data->count = 0;
            printf("[错误] 返回码 6 —— 第 %d 行是空白数据行\n", line_no);
            return 6;
        }

        rc = split_fields(line, fields);
        if (rc != 0) {
            fclose(fp);
            data->count = 0;
            printf("[错误] 返回码 5 —— 第 %d 行字段数不符\n", line_no);
            return 5;
        }

        if (data->count >= MAX_SAMPLES) {
            fclose(fp);
            data->count = 0;
            printf("[错误] 返回码 10 —— 样本数超过上限 %d，拒绝加载\n", MAX_SAMPLES);
            return 10;
        }

        rc = parse_sample(fields, &data->samples[data->count]);
        if (rc != 0) {
            fclose(fp);
            data->count = 0;
            printf("[错误] 第 %d 行不合法，返回码 %d\n", line_no, rc);
            return rc;
        }

        data->count++;
    }

    fclose(fp);

    if (data->count == 0) {
        printf("[错误] 返回码 3 —— 文件里只有表头，没有任何数据行\n");
        return 3;
    }

    printf("[读取成功] %s：共 %d 条样本\n", path, data->count);
    return 0;
}

/* ---------- 本课新增：数据摘要 ---------- */
void print_summary(const Dataset* data)
{
    int i, j;
    int cnt0 = 0, cnt1 = 0;
    double mn, mx;

    printf("---- 数据摘要 ----\n");
    printf("样本条数：%d\n", data->count);

    for (j = 0; j < FEATURE_COUNT; j++) {
        mn = data->samples[0].features[j];
        mx = data->samples[0].features[j];
        for (i = 0; i < data->count; i++) {
            if (data->samples[i].features[j] < mn) mn = data->samples[i].features[j];
            if (data->samples[i].features[j] > mx) mx = data->samples[i].features[j];
        }
        printf("  特征[%d]: min = %-12g max = %g\n", j, mn, mx);
    }

    for (i = 0; i < data->count; i++) {
        if (data->samples[i].label == 0) cnt0++; else cnt1++;
    }
    printf("标签分布：良性(0) = %d 条，可疑(1) = %d 条\n", cnt0, cnt1);
}

int main(void)
{
    char paths[8][128] = {
        "..\\data\\train.csv",
        "..\\tests\\invalid\\T02_empty.csv",
        "..\\tests\\invalid\\T16_invalid_header.csv",
        "..\\tests\\invalid\\T09_exceeds_capacity.csv",
        "..\\tests\\invalid\\T12_entropy_over8.csv",
        "..\\tests\\invalid\\T04_byte00_over1.csv",
        "..\\tests\\invalid\\T19_blank_line.csv",
        "..\\tests\\invalid\\T18_non_integer_label.csv"
    };
    Dataset data;
    int i, rc;

    for (i = 0; i < 8; i++) {
        printf("\n===== %s =====\n", paths[i]);
        rc = load_csv(paths[i], &data, 1);
        printf(">>> load_csv 返回码 = %d\n", rc);
        if (rc == 0) print_summary(&data);
    }
    return 0;
}