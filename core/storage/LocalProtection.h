// SPDX-License-Identifier: MIT
#pragma once
#include <QByteArray>
#include <QString>

// Windows user-bound protection, reusable for future Windows history storage.
// Does not protect against other code executing as the same Windows user.
namespace LocalProtection {
bool protect(const QByteArray &plain, QByteArray &encrypted, QString &error);
bool unprotect(const QByteArray &encrypted, QByteArray &plain, QString &error);
}
