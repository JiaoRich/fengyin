# 0.16.11 — 高音萨克斯三风格

## 范围

仅重做高音萨克斯自然原声、丝滑抒情、明亮舞台的 SWAM 音色参数；原有风吟效果链数值不变，其他乐器和空音不调整。以用户导出的 SWAM Soprano Sax 3.8.2 参数名为依据。

## 参数（插件显示值，不是 0–1 归一化值）

|参数|自然原声|丝滑抒情|明亮舞台|
|---|---:|---:|---:|
|Key Noise|18|8|12|
|Harmonic Structure|0.00|-0.08|0.10|
|Sub. Harm|12|18|8|
|Formant|0.0|-0.8|0.7|
|Modal Res. Gain|50|62|40|
|Breath Noise|48|62|32|
|Timbral Correction|OFF|ON|ON|
|Harmonic A Gain|0|1.2|1.0|
|Harm. A|2|2|3|
|Harmonic B Gain|0|-0.8|0.5|
|Harm. B|3|3|5|
|Compressor|5|5|5|
|EQ Enabled|OFF|ON|ON|
|EQ Low Gain|0|0.5|-0.5|
|EQ Mid Gain|0|-1.5|1.0|
|EQ Mid Freq|1800|2500|1800|
|EQ High Gain|0|-1.0|0.5|
|Random Dynamic|5|8|3|
|Dynamic Pitch|20|25|15|
|Dynamic Harmonic|0.45|0.30|0.70|
|Release Time|10|10|10|
|Breathy ppp|OFF|OFF|OFF|

## 防止“界面变了、参数没变”

- 精确白名单匹配；不向 Expression、Growl、Vibrato、MIDI CC、乐器型号、弯音范围、移调等控制写值。
- 调用插件自己的文本转换，核对返回值的显示文本；不支持时在插件显示域查找，不猜线性比例。
- 写入前检查全部 22 项；有缺项、歧义或目标不可表达时不写入半套参数。
- 写入后读回全部参数，失败时恢复写入前数值；界面提示当前风格未完整应用。
- 导出文件增加 `styleParameterAudit`：expected、verifiedAtApply、rolledBack、逐项目标和读回结果。
- 读回验证的是宿主/插件控制器参数，不等于对音频处理器输出或听感的自动证明。用户导出的当前全量参数及内嵌状态仍保留，可复核处理状态。
- 这批数值是用户批准直接进入正式版的候选值，未宣称已经完成真人听感验收。

## 验收清单

自动化：显示域转换（含非线性、反向、无文本解析）、非法范围、ON/OFF、反复切换、自然原声关闭 EQ/泛音塑形、缺项/重复项、拒绝写入回滚、保留 MIDI/技巧/型号参数；现有测试回归。

Windows 实机：

1. 版本显示 0.16.11；加载高音萨克斯三个默认风格，均能吹奏。
2. 三份导出文件的 expected 和 verifiedAtApply 均为 22，rolledBack 为 false；全量参数与目标表一致。
3. 丝滑→舞台→自然反复切换，自然原声 EQ 和 Timbral Correction 均 OFF，无上一风格残留。
4. 型号切换可用；气息、技巧映射、演奏调、弯音范围不受风格切换影响。
5. 原有空音、自定义音色、伴奏、录音正常。
6. 正式版中试听三风格；读回通过不替代对刺耳、削波、延迟和音色差异的听感验收。

本机无用户 Windows SWAM 实例，实机项不能标记为本机已通过。
