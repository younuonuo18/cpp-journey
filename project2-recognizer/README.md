# 小型恶意代码识别器（教学项目）

《高级语言程序设计》综合实践作业 · 大模型辅助开发 · **个人独立完成**

> ⚠️ **安全边界**：本项目只处理教师提供的 CSV **静态特征数值**（文件大小、字符串数、信息熵、0x00/0xFF 占比），
> **不下载、不运行、不传播**任何真实恶意代码，**不实现**任何绕过检测的功能。
> 程序输出的「良性 / 可疑」仅为**教学模型的类别预测**，**不构成**真实软件的安全判定。

---

## 目录结构

```
project2-recognizer/
├─ docs/                     任务书、报告模板、验收表、教师说明（原件）+ 本人写的设计与 AI 记录
│   ├─ 01-需求与设计.md        ← 任务一交付物（需求清单、数据结构、接口、流程图）
│   ├─ AI协作记录与个人贡献.md  ← 个人必交（至少 3 组记录）
│   └─ ...（教师提供的 4 份说明 + 3 份 docx）
├─ src/                      C 源代码
│   ├─ recognizer.c          ← 主程序（由本人实现，基于 starter 骨架）
│   ├─ recognizer_starter.c  教师提供的未完成骨架（原始留档，勿改）
│   ├─ bug_case_A.c          教学错误案例 A（特征下标错位）
│   ├─ bug_case_B.c          教学错误案例 B（未保存 scaler）
│   └─ Makefile              教师提供的 gcc 构建脚本
├─ data/                     教师提供的教学数据与参考模型
│   ├─ train.csv (160)  test.csv (60)  predict.csv (20)  sample.csv (8)
│   ├─ sample_expected.csv   8 条标准答案（对拍基准）
│   └─ reference_model.txt   TEACHMODEL_V1：权重 + 偏置 + min/max
├─ tests/invalid/            T02–T20 异常与边界用例（T01 故意不提供）
├─ report/                   实验报告与验收记录表（填写版）
└─ screenshots/              运行截图
```

---

## 编译与运行

### 方式一：Visual Studio 2026（MSVC，本机实测环境）

1. 打开解决方案 `src/recognizer.slnx`（或「文件 → 打开 → 项目/解决方案」）；
2. 环境：`Debug | x64`；
3. 生成：`Ctrl+Shift+B`；运行：`Ctrl+F5`（开始执行不调试）。

本机实测信息（提交时填入报告）：

| 项 | 值 |
|---|---|
| 操作系统 | Windows（本机：Windows 11） |
| 编译器 | MSVC（Visual Studio Community 2026，工具集 14.51.36231） |
| 编译配置 | Debug / x64 |

### 方式二：gcc（教师原版 Makefile）

```bash
cd src
make                       # 等价于 gcc -std=c11 -O2 -Wall -Wextra -pedantic recognizer.c -lm -o recognizer
./recognizer
```

> 注意：`recognizer_starter.c` 是**故意未完成的骨架**（5 个 TODO 直接返回"未实现"），不是可提交的成品；
> 真正提交的是 `recognizer.c`。

---

## 输入数据格式

```text
file_size,string_count,entropy,byte_00_ratio,byte_ff_ratio,label
```

- 列顺序**固定**；预测数据 `predict.csv` **没有** `label` 列；
- `file_size ≥ 0`；`string_count` 为非负整数；`entropy ∈ [0,8]`；两个比例 `∈ [0,1]`；`label ∈ {0,1}`。

---

## 提交前自检（对照任务书「九、提交前自检」）

- [ ] 所有必做模块均可编译运行，能展示主要输入、处理和输出过程
- [ ] 正常与异常测试完整（T01–T20），至少修正一项 AI 候选代码错误，并检查模型参数一致性
- [ ] 小组报告、源代码、数据、模型、演示材料齐全；个人 AI 记录与反思已提交
- [ ] 不含真实恶意程序，所有分类输出均注明仅作教学性判断
