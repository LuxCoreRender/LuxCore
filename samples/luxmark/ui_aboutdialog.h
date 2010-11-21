/********************************************************************************
** Form generated from reading UI file 'aboutdialog.ui'
**
** Created: Sun Nov 21 10:53:27 2010
**      by: Qt User Interface Compiler version 4.6.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_ABOUTDIALOG_H
#define UI_ABOUTDIALOG_H

#include <QtCore/QVariant>
#include <QtGui/QAction>
#include <QtGui/QApplication>
#include <QtGui/QButtonGroup>
#include <QtGui/QDialog>
#include <QtGui/QHeaderView>
#include <QtGui/QLabel>
#include <QtGui/QPushButton>
#include <QtGui/QTextBrowser>
#include <QtGui/QVBoxLayout>

QT_BEGIN_NAMESPACE

class Ui_AboutDialog
{
public:
    QVBoxLayout *verticalLayout;
    QLabel *AboutImage;
    QTextBrowser *AboutText;
    QPushButton *AboutOk;

    void setupUi(QDialog *AboutDialog)
    {
        if (AboutDialog->objectName().isEmpty())
            AboutDialog->setObjectName(QString::fromUtf8("AboutDialog"));
        AboutDialog->setWindowModality(Qt::ApplicationModal);
        AboutDialog->resize(480, 579);
        QSizePolicy sizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(AboutDialog->sizePolicy().hasHeightForWidth());
        AboutDialog->setSizePolicy(sizePolicy);
        AboutDialog->setModal(true);
        verticalLayout = new QVBoxLayout(AboutDialog);
        verticalLayout->setSpacing(0);
        verticalLayout->setContentsMargins(0, 0, 0, 0);
        verticalLayout->setObjectName(QString::fromUtf8("verticalLayout"));
        AboutImage = new QLabel(AboutDialog);
        AboutImage->setObjectName(QString::fromUtf8("AboutImage"));
        QSizePolicy sizePolicy1(QSizePolicy::Minimum, QSizePolicy::Minimum);
        sizePolicy1.setHorizontalStretch(0);
        sizePolicy1.setVerticalStretch(0);
        sizePolicy1.setHeightForWidth(AboutImage->sizePolicy().hasHeightForWidth());
        AboutImage->setSizePolicy(sizePolicy1);
        AboutImage->setTextFormat(Qt::RichText);
        AboutImage->setPixmap(QPixmap(QString::fromUtf8(":/images/resources/SLG_luxball_sppm.png")));
        AboutImage->setAlignment(Qt::AlignHCenter|Qt::AlignTop);

        verticalLayout->addWidget(AboutImage);

        AboutText = new QTextBrowser(AboutDialog);
        AboutText->setObjectName(QString::fromUtf8("AboutText"));

        verticalLayout->addWidget(AboutText);

        AboutOk = new QPushButton(AboutDialog);
        AboutOk->setObjectName(QString::fromUtf8("AboutOk"));

        verticalLayout->addWidget(AboutOk);


        retranslateUi(AboutDialog);
        QObject::connect(AboutOk, SIGNAL(clicked()), AboutDialog, SLOT(close()));

        QMetaObject::connectSlotsByName(AboutDialog);
    } // setupUi

    void retranslateUi(QDialog *AboutDialog)
    {
        AboutDialog->setWindowTitle(QApplication::translate("AboutDialog", "About", 0, QApplication::UnicodeUTF8));
        AboutImage->setText(QString());
        AboutText->setHtml(QApplication::translate("AboutDialog", "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0//EN\" \"http://www.w3.org/TR/REC-html40/strict.dtd\">\n"
"<html><head><meta name=\"qrichtext\" content=\"1\" /><style type=\"text/css\">\n"
"p, li { white-space: pre-wrap; }\n"
"</style></head><body style=\" font-family:'Sans'; font-size:10pt; font-weight:400; font-style:normal;\">\n"
"<p style=\" margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">LuxMark bla bla bla</p></body></html>", 0, QApplication::UnicodeUTF8));
        AboutOk->setText(QApplication::translate("AboutDialog", "Ok", 0, QApplication::UnicodeUTF8));
    } // retranslateUi

};

namespace Ui {
    class AboutDialog: public Ui_AboutDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_ABOUTDIALOG_H
