/****************************************************************************
** Meta object code from reading C++ file 'AIA.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.13.0)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../inc/AIA.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'AIA.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.13.0. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_AIA_t {
    QByteArrayData data[11];
    char stringdata0[124];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_AIA_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_AIA_t qt_meta_stringdata_AIA = {
    {
QT_MOC_LITERAL(0, 0, 3), // "AIA"
QT_MOC_LITERAL(1, 4, 9), // "sigCmdRan"
QT_MOC_LITERAL(2, 14, 0), // ""
QT_MOC_LITERAL(3, 15, 3), // "cmd"
QT_MOC_LITERAL(4, 19, 21), // "on_auto_start_changed"
QT_MOC_LITERAL(5, 41, 5), // "state"
QT_MOC_LITERAL(6, 47, 8), // "onCmdRan"
QT_MOC_LITERAL(7, 56, 15), // "onTrayActivated"
QT_MOC_LITERAL(8, 72, 33), // "QSystemTrayIcon::ActivationRe..."
QT_MOC_LITERAL(9, 106, 6), // "reason"
QT_MOC_LITERAL(10, 113, 10) // "onTrayQuit"

    },
    "AIA\0sigCmdRan\0\0cmd\0on_auto_start_changed\0"
    "state\0onCmdRan\0onTrayActivated\0"
    "QSystemTrayIcon::ActivationReason\0"
    "reason\0onTrayQuit"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_AIA[] = {

 // content:
       8,       // revision
       0,       // classname
       0,    0, // classinfo
       5,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       1,       // signalCount

 // signals: name, argc, parameters, tag, flags
       1,    1,   39,    2, 0x06 /* Public */,

 // slots: name, argc, parameters, tag, flags
       4,    1,   42,    2, 0x08 /* Private */,
       6,    1,   45,    2, 0x08 /* Private */,
       7,    1,   48,    2, 0x08 /* Private */,
      10,    0,   51,    2, 0x08 /* Private */,

 // signals: parameters
    QMetaType::Void, QMetaType::QString,    3,

 // slots: parameters
    QMetaType::Void, QMetaType::Int,    5,
    QMetaType::Void, QMetaType::QString,    3,
    QMetaType::Void, 0x80000000 | 8,    9,
    QMetaType::Void,

       0        // eod
};

void AIA::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<AIA *>(_o);
        Q_UNUSED(_t)
        switch (_id) {
        case 0: _t->sigCmdRan((*reinterpret_cast< const QString(*)>(_a[1]))); break;
        case 1: _t->on_auto_start_changed((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 2: _t->onCmdRan((*reinterpret_cast< const QString(*)>(_a[1]))); break;
        case 3: _t->onTrayActivated((*reinterpret_cast< QSystemTrayIcon::ActivationReason(*)>(_a[1]))); break;
        case 4: _t->onTrayQuit(); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (AIA::*)(const QString & );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&AIA::sigCmdRan)) {
                *result = 0;
                return;
            }
        }
    }
}

QT_INIT_METAOBJECT const QMetaObject AIA::staticMetaObject = { {
    &QMainWindow::staticMetaObject,
    qt_meta_stringdata_AIA.data,
    qt_meta_data_AIA,
    qt_static_metacall,
    nullptr,
    nullptr
} };


const QMetaObject *AIA::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *AIA::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_AIA.stringdata0))
        return static_cast<void*>(this);
    if (!strcmp(_clname, "CCPPObject"))
        return static_cast< CCPPObject*>(this);
    return QMainWindow::qt_metacast(_clname);
}

int AIA::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QMainWindow::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 5)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 5;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 5)
            *reinterpret_cast<int*>(_a[0]) = -1;
        _id -= 5;
    }
    return _id;
}

// SIGNAL 0
void AIA::sigCmdRan(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
