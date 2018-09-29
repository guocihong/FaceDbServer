#ifndef CPUAPI_H
#define CPUAPI_H

#include "globalconfig.h"

class CpuApi : public QObject
{
    Q_OBJECT
public:
    explicit CpuApi(QObject *parent = nullptr);

    static CpuApi *newInstance() {
        static QMutex mutex;

        if (!instance) {
            QMutexLocker locker(&mutex);

            if (!instance) {
                instance = new CpuApi();
            }
        }

        return instance;
    }

    void init();


public slots:
    void slotPrintCpuFreq();

private:
    static CpuApi *instance;

    //定时打印cpu的频率和温度
    QTimer *PrintCpuFreqTimer;

    //保存程序已经运行时间
    quint8 second;
    quint8 minute;
    quint8 hour;
    quint64 day;
};

#endif // CPUAPI_H
