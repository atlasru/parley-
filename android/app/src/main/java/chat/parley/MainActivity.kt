// SPDX-License-Identifier: GPL-3.0-or-later
package chat.parley

import android.Manifest
import android.content.ClipData
import android.content.ClipboardManager
import android.content.Intent
import android.os.Build
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.SystemBarStyle
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.*
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material.icons.automirrored.filled.Send
import androidx.compose.material.icons.outlined.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.PasswordVisualTransformation
import androidx.compose.ui.unit.dp
import androidx.core.content.ContextCompat
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import chat.parley.data.*
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

class MainActivity : ComponentActivity() {
    private val repository get() = (application as ParleyApp).repository
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        setContent { Parley() }
    }
    private val settings get() = getSharedPreferences("settings", MODE_PRIVATE)
    private fun connect() {
        settings.edit().putBoolean("connection_enabled", true).apply()
        ContextCompat.startForegroundService(this, Intent(this, ConnectionService::class.java))
    }

    @OptIn(ExperimentalMaterial3Api::class)
    @Composable private fun Parley() {
        val state by repository.state.collectAsStateWithLifecycle()
        val scope = rememberCoroutineScope()
        var localError by remember { mutableStateOf<String?>(null) }
        var theme by rememberSaveable { mutableIntStateOf(settings.getInt("theme", 0).coerceIn(0, 2)) }
        val dark = theme == 2 || (theme == 0 && isSystemInDarkTheme())
        LaunchedEffect(dark) {
            enableEdgeToEdge(
                statusBarStyle = if (dark) SystemBarStyle.dark(android.graphics.Color.TRANSPARENT)
                    else SystemBarStyle.light(android.graphics.Color.TRANSPARENT, android.graphics.Color.TRANSPARENT),
                navigationBarStyle = if (dark) SystemBarStyle.dark(0xFF111318.toInt())
                    else SystemBarStyle.light(0xFFF8F9FD.toInt(), 0xFF111318.toInt()),
            )
        }
        val colors = if (dark) darkColorScheme(primary = Color(0xFFB7C4FF), primaryContainer = Color(0xFF304385), background = Color(0xFF111318), surface = Color(0xFF111318))
            else lightColorScheme(primary = Color(0xFF4361EE), primaryContainer = Color(0xFFE3E8FF), background = Color(0xFFF8F9FD), surface = Color(0xFFF8F9FD))
        fun runTask(block: suspend () -> Unit) { scope.launch { try { block() } catch (e: Exception) { localError = e.message ?: "Не удалось выполнить действие" } } }
        var route by rememberSaveable { mutableStateOf("chats") }
        var selectedKey by rememberSaveable { mutableStateOf<String?>(null) }
        var password by remember { mutableStateOf("") }
        var licenseText by remember { mutableStateOf<String?>(null) }
        var profileName by rememberSaveable { mutableStateOf("") }
        val notificationPermission = androidx.activity.compose.rememberLauncherForActivityResult(ActivityResultContracts.RequestPermission()) { }
        val export = androidx.activity.compose.rememberLauncherForActivityResult(ActivityResultContracts.CreateDocument("application/octet-stream")) { uri ->
            val pass = password; password = ""
            if (uri != null) runTask {
                val encrypted = repository.exportProfile(pass)
                try { withContext(Dispatchers.IO) { contentResolver.openOutputStream(uri)?.use { it.write(encrypted) } ?: error("Файл недоступен") } }
                finally { encrypted.fill(0) }
            }
        }
        val importProfile = androidx.activity.compose.rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()) { uri ->
            val pass = password; password = ""
            if (uri != null) runTask {
                val data = withContext(Dispatchers.IO) {
                    contentResolver.openInputStream(uri)?.use { input ->
                        val output = java.io.ByteArrayOutputStream()
                        val buffer = ByteArray(8192)
                        while (true) {
                            val n = input.read(buffer)
                            if (n < 0) break
                            require(output.size() + n <= 16 * 1024 * 1024) { "Профиль больше 16 МиБ" }
                            output.write(buffer, 0, n)
                        }
                        buffer.fill(0); output.toByteArray()
                    } ?: error("Файл недоступен")
                }
                repository.importProfile(data, pass, profileName)
                connect()
            }
        }
        LaunchedEffect(state.loaded) {
            if (state.loaded && state.hasProfile && settings.getBoolean("connection_enabled", true)) connect()
        }
        MaterialTheme(colorScheme = colors) {
            Surface(Modifier.fillMaxSize()) {
                if (!state.loaded) Box(contentAlignment = Alignment.Center) { CircularProgressIndicator() }
                else if (!state.hasProfile) {
                    Column(Modifier.fillMaxSize().safeDrawingPadding().padding(28.dp).verticalScroll(rememberScrollState()), verticalArrangement = Arrangement.Center) {
                        Text("parley", style = MaterialTheme.typography.displayLarge, fontWeight = FontWeight.Bold, color = colors.primary)
                        Spacer(Modifier.height(16.dp))
                        Text("Разговор начинается с вас.", style = MaterialTheme.typography.headlineMedium)
                        Text("Локальная identity. Прямое соединение через Tox. Без номера телефона.", Modifier.padding(vertical = 16.dp), color = colors.onSurfaceVariant)
                        OutlinedTextField(profileName, { profileName = it }, label = { Text("Ваше имя") }, modifier = Modifier.fillMaxWidth(), singleLine = true)
                        Spacer(Modifier.height(16.dp))
                        Button(onClick = { runTask { repository.create(profileName); connect() } }, Modifier.fillMaxWidth(), enabled = profileName.isNotBlank()) { Text("Создать профиль") }
                        HorizontalDivider(Modifier.padding(vertical = 24.dp))
                        Text("Уже есть профиль Tox?", style = MaterialTheme.typography.titleMedium)
                        OutlinedTextField(password, { password = it }, label = { Text("Пароль .tox, если установлен") }, visualTransformation = PasswordVisualTransformation(), modifier = Modifier.fillMaxWidth())
                        TextButton(onClick = { importProfile.launch(arrayOf("*/*")) }) { Text("Импортировать .tox") }
                        SafetyNote()
                    }
                } else {
                    val contact = state.contacts.firstOrNull { it.key == selectedKey }
                    Scaffold(topBar = {
                        TopAppBar(title = {
                            Column {
                                Text(if (contact != null && route == "chat") contact.name else when (route) { "settings" -> "Профиль"; "add" -> "Новый контакт"; else -> "Parley" }, fontWeight = FontWeight.Bold)
                                Text(if (contact != null && route == "chat") if (contact.connection != 0) "В сети" else "Не в сети" else networkLabel(state), style = MaterialTheme.typography.labelMedium, color = colors.onSurfaceVariant)
                            }
                        }, navigationIcon = { if (route != "chats") IconButton(onClick = { route = "chats"; selectedKey = null }) { Icon(Icons.AutoMirrored.Filled.ArrowBack, "Назад") } },
                            actions = { if (route == "chats") IconButton(onClick = { route = "settings" }) { Icon(Icons.Outlined.Settings, "Профиль и настройки") } })
                    }, floatingActionButton = {
                        if (route == "chats") FloatingActionButton(onClick = { route = "add" }) { Icon(Icons.Outlined.Add, "Добавить контакт") }
                    }) { padding ->
                        when (route) {
                            "chat" -> if (contact != null) Chat(state, contact, Modifier.padding(padding)) { text ->
                                try { repository.send(contact.key, text); true }
                                catch (e: Exception) { localError = e.message ?: "Ошибка отправки"; false }
                            }
                            "add" -> AddContact(Modifier.padding(padding)) { id, request -> runTask { repository.add(id, request); route = "chats" } }
                            "settings" -> Column(Modifier.padding(padding).padding(horizontal = 20.dp).verticalScroll(rememberScrollState())) {
                                Text(state.name, style = MaterialTheme.typography.headlineMedium)
                                Text("Ваш Tox ID", Modifier.padding(top = 24.dp), style = MaterialTheme.typography.labelLarge)
                                Text(state.address, Modifier.padding(vertical = 12.dp), style = MaterialTheme.typography.bodySmall)
                                OutlinedButton(onClick = { getSystemService(ClipboardManager::class.java).setPrimaryClip(ClipData.newPlainText("Tox ID", state.address)) }) { Text("Копировать Tox ID") }
                                Row(verticalAlignment = Alignment.CenterVertically) {
                                    Text("Фоновое соединение", Modifier.weight(1f))
                                    Switch(state.running, { enabled -> if (enabled) connect() else {
                                        settings.edit().putBoolean("connection_enabled", false).apply()
                                        stopService(Intent(this@MainActivity, ConnectionService::class.java))
                                    } })
                                }
                                Text("Android может приостанавливать сеть в Doze или завершать процесс. После принудительной остановки откройте приложение снова.", style = MaterialTheme.typography.bodySmall, color = colors.onSurfaceVariant)
                                TextButton(onClick = { if (Build.VERSION.SDK_INT >= 33) notificationPermission.launch(Manifest.permission.POST_NOTIFICATIONS) }) { Text("Разрешить уведомления") }
                                Text("Тема", Modifier.padding(top = 16.dp), style = MaterialTheme.typography.titleMedium)
                                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) { listOf("Система", "Светлая", "Тёмная").forEachIndexed { i, label -> FilterChip(theme == i, {
                                    theme = i; settings.edit().putInt("theme", i).apply()
                                }, label = { Text(label) }) } }
                                HorizontalDivider(Modifier.padding(vertical = 20.dp))
                                Text("Резервная копия identity", style = MaterialTheme.typography.titleMedium)
                                Text("Зашифрованный .tox совместим с qTox. История хранится только на этом устройстве и в экспорт не входит.", style = MaterialTheme.typography.bodySmall)
                                OutlinedTextField(password, { password = it }, label = { Text("Пароль экспорта · от 8 символов") }, visualTransformation = PasswordVisualTransformation(), modifier = Modifier.fillMaxWidth())
                                Button(onClick = { export.launch("parley.tox") }, enabled = password.length >= 8) { Text("Экспортировать профиль") }
                                CustomSeed { host, port, key -> runTask { repository.customBootstrap(host, port, key) } }
                                TextButton(onClick = { runTask {
                                    licenseText = withContext(Dispatchers.IO) {
                                        "Parley 0.1.0-dev · GPL-3.0-or-later\n" +
                                            "Без гарантий. Исходники: github.com/atlasru/parley-\n" +
                                            "AndroidX, Kotlin и kotlinx.coroutines: Apache-2.0.\n\n" +
                                            assets.list("licenses").orEmpty().sorted().joinToString("\n\n") { file ->
                                                file + "\n" + assets.open("licenses/$file").bufferedReader().use { it.readText() }
                                            }
                                    }
                                } }) { Text("О Parley и лицензии") }
                                SafetyNote()
                                Spacer(Modifier.height(24.dp))
                            }
                            else -> LazyColumn(Modifier.padding(padding).fillMaxSize(), contentPadding = PaddingValues(bottom = 96.dp)) {
                                if (state.requests.isNotEmpty()) item { Text("Заявки в контакты", Modifier.padding(20.dp), style = MaterialTheme.typography.titleMedium) }
                                items(state.requests, key = { "request-${it.key}" }) { r ->
                                    Card(Modifier.padding(horizontal = 16.dp, vertical = 4.dp)) { Column(Modifier.padding(16.dp)) {
                                        Text(r.key.take(16) + "…", fontWeight = FontWeight.Bold); Text(r.message)
                                        Row { TextButton(onClick = { runTask { repository.accept(r.key) } }) { Text("Принять") }; TextButton(onClick = { runTask { repository.reject(r.key) } }) { Text("Отклонить") } }
                                    } }
                                }
                                if (state.contacts.isEmpty()) item { Column(Modifier.fillMaxWidth().padding(horizontal = 32.dp, vertical = 64.dp), horizontalAlignment = Alignment.CenterHorizontally) {
                                    Icon(Icons.Outlined.Person, null, Modifier.size(56.dp), tint = colors.primary)
                                    Text("Место для ваших разговоров", Modifier.padding(top = 20.dp), style = MaterialTheme.typography.titleLarge)
                                    Text("Добавьте друга по Tox ID или поделитесь своим ID из профиля.", Modifier.padding(top = 8.dp), color = colors.onSurfaceVariant)
                                } }
                                items(state.contacts, key = { it.key }) { c ->
                                    val last = state.messages.lastOrNull { it.key == c.key }
                                    ListItem(modifier = Modifier.clickable { selectedKey = c.key; route = "chat" },
                                        leadingContent = { Surface(color = colors.primaryContainer, shape = CircleShape, modifier = Modifier.size(48.dp)) { Box(contentAlignment = Alignment.Center) { Text(c.name.take(1).uppercase(), fontWeight = FontWeight.Bold) } } },
                                        headlineContent = { Text(c.name, fontWeight = FontWeight.SemiBold) },
                                        supportingContent = { Text(last?.text ?: "Начните разговор", maxLines = 1) },
                                        trailingContent = { Text(if (c.connection != 0) "В сети" else "Офлайн", style = MaterialTheme.typography.labelSmall) })
                                }
                            }
                        }
                    }
                }
                val error = localError ?: state.error
                licenseText?.let { text -> AlertDialog(onDismissRequest = { licenseText = null },
                    title = { Text("О Parley и лицензии") },
                    text = { Text(text, Modifier.heightIn(max = 500.dp).verticalScroll(rememberScrollState()), style = MaterialTheme.typography.bodySmall) },
                    confirmButton = { TextButton(onClick = { licenseText = null }) { Text("Закрыть") } }) }
                if (error != null) AlertDialog(onDismissRequest = { localError = null; repository.clearError() }, title = { Text("Не удалось выполнить действие") }, text = { Text(error) }, confirmButton = { TextButton(onClick = { localError = null; repository.clearError() }) { Text("Понятно") } })
            }
        }
    }
}

private fun networkLabel(s: AppState) = if (!s.running) "Соединение выключено" else when (s.connection) { 1 -> "Tox · TCP"; 2 -> "Tox · UDP"; else -> "Подключаемся к Tox…" }

@Composable private fun SafetyNote() {
    Text("Tox не скрывает IP-адреса и все метаданные. Собеседник должен быть в сети для доставки. Полного независимого аудита Parley/Tox нет. Защита хранилища не защищает от скомпрометированного устройства.", Modifier.padding(top = 24.dp), style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
}

@Composable private fun AddContact(modifier: Modifier, onAdd: (String, String) -> Unit) {
    var id by rememberSaveable { mutableStateOf("") }; var request by rememberSaveable { mutableStateOf("") }
    Column(modifier.fillMaxSize().padding(20.dp).verticalScroll(rememberScrollState()), verticalArrangement = Arrangement.spacedBy(16.dp)) {
        Text("Найдите друг друга", style = MaterialTheme.typography.headlineSmall)
        Text("Вставьте полный Tox ID. Попросите друга сверить ID по другому доверенному каналу.")
        OutlinedTextField(id, { id = it }, label = { Text("Tox ID · 76 символов") }, modifier = Modifier.fillMaxWidth(), minLines = 2)
        OutlinedTextField(request, { request = it }, label = { Text("Сообщение в заявке") }, modifier = Modifier.fillMaxWidth())
        Button(onClick = { onAdd(id, request) }, enabled = id.isNotBlank()) { Text("Отправить заявку") }
    }
}

@Composable private fun CustomSeed(onConnect: (String, Int, String) -> Unit) {
    var expanded by remember { mutableStateOf(false) }
    var host by remember { mutableStateOf("") }; var port by remember { mutableStateOf("33445") }; var key by remember { mutableStateOf("") }
    TextButton(onClick = { expanded = !expanded }) { Text("Свой bootstrap-узел") }
    if (expanded) Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
        OutlinedTextField(host, { host = it }, label = { Text("Адрес узла") }, modifier = Modifier.fillMaxWidth())
        OutlinedTextField(port, { port = it }, label = { Text("UDP/TCP порт") }, modifier = Modifier.fillMaxWidth())
        OutlinedTextField(key, { key = it }, label = { Text("Публичный DHT-ключ · 64 символа") }, modifier = Modifier.fillMaxWidth())
        OutlinedButton(onClick = { onConnect(host, port.toIntOrNull() ?: 0, key) }) { Text("Подключиться") }
    }
}

@Composable private fun Chat(state: AppState, contact: Contact, modifier: Modifier, onSend: suspend (String) -> Boolean) {
    val messages = state.messages.filter { it.key == contact.key }
    val list = rememberLazyListState()
    var draft by rememberSaveable(contact.key) { mutableStateOf("") }
    var sending by remember { mutableStateOf(false) }
    val scope = rememberCoroutineScope()
    val sendEnabled = !sending && draft.isNotBlank() && contact.connection != 0 && state.running
    LaunchedEffect(messages.size) { if (messages.isNotEmpty()) list.animateScrollToItem(messages.lastIndex) }
    Column(modifier.fillMaxSize().imePadding()) {
        LazyColumn(Modifier.weight(1f).fillMaxWidth(), state = list, contentPadding = PaddingValues(16.dp), verticalArrangement = Arrangement.spacedBy(10.dp)) {
            items(messages, key = { it.id }) { m ->
                Row(Modifier.fillMaxWidth(), horizontalArrangement = if (m.outgoing) Arrangement.End else Arrangement.Start) {
                    Surface(shape = RoundedCornerShape(18.dp), color = if (m.outgoing) MaterialTheme.colorScheme.primaryContainer else MaterialTheme.colorScheme.surfaceContainerHigh, modifier = Modifier.widthIn(max = 320.dp)) {
                        Column(Modifier.padding(horizontal = 14.dp, vertical = 10.dp)) {
                            Text(if (m.action) "* ${m.text}" else m.text)
                            Text(SimpleDateFormat("HH:mm", Locale.getDefault()).format(Date(m.at)) + if (m.outgoing) " · " + when (m.delivery) {
                                Delivery.PENDING -> "Отправляем"; Delivery.SENT -> "Отправлено"; Delivery.DELIVERED -> "Получено клиентом"; Delivery.FAILED -> "Ошибка"; else -> "Статус неизвестен"
                            } else "", style = MaterialTheme.typography.labelSmall, color = MaterialTheme.colorScheme.onSurfaceVariant, modifier = Modifier.align(Alignment.End).padding(top = 4.dp))
                        }
                    }
                }
            }
        }
        if (contact.connection == 0) Text("Контакт офлайн. Доставка доступна, когда вы оба в сети.", Modifier.padding(horizontal = 20.dp), style = MaterialTheme.typography.bodySmall)
        Row(Modifier.fillMaxWidth().padding(12.dp), verticalAlignment = Alignment.Bottom) {
            OutlinedTextField(draft, { draft = it }, Modifier.weight(1f), placeholder = { Text("Сообщение") }, shape = RoundedCornerShape(24.dp), maxLines = 5)
            IconButton(onClick = {
                val text = draft; sending = true
                scope.launch { try { if (onSend(text) && draft == text) draft = "" } finally { sending = false } }
            }, enabled = sendEnabled) { Icon(Icons.AutoMirrored.Filled.Send, "Отправить", tint = if (sendEnabled) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.onSurface.copy(alpha = 0.38f)) }
        }
    }
}
