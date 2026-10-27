package chat.parley

import chat.parley.data.ToxAddress
import org.junit.Assert.*
import org.junit.Test

class ToxAddressTest {
    private fun address(): String {
        val body = ByteArray(36) { it.toByte() }
        val checksum = ByteArray(2)
        body.forEachIndexed { i, b -> checksum[i % 2] = (checksum[i % 2].toInt() xor b.toInt()).toByte() }
        return (body + checksum).joinToString("") { "%02X".format(it.toInt() and 255) }
    }
    @Test fun acceptsChecksumAndToxUri() {
        assertEquals(38, ToxAddress.decode(" tox:${address().lowercase()} ").size)
    }
    @Test fun rejectsInvalidChecksumAndUnicode() {
        listOf("0".repeat(75), address().dropLast(1) + "F", "А".repeat(76)).forEach {
            assertThrows(IllegalArgumentException::class.java) { ToxAddress.decode(it) }
        }
    }
    @Test fun messageLimitCountsUtf8Bytes() {
        ToxAddress.validateMessage("я".repeat(686))
        assertThrows(IllegalArgumentException::class.java) { ToxAddress.validateMessage("я".repeat(687)) }
        assertThrows(IllegalArgumentException::class.java) { ToxAddress.validateMessage(" \n") }
    }
}
