#define _CRT_SECURE_NO_WARNINGS

#include <math.h>
#include <stdio.h>
#include <string.h>

#define FEATURE_COUNT 5
#define FIELD_COUNT   6      /* 带标签数据 6 列；待预测数据 5 列 */
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

/* ---------- 第 2 课：按半角逗号切分（本课扩展：支持 5 列或 6 列） ---------- */
int split_fields(char line[], char fields[][FIELD_MAX], int expect)
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

    if (count != expect) {
        printf("字段个数不符：应有 %d 个，实际 %d 个\n", expect, count);
        return 5;
    }
    return 0;
}

/* ---------- 第 3、4 课：转数值 + 两层校验 + 值域校验 ---------- */
int parse_sample(char fields[][FIELD_MAX], Sample* s, int has_label)
{
    double v, lv;
    int i;

    for (i = 0; i < FEATURE_COUNT; i++) {
        if (sscanf(fields[i], "%lf", &v) != 1) { printf("第 %d 个字段 \"%s\" 不是合法数值\n", i + 1, fields[i]); return 7; }
        if (!isfinite(v)) { printf("第 %d 个字段 \"%s\" 是非有限值（nan/inf）\n", i + 1, fields[i]); return 7; }
        s->features[i] = v;
    }

    if (s->features[0] < 0.0) { printf("file_size = %g 为负数（必须 >= 0）\n", s->features[0]); return 8; }
    if (s->features[1] < 0.0 || s->features[1] != floor(s->features[1])) { printf("string_count = %g 必须是非负整数\n", s->features[1]); return 8; }
    if (s->features[2] < 0.0 || s->features[2] > 8.0) { printf("entropy = %g 超出合法区间 [0, 8]\n", s->features[2]); return 8; }
    if (s->features[3] < 0.0 || s->features[3] > 1.0) { printf("byte_00_ratio = %g 超出合法区间 [0, 1]\n", s->features[3]); return 8; }
    if (s->features[4] < 0.0 || s->features[4] > 1.0) { printf("byte_ff_ratio = %g 超出合法区间 [0, 1]\n", s->features[4]); return 8; }

    if (!has_label) { s->label = -1; return 0; }      /* 待预测数据没有真实标签 */

    if (sscanf(fields[5], "%lf", &lv) != 1) { printf("label \"%s\" 不是合法数值\n", fields[5]); return 9; }
    if (!isfinite(lv)) { printf("label \"%s\" 是非有限值\n", fields[5]); return 9; }
    if (lv != floor(lv) || (lv != 0.0 && lv != 1.0)) { printf("label = %g 非法（只允许整数 0 或 1）\n", lv); return 9; }
    s->label = (int)lv;

    return 0;
}

/* ---------- 第 4 课：读整个 CSV ---------- */
int load_csv(const char* path, Dataset* data, int mode)
{
    FILE* fp;
    char line[1024];
    char fields[FIELD_COUNT][FIELD_MAX];
    int  expect = mode ? FIELD_COUNT : FEATURE_COUNT;
    int  line_no = 0, i, rc;

    data->count = 0;
    data->has_label = mode;

    fp = fopen(path, "r");
    if (fp == NULL) { printf("[错误] 返回码 2 —— 打不开文件 %s\n", path); return 2; }
    if (fgets(line, sizeof line, fp) == NULL) { fclose(fp); printf("[错误] 返回码 3 —— 文件是空的\n"); return 3; }
    line_no = 1;

    rc = split_fields(line, fields, expect);
    if (rc != 0) { fclose(fp); printf("[错误] 返回码 4 —— 表头字段个数不符\n"); return 4; }
    for (i = 0; i < FEATURE_COUNT; i++) {
        if (strcmp(fields[i], HEADER[i]) != 0) {
            fclose(fp);
            printf("[错误] 返回码 4 —— 表头第 %d 列应为 \"%s\"，实际是 \"%s\"\n", i + 1, HEADER[i], fields[i]);
            return 4;
        }
    }
    if (mode && strcmp(fields[FEATURE_COUNT], "label") != 0) {
        fclose(fp);
        printf("[错误] 返回码 4 —— 表头第 6 列应为 \"label\"，实际是 \"%s\"\n", fields[FEATURE_COUNT]);
        return 4;
    }

    while (fgets(line, sizeof line, fp) != NULL) {
        int blank = 1;
        ++line_no;

        for (i = 0; line[i] != '\0'; i++) {
            if (line[i] != ' ' && line[i] != '\t' && line[i] != '\r' && line[i] != '\n') { blank = 0; break; }
        }
        if (blank) { fclose(fp); data->count = 0; printf("[错误] 返回码 6 —— 第 %d 行是空白数据行\n", line_no); return 6; }

        rc = split_fields(line, fields, expect);
        if (rc != 0) { fclose(fp); data->count = 0; printf("[错误] 返回码 5 —— 第 %d 行字段数不符\n", line_no); return 5; }

        if (data->count >= MAX_SAMPLES) { fclose(fp); data->count = 0; printf("[错误] 返回码 10 —— 样本数超过上限 %d，拒绝加载\n", MAX_SAMPLES); return 10; }

        rc = parse_sample(fields, &data->samples[data->count], mode);
        if (rc != 0) { fclose(fp); data->count = 0; printf("[错误] 第 %d 行不合法，返回码 %d\n", line_no, rc); return rc; }

        data->count++;
    }
    fclose(fp);

    if (data->count == 0) { printf("[错误] 返回码 3 —— 文件里只有表头，没有任何数据行\n"); return 3; }

    printf("[读取成功] %s：共 %d 条样本（%s）\n", path, data->count, mode ? "含 label" : "无 label，用于预测");
    return 0;
}

/* ---------- 第 4 课：数据摘要 ---------- */
void print_summary(const Dataset* data)
{
    int i, j, cnt0 = 0, cnt1 = 0;
    double mn, mx;

    if (data->count <= 0) { printf("（当前没有已加载的数据）\n"); return; }

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
    if (data->has_label) {
        for (i = 0; i < data->count; i++) { if (data->samples[i].label == 0) cnt0++; else cnt1++; }
        printf("标签分布：良性(0) = %d 条，可疑(1) = %d 条\n", cnt0, cnt1);
    }
    else {
        printf("标签：无（这是待预测数据）\n");
    }
}

/* ---------- 第 5 课：读一行里空格分隔的 n 个数值 ---------- */
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
        if (k >= n) return 0;
        if (sscanf(token, "%lf", &out[k]) != 1) return 0;
        if (!isfinite(out[k])) return 0;
        k++;
    }
    return (k == n);
}

/* ---------- 第 5 课：加载模型 ---------- */
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
    if (strcmp(lines[0], "TEACHMODEL_V1") != 0) { printf("[错误] 返回码 11 —— 首行标识不是 TEACHMODEL_V1\n"); return 11; }
    if (sscanf(lines[1], "%lf", &dim) != 1 || dim != (double)FEATURE_COUNT) { printf("[错误] 返回码 11 —— 第 2 行维数应为 %d\n", FEATURE_COUNT); return 11; }
    if (!parse_number_list(lines[2], FEATURE_COUNT, m->weights)) { printf("[错误] 返回码 11 —— 第 3 行应有 %d 个权重\n", FEATURE_COUNT); return 11; }
    if (!parse_number_list(lines[3], 1, &m->bias)) { printf("[错误] 返回码 11 —— 第 4 行应有一个偏置\n"); return 11; }
    if (!parse_number_list(lines[4], FEATURE_COUNT, s->minv)) { printf("[错误] 返回码 11 —— 第 5 行应有 %d 个 min\n", FEATURE_COUNT); return 11; }
    if (!parse_number_list(lines[5], FEATURE_COUNT, s->maxv)) { printf("[错误] 返回码 11 —— 第 6 行应有 %d 个 max\n", FEATURE_COUNT); return 11; }

    m->trained = 1;
    printf("[模型加载成功] %s：5 个权重 + 偏置 + 5 组 min/max\n", path);
    return 0;
}

/* ---------- 第 6 课（新增）：保存模型 ---------- */
int save_model(const char* path, const Model* m, const FeatureScaler* s)
{
    FILE* fp = fopen(path, "w");
    int i;

    if (fp == NULL) { printf("[错误] 返回码 2 —— 无法写入模型文件：%s\n", path); return 2; }

    fprintf(fp, "TEACHMODEL_V1\n");
    fprintf(fp, "%d\n", FEATURE_COUNT);
    for (i = 0; i < FEATURE_COUNT; i++) fprintf(fp, "%.17g%s", m->weights[i], (i == FEATURE_COUNT - 1) ? "\n" : " ");
    fprintf(fp, "%.17g\n", m->bias);
    for (i = 0; i < FEATURE_COUNT; i++) fprintf(fp, "%.17g%s", s->minv[i], (i == FEATURE_COUNT - 1) ? "\n" : " ");
    for (i = 0; i < FEATURE_COUNT; i++) fprintf(fp, "%.17g%s", s->maxv[i], (i == FEATURE_COUNT - 1) ? "\n" : " ");

    fclose(fp);
    printf("[保存成功] 模型（权重 + 偏置 + 归一化 min/max）已写入 \"%s\"\n", path);
    return 0;
}

/* ---------- 第 6 课（新增）：去掉行尾 '\n' 和 '\r' ---------- */
static void strip_newline(char* s)
{
    int L = (int)strlen(s);
    while (L > 0) {
        if (s[L - 1] == '\n' || s[L - 1] == '\r') { s[L - 1] = '\0'; L--; }
        else break;
    }
}

/* ---------- 第 5 课：归一化 + sigmoid ---------- */
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

/* ---------- 第 5 课：逐条预测 ---------- */
int predict_all(const Dataset* data, const Model* m, const FeatureScaler* s)
{
    int i, correct = 0, suspicious = 0;

    if (data->count <= 0) { printf("（当前没有已加载的数据）\n"); return 0; }

    printf("\n---- 预测结果（阈值 0.5：p >= 0.5 判为可疑）----\n");
    for (i = 0; i < data->count; i++) {
        double p = predict_probability(m, s, data->samples[i].features);
        int pred = classify(p);
        if (pred == 1) suspicious++;
        if (data->has_label) {
            if (pred == data->samples[i].label) correct++;
            printf("[%d] p = %.10f  预测 = %s(%d)   真实 = %d\n", i + 1, p, pred ? "可疑" : "良性", pred, data->samples[i].label);
        }
        else {
            printf("[%d] p = %.10f  预测 = %s(%d)\n", i + 1, p, pred ? "可疑" : "良性", pred);
        }
    }
    printf("共 %d 条：可疑 %d 条，良性 %d 条", data->count, suspicious, data->count - suspicious);
    if (data->has_label) printf("；与真实标签一致 %d/%d\n", correct, data->count);
    else printf("\n");

    return correct;
}

/* ---------- 第 6 课（新增）：一致性自检 ---------- */
int consistency_check(const Dataset* data, const Model* m, const FeatureScaler* s)
{
    Model         m2;
    FeatureScaler s2;
    const char* tmp = "model_reloaded.txt";
    double p1, p2, diff;
    int rc;

    if (data->count <= 0 || m->trained != 1) {
        printf("[错误] 返回码 12 —— 需要先加载数据（菜单 1 或 2）与模型（菜单 4）\n");
        return 12;
    }

    p1 = predict_probability(m, s, data->samples[0].features);

    rc = save_model(tmp, m, s);
    if (rc != 0) return rc;

    m2.trained = 0;
    rc = load_model(tmp, &m2, &s2);
    if (rc != 0) { printf("重新加载失败，返回码 %d\n", rc); return 11; }

    p2 = predict_probability(&m2, &s2, data->samples[0].features);
    diff = p1 - p2;
    if (diff < 0) diff = -diff;

    printf("\n---- 模型保存/加载一致性自检（第 1 条样本）----\n");
    printf("p1 = %.12f\n", p1);
    printf("p2 = %.12f\n", p2);
    printf("|p1 - p2| = %.3e（判据 1e-12）→ %s\n", diff, (diff > 1e-12) ? "不一致" : "一致，通过");

    return 0;
}

/* ---------- 第 6 课（新增）：命令行菜单 ---------- */
int main(void)
{
    Dataset       data;
    Model         model;
    FeatureScaler scaler;
    char          line[1024];
    int           choice;

    data.count = 0;
    data.has_label = 0;
    model.trained = 0;

    printf("小型静态特征识别器（教学用）· TEACHMODEL_V1\n");

    for (;;) {
        printf("\n---------------- 菜单 ----------------\n");
        printf(" 1 读取带标签数据集（CSV，6 列）\n");
        printf(" 2 读取待预测数据集（CSV，5 列，无 label）\n");
        printf(" 3 独立校验已加载的数据\n");
        printf(" 4 加载模型文件（TEACHMODEL_V1）\n");
        printf(" 5 保存模型文件\n");
        printf(" 6 预测全部样本\n");
        printf(" 7 一致性自检（保存→重新加载→比较概率）\n");
        printf(" 8 显示安全声明\n");
        printf(" 0 退出\n");
        printf("--------------------------------------\n");
        printf("请选择：");

        if (fgets(line, sizeof line, stdin) == NULL) break;
        if (sscanf(line, "%d", &choice) != 1) { printf("无效选择，请输入 0-8 之间的数字\n"); continue; }

        switch (choice) {
        case 1:
            printf("请输入带标签 CSV 路径（例如 ..\\data\\train.csv）：");
            if (fgets(line, sizeof line, stdin) == NULL) break;
            strip_newline(line);
            if (load_csv(line, &data, 1) == 0) print_summary(&data);
            break;
        case 2:
            printf("请输入待预测 CSV 路径（例如 ..\\data\\predict.csv）：");
            if (fgets(line, sizeof line, stdin) == NULL) break;
            strip_newline(line);
            if (load_csv(line, &data, 0) == 0) print_summary(&data);
            break;
        case 3:
            if (data.count <= 0) { printf("[错误] 返回码 3 —— 还没有加载任何数据\n"); break; }
            if (data.count > MAX_SAMPLES) { printf("[错误] 返回码 10 —— 样本数超过 %d\n", MAX_SAMPLES); break; }
            printf("[校验通过] 当前数据：%d 条，含标签：%s\n", data.count, data.has_label ? "是" : "否");
            break;
        case 4:
            printf("请输入模型文件路径（例如 ..\\data\\reference_model.txt）：");
            if (fgets(line, sizeof line, stdin) == NULL) break;
            strip_newline(line);
            (void)load_model(line, &model, &scaler);
            break;
        case 5:
            if (model.trained != 1) { printf("[错误] 返回码 12 —— 还没有可保存的模型，请先用菜单 4 加载\n"); break; }
            printf("请输入要保存到的模型文件路径（例如 ..\\report\\model_saved.txt）：");
            if (fgets(line, sizeof line, stdin) == NULL) break;
            strip_newline(line);
            (void)save_model(line, &model, &scaler);
            break;
        case 6:
            if (data.count <= 0) { printf("[错误] 返回码 12 —— 请先读取数据（菜单 1 或 2）\n"); break; }
            if (model.trained != 1) { printf("[错误] 返回码 12 —— 请先加载模型（菜单 4）\n"); break; }
            (void)predict_all(&data, &model, &scaler);
            break;
        case 7:
            (void)consistency_check(&data, &model, &scaler);
            break;
        case 8:
            printf("\n安全声明：\n");
            printf(" 本程序只处理教师提供的 CSV 合成静态特征；\n");
            printf(" 不下载、不执行、不传播任何真实恶意程序；\n");
            printf(" 输出的「良性 / 可疑」只是教学模型的类别预测，不构成真实安全判定。\n");
            break;
        case 0:
            printf("\n程序结束。\n");
            return 0;
        default:
            printf("无效选择，请输入 0-8 之间的数字\n");
            break;
        }
    }

    return 0;
}