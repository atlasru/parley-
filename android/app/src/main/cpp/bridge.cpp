// SPDX-License-Identifier: GPL-3.0-or-later
#include <jni.h>
#include <toxcore/tox.h>
#include <toxcore/tox_options.h>
#include <toxcore/tox_private.h>
#include <toxencryptsave/toxencryptsave.h>
#include <sodium.h>
#include <cstdint>
#include <string>
#include <vector>
#include <memory>

namespace {
struct Event { int type; uint32_t friendNo; std::string detail; std::vector<uint8_t> data; };
struct Engine { Tox *tox; std::vector<Event> events; };
Engine *engine(jlong h) { return reinterpret_cast<Engine *>(h); }
void fail(JNIEnv *env, const char *operation, int code) {
    const auto message = std::string(operation) + " failed (" + std::to_string(code) + ")";
    env->ThrowNew(env->FindClass("java/lang/IllegalStateException"), message.c_str());
}
struct Bytes {
    std::vector<uint8_t> value;
    Bytes(JNIEnv *env, jbyteArray a) {
        if (a) { value.resize(env->GetArrayLength(a)); env->GetByteArrayRegion(a, 0, value.size(), reinterpret_cast<jbyte *>(value.data())); }
    }
    ~Bytes() { if (!value.empty()) sodium_memzero(value.data(), value.size()); }
};
jbyteArray bytes(JNIEnv *env, const uint8_t *p, size_t n) {
    auto out = env->NewByteArray(n);
    if (out && n) env->SetByteArrayRegion(out, 0, n, reinterpret_cast<const jbyte *>(p));
    return out;
}
std::string hex(const uint8_t *p, size_t n) {
    static const char digits[] = "0123456789ABCDEF";
    std::string s(n * 2, '0');
    for (size_t i = 0; i < n; ++i) { s[i*2] = digits[p[i] >> 4]; s[i*2+1] = digits[p[i] & 15]; }
    return s;
}
void request(Tox *, const uint8_t *key, const uint8_t *p, size_t n, void *u) {
    static_cast<Engine *>(u)->events.push_back({1, 0, hex(key, TOX_PUBLIC_KEY_SIZE), {p, p+n}});
}
void message(Tox *, uint32_t f, Tox_Message_Type type, const uint8_t *p, size_t n, void *u) {
    static_cast<Engine *>(u)->events.push_back({2, f, std::to_string(type), {p, p+n}});
}
void connection(Tox *, uint32_t f, Tox_Connection s, void *u) {
    static_cast<Engine *>(u)->events.push_back({3, f, std::to_string(s), {}});
}
void receipt(Tox *, uint32_t f, uint32_t id, void *u) {
    static_cast<Engine *>(u)->events.push_back({4, f, std::to_string(id), {}});
}
void friend_name(Tox *, uint32_t f, const uint8_t *p, size_t n, void *u) {
    static_cast<Engine *>(u)->events.push_back({5, f, "", {p, p+n}});
}
}
#define JNI(name) extern "C" JNIEXPORT
#define METHOD(name) Java_chat_parley_tox_NativeBridge_##name
JNI(create) jlong JNICALL METHOD(create)(JNIEnv *env, jobject, jbyteArray saved, jboolean local) {
    Bytes input(env, saved);
    Tox_Err_Options_New oe;
    std::unique_ptr<Tox_Options, decltype(&tox_options_free)> options(tox_options_new(&oe), tox_options_free);
    if (!options) { fail(env, "options", oe); return 0; }
    tox_options_set_ipv6_enabled(options.get(), !local);
    tox_options_set_local_discovery_enabled(options.get(), true);
    if (!input.value.empty()) {
        tox_options_set_savedata_type(options.get(), TOX_SAVEDATA_TYPE_TOX_SAVE);
        tox_options_set_savedata_data(options.get(), input.value.data(), input.value.size());
    } else if (saved != nullptr) { fail(env, "empty identity", 1); return 0; }
    Tox_Err_New error;
    auto tox = tox_new(options.get(), &error);
    if (!tox || error != TOX_ERR_NEW_OK) { if (tox) tox_kill(tox); fail(env, "identity", error); return 0; }
    auto e = new Engine{tox, {}};
    tox_callback_friend_request(tox, request);
    tox_callback_friend_message(tox, message);
    tox_callback_friend_connection_status(tox, connection);
    tox_callback_friend_read_receipt(tox, receipt);
    tox_callback_friend_name(tox, friend_name);
    return reinterpret_cast<jlong>(e);
}
JNI(close) void JNICALL METHOD(close)(JNIEnv *, jobject, jlong h) { auto e = engine(h); if (e) { tox_kill(e->tox); delete e; } }
JNI(savedata) jbyteArray JNICALL METHOD(savedata)(JNIEnv *env, jobject, jlong h) {
    auto t = engine(h)->tox; std::vector<uint8_t> v(tox_get_savedata_size(t)); tox_get_savedata(t, v.data());
    auto a = bytes(env, v.data(), v.size()); sodium_memzero(v.data(), v.size()); return a;
}
JNI(address) jstring JNICALL METHOD(address)(JNIEnv *env, jobject, jlong h) {
    uint8_t a[TOX_ADDRESS_SIZE]; tox_self_get_address(engine(h)->tox, a); return env->NewStringUTF(hex(a, sizeof a).c_str());
}
JNI(name) void JNICALL METHOD(name)(JNIEnv *env, jobject, jlong h, jbyteArray n) {
    Bytes b(env, n); Tox_Err_Set_Info er; if (!tox_self_set_name(engine(h)->tox, b.value.data(), b.value.size(), &er)) fail(env, "name", er);
}
JNI(iterate) jobjectArray JNICALL METHOD(iterate)(JNIEnv *env, jobject, jlong h) {
    auto e = engine(h); e->events.clear(); tox_iterate(e->tox, e);
    auto cls = env->FindClass("chat/parley/tox/NativeEvent");
    auto ctor = env->GetMethodID(cls, "<init>", "(IILjava/lang/String;[B)V");
    auto array = env->NewObjectArray(e->events.size(), cls, nullptr);
    for (size_t i = 0; i < e->events.size(); ++i) {
        const auto &v = e->events[i]; auto d = env->NewStringUTF(v.detail.c_str()); auto b = bytes(env, v.data.data(), v.data.size());
        auto o = env->NewObject(cls, ctor, v.type, static_cast<jint>(v.friendNo), d, b);
        env->SetObjectArrayElement(array, i, o); env->DeleteLocalRef(o); env->DeleteLocalRef(d); env->DeleteLocalRef(b);
    }
    return array;
}
JNI(interval) jint JNICALL METHOD(interval)(JNIEnv *, jobject, jlong h) { return tox_iteration_interval(engine(h)->tox); }
JNI(connection) jint JNICALL METHOD(connection)(JNIEnv *, jobject, jlong h) { return tox_self_get_connection_status(engine(h)->tox); }
JNI(bootstrap) jboolean JNICALL METHOD(bootstrap)(JNIEnv *env, jobject, jlong h, jstring host, jint port, jbyteArray key, jboolean relay) {
    Bytes b(env, key); if (b.value.size() != TOX_PUBLIC_KEY_SIZE || port < 1 || port > 65535) return false;
    const char *s = env->GetStringUTFChars(host, nullptr); Tox_Err_Bootstrap er;
    bool ok = relay ? tox_add_tcp_relay(engine(h)->tox, s, port, b.value.data(), &er) : tox_bootstrap(engine(h)->tox, s, port, b.value.data(), &er);
    env->ReleaseStringUTFChars(host, s); return ok;
}
JNI(friends) jintArray JNICALL METHOD(friends)(JNIEnv *env, jobject, jlong h) {
    auto t = engine(h)->tox; std::vector<uint32_t> v(tox_self_get_friend_list_size(t)); tox_self_get_friend_list(t, v.data());
    auto a = env->NewIntArray(v.size()); if (!v.empty()) env->SetIntArrayRegion(a, 0, v.size(), reinterpret_cast<const jint *>(v.data())); return a;
}
JNI(friendKey) jstring JNICALL METHOD(friendKey)(JNIEnv *env, jobject, jlong h, jint f) {
    uint8_t key[TOX_PUBLIC_KEY_SIZE]; Tox_Err_Friend_Get_Public_Key er;
    if (!tox_friend_get_public_key(engine(h)->tox, f, key, &er)) { fail(env, "friend key", er); return nullptr; }
    return env->NewStringUTF(hex(key, sizeof key).c_str());
}
JNI(friendName) jbyteArray JNICALL METHOD(friendName)(JNIEnv *env, jobject, jlong h, jint f) {
    auto t = engine(h)->tox; Tox_Err_Friend_Query er; auto n = tox_friend_get_name_size(t, f, &er);
    if (er != TOX_ERR_FRIEND_QUERY_OK) { fail(env, "friend name", er); return nullptr; }
    std::vector<uint8_t> v(n); tox_friend_get_name(t, f, v.data(), &er); return bytes(env, v.data(), v.size());
}
JNI(friendConnection) jint JNICALL METHOD(friendConnection)(JNIEnv *, jobject, jlong h, jint f) { return tox_friend_get_connection_status(engine(h)->tox, f, nullptr); }
JNI(add) jint JNICALL METHOD(add)(JNIEnv *env, jobject, jlong h, jbyteArray address, jbyteArray requestData) {
    Bytes a(env, address), r(env, requestData); if (a.value.size() != TOX_ADDRESS_SIZE) { fail(env, "address length", 1); return -1; }
    Tox_Err_Friend_Add er; auto f = tox_friend_add(engine(h)->tox, a.value.data(), r.value.data(), r.value.size(), &er);
    if (er != TOX_ERR_FRIEND_ADD_OK) { fail(env, "add friend", er); return -1; } return f;
}
JNI(accept) jint JNICALL METHOD(accept)(JNIEnv *env, jobject, jlong h, jbyteArray key) {
    Bytes k(env, key); if (k.value.size() != TOX_PUBLIC_KEY_SIZE) { fail(env, "key length", 1); return -1; }
    Tox_Err_Friend_Add er; auto f = tox_friend_add_norequest(engine(h)->tox, k.value.data(), &er);
    if (er != TOX_ERR_FRIEND_ADD_OK) { fail(env, "accept friend", er); return -1; } return f;
}
JNI(send) jlong JNICALL METHOD(send)(JNIEnv *env, jobject, jlong h, jint f, jbyteArray text) {
    Bytes b(env, text); Tox_Err_Friend_Send_Message er;
    auto id = tox_friend_send_message(engine(h)->tox, f, TOX_MESSAGE_TYPE_NORMAL, b.value.data(), b.value.size(), &er);
    if (er != TOX_ERR_FRIEND_SEND_MESSAGE_OK) { fail(env, "send", er); return -1; } return id;
}
JNI(exportEncrypted) jbyteArray JNICALL METHOD(exportEncrypted)(JNIEnv *env, jobject, jlong h, jbyteArray password) {
    Bytes pass(env, password); auto t = engine(h)->tox; std::vector<uint8_t> plain(tox_get_savedata_size(t)); tox_get_savedata(t, plain.data());
    std::vector<uint8_t> encrypted(plain.size() + TOX_PASS_ENCRYPTION_EXTRA_LENGTH); Tox_Err_Encryption er;
    bool ok = tox_pass_encrypt(plain.data(), plain.size(), pass.value.data(), pass.value.size(), encrypted.data(), &er);
    sodium_memzero(plain.data(), plain.size()); if (!ok) { fail(env, "export", er); return nullptr; } return bytes(env, encrypted.data(), encrypted.size());
}
JNI(decryptImport) jbyteArray JNICALL METHOD(decryptImport)(JNIEnv *env, jobject, jbyteArray data, jbyteArray password) {
    Bytes input(env, data), pass(env, password);
    if (input.value.size() < TOX_PASS_ENCRYPTION_EXTRA_LENGTH) { fail(env, "import length", 1); return nullptr; }
    if (!tox_is_data_encrypted(input.value.data())) return bytes(env, input.value.data(), input.value.size());
    std::vector<uint8_t> plain(input.value.size() - TOX_PASS_ENCRYPTION_EXTRA_LENGTH); Tox_Err_Decryption er;
    bool ok = tox_pass_decrypt(input.value.data(), input.value.size(), pass.value.data(), pass.value.size(), plain.data(), &er);
    auto out = ok ? bytes(env, plain.data(), plain.size()) : nullptr; sodium_memzero(plain.data(), plain.size());
    if (!ok) fail(env, "import password", er); return out;
}
JNI(udpPort) jint JNICALL METHOD(udpPort)(JNIEnv *, jobject, jlong h) { return tox_self_get_udp_port(engine(h)->tox, nullptr); }
JNI(dhtKey) jbyteArray JNICALL METHOD(dhtKey)(JNIEnv *env, jobject, jlong h) {
    uint8_t k[TOX_PUBLIC_KEY_SIZE]; tox_self_get_dht_id(engine(h)->tox, k); return bytes(env, k, sizeof k);
}
