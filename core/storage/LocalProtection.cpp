// SPDX-License-Identifier: MIT
#include "LocalProtection.h"
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wincrypt.h>
#include <dpapi.h>
#endif

namespace {
const QByteArray magic("PARLEYDP1");
constexpr qsizetype limit = 32 * 1024 * 1024;
}

bool LocalProtection::protect(const QByteArray &plain, QByteArray &encrypted, QString &error) {
    encrypted.clear();
    if (plain.isEmpty() || plain.size() > limit) { error = QStringLiteral("Invalid protected data size"); return false; }
#ifdef Q_OS_WIN
    DATA_BLOB input{static_cast<DWORD>(plain.size()), reinterpret_cast<BYTE *>(const_cast<char *>(plain.constData()))};
    DATA_BLOB output{};
    if (!CryptProtectData(&input, L"Parley local identity", nullptr, nullptr, nullptr,
                          CRYPTPROTECT_UI_FORBIDDEN, &output)) {
        error = QStringLiteral("Windows identity protection failed"); return false;
    }
    encrypted = magic + QByteArray(reinterpret_cast<const char *>(output.pbData), output.cbData);
    SecureZeroMemory(output.pbData, output.cbData); LocalFree(output.pbData);
    return true;
#else
    error = QStringLiteral("Windows user-bound protection is unavailable on this platform");
    return false;
#endif
}

bool LocalProtection::unprotect(const QByteArray &encrypted, QByteArray &plain, QString &error) {
    plain.clear();
    if (!encrypted.startsWith(magic) || encrypted.size() <= magic.size() || encrypted.size() > limit + 4096) {
        error = QStringLiteral("Invalid protected data format; original file preserved"); return false;
    }
#ifdef Q_OS_WIN
    DATA_BLOB input{static_cast<DWORD>(encrypted.size() - magic.size()),
                    reinterpret_cast<BYTE *>(const_cast<char *>(encrypted.constData() + magic.size()))};
    DATA_BLOB output{};
    if (!CryptUnprotectData(&input, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &output)) {
        error = QStringLiteral("Windows cannot unlock this identity; original file preserved"); return false;
    }
    plain = QByteArray(reinterpret_cast<const char *>(output.pbData), output.cbData);
    SecureZeroMemory(output.pbData, output.cbData); LocalFree(output.pbData);
    return !plain.isEmpty();
#else
    error = QStringLiteral("Windows user-bound protection is unavailable on this platform");
    return false;
#endif
}
