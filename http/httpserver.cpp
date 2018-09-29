
//#include "stdafx.h"

#include "httpserver.h"
#include "httprequest.h"
#include "httpresponse.h"
#include "httpconnection.h"

#include <QTcpServer>
#include "qstringlist.h"
#include "globalconfig.h"

QHash<int, QString> STATUS_CODES;

HttpServer::HttpServer(QObject *parent)
    : QObject(parent)
    , m_tcpServer(0)
{
#define STATUS_CODE(num, reason) STATUS_CODES.insert(num, reason);
    // {{{
    STATUS_CODE(100, "Continue")
            STATUS_CODE(101, "Switching Protocols")
            STATUS_CODE(102, "Processing")                 // RFC 2518) obsoleted by RFC 4918
            STATUS_CODE(200, "OK")
            STATUS_CODE(201, "Created")
            STATUS_CODE(202, "Accepted")
            STATUS_CODE(203, "Non-Authoritative Information")
            STATUS_CODE(204, "No Content")
            STATUS_CODE(205, "Reset Content")
            STATUS_CODE(206, "Partial Content")
            STATUS_CODE(207, "Multi-Status")               // RFC 4918
            STATUS_CODE(300, "Multiple Choices")
            STATUS_CODE(301, "Moved Permanently")
            STATUS_CODE(302, "Moved Temporarily")
            STATUS_CODE(303, "See Other")
            STATUS_CODE(304, "Not Modified")
            STATUS_CODE(305, "Use Proxy")
            STATUS_CODE(307, "Temporary Redirect")
            STATUS_CODE(400, "Bad Request")
            STATUS_CODE(401, "Unauthorized")
            STATUS_CODE(402, "Payment Required")
            STATUS_CODE(403, "Forbidden")
            STATUS_CODE(404, "Not Found")
            STATUS_CODE(405, "Method Not Allowed")
            STATUS_CODE(406, "Not Acceptable")
            STATUS_CODE(407, "Proxy Authentication Required")
            STATUS_CODE(408, "Request Time-out")
            STATUS_CODE(409, "Conflict")
            STATUS_CODE(410, "Gone")
            STATUS_CODE(411, "Length Required")
            STATUS_CODE(412, "Precondition Failed")
            STATUS_CODE(413, "Request Entity Too Large")
            STATUS_CODE(414, "Request-URI Too Large")
            STATUS_CODE(415, "Unsupported Media Type")
            STATUS_CODE(416, "Requested Range Not Satisfiable")
            STATUS_CODE(417, "Expectation Failed")
            STATUS_CODE(418, "I\"m a teapot")              // RFC 2324
            STATUS_CODE(422, "Unprocessable Entity")       // RFC 4918
            STATUS_CODE(423, "Locked")                     // RFC 4918
            STATUS_CODE(424, "Failed Dependency")          // RFC 4918
            STATUS_CODE(425, "Unordered Collection")       // RFC 4918
            STATUS_CODE(426, "Upgrade Required")           // RFC 2817
            STATUS_CODE(500, "Internal Server Error")
            STATUS_CODE(501, "Not Implemented")
            STATUS_CODE(502, "Bad Gateway")
            STATUS_CODE(503, "Service Unavailable")
            STATUS_CODE(504, "Gateway Time-out")
            STATUS_CODE(505, "HTTP Version not supported")
            STATUS_CODE(506, "Variant Also Negotiates")    // RFC 2295
            STATUS_CODE(507, "Insufficient Storage")       // RFC 4918
            STATUS_CODE(509, "Bandwidth Limit Exceeded")
            STATUS_CODE(510, "Not Extended")                // RFC 2774
            // }}}
}

HttpServer::~HttpServer()
{
}

void HttpServer::newConnection()
{
    Q_ASSERT(m_tcpServer);
    while(m_tcpServer->hasPendingConnections()) {
        HttpConnection *connection = new HttpConnection(m_tcpServer->nextPendingConnection(), this);
        connect(connection, SIGNAL(newRequest(HttpRequest *, HttpResponse *)),
                this, SLOT(onRequest(HttpRequest *, HttpResponse *)));
    }
}

bool HttpServer::listen(const QHostAddress &address, quint16 port)
{
    m_tcpServer = new QTcpServer;
    connect(m_tcpServer, SIGNAL(newConnection()), this, SLOT(newConnection()));
    return m_tcpServer->listen(address, port);
}

bool HttpServer::listen(quint16 port)
{
    return listen(QHostAddress::Any, port);
}


void HttpServer::onRequest(HttpRequest *req, HttpResponse *resp)
{
    connect(req, SIGNAL(data(QByteArray)), this, SLOT(data(QByteArray)));
    connect(req, SIGNAL(end()), req, SLOT(deleteLater()));

    QStringList list;
    list.append("<html>");

    list.append("<head><title>数据库服务器配置</title></head>");

    list.append("<body>");    
    list.append("<form id='form' action='.' method='post' accept-charset='UTF-8'>");
    list.append("<p id='p'>平台中心IP</p>");
    list.append(QString("<input id='txt' type='text' name='PlatformCenterIP' value='%1'>").arg(GlobalConfig::PlatformCenterIP));
    list.append("<br />");

    list.append("<p id='p'>平台中心端口</p>");
    list.append(QString("<input id='txt' type='text' name='PlatformCenterPort' value='%1'>").arg(GlobalConfig::PlatformCenterPort));
    list.append("<br />");

    list.append("<p id='p'>人脸比对服务器IP</p>");
    list.append(QString("<input id='txt' type='text' name='FaceServerIP' value='%1'>").arg(GlobalConfig::FaceServerIP));
    list.append("<br />");

    list.append("<p id='p'>人脸比对服务器端口</p>");
    list.append(QString("<input id='txt' type='text' name='FaceServerPort' value='%1'>").arg(GlobalConfig::FaceServerPort));
    list.append("<br />");

    list.append("<p id='p'>AgentID</p>");
    list.append(QString("<input id='txt' type='text' name='AgentID' value='%1'>").arg(GlobalConfig::AgentID));
    list.append("<br />");

    list.append("<input id='btn' type='submit' value='发送'>");
    list.append("</form>");
    list.append("</body>");

    list.append("<style>");
    list.append("*{font-size:30px;}");
    list.append("#form{padding:0px 20px 20px 20px;}");
    list.append("#p{margin-bottom:10px;}");
    list.append("#txt{outline:none;padding:10px;font-size:40px;color:green;height:95px;width:100%;"
                "border-style:solid;border-color:green;border-width:2px;text-align:left;"
                "border-radius:8px;-moz-border-radius:8px; -webkit-border-radius:8px;}");
    list.append("#btn{margin-top:20px;height:95px;width:100%;}");

    list.append("</style>");

    list.append("</html>");
    QString body = list.join("");

    resp->end(body.toUtf8());
}

void HttpServer::data(const QByteArray &data)
{
    qDebug() << "receive data" << data;
    emit receiveData(data);
}
