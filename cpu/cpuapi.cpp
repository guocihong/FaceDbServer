#include "cpuapi.h"

CpuApi *CpuApi::instance = NULL;

CpuApi::CpuApi(QObject *parent) : QObject(parent)
{

}

void CpuApi::init()
{
    PrintCpuFreqTimer = new QTimer(this);
    PrintCpuFreqTimer->setInterval(5000);
    connect(PrintCpuFreqTimer,SIGNAL(timeout()),this,SLOT(slotPrintCpuFreq()));
    PrintCpuFreqTimer->start();

    second = 0;
    minute = 0;
    hour = 0;
    day = 0;
}

void CpuApi::slotPrintCpuFreq()
{
    second += 5;

    if (second == 60) {
        minute += 1;
        second = 0;
    }

    if (minute == 60) {
        hour += 1;
        minute = 0;
    }

    if (hour == 24) {
        day += 1;
        hour = 0;
    }

    CommonSetting::print(QString("已运行%1天%2时%3分%4秒").arg(day).arg(hour).arg(minute).arg(second));
}
