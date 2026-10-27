// SPDX-License-Identifier: GPL-3.0-or-later
package chat.parley.data

import android.content.Context
import android.os.SystemClock
import android.util.Log
import chat.parley.tox.Bootstrap
import chat.parley.tox.NativeBridge as Tox
import java.util.concurrent.Executors
import kotlinx.coroutines.*
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock

class Repository(context: Context) {
    private val worker = Executors.newSingleThreadExecutor { Thread(it, "parley-tox") }.asCoroutineDispatcher()
    private val scope = CoroutineScope(SupervisorJob() + worker)
    private val vault = Vault(context)
    private val mutable = MutableStateFlow(AppState())
    val state = mutable.asStateFlow()
    private var handle = 0L
    private var loop: Job? = null
    private val lifecycle = Mutex()
    private var storageFailed = false
    private var name = ""
    private val requests = mutableListOf<Request>()
    private val messages = mutableListOf<Message>()
    private val receipts = mutableMapOf<Pair<Int, Long>, String>()
    @Volatile var notifyIncoming: (() -> Unit)? = null
    init { scope.launch { mutable.update { it.copy(loaded = true, hasProfile = vault.exists()) } } }

    private fun persist() {
        check(handle != 0L)
        check(!storageFailed) { "Хранилище недоступно. Перезапустите приложение." }
        val saved = Tox.savedata(handle)
        try {
            val encoded = Snapshot(name, saved, requests.toList(), messages.toList()).encode()
            try { vault.write(encoded) }
            catch (_: Exception) {
                storageFailed = true
                loop?.cancel()
                error("Ошибка сохранения. Соединение остановлено, исходное хранилище сохранено.")
                throw IllegalStateException("Не удалось сохранить данные. Перезапустите приложение.")
            } finally { encoded.fill(0) }
        } finally { saved.fill(0) }
    }
    private fun publish() {
        val contacts = if (handle == 0L) emptyList() else Tox.friends(handle).map { f ->
            val key = Tox.friendKey(handle, f)
            Contact(key, f, Tox.friendName(handle, f).toString(Charsets.UTF_8).ifBlank { key.take(12) }, Tox.friendConnection(handle, f))
        }
        mutable.update { it.copy(loaded = true, hasProfile = vault.exists(), name = name,
            address = if (handle != 0L) Tox.address(handle) else it.address, running = loop?.isActive == true,
            connection = if (handle != 0L) Tox.connection(handle) else 0,
            contacts = contacts, requests = requests.toList(), messages = messages.toList()) }
    }
    private fun error(message: String) { mutable.update { it.copy(error = message) }; Log.w("Parley", "operation_failed") }
    fun clearError() { mutable.update { it.copy(error = null) } }
    private suspend fun <T> command(block: () -> T): T = withContext(worker) { block() }
    private fun open() {
        if (handle != 0L) return
        check(vault.exists()) { "Сначала создайте или импортируйте профиль" }
        val encoded = vault.read()
        val snapshot = try { Snapshot.decode(encoded) } finally { encoded.fill(0) }
        name = snapshot.name
        requests.clear(); requests.addAll(snapshot.requests)
        messages.clear(); messages.addAll(snapshot.messages.map {
            if (it.delivery == Delivery.SENT || it.delivery == Delivery.PENDING) it.copy(delivery = Delivery.UNKNOWN) else it
        })
        try { handle = Tox.create(snapshot.savedata) } finally { snapshot.savedata.fill(0) }
        receipts.clear()
    }
    suspend fun create(profileName: String) = command {
        check(!vault.exists() && handle == 0L) { "Профиль уже существует" }
        require(profileName.isNotBlank() && profileName.toByteArray().size <= 128) { "Имя: от 1 до 128 байт" }
        handle = Tox.create(null)
        try { name = profileName.trim(); Tox.name(handle, name.toByteArray()); persist(); publish() }
        finally { Tox.close(handle); handle = 0L }
    }
    suspend fun importProfile(data: ByteArray, password: String, profileName: String) = command {
        check(!vault.exists() && handle == 0L) { "Импорт доступен только до создания профиля" }
        require(data.size in 80..(16 * 1024 * 1024)) { "Недопустимый размер профиля" }
        val pass = password.toByteArray()
        val plain = try { Tox.decryptImport(data, pass) } finally { pass.fill(0); data.fill(0) }
        try {
            handle = Tox.create(plain)
            name = profileName.trim().ifBlank { "Parley" }
            require(name.toByteArray().size <= 128)
            Tox.name(handle, name.toByteArray()); persist(); publish()
        } finally { plain.fill(0); if (handle != 0L) Tox.close(handle); handle = 0L }
    }
    suspend fun exportProfile(password: String): ByteArray = command {
        require(password.length >= 8) { "Пароль экспорта: минимум 8 символов" }
        val temporary = handle == 0L
        open(); val pass = password.toByteArray()
        try { Tox.exportEncrypted(handle, pass) } finally {
            pass.fill(0)
            if (temporary) { Tox.close(handle); handle = 0L }
        }
    }
    fun start() { scope.launch { lifecycle.withLock {
        if (loop?.isActive == true) return@withLock
        try {
            open(); persist()
            loop = scope.launch {
                var nextBootstrap = 0L; var nextPublish = 0L; var nextSave = SystemClock.elapsedRealtime() + 30_000
                try {
                    while (isActive) {
                        val now = SystemClock.elapsedRealtime()
                        if (now >= nextBootstrap && Tox.connection(handle) == 0) {
                            Bootstrap.nodes.shuffled().forEach { Bootstrap.connect(handle, it) }; nextBootstrap = now + 60_000
                            Log.i("Parley", "bootstrap_attempt")
                        }
                        val events = Tox.iterate(handle)
                        for (e in events) when (e.type) {
                            1 -> if (requests.size < 200 && requests.none { it.key == e.detail }) {
                                requests.add(Request(e.detail, e.data.toString(Charsets.UTF_8))); notifyIncoming?.invoke()
                            }
                            2 -> {
                                messages.add(Message(key = Tox.friendKey(handle, e.friend), text = e.data.toString(Charsets.UTF_8),
                                    outgoing = false, delivery = Delivery.RECEIVED, action = e.detail == "1"))
                                notifyIncoming?.invoke()
                            }
                            4 -> receipts.remove(e.friend to e.detail.toLong())?.let { id ->
                                val i = messages.indexOfFirst { it.id == id }
                                if (i >= 0) messages[i] = messages[i].copy(delivery = Delivery.DELIVERED)
                            }
                        }
                        if (events.isNotEmpty() || now >= nextSave) { persist(); nextSave = now + 30_000 }
                        if (events.isNotEmpty() || now >= nextPublish) { publish(); nextPublish = now + 1_000 }
                        delay(Tox.interval(handle).coerceIn(10, 100).toLong())
                    }
                } catch (e: CancellationException) { throw e }
                catch (_: Exception) { error("Соединение остановлено: ошибка сети или защищённого хранилища. Данные сохранены.") }
                finally {
                    try { persist() } catch (_: Exception) { error("Не удалось сохранить последние изменения профиля") }
                    Tox.close(handle); handle = 0L
                    mutable.update { it.copy(running = false, connection = 0, contacts = it.contacts.map { c -> c.copy(connection = 0) }) }
                    Log.i("Parley", "engine_stopped")
                }
            }
            Log.i("Parley", "engine_started"); publish()
        } catch (_: Exception) { if (handle != 0L) { Tox.close(handle); handle = 0L }; error("Не удалось открыть профиль. Исходное хранилище сохранено.") }
    } } }
    fun requestStop() { scope.launch { stop() } }
    suspend fun stop() = withContext(worker) { lifecycle.withLock {
        loop?.cancelAndJoin(); loop = null
        if (handle != 0L) { Tox.close(handle); handle = 0L }
    } }
    suspend fun add(id: String, text: String) = command {
        check(handle != 0L && loop?.isActive == true) { "Включите соединение" }
        val address = ToxAddress.decode(id)
        val request = text.ifBlank { "Привет! Давайте общаться в Parley." }.toByteArray()
        require(request.size <= 1016) { "Заявка слишком длинная" }
        Tox.add(handle, address, request); persist(); publish()
    }
    suspend fun accept(key: String) = command {
        check(handle != 0L && loop?.isActive == true) { "Включите соединение" }
        Tox.accept(handle, ToxAddress.hexBytes(key)); requests.removeAll { it.key == key }; persist(); publish()
    }
    suspend fun reject(key: String) = command {
        val temporary = handle == 0L
        open()
        try { requests.removeAll { it.key == key }; persist(); publish() }
        finally { if (temporary) { Tox.close(handle); handle = 0L } }
    }
    suspend fun send(key: String, text: String) = command {
        check(handle != 0L && loop?.isActive == true) { "Включите соединение" }; ToxAddress.validateMessage(text)
        val f = Tox.friends(handle).first { Tox.friendKey(handle, it) == key }
        check(Tox.friendConnection(handle, f) != 0) { "Контакт не в сети. Офлайн-доставки нет." }
        val m = Message(key = key, text = text, outgoing = true, delivery = Delivery.PENDING)
        messages.add(m); persist()
        try {
            val receipt = Tox.send(handle, f, text.toByteArray())
            receipts[f to receipt] = m.id
            messages[messages.lastIndex] = m.copy(delivery = Delivery.SENT)
        } catch (e: Exception) { messages[messages.lastIndex] = m.copy(delivery = Delivery.FAILED); persist(); publish(); throw e }
        persist(); publish()
    }
    suspend fun customBootstrap(host: String, port: Int, key: String) = command {
        check(handle != 0L && loop?.isActive == true) { "Включите соединение" }
        Bootstrap.connect(handle, Bootstrap.Node(host, port, key))
    }
}
