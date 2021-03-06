#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QFile>
#include <QCompleter>
#include <QProcess>
#include <QDesktopServices>
#include <QTextStream>
#include <QLocalSocket>
#include <QStringListModel>

#include <QThread>

#include "googleresultdelegate.h"
#include "../util/constants.h"
#include "../util/persistencehandler.h"

MainWindow::MainWindow(QList<Target *> *targetList, QString target) :
    QMainWindow(0),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
//    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowFlags(Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint | Qt::CustomizeWindowHint);

    targetList_ = targetList;
    historyList = new QStringList();

    updateTargetCompleter();

    server = new QLocalServer(this);
    connect(server, SIGNAL(newConnection()), this, SLOT(newConnection()));
    server->listen(SERVER_MAIN);

    googleEnabled = true;
    editingCompleter = 0;

    forceFocus();
    initialize(target);

    workerThread = new QThread(this);
    worker = new QueryWorker();
    worker->moveToThread(workerThread);
    connect(this, SIGNAL(startQuery(QString, QString)), worker, SLOT(query(QString, QString)), Qt::QueuedConnection);
    connect(this, SIGNAL(cancelQuery()), worker, SLOT(abort()));
    connect(worker, SIGNAL(queryFinished(QStringList *)), this, SLOT(queryDone(QStringList *)));

    workerThread->start();
}

MainWindow::~MainWindow()
{
    delete ui;
    delete targetList_;
    delete historyList;
    server->close();
    delete server;

    worker->abort();
    workerThread->quit();
    workerThread->wait();

    delete worker;
    delete workerThread;
}

void MainWindow::newConnection()
{
    QLocalSocket *socket = server->nextPendingConnection();
    socket->waitForReadyRead(2000);

    QByteArray array = socket->readLine();
    QString target(array);

    socket->close();
    delete socket;

    initialize(target);
    on_txtArgument_textEdited();
//    showNormal();
//    activateWindow();
}

void MainWindow::initialize(QString const &target)
{
    emit cancelQuery();

    if (target == NULL || target.isEmpty()) {
        QString *target = new QString();
        QString *argument = new QString();
        PersistenceHandler::loadLast(target, argument, this);
        ui->txtTarget->setText(*target);
        ui->txtArgument->setText(*argument);
        delete target;
        delete argument;

        checkTarget(ui->txtTarget->text());

        forceFocus();
        ui->txtTarget->setFocus(Qt::ActiveWindowFocusReason);
        ui->txtTarget->selectAll();

    } else {
        ui->txtTarget->setText(target);
//        ui->txtArgument->setText("");
        forceFocus();
        ui->txtArgument->setFocus(Qt::ActiveWindowFocusReason);
    }
}

void MainWindow::saveHistory(Target const &target)
{
    QString argument = ui->txtArgument->text().trimmed();

    for (int i = 0; i < historyList->size(); i++) {
        if (argument.compare(historyList->at(i), Qt::CaseInsensitive) == 0) {
            historyList->removeAt(i);
        }
    }
    historyList->prepend(argument);

    while (historyList->size() > 100) {
        historyList->removeLast();
    }

    PersistenceHandler::saveHistory(target.getName(), *historyList, this);
}

void MainWindow::checkTarget(QString const &target)
{
    if (target.isEmpty()) {
        ui->txtArgument->setEnabled(false);
    } else {
        bool enabled = false;
        for (QList<Target *>::const_iterator i = targetList_->cbegin(); i != targetList_->cend(); i++) {
            if (target.compare((*i)->getName(), Qt::CaseInsensitive)) {
                enabled = true;
                break;
            }
        }
        ui->txtArgument->setEnabled(enabled);
    }
}

void MainWindow::updateTargetCompleter()
{
    QStringList *targets = new QStringList();
    for (QList<Target *>::const_iterator i = targetList_->cbegin(); i != targetList_->cend(); i++) {
        targets->append((*i)->getName());
    }

    QCompleter *completer = new QCompleter(*targets, ui->txtTarget);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    completer->setCompletionMode(QCompleter::InlineCompletion);
    ui->txtTarget->setCompleter(completer);

    delete targets;
}

void MainWindow::updateArgumentCompleter(QStringList *list, bool google)
{
    editingCompleter++;
    QCompleter *completer = ui->txtArgument->completer();

    if (completer == NULL) {
        completer = new QCompleter(*list, ui->txtArgument);
        ui->txtArgument->setCompleter(completer);
    } else {
        QStringListModel *model = new QStringListModel(*list, completer);
        completer->setModel(model);
    }

    completer->setCaseSensitivity(Qt::CaseInsensitive);
    completer->setCompletionMode(QCompleter::PopupCompletion);
    completer->popup()->installEventFilter(this);
    completer->popup()->setItemDelegate(new GoogleResultDelegate());

    if (google) {
        completer->setMaxVisibleItems(21);
    } else {
        completer->setMaxVisibleItems(7);
    }

    completer->setCompletionPrefix(ui->txtArgument->text());

    if (!list->isEmpty()) {
        completer->complete();
    }

    editingCompleter--;
}

bool MainWindow::eventFilter(QObject *target, QEvent *event)
{
    if (event->type() == QEvent::KeyRelease) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        keyReleaseEvent(keyEvent);
    }

    return QMainWindow::eventFilter(target, event);
}

void MainWindow::on_txtArgument_focussed()
{
    if (editingCompleter) return;
    QString target = ui->txtTarget->text().trimmed();

    if (target == lastTarget) {
        if (!historyList->isEmpty()) {
            ui->txtArgument->completer()->setCompletionPrefix(ui->txtArgument->text());
            ui->txtArgument->completer()->complete();
        }

        return;

    } else {
        lastTarget = target;
    }

    if (!target.isEmpty()) {
        historyList->clear();

        for (QList<Target *>::const_iterator i = targetList_->cbegin(); i != targetList_->cend(); i++) {
            if (target.compare((*i)->getName(), Qt::CaseInsensitive) == 0) {

                //Loading query URL
                queryURL = (*i)->getQuery();

                //Loading history
                PersistenceHandler::loadHistory((*i)->getName(), historyList, this);
                break;
            }
        }

        updateArgumentCompleter(historyList);
    }
}

void MainWindow::execute()
{
    emit cancelQuery();

    QString target = ui->txtTarget->text().trimmed();

    if (!target.isEmpty()) {
        for (QList<Target *>::const_iterator i = targetList_->cbegin(); i != targetList_->cend(); i++) {
            if (target.compare((*i)->getName(), Qt::CaseInsensitive) == 0) {

                //Saving history
                saveHistory(*(*i));

                QString command;

                if ((*i)->getCommand().isEmpty()) {
                    command = target;
                } else {
                    command = (*i)->getCommand();
                }

                if (command.startsWith("http://") || command.startsWith("https://")) {
                    command.append(ui->txtArgument->encodedText());
                    QDesktopServices::openUrl(QUrl(command));
                } else {
                    command.append(" ");
                    command.append(ui->txtArgument->text().trimmed());
                    QProcess *p = new QProcess(this);
                    p->startDetached(command);
                    connect(p, SIGNAL(finished(int)), p, SLOT(deleteLater())); //Clean UP!!
                }

                break;
            }
        }
    }

    PersistenceHandler::saveLast(ui->txtTarget->text(), ui->txtArgument->text(), this);
    qApp->exit(0);
}

void MainWindow::keyReleaseEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_ScrollLock) {
        googleEnabled = !googleEnabled;
        emit cancelQuery();

        if (googleEnabled) {
            on_txtArgument_textEdited();
        } else {
            updateArgumentCompleter(historyList);
        }

    } else if (event->key() == Qt::Key_Escape) {
        qApp->exit(0);
    }
}

void MainWindow::on_txtTarget_textChanged(QString string)
{
    checkTarget(string);
}

void MainWindow::on_txtTarget_returnPressed()
{
    execute();
}

void MainWindow::on_txtArgument_returnPressed()
{
    execute();
}

void MainWindow::on_txtArgument_downReleased()
{
    updateArgumentCompleter(historyList);
}

void MainWindow::on_txtArgument_textEdited()
{
    emit cancelQuery();

    if (queryURL.isEmpty()) {
        return;
    }

    QString argument = ui->txtArgument->encodedText();

    if (argument.isEmpty()) {
        return;
    }

    if (googleEnabled) {
        emit startQuery(queryURL, argument);
    }
}

void MainWindow::queryDone(QStringList *results)
{
    if (!googleEnabled) {
        return;
    }

    if (results->isEmpty()) {
        return;
    }

    *results += *historyList;

    updateArgumentCompleter(results, true);
    delete results;
}
