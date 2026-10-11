# 异常与边界测试目录

说明：`T01_does_not_exist.csv`**故意不提供**，使用这个路径调用数据读取模块应返回“文件不存在”，且不能崩溃。

| 编号 | 文件 | 期望 |
| --- | --- | --- |
| T01 | 不存在的路径`T01_does_not_exist.csv` | 拒绝并提示打开失败 |
| T02 | `T02_empty.csv` | 拒绝空文件 |
| T03 | `T03_invalid_label.csv` | 拒绝标签2 |
| T04 | `T04_byte00_over1.csv` | 拒绝比例1.30 |
| T05 | `T05_byteff_negative.csv` | 拒绝比例-0.20 |
| T06 | `T06_nonnumeric.csv` | 拒绝非数值abc |
| T07 | `T07_missing_column.csv` | 拒绝字段缺失 |
| T08 | `T08_negative_size.csv` | 拒绝负文件大小 |
| T09 | `T09_exceeds_capacity.csv` | 超过MAX_SAMPLES=512，拒绝多余数据 |
| T10 | `T10_bad_model.txt`（模型加载） | 拒绝不完整的模型参数 |
| T11 | `T11_nonfinite.csv` | 拒绝nan |
| T12 | `T12_entropy_over8.csv` | 拒绝熵值8.50 |
| T13 | `T13_extra_column.csv` | 拒绝多余字段 |
| T14 | `T14_fractional_string_count.csv` | 拒绝非整数计数 |
| T15 | `T15_missing_field.csv` | 拒绝空字段 |
| T16 | `T16_invalid_header.csv` | 拒绝字段名称/次序与规定不一致 |
| T17 | `T17_valid_edge.csv` | **应当接受**合法边界值（含0、1、熵8） |
| T18 | `T18_non_integer_label.csv` | 拒绝标签0.5 |
| T19 | `T19_blank_line.csv` | 拒绝空数据行 |
| T20 | `T20_extra_whitespace.csv` | **应当接受**数字字段前的空格 |

评分重点是观察预期/实际输出、程序退出状态、是否有清晰错误提示，而不是“遇到错误就崩溃”。
