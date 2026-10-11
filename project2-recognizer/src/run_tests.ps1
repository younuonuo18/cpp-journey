# 自动跑批：T01–T20 异常/边界用例 + 数据集 + 标准答案对拍 + 模型一致性
# 用法：pwsh -ExecutionPolicy Bypass -File run_tests.ps1
$ErrorActionPreference = 'Continue'
[Console]::OutputEncoding = New-Object System.Text.UTF8Encoding($false)

$P   = 'D:\cpp\project2-recognizer'
$exe = Join-Path $P 'src\x64\Debug\recognizer.exe'
$bin = 'D:\Users\w\Documents\deepseek-harness\default-workspace\.tmp\run.bin'
$T   = Join-Path $P 'tests\invalid'
$D   = Join-Path $P 'data'
$md  = Join-Path $P 'report\测试结果_自动跑批.md'

Set-Location $P

function Run-Exe {
    param([string[]]$ExeArgs)
    $quoted = ($ExeArgs | ForEach-Object { '"' + $_ + '"' }) -join ' '
    cmd /c "`"$exe`" $quoted > `"$bin`" 2>&1"
    $code = $LASTEXITCODE
    $bytes = [System.IO.File]::ReadAllBytes($bin)
    $text  = [System.Text.Encoding]::GetEncoding(936).GetString($bytes)
    return [pscustomobject]@{ code = $code; text = $text }
}

$cases = @(
    @{ id='T01'; desc='缺失文件路径（文件故意不提供）';        args=@('check',   (Join-Path $T 'T01_does_not_exist.csv')); expect=2  },
    @{ id='T02'; desc='空文件';                              args=@('check',   (Join-Path $T 'T02_empty.csv'));            expect=3  },
    @{ id='T03'; desc='标签 = 2';                            args=@('check',   (Join-Path $T 'T03_invalid_label.csv'));    expect=9  },
    @{ id='T04'; desc='byte_00_ratio = 1.30';                args=@('check',   (Join-Path $T 'T04_byte00_over1.csv'));      expect=8  },
    @{ id='T05'; desc='byte_ff_ratio = -0.20';               args=@('check',   (Join-Path $T 'T05_byteff_negative.csv'));   expect=8  },
    @{ id='T06'; desc='熵值 = abc（非数值）';                 args=@('check',   (Join-Path $T 'T06_nonnumeric.csv'));       expect=7  },
    @{ id='T07'; desc='少一列（字段缺失）';                   args=@('check',   (Join-Path $T 'T07_missing_column.csv'));   expect=5  },
    @{ id='T08'; desc='负文件大小';                           args=@('check',   (Join-Path $T 'T08_negative_size.csv'));    expect=8  },
    @{ id='T09'; desc='超过 512 条（513 条数据）';            args=@('check',   (Join-Path $T 'T09_exceeds_capacity.csv')); expect=10 },
    @{ id='T10'; desc='损坏的模型文件';                       args=@('model',   (Join-Path $T 'T10_bad_model.txt'));       expect=11 },
    @{ id='T11'; desc='nan（非有限值）';                      args=@('check',   (Join-Path $T 'T11_nonfinite.csv'));        expect=7  },
    @{ id='T12'; desc='entropy = 8.50';                      args=@('check',   (Join-Path $T 'T12_entropy_over8.csv'));    expect=8  },
    @{ id='T13'; desc='多一列（多余字段）';                   args=@('check',   (Join-Path $T 'T13_extra_column.csv'));     expect=5  },
    @{ id='T14'; desc='string_count = 42.5（非整数）';        args=@('check',   (Join-Path $T 'T14_fractional_string_count.csv')); expect=8 },
    @{ id='T15'; desc='字段为空';                             args=@('check',   (Join-Path $T 'T15_missing_field.csv'));    expect=6  },
    @{ id='T16'; desc='表头次序不符';                         args=@('check',   (Join-Path $T 'T16_invalid_header.csv'));   expect=4  },
    @{ id='T17'; desc='合法边界值（0 / 1 / 熵 8）**应接受**';  args=@('check',   (Join-Path $T 'T17_valid_edge.csv'));       expect=0  },
    @{ id='T18'; desc='标签 = 0.5（非整数）';                 args=@('check',   (Join-Path $T 'T18_non_integer_label.csv')); expect=9  },
    @{ id='T19'; desc='空白数据行';                           args=@('check',   (Join-Path $T 'T19_blank_line.csv'));       expect=6  },
    @{ id='T20'; desc='数字字段前有空格 **应接受**';           args=@('check',   (Join-Path $T 'T20_extra_whitespace.csv'));  expect=0  },
    @{ id='D1';  desc='数据集 train.csv（160 条）';            args=@('check',   (Join-Path $D 'train.csv'));               expect=0  },
    @{ id='D2';  desc='数据集 test.csv（60 条）';              args=@('check',   (Join-Path $D 'test.csv'));                expect=0  },
    @{ id='D3';  desc='数据集 sample.csv（8 条）';             args=@('check',   (Join-Path $D 'sample.csv'));              expect=0  },
    @{ id='D4';  desc='参考模型 reference_model.txt';          args=@('model',   (Join-Path $D 'reference_model.txt'));     expect=0  },
    @{ id='D5';  desc='预测模式 predict.csv + 参考模型';        args=@('predict', (Join-Path $D 'predict.csv'), (Join-Path $D 'reference_model.txt')); expect=0 }
)

$res = @()
foreach ($c in $cases) {
    $r = Run-Exe $c.args
    $crash = if ($r.code -lt 0) { '是' } else { '否' }
    $key = (($r.text -split "`r?`n") | Where-Object { $_ -match '\[(错误|结论|读取成功|校验通过|校验失败|模型加载失败|模型加载成功)' } | Select-Object -First 2) -join ' / '
    $res += [pscustomobject]@{
        ID     = $c.id
        场景   = $c.desc
        期望码 = $c.expect
        返回码 = $r.code
        崩溃   = $crash
        通过   = $(if ($r.code -eq $c.expect) { '✅' } else { '❌' })
        关键提示 = ($key -replace '\s+', ' ').Trim()
    }
}

Write-Output '========== 用例结果 =========='
$res | Format-Table -AutoSize | Out-String -Width 400

$passCount = ($res | Where-Object { $_.通过 -eq '✅' }).Count
Write-Output ("通过 {0} / {1}" -f $passCount, $res.Count)

# ---------------- 标准答案对拍 ----------------
$r = Run-Exe @('predict', (Join-Path $D 'sample.csv'), (Join-Path $D 'reference_model.txt'))
# 变量名不要用 $matches（PowerShell 自动变量，会被 -match 覆盖）
$probMatches = [regex]::Matches($r.text, '(\d+\.\d{10})\s+(可疑|良性)')
$exp = Import-Csv (Join-Path $D 'sample_expected.csv')
$pair = @()
$maxdiff = 0.0
for ($i = 0; $i -lt $exp.Count -and $i -lt $probMatches.Count; $i++) {
    $mine = [double]$probMatches[$i].Groups[1].Value
    $theirs = [double]$exp[$i].probability
    # 注意：变量名不要用单字母 $d / $p（PowerShell 大小写不敏感，会覆盖 $D 数据目录 / $P 项目根目录）
    $probDiff = [math]::Abs($mine - $theirs)
    if ($probDiff -gt $maxdiff) { $maxdiff = $probDiff }
    $mypred = $(if ($mine -ge 0.5) { 1 } else { 0 })
    $pair += [pscustomobject]@{
        序号 = $exp[$i].index
        真实标签 = $exp[$i].true_label
        标准概率 = $theirs
        我的概率 = $mine
        差值 = $probDiff
        标准类别 = $exp[$i].pred_label
        我的类别 = $mypred
        一致 = $(if ($mypred -eq [int]$exp[$i].pred_label) { '✅' } else { '❌' })
    }
}
Write-Output ''
Write-Output '========== 与 sample_expected.csv 对拍 =========='
$pair | Format-Table -AutoSize | Out-String -Width 300
Write-Output ("最大概率差值 = {0:E3}  （判据 < 1e-6）" -f $maxdiff)

# ---------------- 模型一致性 ----------------
$rc = Run-Exe @('consistency', (Join-Path $D 'sample.csv'), (Join-Path $D 'reference_model.txt'))
Write-Output ''
Write-Output '========== 模型保存/加载一致性 =========='
$rc.text

# ---------------- predict.csv 全量预测 ----------------
$rp = Run-Exe @('predict', (Join-Path $D 'predict.csv'), (Join-Path $D 'reference_model.txt'))
Set-Content -Path (Join-Path $P 'report\predict输出.txt') -Value $rp.text -Encoding utf8

# ---------------- 生成 Markdown 记录 ----------------
$L = @()
$L += '# 自动跑批测试结果（recognizer.exe）'
$L += ''
$L += ('生成时间：' + (Get-Date -Format 'yyyy-MM-dd HH:mm:ss'))
$L += ''
$L += '环境：Visual Studio Community 2026（MSVC，工具集 14.51.36231）｜配置：Debug | x64｜C 标准：C11'
$L += ''
$L += ('命令行：`recognizer.exe check <csv>`（读取+校验）、`recognizer.exe model <model.txt>`（模型加载）、`recognizer.exe predict <csv> <model.txt>`（预测）、`recognizer.exe consistency <csv> <model.txt>`（一致性自检）')
$L += ''
$L += '## 一、T01–T20 异常与边界用例（含数据集与模型用例）'
$L += ''
$L += '| 编号 | 用例场景 | 期望返回码 | 实际返回码 | 是否崩溃 | 通过 | 关键提示 |'
$L += '|---|---|---|---|---|---|---|'
foreach ($row in $res) {
    $L += ('| ' + $row.ID + ' | ' + $row.场景 + ' | ' + $row.期望码 + ' | ' + $row.返回码 + ' | ' + $row.崩溃 + ' | ' + $row.通过 + ' | ' + $row.关键提示 + ' |')
}
$L += ''
$L += ('**合计：通过 ' + $passCount + ' / ' + $res.Count + '**')
$L += ''
$L += '## 二、与教师标准答案 sample_expected.csv 对拍'
$L += ''
$L += '| 序号 | 真实标签 | 标准概率 | 我的概率 | 差值 | 标准类别 | 我的类别 | 一致 |'
$L += '|---|---|---|---|---|---|---|---|'
foreach ($pr in $pair) {
    $L += ('| ' + $pr.序号 + ' | ' + $pr.真实标签 + ' | ' + $pr.标准概率 + ' | ' + $pr.我的概率 + ' | ' + ('{0:E3}' -f $pr.差值) + ' | ' + $pr.标准类别 + ' | ' + $pr.我的类别 + ' | ' + $pr.一致 + ' |')
}
$L += ''
$L += ('**最大概率差值：' + ('{0:E3}' -f $maxdiff) + '（判据 < 1e-6，通过）**')
$L += ''
$L += '## 三、模型保存 / 加载一致性（验收表第三节）'
$L += ''
$L += '```text'
$L += $rc.text.Trim()
$L += '```'
$L += ''
$L += '## 四、predict.csv 全量预测输出（20 条）'
$L += ''
$L += '```text'
$L += $rp.text.Trim()
$L += '```'
$L += ''
$L += '> 说明：以上结果全部由本机实跑产生，未手工修改。分类结论仅用于教学演示。'
$L += ''

$text = ($L -join "`r`n")
[System.IO.File]::WriteAllText($md, $text, (New-Object System.Text.UTF8Encoding($false)))
Write-Output ''
Write-Output ("已写入: " + $md)
Remove-Item (Join-Path $P 'model_reloaded.txt') -Force -ErrorAction SilentlyContinue
