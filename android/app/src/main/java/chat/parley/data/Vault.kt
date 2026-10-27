// SPDX-License-Identifier: GPL-3.0-or-later
package chat.parley.data

import android.content.Context
import android.security.keystore.KeyGenParameterSpec
import android.security.keystore.KeyProperties
import android.util.AtomicFile
import java.io.File
import java.security.KeyStore
import javax.crypto.Cipher
import javax.crypto.KeyGenerator
import javax.crypto.SecretKey
import javax.crypto.spec.GCMParameterSpec

/** One authenticated, atomic snapshot: identity, requests and history never hit disk as plaintext. */
class Vault(context: Context, filename: String = "parley.vault", private val alias: String = "parley-vault-v1") {
    private val file = AtomicFile(File(context.noBackupFilesDir, filename))
    fun exists() = file.baseFile.exists() || File(file.baseFile.path + ".bak").exists()
    private fun key(create: Boolean): SecretKey {
        val store = KeyStore.getInstance("AndroidKeyStore").apply { load(null) }
        (store.getKey(alias, null) as? SecretKey)?.let { return it }
        check(create) { "Ключ хранилища недоступен; исходные данные сохранены" }
        return KeyGenerator.getInstance(KeyProperties.KEY_ALGORITHM_AES, "AndroidKeyStore").apply {
            init(KeyGenParameterSpec.Builder(alias, KeyProperties.PURPOSE_ENCRYPT or KeyProperties.PURPOSE_DECRYPT)
                .setBlockModes(KeyProperties.BLOCK_MODE_GCM).setEncryptionPaddings(KeyProperties.ENCRYPTION_PADDING_NONE)
                .setKeySize(256).build())
        }.generateKey()
    }
    fun read(): ByteArray {
        val encrypted = file.openRead().use { it.readBytes() }
        require(encrypted.size >= 32 && encrypted[0] == 1.toByte()) { "Повреждённое хранилище" }
        val cipher = Cipher.getInstance("AES/GCM/NoPadding")
        cipher.init(Cipher.DECRYPT_MODE, key(false), GCMParameterSpec(128, encrypted.copyOfRange(1, 13)))
        cipher.updateAAD("Parley vault v1".toByteArray())
        return cipher.doFinal(encrypted, 13, encrypted.size - 13)
    }
    fun write(plain: ByteArray) {
        val cipher = Cipher.getInstance("AES/GCM/NoPadding")
        cipher.init(Cipher.ENCRYPT_MODE, key(!exists()))
        cipher.updateAAD("Parley vault v1".toByteArray())
        val encrypted = byteArrayOf(1) + cipher.iv + cipher.doFinal(plain)
        val stream = file.startWrite()
        try { stream.write(encrypted); file.finishWrite(stream) }
        catch (e: Exception) { file.failWrite(stream); throw e }
    }
}
