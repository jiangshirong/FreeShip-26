// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QCryptographicHash>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wincrypt.h>
#endif
namespace fs {
inline QByteArray snapshotSha256(QByteArrayView data){
#ifdef Q_OS_WIN
    // The system SHA-256 provider uses the available CPU instructions.
    BYTE digest[32];DWORD size=sizeof(digest);
    if(data.size()<=0xffffffffLL && CryptHashCertificate2(L"SHA256",0,nullptr,
       reinterpret_cast<const BYTE*>(data.data()),DWORD(data.size()),digest,&size) && size==32)
        return QByteArray(reinterpret_cast<const char*>(digest),32).toHex();
#endif
    return QCryptographicHash::hash(data,QCryptographicHash::Sha256).toHex();
}
}
