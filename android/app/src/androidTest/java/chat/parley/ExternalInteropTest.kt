package chat.parley

import android.content.Intent
import androidx.core.content.ContextCompat
import androidx.test.core.app.ActivityScenario
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import chat.parley.data.Delivery
import kotlinx.coroutines.*
import kotlinx.coroutines.flow.first
import org.junit.Assert.*
import org.junit.Assume.assumeTrue
import org.junit.Test
import org.junit.runner.RunWith

/** Opt-in real external client test. Use a fresh emulator install, never a personal profile. */
@RunWith(AndroidJUnit4::class)
class ExternalInteropTest {
    @Test fun exchangesWithExternalClientThroughProductionRepository() = runBlocking {
        val args = InstrumentationRegistry.getArguments()
        val peerId = args.getString("peerAddress")
        assumeTrue("Pass -e peerAddress <external Tox ID>", !peerId.isNullOrBlank())
        val app = InstrumentationRegistry.getInstrumentation().targetContext.applicationContext as ParleyApp
        val repo = app.repository
        repo.state.first { it.loaded }
        ActivityScenario.launch(MainActivity::class.java).use {
            if (!repo.state.value.hasProfile) repo.create("Parley Android")
            ContextCompat.startForegroundService(app, Intent(app, ConnectionService::class.java))
            withTimeout(30_000) { repo.state.first { it.running } }
            args.getString("bootstrapKey")?.let { key -> repo.customBootstrap(args.getString("bootstrapHost") ?: "10.0.2.2", args.getString("bootstrapPort")!!.toInt(), key) }
            val key = peerId!!.take(64).uppercase()
            if (repo.state.value.contacts.none { c -> c.key == key }) repo.add(peerId, "Parley Android interoperability test")
            withTimeout(240_000) { repo.state.first { s -> s.contacts.any { c -> c.key == key && c.connection != 0 } } }
            repo.send(key, "Parley → qTox · Android JNI ✓")
            withTimeout(240_000) { repo.state.first { s -> s.messages.any { m -> m.key == key && !m.outgoing && m.text == "qTox → Parley ✓" } } }
            withTimeout(30_000) { repo.state.first { s -> s.messages.any { m -> m.key == key && m.outgoing && m.delivery == Delivery.DELIVERED } } }
            assertTrue(repo.state.value.connection != 0)
            // Stop/reopen verifies actual encrypted production profile + history persistence.
            repo.stop(); repo.start()
            withTimeout(30_000) { repo.state.first { s -> s.running && s.messages.any { m -> m.key == key && m.text == "qTox → Parley ✓" } } }
            assertTrue(repo.state.value.contacts.any { c -> c.key == key })
        }
    }
}
