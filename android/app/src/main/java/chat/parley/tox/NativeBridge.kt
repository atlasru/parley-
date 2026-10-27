// SPDX-License-Identifier: GPL-3.0-or-later
package chat.parley.tox

/** All calls for one handle must run on the engine's single worker thread. */
object NativeBridge {
    init { System.loadLibrary("parley") }
    external fun create(savedata: ByteArray?, localOnly: Boolean = false): Long
    external fun close(handle: Long)
    external fun savedata(handle: Long): ByteArray
    external fun address(handle: Long): String
    external fun name(handle: Long, name: ByteArray)
    external fun iterate(handle: Long): Array<NativeEvent>
    external fun interval(handle: Long): Int
    external fun connection(handle: Long): Int
    external fun bootstrap(handle: Long, host: String, port: Int, key: ByteArray, relay: Boolean): Boolean
    external fun friends(handle: Long): IntArray
    external fun friendKey(handle: Long, friend: Int): String
    external fun friendName(handle: Long, friend: Int): ByteArray
    external fun friendConnection(handle: Long, friend: Int): Int
    external fun add(handle: Long, address: ByteArray, request: ByteArray): Int
    external fun accept(handle: Long, key: ByteArray): Int
    external fun send(handle: Long, friend: Int, text: ByteArray): Long
    external fun exportEncrypted(handle: Long, password: ByteArray): ByteArray
    external fun decryptImport(data: ByteArray, password: ByteArray): ByteArray
    external fun udpPort(handle: Long): Int
    external fun dhtKey(handle: Long): ByteArray
}

// 1=request, 2=message, 3=friend connection, 4=receipt, 5=friend name.
class NativeEvent(val type: Int, val friend: Int, val detail: String, val data: ByteArray)
