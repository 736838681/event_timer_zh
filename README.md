# Event Timer CN

全新重做的 Guild Wars 2 Nexus 中文事件计时器。

## 设计目标

- 主界面使用 `qjv/event-timers` 的时间轴思路：顶部时间刻度、横向彩色事件条、红色当前时间线。
- 不使用上一版“列表式”界面。
- 中文事件数据直接来自项目内 `event_tracks_zh.json`，构建时生成 C++ 静态数据。
- 中文字体固定使用已经能被 Nexus 识别的 `SarasaUiSC-Regular.ttf`。

## 字体

请确认字体已经存在于：

```text
<Guild Wars 2>/addons/Nexus/Fonts/SarasaUiSC-Regular.ttf
```

插件不会打包或复制字体文件。

## GitHub Actions 构建

推送到 `main` 后：

1. 打开 Actions。
2. 运行/等待 `Build Windows DLL`。
3. 下载 `EventTimerCN` artifact。
4. 将 `EventTimerCN.dll` 放进 `<Guild Wars 2>/addons/`。

## 重新生成事件数据

修改 `event_tracks_zh.json` 后运行：

```bash
python tools/generate_events.py
python tools/generate_glyph_ranges.py
```
