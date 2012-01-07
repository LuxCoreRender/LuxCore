/********************************************************************************
** Form generated from reading UI file 'resultdialog.ui'
**
** Created: Sat Jan 7 18:01:28 2012
**      by: Qt User Interface Compiler version 4.7.0
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_RESULTDIALOG_H
#define UI_RESULTDIALOG_H

#include <QtCore/QVariant>
#include <QtGui/QAction>
#include <QtGui/QApplication>
#include <QtGui/QButtonGroup>
#include <QtGui/QDialog>
#include <QtGui/QFormLayout>
#include <QtGui/QGridLayout>
#include <QtGui/QHeaderView>
#include <QtGui/QLCDNumber>
#include <QtGui/QLabel>
#include <QtGui/QListView>
#include <QtGui/QPushButton>
#include <QtGui/QWidget>

QT_BEGIN_NAMESPACE

class Ui_ResultDialog
{
public:
    QFormLayout *formLayout;
    QLabel *modeLabelDesc;
    QLabel *modeLabel;
    QLabel *sceneLabelDesc;
    QLabel *sceneLabel;
    QLabel *deviceLabel;
    QListView *deviceListView;
    QLabel *sampleSecLabel;
    QLCDNumber *resultLCD;
    QWidget *widget;
    QGridLayout *gridLayout;
    QPushButton *okButton;
    QPushButton *submitButton;

    void setupUi(QDialog *ResultDialog)
    {
        if (ResultDialog->objectName().isEmpty())
            ResultDialog->setObjectName(QString::fromUtf8("ResultDialog"));
        ResultDialog->resize(480, 400);
        QIcon icon;
        icon.addFile(QString::fromUtf8(":/images/resources/SLG_luxball_sppm_small.png"), QSize(), QIcon::Normal, QIcon::Off);
        ResultDialog->setWindowIcon(icon);
        ResultDialog->setModal(true);
        formLayout = new QFormLayout(ResultDialog);
        formLayout->setObjectName(QString::fromUtf8("formLayout"));
        formLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        formLayout->setRowWrapPolicy(QFormLayout::DontWrapRows);
        formLayout->setLabelAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
        formLayout->setFormAlignment(Qt::AlignHCenter|Qt::AlignTop);
        formLayout->setHorizontalSpacing(9);
        formLayout->setVerticalSpacing(9);
        modeLabelDesc = new QLabel(ResultDialog);
        modeLabelDesc->setObjectName(QString::fromUtf8("modeLabelDesc"));
        QSizePolicy sizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(modeLabelDesc->sizePolicy().hasHeightForWidth());
        modeLabelDesc->setSizePolicy(sizePolicy);
        modeLabelDesc->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);

        formLayout->setWidget(0, QFormLayout::LabelRole, modeLabelDesc);

        modeLabel = new QLabel(ResultDialog);
        modeLabel->setObjectName(QString::fromUtf8("modeLabel"));

        formLayout->setWidget(0, QFormLayout::FieldRole, modeLabel);

        sceneLabelDesc = new QLabel(ResultDialog);
        sceneLabelDesc->setObjectName(QString::fromUtf8("sceneLabelDesc"));
        sceneLabelDesc->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);

        formLayout->setWidget(1, QFormLayout::LabelRole, sceneLabelDesc);

        sceneLabel = new QLabel(ResultDialog);
        sceneLabel->setObjectName(QString::fromUtf8("sceneLabel"));

        formLayout->setWidget(1, QFormLayout::FieldRole, sceneLabel);

        deviceLabel = new QLabel(ResultDialog);
        deviceLabel->setObjectName(QString::fromUtf8("deviceLabel"));
        deviceLabel->setAlignment(Qt::AlignCenter);

        formLayout->setWidget(2, QFormLayout::SpanningRole, deviceLabel);

        deviceListView = new QListView(ResultDialog);
        deviceListView->setObjectName(QString::fromUtf8("deviceListView"));
        QSizePolicy sizePolicy1(QSizePolicy::Expanding, QSizePolicy::Expanding);
        sizePolicy1.setHorizontalStretch(0);
        sizePolicy1.setVerticalStretch(2);
        sizePolicy1.setHeightForWidth(deviceListView->sizePolicy().hasHeightForWidth());
        deviceListView->setSizePolicy(sizePolicy1);
        deviceListView->setEditTriggers(QAbstractItemView::NoEditTriggers);

        formLayout->setWidget(3, QFormLayout::SpanningRole, deviceListView);

        sampleSecLabel = new QLabel(ResultDialog);
        sampleSecLabel->setObjectName(QString::fromUtf8("sampleSecLabel"));
        sampleSecLabel->setAlignment(Qt::AlignCenter);

        formLayout->setWidget(4, QFormLayout::SpanningRole, sampleSecLabel);

        resultLCD = new QLCDNumber(ResultDialog);
        resultLCD->setObjectName(QString::fromUtf8("resultLCD"));
        QSizePolicy sizePolicy2(QSizePolicy::Expanding, QSizePolicy::Expanding);
        sizePolicy2.setHorizontalStretch(0);
        sizePolicy2.setVerticalStretch(1);
        sizePolicy2.setHeightForWidth(resultLCD->sizePolicy().hasHeightForWidth());
        resultLCD->setSizePolicy(sizePolicy2);
        resultLCD->setNumDigits(8);
        resultLCD->setProperty("value", QVariant(123456));

        formLayout->setWidget(5, QFormLayout::SpanningRole, resultLCD);

        widget = new QWidget(ResultDialog);
        widget->setObjectName(QString::fromUtf8("widget"));
        gridLayout = new QGridLayout(widget);
        gridLayout->setContentsMargins(0, 0, 0, 0);
        gridLayout->setObjectName(QString::fromUtf8("gridLayout"));
        okButton = new QPushButton(widget);
        okButton->setObjectName(QString::fromUtf8("okButton"));
        QSizePolicy sizePolicy3(QSizePolicy::Fixed, QSizePolicy::Fixed);
        sizePolicy3.setHorizontalStretch(0);
        sizePolicy3.setVerticalStretch(0);
        sizePolicy3.setHeightForWidth(okButton->sizePolicy().hasHeightForWidth());
        okButton->setSizePolicy(sizePolicy3);

        gridLayout->addWidget(okButton, 0, 0, 1, 1);

        submitButton = new QPushButton(widget);
        submitButton->setObjectName(QString::fromUtf8("submitButton"));
        sizePolicy3.setHeightForWidth(submitButton->sizePolicy().hasHeightForWidth());
        submitButton->setSizePolicy(sizePolicy3);

        gridLayout->addWidget(submitButton, 0, 1, 1, 1);


        formLayout->setWidget(6, QFormLayout::SpanningRole, widget);


        retranslateUi(ResultDialog);
        QObject::connect(okButton, SIGNAL(released()), ResultDialog, SLOT(close()));
        QObject::connect(submitButton, SIGNAL(released()), ResultDialog, SLOT(submitResult()));

        QMetaObject::connectSlotsByName(ResultDialog);
    } // setupUi

    void retranslateUi(QDialog *ResultDialog)
    {
        ResultDialog->setWindowTitle(QApplication::translate("ResultDialog", "Dialog", 0, QApplication::UnicodeUTF8));
        modeLabelDesc->setText(QApplication::translate("ResultDialog", "Mode:", 0, QApplication::UnicodeUTF8));
        modeLabel->setText(QString());
        sceneLabelDesc->setText(QApplication::translate("ResultDialog", "Scene name:", 0, QApplication::UnicodeUTF8));
        sceneLabel->setText(QString());
        deviceLabel->setText(QApplication::translate("ResultDialog", "Devices:", 0, QApplication::UnicodeUTF8));
        sampleSecLabel->setText(QApplication::translate("ResultDialog", "Result:", 0, QApplication::UnicodeUTF8));
        okButton->setText(QApplication::translate("ResultDialog", "&Ok", 0, QApplication::UnicodeUTF8));
        submitButton->setText(QApplication::translate("ResultDialog", "&Submit result", 0, QApplication::UnicodeUTF8));
    } // retranslateUi

};

namespace Ui {
    class ResultDialog: public Ui_ResultDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_RESULTDIALOG_H
