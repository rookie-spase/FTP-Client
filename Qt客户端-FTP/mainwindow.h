#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

enum Status_Type {
    UpLoad,             // “上传中”
    DownLoad,           // “下载中”
    Delete,             // “删除中”
    Default,            // 显示ip和端口
    Connect_Fail        // 连接服务器失败
};

class QTcpSocket;
class QStandardItemModel;
class QFileSystemModel;
#include "tree_model.h"
#include "view_for_ftp.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void keyPressEvent(QKeyEvent *event);

signals:
    void Send_Status(Status_Type what_Situation);
private:
    void setupConnections(); // 设置信号和槽的连接({界面ui的信号,内部的status信号}连接)
    void refreshFileList(); // 刷新文件列表
    void refreshDirectoryTree(); // 刷新目录树
private slots:
    void connectToFtp(); // 连接到FTP服务器
    void updateStatus(Status_Type newStatus); // 更新状态显示
    void uploadFile(); // 上传文件
    void downloadFile(); // 下载文件
    void deleteFile(); // 删除文件
    void showTableContextMenu(); // 将tableView中的选项和槽函数连接。(table只是提供action选项)
private:
    QTcpSocket *client; // FTP客户端实例
    QStandardItemModel *tableModel; // 表格模型实例
    TreeModel *treeModel; // 树模型实例
    View_For_FTP* tableView;  // 表格，自带菜单
    QString currentPath; // 当前路径
private:
    Ui::MainWindow *ui;
};
#endif // MAINWINDOW_H
