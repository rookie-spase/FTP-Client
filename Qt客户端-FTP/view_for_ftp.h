#ifndef VIEW_FOR_FTP_H
#define VIEW_FOR_FTP_H

#include <QTableView>
#include <QObject>
class Menu;
class QContextMenuEvent;
class View_For_FTP : public QTableView
{
    Q_OBJECT
public:
    View_For_FTP(QWidget* parent = nullptr);
    ~View_For_FTP();
    virtual void contextMenuEvent(QContextMenuEvent *event) override;


signals:
    void show_Menu();
private:
    QMenu* menu;
public:
    QAction *uploadAction;
    QAction *downloadAction;
    QAction *deleteAction;
    bool Connection_Seted;  // action需要设置槽函数，而槽函数在这个文件之外
                            // 所以写成public，用于外部连接。
                            // 而当右键的时候就发送“显示菜单”，让外界连接
                            // 为了不多次设置槽函数，所以设置了一个bool变量控制
};

#endif // VIEW_FOR_FTP_H
