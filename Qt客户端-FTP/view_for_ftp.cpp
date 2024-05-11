#include "view_for_ftp.h"
#include <QContextMenuEvent>
#include <QMenu>

View_For_FTP::View_For_FTP(QWidget* parent)
    : QTableView(parent),
    menu(new QMenu(this)),
        uploadAction(menu->addAction("上传")),
        downloadAction(menu->addAction("下载")),
        deleteAction(menu->addAction("删除")),
    Connection_Seted(false)
{
}


void View_For_FTP::contextMenuEvent(QContextMenuEvent *event)
{
    emit show_Menu();
    Connection_Seted = true;
    menu->exec(event->globalPos());
}

View_For_FTP::~View_For_FTP()
{
    delete menu;
}
