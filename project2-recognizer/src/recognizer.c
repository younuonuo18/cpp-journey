/*
 * ============================================================================
 *  recognizer.c —— 基于静态特征的小型恶意代码识别器（教学项目）
 *  《高级语言程序设计》综合实践作业 · 个人独立完成
 *
 *  安全边界：本程序只处理 CSV 中的**人工构造静态特征数值**
 *           （file_size, string_count, entropy, byte_00_ratio, byte_ff_ratio），
 *            不下载、不运行、不传播任何真实恶意代码；
 *            输出的「良性 / 可疑」仅为教学模型的类别预测，不构成真实安全判定。
 *
 *  编译（Visual Studio 2026 / MSVC）：打开 recognizer.slnx → Debug|x64 → Ctrl+F5
 *  编译（gcc）：
 *      gcc -std=c11 -O2 -Wall -Wextra -pedantic recognizer.c -lm -o recognizer
 *
 *  运行方式：
 *    recognizer.exe                              交互菜单（默认）
 *    recognizer.exe check   <csv>                校验模式：读取+校验，用退出码表示结果
 *    recognizer.exe predict <csv> <model.txt>    预测模式：读无标签数据+模型，打印预测
 * ============================================================================
 */

#define _CRT_SECURE_NO_WARNINGS

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ 常量 */
#define FEATURE_COUNT 5      /* 五个静态特征 */
#define MAX_SAMPLES   512    /* 最大样本数（教师框架规定，超出必须拒绝） */
#define LINE_MAX      1024   /* 一行最多读入的字符数 */
#define MODEL_MAXLINE 10     /* 模型文件最多允许的行数（防越界） */

/* ---------------------------------------------------------------- 错误码 */
/* 约定：0 表示成功，非 0 表示失败；不同数字对应不同原因，便于测试与记录 */
#define ERR_OK           0
#define ERR_OPEN         2   /* 文件不存在 / 打不开 */
#define ERR_EMPTY        3   /* 空文件、只有表头、没有数据行 */
#define ERR_HEADER       4   /* 表头字段名或次序不符 */
#define ERR_FIELDCOUNT   5   /* 一行里字段个数不符（多了或少了） */
#define ERR_EMPTYFIELD   6   /* 有字段是空的，或整行是空白 */
#define ERR_NONNUMERIC   7   /* 不是合法数值（abc / nan / inf / 42abc） */
#define ERR_RANGE        8   /* 数值超出合法范围，或该整数却是小数 */
#define ERR_LABEL        9   /* 标签不是 0 或 1 */
#define ERR_CAPACITY    10   /* 样本数超过 MAX_SAMPLES */
#define ERR_MODELFORMAT 11   /* 模型文件缺行 / 缺参数 / 损坏 */
#define ERR_NOMODEL     12   /* 还没加载模型就要求预测 */

/* 特征名字（表头校验用，次序固定） */
static const char *FEATURE_NAME[FEATURE_COUNT] = {
    "file_size", "string_count", "entropy", "byte_00_ratio", "byte_ff_ratio"
};

/* ------------------------------------------------------------ 数据结构 */
/* 说明：Sample / Dataset / FeatureScaler / Model 沿用教师提供的骨架；
 *       Dataset 里额外增加了 has_label 字段，用来区分
 *       「带标签的训练/测试数据」和「不带标签的待预测数据」。 */
typedef struct {
    double features[FEATURE_COUNT];
    int    label;
} Sample;

typedef struct {
    Sample samples[MAX_SAMPLES];
    int    count;
    int    has_label;          /* 1 = 每行都有 label；0 = 待预测数据 */
} Dataset;

typedef struct {
    double minv[FEATURE_COUNT];
    double maxv[FEATURE_COUNT];
} FeatureScaler;

typedef struct {
    double weights[FEATURE_COUNT];
    double bias;
    int    trained;
} Model;

/* ====================================================================== */
/*  第一部分：字符串与数值的基础工具                                       */
/* ====================================================================== */

/* 判断一段文本是不是"全是空白"（空行、只有空格的行都算） */
static int is_blank(const char *s)
{
    if (s == NULL) return 1;
    for (; *s != '\0'; ++s) {
        if (!isspace((unsigned char)*s)) return 0;
    }
    return 1;
}

/* 就地去掉字符串首尾的空白字符，返回新的起点 */
static char *trim(char *s)
{
    char *end;
    if (s == NULL) return NULL;
    while (*s != '\0' && isspace((unsigned char)*s)) ++s;      /* 去头部 */
    end = s + strlen(s);
    while (end > s && isspace((unsigned char)end[-1])) --end;  /* 去尾部 */
    *end = '\0';
    return s;
}

/* 数一行里有多少个逗号分隔的字段（"a,b,c" → 3；空行 → 1） */
static int count_fields(const char *line)
{
    int n = 1;
    for (; *line != '\0'; ++line) {
        if (*line == ',') ++n;
    }
    return n;
}

/* 把一行按逗号切开，就地改写成多个以 '\0' 结尾的片段。
 * 要求：调用前已经用 count_fields 确认字段数不会超过 max_fields。 */
static void split_fields(char *line, char *fields[], int field_count)
{
    int i = 0;
    char *p = line;
    fields[i++] = p;
    for (; *p != '\0'; ++p) {
        if (*p == ',') {
            *p = '\0';
            if (i < field_count) fields[i++] = p + 1;
        }
    }
    /* 每一段都去掉首尾空白（T20：数字前面有空格也必须能解析） */
    for (i = 0; i < field_count; ++i) {
        fields[i] = trim(fields[i]);
    }
}

/* 严格解析一个数值字段。
 * 返回 1 = 成功，0 = 不是合法数值。
 * 拒绝的情况：空、多个字符里混着非数字（42abc）、nan、inf、0x 形式的十六进制。 */
static int parse_number(const char *tok, double *out)
{
    char *end = NULL;
    double v;
    unsigned char first;

    if (tok == NULL || *tok == '\0') return 0;

    first = (unsigned char)tok[0];
    /* 必须以数字、正负号或小数点开头：这样 "nan" "inf" "NaN" 一进来就被挡住 */
    if (!(isdigit(first) || first == '+' || first == '-' || first == '.')) return 0;
    /* 不接受的写法（本作业的数值特征不会出现 x/X） */
    if (strpbrk(tok, "xX") != NULL) return 0;

    errno = 0;
    v = strtod(tok, &end);
    if (end == tok) return 0;                       /* 一个数字都没解析出来 */
    while (end != NULL && *end != '\0' && isspace((unsigned char)*end)) ++end;
    if (end == NULL || *end != '\0') return 0;      /* 后面还有多余的字符 */
    if (!isfinite(v)) return 0;                     /* inf / nan 一律拒绝 */

    *out = v;
    return 1;
}

/* 判断一个 double 是不是"整数值"（42.0 是，42.5 不是） */
static int is_integer_value(double v)
{
    return v == floor(v);
}

/* 取出路径里的文件名部分，仅用于打印提示 */
static const char *base_name(const char *path)
{
    const char *slash1 = strrchr(path, '/');
    const char *slash2 = strrchr(path, '\\');
    const char *s = path;
    if (slash1 != NULL && slash1 + 1 > s) s = slash1 + 1;
    if (slash2 != NULL && slash2 + 1 > s) s = slash2 + 1;
    return s;
}

/* 统一打印错误信息：给出错误码、文件名、具体原因 */
static void report_error(int code, const char *path, const char *reason)
{
    printf("\n[错误] 返回码 %d —— 处理文件 \"%s\" 失败\n", code, base_name(path));
    printf("       原因：%s\n", reason);
}

/* ====================================================================== */
/*  第二部分：CSV 读取与数据校验（任务二）                                 */
/* ====================================================================== */

/* 只校验五个特征的取值范围；成功返回 ERR_OK，否则返回对应错误码 */
static int check_feature_range(const char *path, int line_no,
                               const double v[FEATURE_COUNT], char *why, size_t why_size)
{
    if (v[0] < 0.0) {
        snprintf(why, why_size, "第 %d 行 file_size = %.6g 为负数（必须 >= 0）", line_no, v[0]);
        return ERR_RANGE;
    }
    if (v[1] < 0.0 || !is_integer_value(v[1])) {
        snprintf(why, why_size, "第 %d 行 string_count = %.6g 必须是非负整数", line_no, v[1]);
        return ERR_RANGE;
    }
    if (v[2] < 0.0 || v[2] > 8.0) {
        snprintf(why, why_size, "第 %d 行 entropy = %.6g 超出合法区间 [0, 8]", line_no, v[2]);
        return ERR_RANGE;
    }
    if (v[3] < 0.0 || v[3] > 1.0) {
        snprintf(why, why_size, "第 %d 行 byte_00_ratio = %.6g 超出合法区间 [0, 1]", line_no, v[3]);
        return ERR_RANGE;
    }
    if (v[4] < 0.0 || v[4] > 1.0) {
        snprintf(why, why_size, "第 %d 行 byte_ff_ratio = %.6g 超出合法区间 [0, 1]", line_no, v[4]);
        return ERR_RANGE;
    }
    (void)path;
    return ERR_OK;
}

/* 读取 CSV 的核心函数。
 * mode = 1：必须 6 列（带答案，用于训练/测试）
 * mode = 0：必须 5 列（不带答案，用于预测）
 * mode = 2：自动按表头列数判断（6 列 = 带标签，5 列 = 待预测）
 * 只要有任何一行不合法，整份数据都不加载（data->count = 0）。 */
#define LOAD_MODE_PREDICT 0
#define LOAD_MODE_LABELED 1
#define LOAD_MODE_AUTO    2

static int load_csv(const char *path, Dataset *data, int mode)
{
    FILE *fp;
    char line[LINE_MAX];
    char reason[256];
    char *fields[8];
    int  expect_label = (mode == LOAD_MODE_LABELED);
    int  expect = expect_label ? (FEATURE_COUNT + 1) : FEATURE_COUNT;
    int  line_no = 0;
    int  data_rows = 0;
    int  overflow = 0;
    int  i, j;

    data->count = 0;
    data->has_label = expect_label;

    /* ---- 1. 打开文件 ---- */
    fp = fopen(path, "r");
    if (fp == NULL) {
        snprintf(reason, sizeof reason, "文件不存在或无法打开（请检查路径是否正确）");
        report_error(ERR_OPEN, path, reason);
        return ERR_OPEN;
    }

    /* ---- 2. 读表头并逐列核对 ---- */
    if (fgets(line, sizeof line, fp) == NULL) {
        fclose(fp);
        report_error(ERR_EMPTY, path, "文件是空的，连表头都没有");
        return ERR_EMPTY;
    }
    line_no = 1;
    line[strcspn(line, "\r\n")] = '\0';
    if (is_blank(line)) {
        fclose(fp);
        report_error(ERR_HEADER, path, "第 1 行（表头）是空白行");
        return ERR_HEADER;
    }
    if (mode == LOAD_MODE_AUTO) {
        /* 自动模式：先看表头有几列，再决定这是"带标签数据"还是"待预测数据" */
        int nf = count_fields(line);
        if (nf != FEATURE_COUNT && nf != FEATURE_COUNT + 1) {
            snprintf(reason, sizeof reason,
                     "表头字段个数不符：应为 %d 列（无标签）或 %d 列（带标签），实际 %d 列",
                     FEATURE_COUNT, FEATURE_COUNT + 1, nf);
            fclose(fp);
            report_error(ERR_HEADER, path, reason);
            return ERR_HEADER;
        }
        expect_label = (nf == FEATURE_COUNT + 1);
        expect = nf;
        data->has_label = expect_label;
    } else if (count_fields(line) != expect) {
        snprintf(reason, sizeof reason, "表头字段个数不符：应有 %d 列，实际 %d 列",
                 expect, count_fields(line));
        fclose(fp);
        report_error(ERR_HEADER, path, reason);
        return ERR_HEADER;
    }
    split_fields(line, fields, expect);
    for (j = 0; j < FEATURE_COUNT; ++j) {
        if (strcmp(fields[j], FEATURE_NAME[j]) != 0) {
            snprintf(reason, sizeof reason,
                     "表头第 %d 列应为 \"%s\"，实际是 \"%s\"（字段名称或次序不符）",
                     j + 1, FEATURE_NAME[j], fields[j]);
            fclose(fp);
            report_error(ERR_HEADER, path, reason);
            return ERR_HEADER;
        }
    }
    if (expect_label && strcmp(fields[FEATURE_COUNT], "label") != 0) {
        snprintf(reason, sizeof reason, "表头第 %d 列应为 \"label\"，实际是 \"%s\"",
                 FEATURE_COUNT + 1, fields[FEATURE_COUNT]);
        fclose(fp);
        report_error(ERR_HEADER, path, reason);
        return ERR_HEADER;
    }

    /* ---- 3. 逐行解析数据 ---- */
    while (fgets(line, sizeof line, fp) != NULL) {
        double v[FEATURE_COUNT];
        int    label = -1;
        int    rc;

        ++line_no;
        line[strcspn(line, "\r\n")] = '\0';

        if (is_blank(line)) {
            snprintf(reason, sizeof reason, "第 %d 行是空白数据行（不允许）", line_no);
            fclose(fp);
            report_error(ERR_EMPTYFIELD, path, reason);
            return ERR_EMPTYFIELD;
        }

        ++data_rows;
        if (data_rows > MAX_SAMPLES) {
            overflow = 1;          /* 超限的行不再解析，读完文件后统一报错 */
            continue;
        }

        if (count_fields(line) != expect) {
            snprintf(reason, sizeof reason, "第 %d 行字段个数不符：应有 %d 个，实际 %d 个",
                     line_no, expect, count_fields(line));
            fclose(fp);
            report_error(ERR_FIELDCOUNT, path, reason);
            return ERR_FIELDCOUNT;
        }
        split_fields(line, fields, expect);

        for (j = 0; j < FEATURE_COUNT; ++j) {
            if (is_blank(fields[j])) {
                snprintf(reason, sizeof reason, "第 %d 行第 %d 列是空字段，无法解析",
                         line_no, j + 1);
                fclose(fp);
                report_error(ERR_EMPTYFIELD, path, reason);
                return ERR_EMPTYFIELD;
            }
            if (!parse_number(fields[j], &v[j])) {
                snprintf(reason, sizeof reason, "第 %d 行第 %d 列 = \"%s\" 不是合法数值",
                         line_no, j + 1, fields[j]);
                fclose(fp);
                report_error(ERR_NONNUMERIC, path, reason);
                return ERR_NONNUMERIC;
            }
        }

        rc = check_feature_range(path, line_no, v, reason, sizeof reason);
        if (rc != ERR_OK) {
            fclose(fp);
            report_error(rc, path, reason);
            return rc;
        }

        if (expect_label) {
            double lv;
            if (is_blank(fields[FEATURE_COUNT])) {
                snprintf(reason, sizeof reason, "第 %d 行 label 是空字段", line_no);
                fclose(fp);
                report_error(ERR_EMPTYFIELD, path, reason);
                return ERR_EMPTYFIELD;
            }
            if (!parse_number(fields[FEATURE_COUNT], &lv)) {
                snprintf(reason, sizeof reason, "第 %d 行 label = \"%s\" 不是合法数值",
                         line_no, fields[FEATURE_COUNT]);
                fclose(fp);
                report_error(ERR_NONNUMERIC, path, reason);
                return ERR_NONNUMERIC;
            }
            if (!is_integer_value(lv) || (lv != 0.0 && lv != 1.0)) {
                snprintf(reason, sizeof reason, "第 %d 行 label = %.6g 非法（只允许整数 0 或 1）",
                         line_no, lv);
                fclose(fp);
                report_error(ERR_LABEL, path, reason);
                return ERR_LABEL;
            }
            label = (int)lv;
        }

        for (i = 0; i < FEATURE_COUNT; ++i) {
            data->samples[data->count].features[i] = v[i];
        }
        data->samples[data->count].label = label;
        ++data->count;
    }

    fclose(fp);

    /* ---- 4. 收尾检查 ---- */
    if (overflow) {
        snprintf(reason, sizeof reason,
                 "样本数 %d 超过上限 %d，为防数组越界已拒绝加载整份数据",
                 data_rows, MAX_SAMPLES);
        data->count = 0;
        report_error(ERR_CAPACITY, path, reason);
        return ERR_CAPACITY;
    }
    if (data->count == 0) {
        report_error(ERR_EMPTY, path, "文件里只有表头，没有任何数据行");
        return ERR_EMPTY;
    }

    printf("[读取成功] %s：共 %d 条样本（%s）\n",
           base_name(path), data->count,
           expect_label ? "含 label" : "无 label，用于预测");
    return ERR_OK;
}

/* 教师框架里的接口：读取带标签数据集 */
static int load_dataset(const char *path, Dataset *data)
{
    return load_csv(path, data, LOAD_MODE_LABELED);
}

/* 读取待预测数据集（没有 label 列） */
static int load_predict(const char *path, Dataset *data)
{
    return load_csv(path, data, LOAD_MODE_PREDICT);
}

/* 自动判断：6 列按带标签读，5 列按待预测读（命令行预测模式使用） */
static int load_auto(const char *path, Dataset *data)
{
    return load_csv(path, data, LOAD_MODE_AUTO);
}

/* 对已经加载进内存的数据做一次独立复检（不依赖读取过程） */
static int validate_dataset(const Dataset *data)
{
    int i, j;

    if (data == NULL || data->count <= 0) {
        printf("[校验失败] 返回码 %d —— 数据集为空（没有样本可校验）\n", ERR_EMPTY);
        return ERR_EMPTY;
    }
    if (data->count > MAX_SAMPLES) {
        printf("[校验失败] 返回码 %d —— 样本数 %d 超过上限 %d\n",
               ERR_CAPACITY, data->count, MAX_SAMPLES);
        return ERR_CAPACITY;
    }

    for (i = 0; i < data->count; ++i) {
        const Sample *s = &data->samples[i];
        if (data->has_label && s->label != 0 && s->label != 1) {
            printf("[校验失败] 返回码 %d —— 第 %d 条样本 label = %d 非法\n",
                   ERR_LABEL, i + 1, s->label);
            return ERR_LABEL;
        }
        if (s->features[0] < 0.0) {
            printf("[校验失败] 返回码 %d —— 第 %d 条样本 file_size 为负\n",
                   ERR_RANGE, i + 1);
            return ERR_RANGE;
        }
        if (s->features[1] < 0.0 || !is_integer_value(s->features[1])) {
            printf("[校验失败] 返回码 %d —— 第 %d 条样本 string_count 不是非负整数\n",
                   ERR_RANGE, i + 1);
            return ERR_RANGE;
        }
        if (s->features[2] < 0.0 || s->features[2] > 8.0) {
            printf("[校验失败] 返回码 %d —— 第 %d 条样本 entropy = %.6g 越界\n",
                   ERR_RANGE, i + 1, s->features[2]);
            return ERR_RANGE;
        }
        for (j = 3; j <= 4; ++j) {
            if (s->features[j] < 0.0 || s->features[j] > 1.0) {
                printf("[校验失败] 返回码 %d —— 第 %d 条样本 %s = %.6g 越界\n",
                       ERR_RANGE, i + 1, FEATURE_NAME[j], s->features[j]);
                return ERR_RANGE;
            }
        }
    }

    printf("[校验通过] %d 条样本全部合法（含空集、标签、区间、整数性检查）\n", data->count);
    return ERR_OK;
}

/* 打印数据摘要：条数、每维最小/最大值、标签分布 */
static void print_summary(const Dataset *data)
{
    int i, j;
    double mn[FEATURE_COUNT], mx[FEATURE_COUNT];
    int cnt0 = 0, cnt1 = 0;

    if (data->count <= 0) {
        printf("（当前没有已加载的数据）\n");
        return;
    }
    for (j = 0; j < FEATURE_COUNT; ++j) {
        mn[j] = data->samples[0].features[j];
        mx[j] = data->samples[0].features[j];
    }
    for (i = 0; i < data->count; ++i) {
        for (j = 0; j < FEATURE_COUNT; ++j) {
            double v = data->samples[i].features[j];
            if (v < mn[j]) mn[j] = v;
            if (v > mx[j]) mx[j] = v;
        }
        if (data->has_label) {
            if (data->samples[i].label == 0) ++cnt0; else ++cnt1;
        }
    }

    printf("---- 数据摘要 ----\n");
    printf("样本条数：%d\n", data->count);
    printf("%-16s %14s %14s\n", "特征", "最小值", "最大值");
    for (j = 0; j < FEATURE_COUNT; ++j) {
        printf("%-16s %14.6g %14.6g\n", FEATURE_NAME[j], mn[j], mx[j]);
    }
    if (data->has_label) {
        printf("标签分布：良性(0) = %d 条，可疑(1) = %d 条\n", cnt0, cnt1);
    } else {
        printf("标签：无（这是待预测数据）\n");
    }
}

/* ====================================================================== */
/*  第三部分：归一化、预测与模型存取（任务三）                             */
/* ====================================================================== */

/* 只用训练数据拟合 min/max。注意：预测时**不允许**重新调用它。 */
static void fit_scaler(const Dataset *data, FeatureScaler *scaler)
{
    int i, j;

    for (j = 0; j < FEATURE_COUNT; ++j) {
        scaler->minv[j] = data->samples[0].features[j];
        scaler->maxv[j] = data->samples[0].features[j];
    }
    for (i = 1; i < data->count; ++i) {
        for (j = 0; j < FEATURE_COUNT; ++j) {
            double v = data->samples[i].features[j];
            if (v < scaler->minv[j]) scaler->minv[j] = v;
            if (v > scaler->maxv[j]) scaler->maxv[j] = v;
        }
    }
}

/* 单条样本的归一化：x = (raw - min) / (max - min)；若 min == max 约定为 0 */
static void scale_features(const FeatureScaler *scaler,
                           const double raw[FEATURE_COUNT],
                           double scaled[FEATURE_COUNT])
{
    int j;
    for (j = 0; j < FEATURE_COUNT; ++j) {
        double span = scaler->maxv[j] - scaler->minv[j];
        scaled[j] = (span == 0.0) ? 0.0 : (raw[j] - scaler->minv[j]) / span;
    }
}

/* 教学预测概率：p = 1 / (1 + exp(-(bias + Σ w[j]·x[j]))) */
static double predict_probability(const Model *model,
                                  const FeatureScaler *scaler,
                                  const double raw_features[FEATURE_COUNT])
{
    double scaled[FEATURE_COUNT];
    double z;
    int j;

    scale_features(scaler, raw_features, scaled);

    z = model->bias;
    for (j = 0; j < FEATURE_COUNT; ++j) {
        z += model->weights[j] * scaled[j];
    }
    /* 防止极端值让 exp() 溢出（数值技巧，不改变正常范围内的结果） */
    if (z > 700.0)  z = 700.0;
    if (z < -700.0) z = -700.0;

    return 1.0 / (1.0 + exp(-z));
}

/* 按阈值 0.5 把概率转成教学类别：>=0.5 判为 1（可疑），否则 0（良性） */
static int classify(double probability)
{
    return (probability >= 0.5) ? 1 : 0;
}

/* 保存模型：必须同时保存权重、偏置**和完整的归一化参数** */
static int save_model(const char *path, const Model *model, const FeatureScaler *scaler)
{
    FILE *fp = fopen(path, "w");
    int j;

    if (fp == NULL) {
        printf("[错误] 返回码 %d —— 无法写入模型文件 \"%s\"\n", ERR_OPEN, path);
        return ERR_OPEN;
    }
    fprintf(fp, "TEACHMODEL_V1\n");
    fprintf(fp, "%d\n", FEATURE_COUNT);
    for (j = 0; j < FEATURE_COUNT; ++j) {
        fprintf(fp, "%.17g%s", model->weights[j], (j == FEATURE_COUNT - 1) ? "\n" : " ");
    }
    fprintf(fp, "%.17g\n", model->bias);
    for (j = 0; j < FEATURE_COUNT; ++j) {
        fprintf(fp, "%.17g%s", scaler->minv[j], (j == FEATURE_COUNT - 1) ? "\n" : " ");
    }
    for (j = 0; j < FEATURE_COUNT; ++j) {
        fprintf(fp, "%.17g%s", scaler->maxv[j], (j == FEATURE_COUNT - 1) ? "\n" : " ");
    }
    fclose(fp);
    printf("[保存成功] 模型（权重 + 偏置 + 归一化 min/max）已写入 \"%s\"\n", base_name(path));
    return ERR_OK;
}

/* 从一行里严格读入 n 个"空格分隔"的数值；成功返回 1，失败返回 0。
 * 要求：正好 n 个数值，不能多也不能少，每个数值都要能通过 parse_number。 */
static int parse_number_list(char *line, int n, double *out)
{
    char *p = trim(line);
    int i;

    for (i = 0; i < n; ++i) {
        char token[64];
        size_t len = 0;

        while (*p != '\0' && isspace((unsigned char)*p)) ++p;   /* 跳过空格 */
        if (*p == '\0') return 0;                               /* 数值个数不够 */

        while (p[len] != '\0' && !isspace((unsigned char)p[len]) && len < sizeof token - 1) {
            token[len] = p[len];
            ++len;
        }
        token[len] = '\0';
        if (!parse_number(token, &out[i])) return 0;             /* 某个数值不合法 */
        p += len;
    }

    while (*p != '\0' && isspace((unsigned char)*p)) ++p;
    return (*p == '\0');                                        /* 后面不能再有多余内容 */
}

/* 读取模型文件：权重的个数、偏置、min、max 必须齐全，否则拒绝 */
static int load_model(const char *path, Model *model, FeatureScaler *scaler)
{
    FILE *fp;
    char lines[MODEL_MAXLINE][LINE_MAX];
    int  count = 0;
    int  j;

    model->trained = 0;

    fp = fopen(path, "r");
    if (fp == NULL) {
        printf("[错误] 返回码 %d —— 模型文件 \"%s\" 不存在或无法打开\n",
               ERR_OPEN, base_name(path));
        return ERR_OPEN;
    }
    while (count < MODEL_MAXLINE && fgets(lines[count], LINE_MAX, fp) != NULL) {
        lines[count][strcspn(lines[count], "\r\n")] = '\0';
        if (is_blank(lines[count])) continue;      /* 跳过空行 */
        ++count;
    }
    fclose(fp);

    /* 合法模型共 6 行内容：magic / 维数 / 权重 / 偏置 / min / max */
    if (count != 6) {
        printf("[模型加载失败] 返回码 %d —— 文件应有 6 行内容（标识/维数/权重/偏置/min/max），实际 %d 行\n",
               ERR_MODELFORMAT, count);
        return ERR_MODELFORMAT;
    }
    if (strcmp(trim(lines[0]), "TEACHMODEL_V1") != 0) {
        printf("[模型加载失败] 返回码 %d —— 首行标识不是 TEACHMODEL_V1（实际 \"%s\"）\n",
               ERR_MODELFORMAT, trim(lines[0]));
        return ERR_MODELFORMAT;
    }
    {
        double dims = 0.0;
        if (!parse_number(trim(lines[1]), &dims) || dims != (double)FEATURE_COUNT) {
            printf("[模型加载失败] 返回码 %d —— 第 2 行特征维数应为 %d（实际 \"%s\"）\n",
                   ERR_MODELFORMAT, FEATURE_COUNT, trim(lines[1]));
            return ERR_MODELFORMAT;
        }
    }

    if (!parse_number_list(lines[2], FEATURE_COUNT, model->weights)) {
        printf("[模型加载失败] 返回码 %d —— 第 3 行应有 %d 个权重值（实际 \"%s\"）\n",
               ERR_MODELFORMAT, FEATURE_COUNT, trim(lines[2]));
        return ERR_MODELFORMAT;
    }
    if (!parse_number_list(lines[3], 1, &model->bias)) {
        printf("[模型加载失败] 返回码 %d —— 第 4 行应有一个偏置值（实际 \"%s\"）\n",
               ERR_MODELFORMAT, trim(lines[3]));
        return ERR_MODELFORMAT;
    }
    if (!parse_number_list(lines[4], FEATURE_COUNT, scaler->minv)) {
        printf("[模型加载失败] 返回码 %d —— 第 5 行应有 %d 个归一化 min 值（实际 \"%s\"）\n",
               ERR_MODELFORMAT, FEATURE_COUNT, trim(lines[4]));
        return ERR_MODELFORMAT;
    }
    if (!parse_number_list(lines[5], FEATURE_COUNT, scaler->maxv)) {
        printf("[模型加载失败] 返回码 %d —— 第 6 行应有 %d 个归一化 max 值（实际 \"%s\"）\n",
               ERR_MODELFORMAT, FEATURE_COUNT, trim(lines[5]));
        return ERR_MODELFORMAT;
    }
    for (j = 0; j < FEATURE_COUNT; ++j) {
        if (scaler->maxv[j] < scaler->minv[j]) {
            printf("[模型加载失败] 返回码 %d —— 第 %d 维 max(%.6g) < min(%.6g)，参数不合法\n",
                   ERR_MODELFORMAT, j + 1, scaler->maxv[j], scaler->minv[j]);
            return ERR_MODELFORMAT;
        }
    }

    model->trained = 1;
    printf("[模型加载成功] %s：5 个权重 + 偏置 + 5 组 min/max 全部读入\n", base_name(path));
    return ERR_OK;
}

/* 预测并打印全部样本；返回预测为「可疑」的条数 */
static int predict_all(const Dataset *data, const Model *model, const FeatureScaler *scaler)
{
    int i;
    int suspicious = 0, benign = 0;
    int correct = 0, labeled = 0;

    printf("\n---- 预测结果（阈值 0.5：p >= 0.5 判为可疑）----\n");
    printf("%4s  %8s %6s %6s %8s %8s   %-12s  %s\n",
           "序号", "size", "str", "H", "00", "FF", "概率 p", "类别");
    for (i = 0; i < data->count; ++i) {
        const Sample *s = &data->samples[i];
        double p = predict_probability(model, scaler, s->features);
        int    pred = classify(p);
        if (pred == 1) ++suspicious; else ++benign;
        printf("%4d  %8.0f %6.0f %6.2f %8.4f %8.4f   %-12.10f  %s(1=可疑/0=良性)\n",
               i + 1, s->features[0], s->features[1], s->features[2],
               s->features[3], s->features[4], p, pred == 1 ? "可疑" : "良性");
        if (data->has_label) {
            ++labeled;
            if (pred == s->label) ++correct;
        }
    }
    printf("共 %d 条：可疑 %d 条，良性 %d 条\n", data->count, suspicious, benign);
    if (labeled > 0) {
        printf("与真实标签对照：正确 %d / %d，准确率 = %.2f%%\n",
               correct, labeled, 100.0 * correct / (double)labeled);
    }
    return suspicious;
}

/* 一致性自检：把当前模型保存到新文件 → 重新加载 → 比较同一输入的概率 */
static int consistency_check(const Dataset *data, const Model *model, const FeatureScaler *scaler)
{
    Model         m2 = { {0}, 0.0, 0 };
    FeatureScaler s2 = { {0}, {0} };
    const char *tmp = "model_reloaded.txt";
    double p1, p2, diff;
    int rc;

    if (data->count <= 0 || !model->trained) {
        printf("[一致性自检] 返回码 %d —— 需要先加载数据与模型\n", ERR_NOMODEL);
        return ERR_NOMODEL;
    }

    p1 = predict_probability(model, scaler, data->samples[0].features);
    rc = save_model(tmp, model, scaler);
    if (rc != ERR_OK) return rc;
    rc = load_model(tmp, &m2, &s2);
    if (rc != ERR_OK) return rc;
    p2 = predict_probability(&m2, &s2, data->samples[0].features);

    diff = fabs(p1 - p2);
    printf("\n---- 模型保存/加载一致性自检（第 1 条样本）----\n");
    printf("保存前预测概率 p1 = %.12f\n", p1);
    printf("重新加载后   p2 = %.12f\n", p2);
    printf("|p1 - p2| = %.3e  （通过阈值 1e-12）→ %s\n",
           diff, (diff < 1e-12) ? "一致，通过" : "不一致，失败");
    return ERR_OK;
}

/* ====================================================================== */
/*  第四部分：菜单与命令行入口                                             */
/* ====================================================================== */

static void show_notice(void)
{
    printf("\n");
    printf("========================================================\n");
    printf(" 安全声明：本程序只处理 CSV 中的合成静态特征数值，\n");
    printf(" 不下载、不运行、不传播任何真实恶意代码；\n");
    printf(" 输出的「良性 / 可疑」仅为教学模型的类别预测，\n");
    printf(" 不构成真实软件的安全判定。\n");
    printf("========================================================\n");
}

/* 读一行用户输入到 buf，返回 0 表示读不到（EOF） */
static int read_line(char *buf, int size)
{
    if (fgets(buf, size, stdin) == NULL) return 0;
    buf[strcspn(buf, "\r\n")] = '\0';
    return 1;
}

static void menu_loop(void)
{
    Dataset       data  = { {0}, 0, 0 };
    FeatureScaler scaler = { {0}, {0} };
    Model         model = { {0}, 0.0, 0 };
    char input[128];
    char path[512];
    int  running = 1;

    show_notice();

    while (running) {
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

        if (!read_line(input, sizeof input)) break;
        if (is_blank(input)) continue;

        switch (atoi(input)) {
        case 1:
            printf("请输入带标签 CSV 路径（例如 ..\\data\\train.csv）：");
            if (!read_line(path, sizeof path)) return;
            if (load_dataset(trim(path), &data) == ERR_OK) print_summary(&data);
            break;
        case 2:
            printf("请输入待预测 CSV 路径（例如 ..\\data\\predict.csv）：");
            if (!read_line(path, sizeof path)) return;
            if (load_predict(trim(path), &data) == ERR_OK) print_summary(&data);
            break;
        case 3:
            (void)validate_dataset(&data);
            break;
        case 4:
            printf("请输入模型文件路径（例如 ..\\data\\reference_model.txt）：");
            if (!read_line(path, sizeof path)) return;
            (void)load_model(trim(path), &model, &scaler);
            break;
        case 5:
            printf("请输入要保存到的模型文件路径（例如 ..\\report\\model_saved.txt）：");
            if (!read_line(path, sizeof path)) return;
            if (!model.trained) {
                printf("[错误] 返回码 %d —— 还没有可保存的模型\n", ERR_NOMODEL);
            } else {
                (void)save_model(trim(path), &model, &scaler);
            }
            break;
        case 6:
            if (data.count <= 0) {
                printf("[错误] 返回码 %d —— 请先读取数据（菜单 1 或 2）\n", ERR_EMPTY);
            } else if (!model.trained) {
                printf("[错误] 返回码 %d —— 请先加载模型（菜单 4）\n", ERR_NOMODEL);
            } else {
                (void)predict_all(&data, &model, &scaler);
            }
            break;
        case 7:
            (void)consistency_check(&data, &model, &scaler);
            break;
        case 8:
            show_notice();
            break;
        case 0:
            running = 0;
            break;
        default:
            printf("无效选择，请输入 0-8 之间的数字。\n");
            break;
        }
    }
    printf("\n程序结束。\n");
}

/* 非交互模式：recognizer.exe check <csv>  —— 便于批量跑测试用例 */
static int cli_check(const char *path)
{
    Dataset data = { {0}, 0, 0 };
    int rc = load_dataset(path, &data);

    if (rc == ERR_OK) {
        rc = validate_dataset(&data);
    }
    if (rc == ERR_OK) {
        print_summary(&data);
        printf("[结论] 该文件合法，已接受（返回码 0）\n");
    } else {
        printf("[结论] 该文件被拒绝（返回码 %d）\n", rc);
    }
    return rc;
}

/* 非交互模式：recognizer.exe predict <csv> <model> */
static int cli_predict(const char *csv, const char *model_path)
{
    Dataset       data = { {0}, 0, 0 };
    FeatureScaler scaler = { {0}, {0} };
    Model         model = { {0}, 0.0, 0 };
    int rc;

    rc = load_model(model_path, &model, &scaler);
    if (rc != ERR_OK) return rc;
    rc = load_auto(csv, &data);
    if (rc != ERR_OK) return rc;
    (void)predict_all(&data, &model, &scaler);
    return ERR_OK;
}

/* 非交互模式：recognizer.exe model <model.txt> —— 只检查模型文件是否可用 */
static int cli_model(const char *model_path)
{
    FeatureScaler scaler = { {0}, {0} };
    Model         model = { {0}, 0.0, 0 };
    return load_model(model_path, &model, &scaler);
}

/* 非交互模式：recognizer.exe consistency <csv> <model.txt>
 * 用于验收表「三、模型加载与预测一致性」：打印 p1、p2 与差值。 */
static int cli_consistency(const char *csv, const char *model_path)
{
    Dataset       data = { {0}, 0, 0 };
    FeatureScaler scaler = { {0}, {0} };
    Model         model = { {0}, 0.0, 0 };
    int rc;

    rc = load_model(model_path, &model, &scaler);
    if (rc != ERR_OK) return rc;
    rc = load_dataset(csv, &data);
    if (rc != ERR_OK) return rc;
    return consistency_check(&data, &model, &scaler);
}

int main(int argc, char *argv[])
{
    printf("小型静态特征识别器（教学用）· TEACHMODEL_V1\n");

    if (argc >= 3 && strcmp(argv[1], "check") == 0) {
        return cli_check(argv[2]);
    }
    if (argc >= 3 && strcmp(argv[1], "model") == 0) {
        return cli_model(argv[2]);
    }
    if (argc >= 4 && strcmp(argv[1], "predict") == 0) {
        return cli_predict(argv[2], argv[3]);
    }
    if (argc >= 4 && strcmp(argv[1], "consistency") == 0) {
        return cli_consistency(argv[2], argv[3]);
    }

    menu_loop();
    return ERR_OK;
}
