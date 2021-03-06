#ifndef SERIALTERM_H
#define SERIALTERM_H

#include <QObject>
#include <QWidget>
#include <QSerialPort>
#include <qtermwidget5/qtermwidget.h>
#include <QSettings>
#include <QThread>

#include <QDebug>

#include "macro/macroWorker.h"

class SerialTerm : public QTermWidget
{
    Q_OBJECT
public:
    explicit SerialTerm(QWidget *parent = 0, QString comName = "/dev/ttyS0", QSettings *settings = NULL);
    ~SerialTerm();
    QString get_name();
    QString get_status();
    void apply_setting();
    //return serial is open or not
    bool isOpen();

    //log
    void setLogDatetime(bool bLogDatetime);
    void setLogEnable(bool bLogEnable);
    void setLogFilename(QString filename);
    QString getLogFileName();
    void logToFile(QByteArray log);
    void logToFile(QString lineToBelogged);

    /*
     * try open serial port if successful return true else return false
     */
    bool openSerialPort();
    void closeSerialPort();
    void writeln(const QByteArray &data);

    void setScrollToBottom(bool bScrollToBottom);
    bool getScrollToBottom();
    //macro
    void macroStart();
    void macroStop();
    bool isMacroRunning();
    Qt::HANDLE getMacroThreadId();


signals:
    void sig_updateStatus(QString sMsg);
    void sig_updateActionBtnStatus(bool bStatus);
    void sig_DataReady(const QByteArray &data);

private slots:
    void readDataFromSerial();
    void writeDataToSerial(const QByteArray &data);
    void activateUrl(const QUrl &url, bool fromContextMenu);
    void slot_handleError(QSerialPort::SerialPortError error);
    void slot_baudRateChanged(qint32 baudRate,QSerialPort::Directions directions);
    void on_keyPressed(QKeyEvent *keyEvent);
    Q_SLOT void on_receivedData(QString data);
    Q_SLOT void on_sendData(const char* data,int len);

private:
    QString getCurrentDateTimeString();
private:
    QSerialPort *serial;
    QString mComName;
    QSettings *mSettings;
    QByteArray lastSend;
    //log
    bool _bLogEnable;
    bool _bLogDatetime;
    QString _sLogFilename;

    bool _bScrollToBottom;
    //macro
    /**
     * @brief Thread object which will let us manipulate the running thread
     */
    QThread *macrothread;
    /**
     * @brief Object which contains methods that should be runned in another thread
     */
    macroWorker *macroworker;
};

#endif // SERIALTERM_H
