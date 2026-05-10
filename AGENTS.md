# Project Instructions

你现在参与的是一个 C++/Qt Word 文档编辑器项目。

项目目标：
- 实现一个可编辑 .docx 文档的 C++ 库。
- 基于该库实现 Qt 桌面端 Word 编辑器。
- 当前优先支持 .docx，不考虑旧版 .doc。
- .docx 应按 OPC + WordprocessingML 结构处理，不允许把 docx 当作普通文本文件处理。

技术约束：
- 使用现代 C++20。
- Qt 用于 GUI、文本显示、交互、文件对话框、主窗口等。
- 核心 Word 文档解析与编辑逻辑应放在独立库中，不应依赖 QWidget。
- UI 层只能调用库层公开 API。
- 不要在 UI 层直接读写 docx 内部 XML。
- 修改代码前先阅读相关文件，理解已有架构。
- 每次任务尽量小步提交，不要大规模重构无关代码。

参考文档：
- docx文档结构：`doc\Ecma Office Open XML Part 1`,`doc\Ecma Office Open XML Part 2`,`doc\Ecma Office Open XML Part 3`,`doc\Ecma Office Open XML Part 4`的pdf文件
- libxml2开发文档位于`doc\libxml2-v2.12.10-devhelp`

架构分层：
1. docxcore（CppWordKit）
   - 负责 OPC 包读取/写入
   - 负责 WordprocessingML 解析
   - 负责文档模型 Document/Paragraph/Run/Table
   - 负责编辑操作 insertText/deleteRange/applyStyle
   - 负责保存回 .docx
   - 还提供了一个类似Python docxtpl的模板渲染功能
   - 整体分为两层；docx核心和docxtpl

2. editorcore
   - 负责编辑命令、撤销重做、选择区、光标模型
   - 不直接依赖 QWidget

3. qteditor
   - 负责 Qt 界面
   - 负责渲染文档、响应键盘鼠标事件
   - 通过 docxcore/editorcore 操作文档

代码风格：
- 保持现有命名风格。
- 新增类要有清晰职责。
- 代码需要中文注释。
- 重要逻辑需要单元测试。
- 不要引入大型依赖，除非先说明原因。

每次完成任务后：
- 总结改了哪些文件。
- 说明如何测试。
- 说明还有哪些限制。