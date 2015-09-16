#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <QApplication>
#include <QProcess>


#include <QDebug>


#include "trayicon.h"
#include "hotkeycapturer.h"
#include "../util/constants.h"

class Controller : public QObject
{
    Q_OBJECT
public:
    explicit Controller(QObject *parent = 0);
    ~Controller();

    void initialize();
    void finalize();

private:

    TrayIcon *tray;
    HotkeyCapturer *capturer;

private slots:
    void launch(QString);
    void trayOpen();
    void closeApp();
    void edit();
    void editClosed(int code);
};

#endif // CONTROLLER_H
