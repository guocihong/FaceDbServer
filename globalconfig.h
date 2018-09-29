#ifndef GLOBALCONFIG_H
#define GLOBALCONFIG_H

#include "CommonSetting.h"

class GlobalConfig : public QObject
{
    Q_OBJECT
public:
    explicit GlobalConfig(QObject *parent = 0);

    static GlobalConfig *newInstance() {
        static QMutex mutex;

        if (!instance) {
            QMutexLocker locker(&mutex);

            if (!instance) {
                instance = new GlobalConfig();
            }
        }

        return instance;
    }

    void init(void);

public:
    static GlobalConfig *instance;

    //配置文件的文件名
    static QString ConfigFileName;

    //程序版本
    static QString Version;

    //本机IP地址
    static QString LocalHostIP;

    //本机子网掩码
    static QString Netmask;

    //本机网关
    static QString Gateway;

    //本机MAC地址
    static QString MAC;

    //本机MAC地址
    static QString DNS;

    //接收端口
    static quint16 RecvPort;

    //发送端口
    static quint16 SendPort;

    //手机配置端口
    static quint16 PhonePort;

    //日志端口
    static quint16 LogPort;

    //升级端口
    static quint16 UpgradePort;

    //组播地址
    static QString UdpGroupAddr;

    //组播端口
    static quint16 UdpGroupPort;

    //平台中心IP
    static QString PlatformCenterIP;

    //平台中心端口
    static quint16 PlatformCenterPort;

    //专门用来提取特征值设备的IP
    static QString FaceServerIP;

    //专门用来提取特征值设备的端口
    static quint16 FaceServerPort;

    //AgentID
    static QString AgentID;

    //保存最后一次更新的时间
    static QDateTime LastUpdateDateTime;

    //全局锁
    static QMutex GlobalLock;
};

#endif // GLOBALCONFIG_H
