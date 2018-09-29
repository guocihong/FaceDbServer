#ifndef MAINFORM_H
#define MAINFORM_H

#include "globalconfig.h"

namespace Ui {
class MainForm;
}

class MainForm : public QWidget
{
    Q_OBJECT

public:
    explicit MainForm(QWidget *parent = 0);
    ~MainForm();

private slots:

private:
    Ui::MainForm *ui;
};

#endif // MAINFORM_H
