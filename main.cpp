#include "mainform.h"
#include "log/loghelper.h"
#include "tcp/tcpserverapi.h"
#include "tcp/tcpclientapi.h"
#include "tcp/phoneapi.h"
#include "upgrade/deviceupgradethreadserver.h"
#include "cpu/cpuapi.h"
#include "udp/udpapi.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    //安装日志钩子,把所有打印信息通过网络输出
    LogHelper::newInstance()->start();

    a.setFont(QFont("WenQuanYi Micro Hei", 14));

    CommonSetting::SetUTF8Code();

    //保存比对记录的图片
    QString Path = CommonSetting::GetCurrentPath() + "log";

    if (!CommonSetting::DirExists(Path)) {
        CommonSetting::CreateFolder(CommonSetting::GetCurrentPath(),"log");
    }

    //保存录入图片
    Path = CommonSetting::GetCurrentPath() + "images";

    if (!CommonSetting::DirExists(Path)) {
        CommonSetting::CreateFolder(CommonSetting::GetCurrentPath(),"images");
    }

#ifdef jiance
    //保存录入图片
    Path = CommonSetting::GetCurrentPath() + "snap";

    if (!CommonSetting::DirExists(Path)) {
        CommonSetting::CreateFolder(CommonSetting::GetCurrentPath(),"snap");
    }
#endif

    GlobalConfig::newInstance()->init();

    //3、开启组播监听
    UdpApi::newInstance()->Listen();

    //接收tcp数据
    Server::TcpServerApi::newInstance()->Listen();

#ifdef jiance
    Phone::PhoneApi::newInstance()->Listen();
#endif

    //开启tcp升级监听
    UpgradeServerApi::newInstance()->Listen();

    //发送tcp数据
    Client::TcpClientApi *SendPersonInfo =
            new Client::TcpClientApi(Client::TcpClientApi::sendPersonInfo);

    Client::TcpClientApi *SendCompareRecordInfo =
            new Client::TcpClientApi(Client::TcpClientApi::sendCompareRecordInfo);

    Client::TcpClientApi *SendDeviceInfo =
            new Client::TcpClientApi(Client::TcpClientApi::sendDeviceInfo);

    MainForm w;
    w.showMaximized();

    //查看cpu温度和频率
    CpuApi::newInstance()->init();

    return a.exec();
}
