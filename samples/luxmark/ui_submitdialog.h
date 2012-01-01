/********************************************************************************
** Form generated from reading UI file 'submitdialog.ui'
**
** Created: Sun Jan 1 12:43:37 2012
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
#include <QtGui/QFormLayout>
#include <QtGui/QHBoxLayout>
#include <QtGui/QHeaderView>
#include <QtGui/QLabel>
#include <QtGui/QLineEdit>
#include <QtGui/QPushButton>
#include <QtGui/QTextEdit>
#include <QtGui/QWidget>

QT_BEGIN_NAMESPACE

class Ui_SubmitDialog
{
public:
    QFormLayout *formLayout;
    QLabel *noteLabel;
    QLabel *accountNameLabel;
    QLineEdit *pwdEdit;
    QLabel *accountPwdLabel;
    QLineEdit *nameEdit;
    QLabel *label;
    QTextEdit *resultText;
    QWidget *widget;
    QHBoxLayout *horizontalLayout;
    QPushButton *genericButton;

    void setupUi(QDialog *SubmitDialog)
    {
        if (SubmitDialog->objectName().isEmpty())
            SubmitDialog->setObjectName(QString::fromUtf8("SubmitDialog"));
        SubmitDialog->resize(480, 300);
        QIcon icon;
        icon.addFile(QString::fromUtf8(":/images/resources/SLG_luxball_sppm_small.png"), QSize(), QIcon::Normal, QIcon::Off);
        SubmitDialog->setWindowIcon(icon);
        SubmitDialog->setModal(true);
        formLayout = new QFormLayout(SubmitDialog);
        formLayout->setObjectName(QString::fromUtf8("formLayout"));
        formLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        formLayout->setLabelAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
        formLayout->setFormAlignment(Qt::AlignHCenter|Qt::AlignTop);
        noteLabel = new QLabel(SubmitDialog);
        noteLabel->setObjectName(QString::fromUtf8("noteLabel"));
        noteLabel->setTextFormat(Qt::RichText);
        noteLabel->setAlignment(Qt::AlignLeading|Qt::AlignLeft|Qt::AlignTop);
        noteLabel->setWordWrap(true);
        noteLabel->setOpenExternalLinks(true);

        formLayout->setWidget(0, QFormLayout::SpanningRole, noteLabel);

        accountNameLabel = new QLabel(SubmitDialog);
        accountNameLabel->setObjectName(QString::fromUtf8("accountNameLabel"));
        QSizePolicy sizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(accountNameLabel->sizePolicy().hasHeightForWidth());
        accountNameLabel->setSizePolicy(sizePolicy);
        accountNameLabel->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);

        formLayout->setWidget(3, QFormLayout::LabelRole, accountNameLabel);

        pwdEdit = new QLineEdit(SubmitDialog);
        pwdEdit->setObjectName(QString::fromUtf8("pwdEdit"));
        QSizePolicy sizePolicy1(QSizePolicy::Maximum, QSizePolicy::Preferred);
        sizePolicy1.setHorizontalStretch(0);
        sizePolicy1.setVerticalStretch(0);
        sizePolicy1.setHeightForWidth(pwdEdit->sizePolicy().hasHeightForWidth());
        pwdEdit->setSizePolicy(sizePolicy1);

        formLayout->setWidget(3, QFormLayout::FieldRole, pwdEdit);

        accountPwdLabel = new QLabel(SubmitDialog);
        accountPwdLabel->setObjectName(QString::fromUtf8("accountPwdLabel"));

        formLayout->setWidget(4, QFormLayout::LabelRole, accountPwdLabel);

        nameEdit = new QLineEdit(SubmitDialog);
        nameEdit->setObjectName(QString::fromUtf8("nameEdit"));
        sizePolicy1.setHeightForWidth(nameEdit->sizePolicy().hasHeightForWidth());
        nameEdit->setSizePolicy(sizePolicy1);
        nameEdit->setEchoMode(QLineEdit::Password);

        formLayout->setWidget(4, QFormLayout::FieldRole, nameEdit);

        label = new QLabel(SubmitDialog);
        label->setObjectName(QString::fromUtf8("label"));
        label->setAlignment(Qt::AlignCenter);

        formLayout->setWidget(5, QFormLayout::SpanningRole, label);

        resultText = new QTextEdit(SubmitDialog);
        resultText->setObjectName(QString::fromUtf8("resultText"));
        resultText->setReadOnly(true);

        formLayout->setWidget(8, QFormLayout::SpanningRole, resultText);

        widget = new QWidget(SubmitDialog);
        widget->setObjectName(QString::fromUtf8("widget"));
        sizePolicy.setHeightForWidth(widget->sizePolicy().hasHeightForWidth());
        widget->setSizePolicy(sizePolicy);
        horizontalLayout = new QHBoxLayout(widget);
        horizontalLayout->setContentsMargins(0, 0, 0, 0);
        horizontalLayout->setObjectName(QString::fromUtf8("horizontalLayout"));
        genericButton = new QPushButton(widget);
        genericButton->setObjectName(QString::fromUtf8("genericButton"));
        QSizePolicy sizePolicy2(QSizePolicy::Fixed, QSizePolicy::Fixed);
        sizePolicy2.setHorizontalStretch(0);
        sizePolicy2.setVerticalStretch(0);
        sizePolicy2.setHeightForWidth(genericButton->sizePolicy().hasHeightForWidth());
        genericButton->setSizePolicy(sizePolicy2);

        horizontalLayout->addWidget(genericButton);


        formLayout->setWidget(9, QFormLayout::SpanningRole, widget);


        retranslateUi(SubmitDialog);
        QObject::connect(genericButton, SIGNAL(released()), SubmitDialog, SLOT(genericButton()));

        QMetaObject::connectSlotsByName(SubmitDialog);
    } // setupUi

    void retranslateUi(QDialog *SubmitDialog)
    {
        SubmitDialog->setWindowTitle(QApplication::translate("SubmitDialog", "Dialog", 0, QApplication::UnicodeUTF8));
        noteLabel->setText(QApplication::translate("SubmitDialog", "<b>Note:</b> submiting a result to <a href=\"http://www.luxrender.net/luxmark\">www.luxrender.net/luxmark</a> requires a valid forum account. If you don't have an account, you can create one <a href=\"http://www.luxrender.net/forum/ucp.php?mode=register\">here<a/>.", 0, QApplication::UnicodeUTF8));
        accountNameLabel->setText(QApplication::translate("SubmitDialog", "Account name:", 0, QApplication::UnicodeUTF8));
        accountPwdLabel->setText(QApplication::translate("SubmitDialog", "Account password:", 0, QApplication::UnicodeUTF8));
        label->setText(QApplication::translate("SubmitDialog", "Submission log:", 0, QApplication::UnicodeUTF8));
        genericButton->setText(QApplication::translate("SubmitDialog", "&Cancel", 0, QApplication::UnicodeUTF8));
    } // retranslateUi

};

namespace Ui {
    class SubmitDialog: public Ui_SubmitDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_SUBMITDIALOG_H
