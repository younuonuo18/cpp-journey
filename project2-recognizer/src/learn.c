#define _CRT_SECURE_NO_WARNINGS

#include <math.h>
#include <stdio.h>
#include <string.h>

#define FEATURE_COUNT 5
#define FIELD_COUNT   6
#define FIELD_MAX     64
#define MAX_SAMPLES   512
#define MODEL_LINES   10

typedef struct { double features[FEATURE_COUNT]; int label; } Sample;
typedef struct { Sample samples[MAX_SAMPLES]; int count; int has_label; } Dataset;
typedef struct { double minv[FEATURE_COUNT]; double maxv[FEATURE_COUNT]; } FeatureScaler;
typedef struct { double weights[FEATURE_COUNT]; double bias; int trained; } Model;

static char HEADER[FIELD_COUNT][16] = {
    "file_size", "string_count", "entropy", "byte_00_ratio", "byte_ff_ratio", "label"
};

/* ---------- 第 2 课：切分 ---------- */
int split_fields(char line[], char fields[][FIELD_MAX])
{
    int count = 1, f = 0, k = 0, i;
    for (i = 0; i < FIELD_COUNT; i++) fields[i][0] = '\0';
    for (i = 0; line[i] != '\0'; i++) if (line[i] == '\n') { line[i] = '\0'; break; }
    for (i = 0; line[i] != '\0'; i++) {
        if (line[i] == ',') {
            if (f < FIELD_COUNT) fields[f][k] = '\0';
            count++; f++; k = 0;
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

/* ---------- 第 3、4 课：转数值 + 两层校验 + 值域校验 ---------- */
int parse_sample(char fields[][FIELD_MAX], Sample* s)
{
    double v, lv;
    int i;
    for (i = 0; i < FEATURE_COUNT; i++) {
        if (sscanf(fields[i], "%lf", &v) != 1) { printf("第 %d 个字段 \"%s\" 不是合法数值\n", i + 1, fields[i]); return 7; }
        if (!isfinite(v)) { printf("第 %d 个字段 \"%s\" 是非有限值\n", i + 1, fields[i]); return 7; }
        s->features[i] = v;
    }
    if (s->features[0] < 0.0) { printf("file_size = %g 为负数\n", s->features[0]); return 8; }
    if (s->features[1] < 0.0 || s->features[1] != floor(s->features[1])) { printf("string_count = %g 必须是非负整数\n", s->features[1]); return 8; }
    if (s->features[2] < 0.0 || s->features[2] > 8.0) { printf("entropy = %g 超出 [0, 8]\n", s->features[2]); return 8; }
    if (s->features[3] < 0.0 || s->features[3] > 1.0) { printf("byte_00_ratio = %g 超出 [0, 1]\n", s->features[3]); return 8; }
    if (s->features[4] < 0.0 || s->features[4] > 1.0) { printf("byte_ff_ratio = %g 超出 [0, 1]\n", s->features[4]); return 8; }
    if (sscanf(fields[5], "%lf", &lv) != 1) { printf("label \"%s\" 不是合法数值\n", fields[5]); return 9; }
    if (!isfinite(lv)) { printf("label \"%s\" 是非有限值\n", fields[5]); return 9; }
    if (lv != floor(lv) || (lv != 0.0 && lv != 1.0)) { printf("label = %g 非法\n", lv); return 9; }
    s->label = (int)lv;
    return 0;
}

/* ---------- 第 4 课：读整个文件 ---------- */
int load_csv(const char* path, Dataset* data, int mode)
{
    FILE* fp;
    char line[1024];
    char fields[FIELD_COUNT][FIELD_MAX];
    int  line_no = 0, i, rc;

    data->count = 0;
    data->has_label = mode;

    fp = fopen(path, "r");
    if (fp == NULL) { printf("[错误] 返回码 2 —— 打不开文件 %s\n", path); return 2; }
    if (fgets(line, sizeof line, fp) == NULL) { fclose(fp); printf("[错误] 返回码 3 —— 空文件\n"); return 3; }
    line_no = 1;

    rc = split_fields(line, fields);
    if (rc != 0) { fclose(fp); printf("[错误] 返回码 4 —— 表头字段个数不符\n"); return 4; }
    for (i = 0; i < FIELD_COUNT; i++) {
        if (strcmp(fields[i], HEADER[i]) != 0) {
            fclose(fp);
            printf("[错误] 返回码 4 —— 表头第 %d 列应为 \"%s\"，实际是 \"%s\"\n", i + 1, HEADER[i], fields[i]);
            return 4;
        }
    }

    while (fgets(line, sizeof line, fp) != NULL) {
        int blank = 1;
        ++line_no;
        for (i = 0; line[i] != '\0'; i++) {
            if (line[i] != ' ' && line[i] != '\t' && line[i] != '\r' && line[i] != '\n') { blank = 0; break; }
        }
        if (blank) { fclose(fp); data->count = 0; printf("[错误] 返回码 6 —— 第 %d 行是空白行\n", line_no); return 6; }

        rc = split_fields(line, fields);
        if (rc != 0) { fclose(fp); data->count = 0; printf("[错误] 返回码 5 —— 第 %d 行字段数不符\n", line_no); return 5; }

        if (data->count >= MAX_SAMPLES) { fclose(fp); data->count = 0; printf("[错误] 返回码 10 —— 超过上限 %d\n", MAX_SAMPLES); return 10; }

        rc = parse_sample(fields, &data->samples[data->count]);
        if (rc != 0) { fclose(fp); data->count = 0; printf("[错误] 第 %d 行不合法，返回码 %d\n", line_no, rc); return rc; }

        data->count++;
    }
    fclose(fp);

    if (data->count == 0) { printf("[错误] 返回码 3 —— 只有表头\n"); return 3; }
    printf("[读取成功] %s：共 %d 条样本\n", path, data->count);
    return 0;
}

/* ---------- 本课：读一行里空格分隔的 n 个数值 ---------- */
static int parse_number_list(const char* line, int n, double* out)
{
    int i = 0, k = 0;
    int len = (int)strlen(line);

    while (i < len) {
        char token[64];
        int t = 0;

        while (i < len && (line[i] == ' ' || line[i] == '\t' || line[i] == '\r' || line[i] == '\n')) i++;
        if (i >= len) break;

        while (i < len && line[i] != ' ' && line[i] != '\t' && line[i] != '\r' && line[i] != '\n') {
            if (t < (int)sizeof token - 1) token[t++] = line[i];
            i++;
        }
        token[t] = '\0';

        if (k >= n) return 0;                                     /* 数值个数太多 */
        if (sscanf(token, "%lf", &out[k]) != 1) return 0;         /* 非数值 */
        if (!isfinite(out[k])) return 0;                          /* nan / inf */
        k++;
    }
    return (k == n);                                              /* 必须正好 n 个 */
}

/* ---------- 本课：加载模型 ---------- */
int load_model(const char* path, Model* m, FeatureScaler* s)
{
    FILE* fp;
    char lines[MODEL_LINES][256];
    int  count = 0, i;
    double dim = 0.0;

    m->trained = 0;

    fp = fopen(path, "r");
    if (fp == NULL) { printf("[错误] 返回码 2 —— 模型文件打不开：%s\n", path); return 2; }

    while (count < MODEL_LINES && fgets(lines[count], sizeof lines[count], fp) != NULL) {
        int blank = 1;
        lines[count][strcspn(lines[count], "\r\n")] = '\0';
        for (i = 0; lines[count][i] != '\0'; i++) {
            if (lines[count][i] != ' ' && lines[count][i] != '\t') { blank = 0; break; }
        }
        if (blank) continue;
        count++;
    }
    fclose(fp);

    if (count != 6) { printf("[错误] 返回码 11 —— 模型文件应有 6 行，实际 %d 行\n", count); return 11; }
    if (strcmp(lines[0], "TEACHMODEL_V1") != 0) { printf("[错误] 返回码 11 —— 标识不是 TEACHMODEL_V1\n"); return 11; }
    if (sscanf(lines[1], "%lf", &dim) != 1 || dim != (double)FEATURE_COUNT) {
        printf("[错误] 返回码 11 —— 维数应为 %d\n", FEATURE_COUNT);
        return 11;
    }
    if (!parse_number_list(lines[2], FEATURE_COUNT, m->weights)) { printf("[错误] 返回码 11 —— 权重行不合法\n"); return 11; }
    if (!parse_number_list(lines[3], 1, &m->bias)) { printf("[错误] 返回码 11 —— 偏置行不合法\n"); return 11; }
    if (!parse_number_list(lines[4], FEATURE_COUNT, s->minv)) { printf("[错误] 返回码 11 —— min 行不合法\n"); return 11; }
    if (!parse_number_list(lines[5], FEATURE_COUNT, s->maxv)) { printf("[错误] 返回码 11 —— max 行不合法\n"); return 11; }

    m->trained = 1;
    printf("[模型加载成功] %s：5 个权重 + 偏置 + 5 组 min/max\n", path);
    return 0;
}

/* ---------- 本课：归一化 + sigmoid ---------- */
double predict_probability(const Model* m, const FeatureScaler* s, const double raw[FEATURE_COUNT])
{
    double z = m->bias;
    int i;

    for (i = 0; i < FEATURE_COUNT; i++) {
        double span = s->maxv[i] - s->minv[i];
        double x = (span == 0.0) ? 0.0 : (raw[i] - s->minv[i]) / span;
        z += m->weights[i] * x;
    }
    if (z > 700.0)  z = 700.0;
    if (z < -700.0) z = -700.0;

    return 1.0 / (1.0 + exp(-z));
}

int classify(double p) { return (p >= 0.5) ? 1 : 0; }

/* ---------- 本课：逐条预测并打印（便于与标准答案对拍） ---------- */
int predict_all(const Dataset* data, const Model* m, const FeatureScaler* s)
{
    int i, correct = 0, suspicious = 0;

    printf("\n---- 预测结果（阈值 0.5）----\n");
    for (i = 0; i < data->count; i++) {
        double p = predict_probability(m, s, data->samples[i].features);
        int pred = classify(p);
        if (pred == 1) suspicious++;
        if (pred == data->samples[i].label) correct++;
        printf("[%d] p = %.10f  预测 = %s(%d)   真实 = %d\n",
            i + 1, p, pred ? "可疑" : "良性", pred, data->samples[i].label);
    }
    printf("共 %d 条：可疑 %d 条；与真实标签一致 %d/%d\n", data->count, suspicious, correct, data->count);
    return correct;
}

int main(void)
{
    Dataset data;
    Model   model;
    FeatureScaler scaler;
    Model   bad_model;
    FeatureScaler bad_scaler;
    int rc;

    printf("===== 1) 加载模型 =====\n");
    rc = load_model("..\\data\\reference_model.txt", &model, &scaler);
    if (rc != 0) return rc;

    printf("\n===== 2) 读取 sample.csv（8 条带标签）=====\n");
    rc = load_csv("..\\data\\sample.csv", &data, 1);
    if (rc != 0) return rc;

    printf("\n===== 3) 逐条预测 =====\n");
    predict_all(&data, &model, &scaler);

    printf("\n===== 4) 坏模型文件应被拒绝 =====\n");
    rc = load_model("..\\tests\\invalid\\T10_bad_model.txt", &bad_model, &bad_scaler);
    printf(">>> load_model 返回码 = %d（期望 11）\n", rc);

    return 0;
}