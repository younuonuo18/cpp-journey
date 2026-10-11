#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>

#define FIELD_COUNT 6      /* 一行 6 个字段 */
#define FIELD_MAX   64     /* 每个字段最多 63 字符 + '\0' */

/* 把一行 CSV 按半角逗号切成 6 段并打印
 * 返回码：0 = 成功；5 = 字段个数不符
 * 注意：本函数会修改传入的那一行（去掉行尾换行符）。 */
int split_and_print(char line[])
{
    char fields[FIELD_COUNT][FIELD_MAX];
    char names[FIELD_COUNT][16] = {
        "file_size", "string_count", "entropy",
        "byte_00_ratio", "byte_ff_ratio", "label"
    };
    int count = 1;     /* 字段个数 = 逗号数 + 1 */
    int f = 0;         /* 当前第几个字段 */
    int k = 0;         /* 当前字段已存几个字符 */
    int i;

    for (i = 0; i < FIELD_COUNT; i++) {      /* 先清空每个字段，避免打印未初始化内容 */
        fields[i][0] = '\0';
    }

    /* ① 去掉行尾换行符 */
    for (i = 0; line[i] != '\0'; i++) {
        if (line[i] == '\n') { line[i] = '\0'; break; }
    }

    /* ② 逐字符扫描：遇到半角逗号就切到下一个字段。
     *    即使字段已经装不下（f >= FIELD_COUNT），也继续数逗号 ——
     *    这样错误信息里的"实际几个"才准确；但绝不再写入 fields。 */
    for (i = 0; line[i] != '\0'; i++) {
        if (line[i] == ',') {
            if (f < FIELD_COUNT) {                       /* 还装得下 → 给字段收尾 */
                fields[f][k] = '\0';
            }
            count++;
            f++;
            k = 0;
        }
        else {
            if (f < FIELD_COUNT && k < FIELD_MAX - 1) {  /* 装不下就跳过，不越界写 */
                fields[f][k] = line[i];
                k++;
            }
            /* 字段超过 63 字符时丢弃多余字符，保证不越界 */
        }
    }

    /* ③ 最后一个字段收尾（f 可能已等于 FIELD_COUNT，必须先判断） */
    if (f < FIELD_COUNT) {
        fields[f][k] = '\0';
    }

    /* ④ 校验字段个数 */
    if (count != FIELD_COUNT) {
        printf("字段个数不符：应有 %d 个，实际 %d 个\n", FIELD_COUNT, count);
        return 5;
    }

    for (i = 0; i < FIELD_COUNT; i++) {
        printf("[%d] %-16s = %s\n", i, names[i], fields[i]);
    }
    return 0;
}

/* main：一次跑 5 个文件（教材例 6.4 的"二维字符数组 + for 循环"写法） */
int main(void)
{
    char paths[5][128] = {          /* 128 而不是 64：给长路径留余量 */
        "..\\data\\train.csv",
        "..\\tests\\invalid\\T13_extra_column.csv",
        "..\\tests\\invalid\\T07_missing_column.csv",
        "..\\tests\\invalid\\T20_extra_whitespace.csv",
        "..\\tests\\invalid\\T13b_eight_columns.csv"
    };
    FILE* fp;
    char  line[1024];
    int   i;

    for (i = 0; i < 5; i++) {
        printf("\n===== %s =====\n", paths[i]);
        fp = fopen(paths[i], "r");
        if (fp == NULL) { printf("打开文件失败\n"); continue; }
        if (fgets(line, sizeof line, fp) == NULL) { fclose(fp); printf("文件里没有内容\n"); continue; }
        if (fgets(line, sizeof line, fp) == NULL) { fclose(fp); printf("没有数据行\n"); continue; }
        fclose(fp);
        printf("返回码 = %d\n", split_and_print(line));
    }
    return 0;
}