#include "globalconfig.h"

GlobalConfig *GlobalConfig::instance = NULL;

QString GlobalConfig::ConfigFileName = "FaceDbServer_Config.ini";
QString GlobalConfig::Version = QString("2018-08-19");

QString GlobalConfig::LocalHostIP = CommonSetting::GetLocalHostIP();
QString GlobalConfig::Netmask = CommonSetting::GetMask();
QString GlobalConfig::Gateway = CommonSetting::GetGateway();
QString GlobalConfig::MAC = CommonSetting::GetMacAddress();
QString GlobalConfig::DNS = CommonSetting::GetDNS();

quint16 GlobalConfig::RecvPort = 6666;
quint16 GlobalConfig::SendPort = 6667;
quint16 GlobalConfig::PhonePort = 6671;
quint16 GlobalConfig::LogPort = 8899;
quint16 GlobalConfig::UpgradePort = 6670;
QString GlobalConfig::UdpGroupAddr = "224.0.0.17";
quint16 GlobalConfig::UdpGroupPort = 6900;
QString GlobalConfig::PlatformCenterIP = QString("192.168.1.204");
quint16 GlobalConfig::PlatformCenterPort = 5555;
QString GlobalConfig::FaceServerIP = QString("192.168.1.239");
quint16 GlobalConfig::FaceServerPort = 6662;
QString GlobalConfig::AgentID = QString("SGTEST000001");

QDateTime GlobalConfig::LastUpdateDateTime = QDateTime::fromString("1970-01-01 00:00:00","yyyy-MM-dd hh:mm:ss");

QMutex GlobalConfig::GlobalLock;

GlobalConfig::GlobalConfig(QObject *parent) :
    QObject(parent)
{
}

void GlobalConfig::init(void)
{
    GlobalConfig::ConfigFileName = CommonSetting::GetCurrentPath() + "FaceDbServer_Config.ini";//配置文件的文件名

    QString temp;

    if (CommonSetting::FileExists(GlobalConfig::ConfigFileName)) {//配置文件存在
        temp = CommonSetting::ReadSettings(GlobalConfig::ConfigFileName,"AppGlobalConfig/PlatformCenterIP");
        if (!temp.isEmpty()) {
            GlobalConfig::PlatformCenterIP = temp;
        }

        temp = CommonSetting::ReadSettings(GlobalConfig::ConfigFileName,
                                           "AppGlobalConfig/PlatformCenterPort");
        if (!temp.isEmpty()) {
            GlobalConfig::PlatformCenterPort = temp.toUShort();
        }

        temp = CommonSetting::ReadSettings(GlobalConfig::ConfigFileName,"AppGlobalConfig/FaceServerIP");
        if (!temp.isEmpty()) {
            GlobalConfig::FaceServerIP = temp;
        }

        temp = CommonSetting::ReadSettings(GlobalConfig::ConfigFileName,
                                           "AppGlobalConfig/FaceServerPort");
        if (!temp.isEmpty()) {
            GlobalConfig::FaceServerPort = temp.toUShort();
        }

        temp = CommonSetting::ReadSettings(GlobalConfig::ConfigFileName,"AppGlobalConfig/AgentID");
        if (!temp.isEmpty()) {
            GlobalConfig::AgentID = temp;
        }
    } else {//配置文件不存在,使用默认值生成配置文件
        CommonSetting::WriteSettings(GlobalConfig::ConfigFileName,
                                     "AppGlobalConfig/PlatformCenterIP",
                                     GlobalConfig::PlatformCenterIP);

        CommonSetting::WriteSettings(GlobalConfig::ConfigFileName,
                                     "AppGlobalConfig/PlatformCenterPort",
                                     QString::number(GlobalConfig::PlatformCenterPort));

        CommonSetting::WriteSettings(GlobalConfig::ConfigFileName,
                                     "AppGlobalConfig/FaceServerIP",
                                     GlobalConfig::FaceServerIP);

        CommonSetting::WriteSettings(GlobalConfig::ConfigFileName,
                                     "AppGlobalConfig/FaceServerPort",
                                     QString::number(GlobalConfig::FaceServerPort));

        CommonSetting::WriteSettings(GlobalConfig::ConfigFileName,
                                     "AppGlobalConfig/AgentID",
                                     GlobalConfig::AgentID);
    }
}
