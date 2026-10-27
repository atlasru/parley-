// SPDX-License-Identifier: GPL-3.0-or-later
package chat.parley.data

import android.util.Base64
import org.json.JSONArray
import org.json.JSONObject

data class Snapshot(val name: String, val savedata: ByteArray, val requests: List<Request>, val messages: List<Message>) {
    fun encode(): ByteArray = JSONObject().apply {
        put("version", 1); put("name", name)
        put("savedata", Base64.encodeToString(savedata, Base64.NO_WRAP))
        put("requests", JSONArray().apply { requests.forEach { put(JSONObject().put("key", it.key).put("message", it.message)) } })
        put("messages", JSONArray().apply { messages.forEach {
            put(JSONObject().put("id", it.id).put("key", it.key).put("text", it.text).put("outgoing", it.outgoing)
                .put("at", it.at).put("delivery", it.delivery.name).put("action", it.action))
        } })
    }.toString().toByteArray()
    companion object {
        fun decode(bytes: ByteArray): Snapshot {
            val j = JSONObject(bytes.toString(Charsets.UTF_8))
            require(j.getInt("version") == 1) { "Неподдерживаемая версия хранилища" }
            val r = j.getJSONArray("requests"); val m = j.getJSONArray("messages")
            return Snapshot(j.getString("name"), Base64.decode(j.getString("savedata"), Base64.NO_WRAP),
                List(r.length()) { r.getJSONObject(it).let { v -> Request(v.getString("key"), v.getString("message")) } },
                List(m.length()) { m.getJSONObject(it).let { v -> Message(v.getString("id"), v.getString("key"), v.getString("text"),
                    v.getBoolean("outgoing"), v.getLong("at"), Delivery.valueOf(v.getString("delivery")), v.optBoolean("action")) } })
        }
    }
}
