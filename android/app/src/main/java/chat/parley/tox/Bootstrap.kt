// SPDX-License-Identifier: GPL-3.0-or-later
package chat.parley.tox

import chat.parley.data.ToxAddress

object Bootstrap {
    // Static discovery seeds only; no application server. Users may add a seed in Settings.
    data class Node(val host: String, val port: Int, val key: String, val tcpPort: Int? = port)
    val nodes = listOf(
        Node("tox.abilinski.com", 33445, "10C00EB250C3233E343E2AEBA07115A5C28920E9C8D29492F6D00B29049EDC7E", null),
        Node("tox1.mf-net.eu", 33445, "B3E5FA80DC8EBD1149AD2AB35ED8B85BD546DEDE261CA593234C619249419506"),
        Node("tox.initramfs.io", 33445, "3F0A45A268367C1BEA652F258C85F4A66DA76BCAA667A49E770BCC4917AB6A25"),
        Node("144.217.167.73", 33445, "7E5668E0EE09E19F320AD47902419331FFEE147BB3606769CFBE921A2A2FD34C", 3389),
        Node("205.185.115.131", 53, "3091C6BEB2A993F1C6300C16549FABA67098FF3D62C6D253828B531470B53D68", 443),
    )
    fun connect(handle: Long, node: Node) {
        require(node.host.isNotBlank() && node.port in 1..65535 && (node.tcpPort == null || node.tcpPort in 1..65535))
        val key = ToxAddress.hexBytes(node.key); require(key.size == 32)
        NativeBridge.bootstrap(handle, node.host, node.port, key, false)
        node.tcpPort?.let { NativeBridge.bootstrap(handle, node.host, it, key, true) }
    }
}
