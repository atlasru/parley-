// SPDX-License-Identifier: GPL-3.0-or-later
package chat.parley

import android.Manifest
import android.app.*
import android.content.Intent
import android.content.pm.PackageManager
import android.content.pm.ServiceInfo
import android.os.Build
import android.os.IBinder
import androidx.core.app.NotificationCompat
import androidx.core.content.ContextCompat
import kotlinx.coroutines.*
import kotlinx.coroutines.flow.distinctUntilChanged
import kotlinx.coroutines.flow.map

class ConnectionService : Service() {
    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.Main.immediate)
    private val repository get() = (application as ParleyApp).repository
    private val manager get() = getSystemService(NotificationManager::class.java)
    private fun openApp() = PendingIntent.getActivity(this, 0, Intent(this, MainActivity::class.java), PendingIntent.FLAG_IMMUTABLE or PendingIntent.FLAG_UPDATE_CURRENT)
    private fun ongoing(text: String): Notification {
        val stop = PendingIntent.getService(this, 1, Intent(this, ConnectionService::class.java).setAction("STOP"), PendingIntent.FLAG_IMMUTABLE)
        return NotificationCompat.Builder(this, "connection").setSmallIcon(R.drawable.ic_parley)
            .setContentTitle("Parley").setContentText(text).setContentIntent(openApp()).setOngoing(true)
            .setSilent(true).addAction(0, "Отключиться", stop).build()
    }
    override fun onCreate() {
        super.onCreate()
        manager.createNotificationChannel(NotificationChannel("connection", "Соединение Tox", NotificationManager.IMPORTANCE_LOW))
        manager.createNotificationChannel(NotificationChannel("messages", "Сообщения и заявки", NotificationManager.IMPORTANCE_DEFAULT))
        val notification = ongoing("Подключаемся к Tox…")
        if (Build.VERSION.SDK_INT >= 34) startForeground(1, notification, ServiceInfo.FOREGROUND_SERVICE_TYPE_SPECIAL_USE)
        else startForeground(1, notification)
        repository.notifyIncoming = {
            if (ContextCompat.checkSelfPermission(this, Manifest.permission.POST_NOTIFICATIONS) == PackageManager.PERMISSION_GRANTED) {
                manager.notify(2, NotificationCompat.Builder(this, "messages").setSmallIcon(R.drawable.ic_parley)
                    .setContentTitle("Parley").setContentText("Новое сообщение или заявка")
                    .setContentIntent(openApp()).setAutoCancel(true).setVisibility(NotificationCompat.VISIBILITY_PRIVATE).build())
            }
        }
        scope.launch {
            repository.state.map { it.connection to it.error }.distinctUntilChanged().collect { (connection, error) ->
                val text = if (error != null) "Ошибка соединения · откройте Parley" else when (connection) {
                    1 -> "В сети · TCP"; 2 -> "В сети · UDP"; else -> "Подключаемся к Tox…"
                }
                manager.notify(1, ongoing(text))
            }
        }
    }
    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        if (intent?.action == "STOP") {
            getSharedPreferences("settings", MODE_PRIVATE).edit().putBoolean("connection_enabled", false).apply()
            scope.launch { repository.stop(); stopForeground(STOP_FOREGROUND_REMOVE); stopSelf() }
            return START_NOT_STICKY
        }
        if (!getSharedPreferences("settings", MODE_PRIVATE).getBoolean("connection_enabled", true)) {
            stopForeground(STOP_FOREGROUND_REMOVE); stopSelf(); return START_NOT_STICKY
        }
        repository.start()
        return START_STICKY
    }
    override fun onDestroy() {
        repository.notifyIncoming = null
        scope.cancel()
        // The repository serializes cancellation and tox_kill with every JNI call.
        repository.requestStop()
        super.onDestroy()
    }
    override fun onBind(intent: Intent?): IBinder? = null
}
