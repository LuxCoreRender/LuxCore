/****************************************************************************
** Meta object code from reading C++ file 'submitdialog.h'
**
** Created: Sun Jan 1 12:38:49 2012
**      by: The Qt Meta Object Compiler version 62 (Qt 4.7.0)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "submitdialog.h"
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'submitdialog.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 62
#error "This file was generated using the moc from 4.7.0. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
static const uint qt_meta_data_SubmitDialog[] = {

 // content:
       5,       // revision
       0,       // classname
       0,    0, // classinfo
       3,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // slots: signature, parameters, type, tag, flags
      14,   13,   13,   13, 0x08,
      30,   13,   13,   13, 0x08,
      51,   45,   13,   13, 0x08,

       0        // eod
};

static const char qt_meta_stringdata_SubmitDialog[] = {
    "SubmitDialog\0\0genericButton()\0"
    "httpFinished()\0error\0"
    "httpError(QNetworkReply::NetworkError)\0"
};

const QMetaObject SubmitDialog::staticMetaObject = {
    { &QDialog::staticMetaObject, qt_meta_stringdata_SubmitDialog,
      qt_meta_data_SubmitDialog, 0 }
};

#ifdef Q_NO_DATA_RELOCATION
const QMetaObject &SubmitDialog::getStaticMetaObject() { return staticMetaObject; }
#endif //Q_NO_DATA_RELOCATION

const QMetaObject *SubmitDialog::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->metaObject : &staticMetaObject;
}

void *SubmitDialog::qt_metacast(const char *_clname)
{
    if (!_clname) return 0;
    if (!strcmp(_clname, qt_meta_stringdata_SubmitDialog))
        return static_cast<void*>(const_cast< SubmitDialog*>(this));
    return QDialog::qt_metacast(_clname);
}

int SubmitDialog::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QDialog::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: genericButton(); break;
        case 1: httpFinished(); break;
        case 2: httpError((*reinterpret_cast< QNetworkReply::NetworkError(*)>(_a[1]))); break;
        default: ;
        }
        _id -= 3;
    }
    return _id;
}
QT_END_MOC_NAMESPACE
