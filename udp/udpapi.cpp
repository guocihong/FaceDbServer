#include "udpapi.h"

UdpApi *UdpApi::instance = NULL;

UdpApi::UdpApi(QObject *parent) :
    QObject(parent)
{
}

void UdpApi::Listen(void)
{
#ifdef Q_OS_LINUX
//    system("route add -net 224.0.0.0 netmask 224.0.0.0 eth0");
#endif

    udp_socket = new QUdpSocket(this);

#if (QT_VERSION < QT_VERSION_CHECK(5,0,0))
    udp_socket->bind(QHostAddress::Any, GlobalConfig::UdpGroupPort);
#else
    udp_socket->bind(QHostAddress::AnyIPv4, GlobalConfig::UdpGroupPort);
#endif

    udp_socket->setSocketOption(QAbstractSocket::MulticastLoopbackOption, 0);//禁止回环许可
    udp_socket->joinMulticastGroup(QHostAddress(GlobalConfig::UdpGroupAddr));//加入组播地址
    connect(udp_socket,SIGNAL(readyRead()),this,SLOT(slotProcessPendingDatagrams()));

    ParseGroupMsgTimer = new QTimer(this);
    ParseGroupMsgTimer->setInterval(100);
    connect(ParseGroupMsgTimer,SIGNAL(timeout()),this,SLOT(slotParseGroupMsg()));
    ParseGroupMsgTimer->start();

    ParseVaildGroupMsgTimer = new QTimer(this);
    ParseVaildGroupMsgTimer->setInterval(100);
    connect(ParseVaildGroupMsgTimer,SIGNAL(timeout()),this,SLOT(slotParseVaildGroupMsg()));
    ParseVaildGroupMsgTimer->start();
}

void UdpApi::slotProcessPendingDatagrams()
{
    while (udp_socket->hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(udp_socket->pendingDatagramSize());
        udp_socket->readDatagram(datagram.data(), datagram.size());

        GroupMsgBuffer.append(datagram);

        CommonSetting::print(QString("接收:") + datagram);
    }
}

void UdpApi::slotParseGroupMsg()
{
    while(GroupMsgBuffer.size() > 0) {
        int size = GroupMsgBuffer.size();

        //寻找帧头的索引
        int FrameHeadIndex = GroupMsgBuffer.indexOf("IDOOR");
        if (FrameHeadIndex < 0) {
            break;
        }


        if (size < (FrameHeadIndex + 20)) {
            break;
        }

        //取出xml数据包的长度，不包括帧头的20个字节
        int length = GroupMsgBuffer.mid(FrameHeadIndex + 6,14).toUInt();

        //没有收到一个完整的数据包
        if (size < (FrameHeadIndex + 20 + length)) {
            break;
        }

        //取出一个完整的xml数据包,不包括帧头20个字节
        QByteArray VaildCompletePackage = GroupMsgBuffer.mid(FrameHeadIndex + 20,length);

        //更新GroupMsgBuffer内容
        GroupMsgBuffer = GroupMsgBuffer.mid(FrameHeadIndex + 20 + length);

        //保存完整数据包
        VaildGroupMsgBuffer.append(VaildCompletePackage);
    }
}

void UdpApi::slotParseVaildGroupMsg()
{
    while (VaildGroupMsgBuffer.size() > 0) {
        QByteArray datagram = VaildGroupMsgBuffer.takeFirst();

        QDomDocument dom;
        QString errorMsg;
        int errorLine, errorColumn;

        if(!dom.setContent(datagram, &errorMsg, &errorLine, &errorColumn)) {
            continue;
        }

        QDomElement RootElement = dom.documentElement();//获取根元素

        bool isSearchDevice = false;

        if (RootElement.tagName() == "PCClient") { //根元素名称
            QDomNode firstChildNode = RootElement.firstChild();//第一个子节点
            while (!firstChildNode.isNull()) {
                if (firstChildNode.nodeName() == "SearchDevice") {
                    isSearchDevice = true;
                }

                firstChildNode = firstChildNode.nextSibling();//下一个节点
            }

            if (isSearchDevice) {
                QString Msg;
                QDomDocument AckDom;

                //xml声明
                QString XmlHeader("version=\"1.0\" encoding=\"UTF-8\"");
                AckDom.appendChild(AckDom.createProcessingInstruction("xml", XmlHeader));

                //创建根元素
                QDomElement RootElement = AckDom.createElement("DoorDevice");
                AckDom.appendChild(RootElement);

                //创建ConfigInfo元素
                QDomElement ConfigInfo = AckDom.createElement("ConfigInfo");
                ConfigInfo.setAttribute("RemoteDeviceIP",GlobalConfig::LocalHostIP);
                ConfigInfo.setAttribute("RemoteDeviceNetmask",GlobalConfig::Netmask);
                ConfigInfo.setAttribute("RemoteDeviceGateway",GlobalConfig::Gateway);
                ConfigInfo.setAttribute("RemoteDeviceMAC",GlobalConfig::MAC);
                ConfigInfo.setAttribute("RemoteDeviceDNS",GlobalConfig::DNS);
                ConfigInfo.setAttribute("RemoteDeviceVersion",GlobalConfig::Version);

                RootElement.appendChild(ConfigInfo);

                QTextStream Out(&Msg);
                AckDom.save(Out,4);

                int length = Msg.toLocal8Bit().size();
                Msg = QString("IDOOR:") + QString("%1").arg(length,14,10,QLatin1Char('0')) + Msg;
                udp_socket->writeDatagram(Msg.toLatin1(),
                                          QHostAddress(GlobalConfig::UdpGroupAddr),
                                          GlobalConfig::UdpGroupPort);

                CommonSetting::print(QString("发送:") + Msg);
            }
        }
    }
}
