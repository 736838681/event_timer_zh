# Event Timer CN — Nexus FontUI 版

这个版本**不再由插件自己加载字体**，也不再调用 `Fonts_AddFromFile`、不再维护自定义中文 glyph range。

插件直接使用 Nexus 提供的 `NexusLinkData_t::FontUI`。因此你在 Nexus 中选择的 UI 字体，就是 Event Timer CN 使用的字体。

## 运行前设置

1. 把你已经验证能正常显示中文的 `Font.ttf` 放到：

   `Guild Wars 2/addons/Nexus/Fonts/Font.ttf`

2. Nexus 的 `addons/Nexus/Settings.json` 中确认：

```json
"Language": "Chinese",
"UserFont": "Font.ttf"
```

3. 完全退出 Guild Wars 2 后重新启动。

> 本项目压缩包**不包含字体文件**。使用你自己现有的 `Font.ttf`。

## 构建

项目已经包含 GitHub Actions：`.github/workflows/build.yml`。

把整个项目提交到 GitHub 后运行 Actions，产物为 `EventTimerCN.dll`。

## 这版和上一版的区别

- 删除插件自己的 `SarasaUiSC-Regular.ttf` 加载逻辑。
- 删除 `Fonts_AddFromFile`。
- 删除 `ImFontGlyphRangesBuilder` / 自定义 glyph ranges。
- 从 `DL_NEXUS_LINK` 获取 `NexusLinkData_t`。
- 所有 Event Timer CN UI 绘制前直接 `ImGui::PushFont(FontUI)`。
- 使用的仍是与 Nexus 模板兼容的 `imgui18000` 固定版本，避免此前 imgui 1.92 ABI 崩溃问题。
