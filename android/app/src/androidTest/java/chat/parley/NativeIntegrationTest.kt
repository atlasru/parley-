package chat.parley

import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import chat.parley.data.*
import chat.parley.tox.NativeBridge as Tox
import org.junit.Assert.*
import org.junit.Test
import org.junit.runner.RunWith
import java.io.File

/** Real sockets, real cryptography and real JNI, including the callback event loop. */
@RunWith(AndroidJUnit4::class)
class NativeIntegrationTest {
    @Test fun savedataEncryptedExportAndCorruption() {
        val a = Tox.create(null, true)
        var b = 0L
        try {
            val id = Tox.address(a)
            val encrypted = Tox.exportEncrypted(a, "integration-passphrase".toByteArray())
            assertFalse(encrypted.contentEquals(Tox.savedata(a)))
            assertThrows(IllegalStateException::class.java) { Tox.decryptImport(encrypted, "wrong".toByteArray()) }
            val plain = Tox.decryptImport(encrypted, "integration-passphrase".toByteArray())
            b = Tox.create(plain, true); plain.fill(0)
            assertEquals(id, Tox.address(b))
            assertThrows(IllegalStateException::class.java) { Tox.create(ByteArray(100), true) }
        } finally { Tox.close(a); if (b != 0L) Tox.close(b) }
    }
    @Test fun vaultEncryptsAuthenticatesAndPreservesHistory() {
        val c = InstrumentationRegistry.getInstrumentation().targetContext
        val filename = "test-${System.nanoTime()}.vault"
        val vault = Vault(c, filename, "parley-test-key")
        val snapshot = Snapshot("Test", byteArrayOf(1, 2, 3), listOf(Request("key", "request")),
            listOf(Message(key = "friend", text = "secret-history-Привет", outgoing = false, delivery = Delivery.RECEIVED)))
        try {
            val plain = snapshot.encode(); vault.write(plain)
            assertArrayEquals(plain, vault.read())
            assertEquals(snapshot.messages, Snapshot.decode(vault.read()).messages)
            val file = File(c.noBackupFilesDir, filename)
            val encrypted = file.readBytes()
            assertFalse(encrypted.toString(Charsets.ISO_8859_1).contains("secret-history"))
            encrypted[encrypted.lastIndex] = (encrypted.last().toInt() xor 1).toByte(); file.writeBytes(encrypted)
            assertThrows(Exception::class.java) { vault.read() }
            assertTrue(vault.exists())
        } finally { File(c.noBackupFilesDir, filename).delete() }
    }
    @Test fun twoRealToxPeersExchangeUnicodeAndReceipt() {
        val a = Tox.create(null, true); val b = Tox.create(null, true)
        try {
            val idB = Tox.address(b)
            val friend = Tox.add(a, ToxAddress.decode(idB), "Parley integration request".toByteArray())
            var accepted = false; var sent = false; var gotA = false; var gotB = false; var receipt = false
            var sentId = -1L
            val end = System.currentTimeMillis() + 90_000
            var nextBootstrap = 0L
            while (System.currentTimeMillis() < end && !(gotA && gotB && receipt)) {
                if (System.currentTimeMillis() >= nextBootstrap) {
                    assertTrue(Tox.bootstrap(a, "127.0.0.1", Tox.udpPort(b), Tox.dhtKey(b), false))
                    assertTrue(Tox.bootstrap(b, "127.0.0.1", Tox.udpPort(a), Tox.dhtKey(a), false))
                    nextBootstrap = System.currentTimeMillis() + 1000
                }
                Tox.iterate(a).forEach {
                    if (it.type == 2) { assertEquals("Ответ из Tox ✓", it.data.toString(Charsets.UTF_8)); gotA = true }
                    if (it.type == 4 && it.detail.toLong() == sentId) receipt = true
                }
                Tox.iterate(b).forEach {
                    if (it.type == 1) {
                        assertEquals("Parley integration request", it.data.toString(Charsets.UTF_8))
                        Tox.accept(b, ToxAddress.hexBytes(it.detail)); accepted = true
                    }
                    if (it.type == 2) {
                        assertEquals("Привет из Parley 👋", it.data.toString(Charsets.UTF_8)); gotB = true
                        Tox.send(b, it.friend, "Ответ из Tox ✓".toByteArray())
                    }
                }
                if (!sent && accepted && Tox.friendConnection(a, friend) != 0) {
                    sentId = Tox.send(a, friend, "Привет из Parley 👋".toByteArray()); sent = true
                }
                Thread.sleep(20)
            }
            assertTrue("request accepted", accepted); assertTrue("A → B", gotB); assertTrue("B → A", gotA); assertTrue("receipt", receipt)
            val save = Tox.savedata(a); val restored = Tox.create(save, true)
            try { assertEquals(Tox.friendKey(a, friend), Tox.friendKey(restored, Tox.friends(restored).single())) }
            finally { save.fill(0); Tox.close(restored) }
        } finally { Tox.close(a); Tox.close(b) }
    }
}
