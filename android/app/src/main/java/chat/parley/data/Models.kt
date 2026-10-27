// SPDX-License-Identifier: GPL-3.0-or-later
package chat.parley.data

import java.util.UUID

data class Contact(val key: String, val number: Int, val name: String, val connection: Int = 0)
data class Request(val key: String, val message: String)
enum class Delivery { PENDING, SENT, DELIVERED, RECEIVED, FAILED, UNKNOWN }
data class Message(
    val id: String = UUID.randomUUID().toString(), val key: String, val text: String,
    val outgoing: Boolean, val at: Long = System.currentTimeMillis(),
    val delivery: Delivery, val action: Boolean = false,
)
data class AppState(
    val loaded: Boolean = false, val hasProfile: Boolean = false,
    val name: String = "", val address: String = "", val running: Boolean = false,
    val connection: Int = 0, val contacts: List<Contact> = emptyList(),
    val requests: List<Request> = emptyList(), val messages: List<Message> = emptyList(),
    val error: String? = null,
)

object ToxAddress {
    fun decode(text: String): ByteArray {
        val s = text.trim().removePrefix("tox:")
        require(s.length == 76 && s.all { it in "0123456789abcdefABCDEF" }) { "Tox ID: нужны 76 шестнадцатеричных символов" }
        val bytes = hexBytes(s)
        val check = ByteArray(2)
        for (i in 0 until 36) check[i % 2] = (check[i % 2].toInt() xor bytes[i].toInt()).toByte()
        require(check[0] == bytes[36] && check[1] == bytes[37]) { "Неверная контрольная сумма Tox ID" }
        return bytes
    }
    fun hexBytes(s: String): ByteArray {
        require(s.length % 2 == 0 && s.all { it in "0123456789abcdefABCDEF" })
        return ByteArray(s.length / 2) { s.substring(it * 2, it * 2 + 2).toInt(16).toByte() }
    }
    fun validateMessage(text: String) {
        require(text.isNotBlank()) { "Сообщение пустое" }
        require(text.toByteArray(Charsets.UTF_8).size <= 1372) { "Сообщение длиннее 1372 байт UTF-8" }
    }
}
