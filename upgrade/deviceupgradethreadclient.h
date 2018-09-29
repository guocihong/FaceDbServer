#ifndef DEVICEUPGRADETHREADCLIENT_H
#define DEVICEUPGRADETHREADCLIENT_H

#include <QThread>
#include <QTcpSocket>
#include <QProgressBar>
#include <QFile>
#include <QFileInfo>

class DeviceUpgradeThreadClient : public QThread
{
    Q_OBJECT
public:
    explicit DeviceUpgradeThreadClient(QString FilePath, QString ServerIP, quint16 ServerPort, QProgressBar *ProgressBar, QObject *parent = 0);

protected:
    void run();

public slots:
    //发送失败处理
    void slotSendError();

    //设置进度条最大值
    void slotSetProgressBarMaximum(qint64 TotalBytesToWritten);

    //更新发送进度
    void slotUpdateProgressBar(qint64 BytesWritten);

private:
    //升级包绝对路径
    QString FilePath;

    //服务器IP地址
    QString ServerIP;

    //服务器监听端口
    quint16 ServerPort;

    //进度条指针
    QProgressBar *ProgressBar;
};

class DeviceUpgradeSocketClient : public QTcpSocket
{
    Q_OBJECT
public:
    explicit DeviceUpgradeSocketClient(QString FilePath, QString ServerIP, quint16 ServerPort, QObject *parent = 0);

    enum SendProgressState {
        SendStartIdentifier = 0x01,//发送开始
        SendFileContentIdentifier = 0x02,//发送文件内容
        SendEndIdentifier = 0x03//发送结束
    };

    enum SendResultState {
        SendFailed = 0x00,//发送失败
        SendSucceed = 0x01//发送成功
    };

signals:
    //设置进度条最大值
    void setProgressBarMaximum(qint64 TotalBytesToWritten);

    //更新发送进度
    void updateProgressBar(qint64 BytesWritten);

    //发送出错
    void error();

    //发送完成
    void finish();

private slots:
    //连接已建立 -> 开始发数据
    void slotStartTransfers();

    //数据已发出 -> 继续发
    void slotContinueTransfers(qint64 bytes);

    //接收服务器返回的数据包
    void slotRecvMsg();

    //服务器断开tcp连接
    void slotDisconnect();

    //连接错误处理
    void slotDisplayError(QAbstractSocket::SocketError socketError);

private:
    //升级包绝对路径
    QString FilePath;

    //服务器IP地址
    QString ServerIP;

    //服务器监听端口
    quint16 ServerPort;

    //文件大小
    qint64 TotalBytesToWritten;

    //已经发送字节数
    qint64 BytesWritten;

    //待发送字节数
    qint64 BytesToWrite;

    //发送状态标志
    enum SendResultState flag;

    QFile *file;
};

#endif // DEVICEUPGRADETHREADCLIENT_H
