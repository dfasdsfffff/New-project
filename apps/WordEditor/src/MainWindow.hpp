//! @file MainWindow.hpp 应用程序主窗口，协调编辑器各组件交互

#pragma once

#include <QMainWindow>
#include <QTextEdit>
#include <QLabel>
#include <QMenuBar>
#include <QStatusBar>

#include "DocumentController.hpp"
#include "QtDocumentMapper.hpp"
#include "ImageStore.hpp"
#include "FormatToolbar.hpp"

/**
 * @brief 应用程序主窗口
 *
 * 负责组织文档编辑器的整体布局，包括富文本编辑器、格式工具栏、菜单栏和状态栏。
 * 作为 MVC 架构中的视图层，通过 DocumentController 访问底层文档模型，
 * 通过 QtDocumentMapper 在 QTextDocument 与 cppwordkit::DocumentModel 之间双向转换。
 */
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

private slots:
    // 文件菜单槽函数：处理新建、打开、保存、另存为、导出 PDF
    void onNewDocument();
    void onOpenDocument();
    void onSaveDocument();
    void onSaveDocumentAs();
    void onExportPdf();
    // 插入菜单槽函数：处理插入图片和表格
    void onInsertImage();
    void onInsertTable();
    /// 响应编辑器内容修改状态变化
    void onEditorModified();
    /// 同步控制器状态到界面
    void updateStatusFromController();

private:
    void setupMenus();      // 构建菜单栏，包括文件、编辑、插入菜单
    void setupToolbars();   // 将格式工具栏添加到主窗口
    void setupStatusBar();  // 初始化状态栏：文件路径、修改状态、消息提示
    void setupEditor();     // 将 QTextEdit 设为中心控件并配置默认字体
    void connectSignals();  // 连接控制器信号到界面更新槽函数
    void updateTitle();     // 根据文件名和修改状态刷新窗口标题

    void doImport();  // 从控制器模型导入到 QTextDocument
    void doExport();  // 从 QTextDocument 导出回控制器模型

    QTextEdit* editor_;                 ///< 富文本编辑器核心控件
    FormatToolbar* formatToolbar_;       ///< 格式工具栏（字体、字号、样式、对齐）
    DocumentController* controller_;     ///< 文档控制器，管理底层 WordDocument 生命周期
    ImageStore* imageStore_;             ///< 图片资源管理器，暂存文档中的图片数据
    QtDocumentMapper* mapper_;           ///< 模型映射器，桥接 QTextDocument 和 cppwordkit 模型

    QLabel* filePathLabel_;     ///< 状态栏：当前文件路径
    QLabel* modifiedLabel_;     ///< 状态栏：文档修改状态指示
    QLabel* messageLabel_;      ///< 状态栏：操作提示消息

    QMenu* fileMenu_;
    QMenu* editMenu_;
    QMenu* insertMenu_;

    QAction* newAction_;
    QAction* openAction_;
    QAction* saveAction_;
    QAction* saveAsAction_;
    QAction* exportPdfAction_;
    QAction* exitAction_;

    QAction* undoAction_;
    QAction* redoAction_;
    QAction* cutAction_;
    QAction* copyAction_;
    QAction* pasteAction_;
    QAction* selectAllAction_;

    QAction* insertImageAction_;
    QAction* insertTableAction_;
};
