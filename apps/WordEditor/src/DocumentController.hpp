//! @file DocumentController.hpp 文档控制器，管理文档生命周期和文件 I/O

#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

#include <memory>
#include <cppwordkit/CppWordKit.hpp>

/**
 * @brief 文档控制器
 *
 * 封装 cppwordkit::WordDocument 的生命周期管理，提供新建、打开、保存、另存为等操作接口。
 * 通过 Qt 信号机制将状态变化（路径、修改状态、错误等）通知视图层。
 * 作为 MVC 架构中的模型层代理，视图层不直接操作 WordDocument。
 */
class DocumentController : public QObject {
    Q_OBJECT
public:
    explicit DocumentController(QObject* parent = nullptr);

    bool newDocument();
    bool openDocument(const QString& path);
    bool saveDocument();
    bool saveDocumentAs(const QString& path);
    bool exportPdf(const QString& path);

    [[nodiscard]] bool isOpen() const;
    [[nodiscard]] bool isModified() const;
    [[nodiscard]] QString currentPath() const;
    [[nodiscard]] QString currentFileName() const;

    cppwordkit::WordDocument* wordDocument();
    const cppwordkit::WordDocument* wordDocument() const;

    void setModified(bool modified);

signals:
    // 文档修改状态变化时触发
    void modifiedChanged(bool modified);
    // 文档路径变化时触发（新建/打开/另存为）
    void pathChanged(const QString& path);
    // 状态消息提示
    void statusMessage(const QString& message);
    // 操作出错时触发
    void errorOccurred(const QString& message);

private:
    std::unique_ptr<cppwordkit::WordDocument> document_;  ///< 底层 Word 文档对象
    QString currentPath_;                                 ///< 当前文档的保存路径
    bool modified_ = false;                               ///< 文档是否有未保存的修改
};
