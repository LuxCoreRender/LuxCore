/********************************************************************************
** Form generated from reading UI file 'resultdialog.ui'
**
** Created: Tue Dec 27 13:23:01 2011
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
    QLabel *sampleSecLabel;
    QLCDNumber *resultLCD;
    QWidget *widget;
    QGridLayout *gridLayout;
    QPushButton *okButton;

    void setupUi(QDialog *ResultDialog)
    {
        if (ResultDialog->objectName().isEmpty())
            ResultDialog->setObjectName(QString::fromUtf8("ResultDialog"));
        ResultDialog->resize(400, 200);
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

        sampleSecLabel = new QLabel(ResultDialog);
        sampleSecLabel->setObjectName(QString::fromUtf8("sampleSecLabel"));
        sampleSecLabel->setAlignment(Qt::AlignCenter);

        formLayout->setWidget(2, QFormLayout::SpanningRole, sampleSecLabel);

        resultLCD = new QLCDNumber(ResultDialog);
        resultLCD->setObjectName(QString::fromUtf8("resultLCD"));
        QSizePolicy sizePolicy1(QSizePolicy::Expanding, QSizePolicy::Expanding);
        sizePolicy1.setHorizontalStretch(0);
        sizePolicy1.setVerticalStretch(1);
        sizePolicy1.setHeightForWidth(resultLCD->sizePolicy().hasHeightForWidth());
        resultLCD->setSizePolicy(sizePolicy1);
        resultLCD->setNumDigits(8);
        resultLCD->setProperty("value", QVariant(123456));

        formLayout->setWidget(3, QFormLayout::SpanningRole, resultLCD);

        widget = new QWidget(ResultDialog);
        widget->setObjectName(QString::fromUtf8("widget"));
        gridLayout = new QGridLayout(widget);
        gridLayout->setContentsMargins(0, 0, 0, 0);
        gridLayout->setObjectName(QString::fromUtf8("gridLayout"));
        okButton = new QPushButton(widget);
        okButton->setObjectName(QString::fromUtf8("okButton"));
        QSizePolicy sizePolicy2(QSizePolicy::Fixed, QSizePolicy::Fixed);
        sizePolicy2.setHorizontalStretch(0);
        sizePolicy2.setVerticalStretch(0);
        sizePolicy2.setHeightForWidth(okButton->sizePolicy().hasHeightForWidth());
        okButton->setSizePolicy(sizePolicy2);

        gridLayout->addWidget(okButton, 0, 0, 1, 1);


        formLayout->setWidget(4, QFormLayout::SpanningRole, widget);


        retranslateUi(ResultDialog);
        QObject::connect(okButton, SIGNAL(released()), ResultDialog, SLOT(close()));

        QMetaObject::connectSlotsByName(ResultDialog);
    } // setupUi

    void retranslateUi(QDialog *ResultDialog)
    {
        ResultDialog->setWindowTitle(QApplication::translate("ResultDialog", "Dialog", 0, QApplication::UnicodeUTF8));
        modeLabelDesc->setText(QApplication::translate("ResultDialog", "Mode:", 0, QApplication::UnicodeUTF8));
        modeLabel->setText(QString());
        sceneLabelDesc->setText(QApplication::translate("ResultDialog", "Scene name:", 0, QApplication::UnicodeUTF8));
        sceneLabel->setText(QString());
        sampleSecLabel->setText(QApplication::translate("ResultDialog", "Result:", 0, QApplication::UnicodeUTF8));
        okButton->setText(QApplication::translate("ResultDialog", "&Ok", 0, QApplication::UnicodeUTF8));
    } // retranslateUi

};

namespace Ui {
    class ResultDialog: public Ui_ResultDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_RESULTDIALOG_H
