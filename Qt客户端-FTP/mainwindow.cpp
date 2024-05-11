#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include <QTcpSocket>
#include <QStandardItemModel>
#include <QFileSystemModel>
#include <QMenu>
#include <QNetworkProxy>
#include "tree_model.h"
#include "tree_item.h"
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , client(new QTcpSocket(this))
    , tableModel(new QStandardItemModel(this))
    , treeModel(new TreeModel(this))
    , tableView(new View_For_FTP(this))
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    this->setWindowTitle("FTP客户端");
    this->resize(QSize(700,471));   // 提供默认的大小
    this->tableView->setGeometry(9,60,556,160);
    this->ui->centralwidget->layout()->addWidget(tableView);

    this->ui->progress->reset();
    this->ui->progress->setTextVisible(false);
    this->tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    this->setupConnections();

    client->setProxy(QNetworkProxy::NoProxy);   // 禁用代理连接

    currentPath = "/code_side/libevent/FTP_Dictionary";  // 默认路径

    // 默认设置
    this->ui->lineEdit_ip->setText("192.168.118.128");
    this->ui->lineEdit_port->setText("1231");

    // 视图和模型的设置
    this->tableView->setModel(tableModel);
    ui->treeView->setModel(treeModel);

}

MainWindow::~MainWindow()
{
    delete ui;
    delete tableModel;
    delete treeModel;
    delete tableView;
}


// 当用户按下enter之后，修改currentpath.
// 注意，只有当用户选中一个文件夹之后按下enter，那么才会更换table的内容
void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if(event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return){  // 当按下Enter或Return键后
        currentPath = "/code_side/libevent/FTP_Dictionary";
        bool have_children = false;

        QModelIndex currentIndex = ui->treeView->currentIndex();  // 获取当前在TreeView中选中的项
        if (currentIndex.isValid()) {  // 如果有选中项
            if(this->treeModel->hasChildren(currentIndex)){  // 如果选中的是一个文件夹，检查是否有子项
                have_children = true;
            }
            QString selectedPath;
            while(currentIndex.isValid()) { // 当前索引有效时循环
                QString currentName = ui->treeView->model()->data(currentIndex, Qt::DisplayRole).toString();  // 获取当前项的显示名
                selectedPath = "/" + currentName + selectedPath;  // 构建从当前项到根项的路径
                currentIndex = this->treeModel->parent(currentIndex);  // 将currentIndex更新为其父项
            }
            if (!have_children) {  // 如果没有子项，从路径中移除文件名
                int lastSlashIndex = selectedPath.lastIndexOf('/');
                selectedPath = selectedPath.left(lastSlashIndex);
            }
            currentPath += selectedPath;  // 更新当前路径

            if(have_children) {
                this->refreshFileList();  // 根据新的currentPath刷新文件列表
            }
            qDebug() << "Selected Path: " << currentPath;
        }
    }
}

// --------------------------------------------------------------------
void MainWindow::connectToFtp()
{
#ifdef DEBUG
    qDebug() << "连接测试";
    qDebug() << ui->lineEdit_ip->text() << ":" << ui->lineEdit_port->text().toInt();
#else

    QString ip = ui->lineEdit_ip->text();
    unsigned int port = ui->lineEdit_port->text().toUInt();

    client->connectToHost(ip, port);
    if (client->waitForConnected(2000)) {
        emit Send_Status(Status_Type::Default);  // 告知status Bar


        this->ui->lineEdit_ip->setEnabled(false);   // 当连接成功之后就不能更改了
        this->ui->lineEdit_port->setEnabled(false);
        this->ui->btn_connect->setEnabled(false);

        refreshFileList();
        refreshDirectoryTree();
    } else {
        qDebug() << "连接失败：" << client->errorString();
        emit Send_Status(Status_Type::Connect_Fail);
        this->ui->btn_connect->setEnabled(true);
    }

#endif
}

void MainWindow::setupConnections()
{
    QObject::connect(this->ui->btn_connect,&QPushButton::clicked,this,&MainWindow::connectToFtp);
    QObject::connect(this,&MainWindow::Send_Status,this,&MainWindow::updateStatus);
    QObject::connect(this->tableView,&View_For_FTP::show_Menu,this,&MainWindow::showTableContextMenu);
}

void MainWindow::updateStatus(Status_Type newStatus)
{
    switch(newStatus){
        case Default:{
            QString ip = ui->lineEdit_ip->text();
            unsigned int port = ui->lineEdit_port->text().toUInt();
            this->ui->statusbar->showMessage(QString("服务器信息{[ip]:%1 [port]:%2}").arg(ip).arg(port));
            break;
        }
        case Connect_Fail:{
            this->ui->statusbar->showMessage("连接服务器失败");
            break;
        }
        default:{
            break;
        }
    }
}

void MainWindow::showTableContextMenu()
{
    if(this->tableView->Connection_Seted == false){
        connect(this->tableView->uploadAction, &QAction::triggered, this, &MainWindow::uploadFile);
        connect(this->tableView->downloadAction, &QAction::triggered, this, &MainWindow::downloadFile);
        connect(this->tableView->deleteAction, &QAction::triggered, this, &MainWindow::deleteFile);
    };
}



// --------------------------------------------------------------------
// 弹出一个文件选择框，然后发送(UPL)UPLOAD命令和本地文件路径
// 然后把数据发送到服务器，服务器需要正确识别文件名并且保存然后Qt刷新

// 怎么说，Qt的发送逻辑是没问题的了，但是接收那边就麻烦了，现在不粘包了，但是UPL_instruction同样也接收不到数据。
// 难搞哦
// 这个发送逻辑折磨我一晚上，太爆炸了。
#include <QFileDialog>
#include <QMessageBox>
void MainWindow::uploadFile()
{

//    QByteArray command = "UPL ";
//    command.append(currentPath.size());  // 这里需要发送保存文件路径的大小，为了服务器好接收
//    command.append(currentPath.toStdString()); // 只能在tableview里面点击上传
//    // 而tableview正好又currentPath记录文件夹位置
//    client->write(command);

    // 弹出一个对话框，发送一个文件到Linux，并且和DownLoad一样同步ui->progress
    // 发送文件的格式：[文件名的大小][文件名][文件大小][文件内容]

    QByteArray command = "UPL ";    // 这个ByteAraay只能把字符给发送过去，不能发送其他类型
    command.append(QString::number(currentPath.size()).toStdString() + ' ');
    command.append(currentPath.toStdString());
    client->write(command);   // 当客户端发送过去的时候，启动的是读回调。所以第二次调用，肯定进的也是读回调
                                // 然而不存在第二次调用。因为粘包了

    qDebug() << command;


//    _sleep(1000);  // 没用，还是粘包  // 算了，粘包就粘包吧，只要接收了就行
    // 这段一起发送会导致粘包问题,在command和sendfile之间加sleep看看

    // 弹出一个窗口，选择一个文件，发送的顺序为: [文件名长度-固定为10个字符][文件名长度][文件内容长度-固定为10个字符][文件内容]

    QByteArray Send_File;
//    Send_File.append("112312323");  // 放入文件名的长度
    client->write(Send_File);



    QString filePath = QFileDialog::getOpenFileName(this, tr("选择文件"), "", tr("所有文件 (*)"));
    if (!filePath.isEmpty()) {
        QFile file(filePath);
        if (file.open(QIODevice::ReadOnly)) {
            QByteArray fileContent = file.readAll();
            QByteArray Send_File;
            QFileInfo fileInfo(filePath);
            QString fileName = fileInfo.fileName();
            QString fileNameSize = QString("%1").arg(fileName.size(), 10, 10, QLatin1Char(' ')).trimmed(); // 文件名长度，固定为10个字符，左对齐
            QString fileSize = QString("%1").arg(fileContent.size(), 10, 10, QLatin1Char(' ')).trimmed(); // 文件内容长度，固定为10个字符，左对齐

            // 确保文件名长度和文件内容长度固定为10个字符
            if (fileNameSize.size() < 10) {
                for (int i = fileNameSize.size(); i < 10; i++) {
                    fileNameSize.append(' ');
                    if(fileNameSize.size() >= 10)break;
                }
            }
            if (fileSize.size() < 10) {
                for (int i = fileSize.size(); i < 10; i++) {
                    fileSize.append(' ');
                    if(fileSize.size() >= 10)break;
                }
            }

            Send_File.append(fileNameSize.toUtf8() + " ");
            Send_File.append(fileName.toUtf8() + " ");
            Send_File.append(fileSize.toUtf8() + " ");
            Send_File.append(fileContent);
            client->write(Send_File);
            client->flush();  // 本来以为刷新发送缓冲区能够解决
            qDebug() << Send_File;
        }
        file.close();
    }



    // 这段肯定会粘包，但没法，不粘包第二次服务器的libevent什么都读不出来
}

//------------------------- ↓这段代码能够正常运行↓ -------------------------
// 发送DNL(DOWNLOAD)命令和指定的文件
// 接收Linux的数据，并且同步Progress部件
// 最后需要选择保存的位置，然后正确的保存

void MainWindow::downloadFile()   // 远端那边我第一次写下载逻辑的时候，在最后关闭了客户端。所以出现了这个问题。难怪获取文件树和文件列表超时。
{
    if (!tableView->currentIndex().isValid()) {
        qDebug() << "没有选中任何文件";
        return;
    }

    // 获取选择到的路径
    QModelIndex currentIndex =  this->tableView->currentIndex();
    QString file_path = currentPath + "/" + this->tableModel->data(currentIndex).toString();

    // 调试用的
    qDebug() << file_path;

    // 发送命令和文件的绝对路径
    QByteArray command = "DNL ";
    command.append(file_path.toStdString());
    client->write(command);


            // 接收文件名大小
            int filenameSize;
            if (client->bytesAvailable() < sizeof(int)) {
                client->waitForReadyRead(500);
            }
            client->read(reinterpret_cast<char*>(&filenameSize), sizeof(int));
            filenameSize = qFromBigEndian(filenameSize);

            // 接收文件名
            QByteArray filename;
            if (client->bytesAvailable() < filenameSize) {
                client->waitForReadyRead(500);
            }
            filename = client->read(filenameSize);

            // 接收文件大小
            int fileSize;
            if (client->bytesAvailable() < sizeof(int)) {
                client->waitForReadyRead(500);
            }
            client->read(reinterpret_cast<char*>(&fileSize), sizeof(int));

            // 接收文件内容
            QByteArray fileContent;
            qint64 bytesReceived = 0;
            int lastProgress = 0;
            while (bytesReceived < fileSize) {
                if (client->bytesAvailable() > 0) {
                    QByteArray chunk = client->readAll();
                    fileContent.append(chunk);
                    bytesReceived += chunk.size();
                    int currentProgress = static_cast<int>(100.0 * bytesReceived / fileSize);
                    if (currentProgress != lastProgress) {
                        ui->progress->setValue(currentProgress);
                        lastProgress = currentProgress;
                    }
                } else {
                    client->waitForReadyRead(500);
                }
            }

            // 处理接收到的文件信息
            qDebug() << "接收到的文件名大小" << filenameSize;
            qDebug() << "接收到的文件名：" << filename;
            qDebug() << "接收到的文件大小：" << fileSize;
            qDebug() << "文件内容:" << fileContent;


    // 最后创建个Dialog，让用户选择保存的地方
    QString selectedPath = QFileDialog::getSaveFileName(this, tr("保存文件"), QDir::homePath(), tr("All Files (*)"));
    if (!selectedPath.isEmpty()) {
        QFile file(selectedPath);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(fileContent);
            file.close();
            QMessageBox::information(this, tr("下载完成"), tr("文件已保存到: %1").arg(selectedPath));
        } else {
            QMessageBox::critical(this, tr("错误"), tr("无法保存文件"));
        }
    }

    this->refreshDirectoryTree();
    this->refreshFileList(); // 远端那边我第一次写下载逻辑的时候，在最后关闭了客户端。所以出现了这个问题。难怪获取文件树和文件列表超时。
}
    //------------------------- ↑这段代码能够正常运行↑ -------------------------


//------------------------- ↓这段代码能够正常运行↓ -------------------------
// 发送DEL命令和指定的文件，服务器接收之后删除，然后Qt刷新数据
void MainWindow::deleteFile()
{

    if (!tableView->currentIndex().isValid()) {
        qDebug() << "没有选中任何文件";
        return;
    }
    // 获取选择到的路径
    QModelIndex currentIndex =  this->tableView->currentIndex();
    QString file_path = currentPath + "/" + this->tableModel->data(currentIndex).toString();

    // 调试用的
    qDebug() << file_path;

    // 发送命令和文件的绝对路径
    QByteArray command = "DEL ";
    command.append(file_path.toStdString());
//    client->write(command);  // 在下面发送

    qDebug() << command;

    if (client->write(command) == -1) {
        qDebug() << "发送删除命令失败";
    } else {
        qDebug() << "已发送删除命令：" << command;
    }

    // 这里不能调用刷新，因为刷新的前几个字符莫名其妙跑到这个函数里面去了。
    // 造成：DEL a/b/cTRE 这样的情况，后面多了个TRE，本来是获取文件树的
    // 新需求：干脆把刷新写道QMainwindow的Menubar里面，反正又不麻烦
//    this->refreshDirectoryTree();
//    this->refreshFileList();
}
//------------------------- ↑这段代码能够正常运行↑ -------------------------


//------------------------- ↓这段代码能够正常运行↓ -------------------------
// 发送DIR命令，然后服务器接收，最后服务器将文件列表发送到Qt。
// 而Qt需要在Table List放入文件，并且更新Tree List。
void MainWindow::refreshFileList()
{
    QByteArray command = "DIR ";
    command.append(currentPath.toStdString());  // 发送的路径，不用发送path的大小，因为发送的就是['DIR /abcd']，那么把这些读取完就好了
    client->write(command);

    // 优化：发送DIR之后，再发送一个path，然后linux讲接收path，Linux将path的文件名都传输过来
    // 发送path大小
    // 发送path字符串
    // 结果：已经玩好了，多加个path没必要。


    // 等待服务器响应
    QByteArray fileList;
    while (true) {
        if (!client->waitForReadyRead(500)) {
            if (fileList.isEmpty()) {
                qDebug() << "等待服务器响应超时";
                return;
            } else {
                break;  // 已经接收到一部分数据，停止等待
            }
        }

        // 读取服务器发送的文件列表
        QByteArray part = client->readAll();
        fileList.append(part);
    }
    qDebug() << "接收到的文件列表数据：" << fileList;  // 首先将接收到的数据打印出来
            QStringList files = QString(fileList).split('\n', Qt::SkipEmptyParts);

    // 清空当前的文件列表模型
    tableModel->clear();
    tableModel->setHorizontalHeaderLabels(QStringList() << QStringLiteral("文件名"));

    // 将文件列表添加到模型中
    foreach (const QString &file, files) {
        if (file != "." && file != "..") {  // 过滤掉当前目录和上级目录的表示
            QStandardItem *item = new QStandardItem(file);
            tableModel->appendRow(item);
        }
    }
}
//------------------------- ↑这段代码能够正常运行↑ -------------------------


//------------------------- ↓这段代码能够正常运行↓ -------------------------

/* 为了递归获取文件目录，我制定了如下的文件列表协议
 * 文件树
├── 1.txt
├── 2.txt
└── abcd
    ├── 123123
    └── abcdabcd
        └── bbbbbbbbbbb
 * 那么发送则是这种形式， 如此往复，每多加一层则多一个'-'(深度)。
 * Qt端使用出了QStack来处理，Linux端使用出了递归来发送
send:1.txt|FILE
send:2.txt|FILE
send:abcd|DIR
send:123123|-FILE
send:abcdabcd|-DIR
send:bbbbbbbbbbb|--FILE
 */



#include <QMessageBox>
#include <QStack>
#include <QRegularExpression>
void MainWindow::refreshDirectoryTree()
{
    QByteArray command = "TRE ";
    client->write(command);

    // 等待服务器响应
    QByteArray fileList;
    while (true) {
        if (!client->waitForReadyRead(500)) {
            if (fileList.isEmpty()) {
                qDebug() << "等待服务器响应超时";
                return;
            } else {
                break;  // 已经接收到一部分数据，停止等待
            }
        }

        // 读取服务器发送的文件列表
        QByteArray part = client->readAll();
        fileList.append(part);
    }
    qDebug() << "接收到的文件列表数据：" << fileList;  // 首先将接收到的数据打印出来
            QStringList files = QString(fileList).split('\n', Qt::SkipEmptyParts);

    // 清空当前的目录树视图
    TreeModel *model = static_cast<TreeModel*>(this->ui->treeView->model());
    model->clear();

    // 将目录数据添加到视图中
    QStack<TreeItem*> parents;
    parents.push(nullptr);  // 根节点的父节点是nullptr

    foreach (const QString &fileInfo, files) {
        QStringList fileDetails = fileInfo.split('|');
        QString fileName = fileDetails[0].trimmed();
        QString fileType = fileDetails[1].trimmed();
        int depth = fileType.count('-');  // 计算文件深度
        fileType.remove("-");  // 移除文件名前的'-'字符

        if (fileName != "." && fileName != "..") {  // 过滤掉当前目录和上级目录的表示
            while (depth < parents.size() - 1) {
                parents.pop();  // 返回到正确的深度
            }

            TreeItem *parentItem = parents.top();
            auto newItem = new TreeItem();
            newItem->setData(fileName);

            if (fileType == "DIR") {
                if (parentItem) {
                    model->addItem(parentItem,newItem);
                } else {
                    model->addTopLevelItem(newItem);  // 顶层目录
                }
                parents.push(newItem);  // 新目录成为后续文件的父节点
            } else if (fileType == "FILE") {
                if (parentItem) {
                    model->addItem(parentItem,newItem);
                } else {
                    model->addTopLevelItem(newItem);  // 顶层文件
                }
            }
        }
    }
}
//------------------------- ↑这段代码能够正常运行↑ -------------------------

