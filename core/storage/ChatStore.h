// SPDX-License-Identifier: MIT
#pragma once
#include "core/profile/ProfileStore.h"
#include <QJsonArray>

struct ChatState {
    QString owner;
    QJsonArray requests;
    QJsonArray messages;
};

// Call only on the Tox worker. No plaintext message bodies are written to disk.
class ChatStore {
public:
    explicit ChatStore(Profile profile) : profile_(std::move(profile)) {}
    bool load(const QString &owner, ChatState &state, QString &error) const;
    bool save(const ChatState &state, QString &error) const;
    QString path() const { return profile_.directory + QStringLiteral("/history.vault"); }
private:
    Profile profile_;
};
