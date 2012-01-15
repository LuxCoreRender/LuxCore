/********************************************************************************
** Form generated from reading UI file 'submitdialog.ui'
**
** Created: Sun Jan 15 22:19:16 2012
**      by: Qt User Interface Compiler version 4.7.0
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_SUBMITDIALOG_H
#define UI_SUBMITDIALOG_H

#include <QtCore/QVariant>
#include <QtGui/QAction>
#include <QtGui/QApplication>
#include <QtGui/QButtonGroup>
#include <QtGui/QDialog>
#include <QtGui/QGridLayout>
#include <QtGui/QHBoxLayout>
#include <QtGui/QHeaderView>
#include <QtGui/QLabel>
#include <QtGui/QLineEdit>
#include <QtGui/QPlainTextEdit>
#include <QtGui/QPushButton>
#include <QtGui/QSpacerItem>
#include <QtGui/QTextEdit>
#include <QtGui/QWidget>

QT_BEGIN_NAMESPACE

class Ui_SubmitDialog
{
public:
    QGridLayout *gridLayout;
    QLabel *noteLabel;
    QLabel *accountNameLabel;
    QLineEdit *nameEdit;
    QLabel *accountPwdLabel;
    QLineEdit *pwdEdit;
    QLabel *resultNoteLabel;
    QPlainTextEdit *noteTextEdit;
    QLabel *logLabel;
    QTextEdit *resultText;
    QWidget *widget;
    QHBoxLayout *horizontalLayout;
    QPushButton *genericButton;
    QSpacerItem *horizontalSpacer;

    void setupUi(QDialog *SubmitDialog)
    {
        if (SubmitDialog->objectName().isEmpty())
            SubmitDialog->setObjectName(QString::fromUtf8("SubmitDialog"));
        SubmitDialog->resize(600, 480);
        QIcon icon;
        icon.addFile(QString::fromUtf8(":/images/resources/SLG_luxball_sppm_small.png"), QSize(), QIcon::Normal, QIcon::Off);
        SubmitDialog->setWindowIcon(icon);
        SubmitDialog->setModal(true);
        gridLayout = new QGridLayout(SubmitDialog);
        gridLayout->setObjectName(QString::fromUtf8("gridLayout"));
        noteLabel = new QLabel(SubmitDialog);
        noteLabel->setObjectName(QString::fromUtf8("noteLabel"));
        QSizePolicy sizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        sizePolicy.setHorizontalStretch(1);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(noteLabel->sizePolicy().hasHeightForWidth());
        noteLabel->setSizePolicy(sizePolicy);
        noteLabel->setTextFormat(Qt::RichText);
        noteLabel->setAlignment(Qt::AlignLeading|Qt::AlignLeft|Qt::AlignTop);
        noteLabel->setWordWrap(true);
        noteLabel->setIndent(-1);
        noteLabel->setOpenExternalLinks(true);

        gridLayout->addWidget(noteLabel, 0, 0, 1, 4);

        accountNameLabel = new QLabel(SubmitDialog);
        accountNameLabel->setObjectName(QString::fromUtf8("accountNameLabel"));
        accountNameLabel->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);

        gridLayout->addWidget(accountNameLabel, 1, 0, 1, 1);

        nameEdit = new QLineEdit(SubmitDialog);
        nameEdit->setObjectName(QString::fromUtf8("nameEdit"));
        QSizePolicy sizePolicy1(QSizePolicy::Expanding, QSizePolicy::Preferred);
        sizePolicy1.setHorizontalStretch(2);
        sizePolicy1.setVerticalStretch(0);
        sizePolicy1.setHeightForWidth(nameEdit->sizePolicy().hasHeightForWidth());
        nameEdit->setSizePolicy(sizePolicy1);
        nameEdit->setEchoMode(QLineEdit::Normal);

        gridLayout->addWidget(nameEdit, 1, 1, 1, 1);

        accountPwdLabel = new QLabel(SubmitDialog);
        accountPwdLabel->setObjectName(QString::fromUtf8("accountPwdLabel"));
        accountPwdLabel->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);

        gridLayout->addWidget(accountPwdLabel, 2, 0, 1, 1);

        pwdEdit = new QLineEdit(SubmitDialog);
        pwdEdit->setObjectName(QString::fromUtf8("pwdEdit"));
        sizePolicy1.setHeightForWidth(pwdEdit->sizePolicy().hasHeightForWidth());
        pwdEdit->setSizePolicy(sizePolicy1);
        pwdEdit->setEchoMode(QLineEdit::Password);

        gridLayout->addWidget(pwdEdit, 2, 1, 1, 1);

        resultNoteLabel = new QLabel(SubmitDialog);
        resultNoteLabel->setObjectName(QString::fromUtf8("resultNoteLabel"));
        resultNoteLabel->setAlignment(Qt::AlignRight|Qt::AlignTop|Qt::AlignTrailing);

        gridLayout->addWidget(resultNoteLabel, 3, 0, 1, 1);

        noteTextEdit = new QPlainTextEdit(SubmitDialog);
        noteTextEdit->setObjectName(QString::fromUtf8("noteTextEdit"));
        QSizePolicy sizePolicy2(QSizePolicy::Expanding, QSizePolicy::Expanding);
        sizePolicy2.setHorizontalStretch(0);
        sizePolicy2.setVerticalStretch(1);
        sizePolicy2.setHeightForWidth(noteTextEdit->sizePolicy().hasHeightForWidth());
        noteTextEdit->setSizePolicy(sizePolicy2);

        gridLayout->addWidget(noteTextEdit, 3, 1, 1, 3);

        logLabel = new QLabel(SubmitDialog);
        logLabel->setObjectName(QString::fromUtf8("logLabel"));
        sizePolicy.setHeightForWidth(logLabel->sizePolicy().hasHeightForWidth());
        logLabel->setSizePolicy(sizePolicy);
        logLabel->setAlignment(Qt::AlignCenter);

        gridLayout->addWidget(logLabel, 4, 0, 1, 4);

        resultText = new QTextEdit(SubmitDialog);
        resultText->setObjectName(QString::fromUtf8("resultText"));
        QSizePolicy sizePolicy3(QSizePolicy::Expanding, QSizePolicy::Expanding);
        sizePolicy3.setHorizontalStretch(1);
        sizePolicy3.setVerticalStretch(0);
        sizePolicy3.setHeightForWidth(resultText->sizePolicy().hasHeightForWidth());
        resultText->setSizePolicy(sizePolicy3);
        resultText->setLineWrapMode(QTextEdit::NoWrap);
        resultText->setReadOnly(true);

        gridLayout->addWidget(resultText, 5, 0, 1, 4);

        widget = new QWidget(SubmitDialog);
        widget->setObjectName(QString::fromUtf8("widget"));
        sizePolicy.setHeightForWidth(widget->sizePolicy().hasHeightForWidth());
        widget->setSizePolicy(sizePolicy);
        horizontalLayout = new QHBoxLayout(widget);
        horizontalLayout->setContentsMargins(0, 0, 0, 0);
        horizontalLayout->setObjectName(QString::fromUtf8("horizontalLayout"));
        genericButton = new QPushButton(widget);
        genericButton->setObjectName(QString::fromUtf8("genericButton"));
        QSizePolicy sizePolicy4(QSizePolicy::Fixed, QSizePolicy::Fixed);
        sizePolicy4.setHorizontalStretch(0);
        sizePolicy4.setVerticalStretch(0);
        sizePolicy4.setHeightForWidth(genericButton->sizePolicy().hasHeightForWidth());
        genericButton->setSizePolicy(sizePolicy4);

        horizontalLayout->addWidget(genericButton);


        gridLayout->addWidget(widget, 6, 0, 1, 4);

        horizontalSpacer = new QSpacerItem(60, 20, QSizePolicy::Minimum, QSizePolicy::Minimum);

        gridLayout->addItem(horizontalSpacer, 1, 2, 1, 1);


        retranslateUi(SubmitDialog);
        QObject::connect(genericButton, SIGNAL(released()), SubmitDialog, SLOT(genericButton()));

        QMetaObject::connectSlotsByName(SubmitDialog);
    } // setupUi

    void retranslateUi(QDialog *SubmitDialog)
    {
        SubmitDialog->setWindowTitle(QApplication::translate("SubmitDialog", "Dialog", 0, QApplication::UnicodeUTF8));
        noteLabel->setText(QApplication::translate("SubmitDialog", "<b>Note:</b> submiting a result to <a href=\"http://www.luxrender.net/luxmark\">www.luxrender.net/luxmark</a> requires a valid forum account. If you don't have an account, you can create one <a href=\"http://www.luxrender.net/forum/ucp.php?mode=register\">here</a>. Account names are case sensitive.", 0, QApplication::UnicodeUTF8));
        accountNameLabel->setText(QApplication::translate("SubmitDialog", "Account name:", 0, QApplication::UnicodeUTF8));
        accountPwdLabel->setText(QApplication::translate("SubmitDialog", "Account password:", 0, QApplication::UnicodeUTF8));
        resultNoteLabel->setText(QApplication::translate("SubmitDialog", "Result note:", 0, QApplication::UnicodeUTF8));
        logLabel->setText(QApplication::translate("SubmitDialog", "Submission log:", 0, QApplication::UnicodeUTF8));
        genericButton->setText(QApplication::translate("SubmitDialog", "&Cancel", 0, QApplication::UnicodeUTF8));
    } // retranslateUi

};

namespace Ui {
    class SubmitDialog: public Ui_SubmitDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_SUBMITDIALOG_H
