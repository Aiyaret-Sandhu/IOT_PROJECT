package com.example.iot_mess_system.viewmodel

import android.content.ContentValues.TAG
import android.util.Log
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.runtime.mutableStateOf
import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import com.example.iot_mess_system.network.WiFiConnectionManager
import kotlinx.coroutines.launch
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

class ESP32ViewModel : ViewModel() {
    private val connectionManager = WiFiConnectionManager()

    val ipAddress = mutableStateOf("192.168.72.172") // Your ESP32's IP
    val port = mutableStateOf("23") // Default Telnet port

    val isConnected = connectionManager.isConnected
    val connectionStatus = connectionManager.connectionStatus

    val serialOutput = mutableStateListOf<SerialLogEntry>()
    val autoScroll = mutableStateOf(true)

    fun connect() {
        viewModelScope.launch {
            val success = connectionManager.connect(ipAddress.value, port.value.toInt())
            if (success) {
                addLogEntry("Connected to ${ipAddress.value}:${port.value}", LogType.SYSTEM)
                collectSerialData()
            }
        }
    }

    fun disconnect() {
        viewModelScope.launch {
            connectionManager.disconnect()
            addLogEntry("Disconnected from server", LogType.SYSTEM)
        }
    }

    fun sendCommand(command: String) {
        viewModelScope.launch {
            if (connectionManager.sendCommand(command)) {
                addLogEntry("> $command", LogType.SENT)
            } else {
                addLogEntry("Failed to send: $command", LogType.ERROR)
            }
        }
    }

    // Just the relevant part that handles receiving data

    // Make sure your ESP32ViewModel has a function like this:
    private fun collectSerialData() {
        viewModelScope.launch {
            connectionManager.receiveData().collect { line ->
                addLogEntry(line, LogType.RECEIVED)
                Log.d(TAG, "ViewModel received line: $line") 
            }
        }
    }
    
    fun clearSerialOutput() {
        serialOutput.clear()
        addLogEntry("Output cleared", LogType.SYSTEM)
    }

    private fun addLogEntry(message: String, type: LogType) {
        val timestamp = SimpleDateFormat("HH:mm:ss", Locale.getDefault()).format(Date())

        // Make sure we're not limiting the buffer too much
        if (serialOutput.size > 1000) {
            // Remove older entries when buffer gets very large
            serialOutput.removeRange(0, 200)
            serialOutput.add(SerialLogEntry("--- Older messages removed ---", timestamp, LogType.SYSTEM))
        }

        serialOutput.add(SerialLogEntry(message, timestamp, type))
    }
}

data class SerialLogEntry(
    val message: String,
    val timestamp: String,
    val type: LogType
)

enum class LogType {
    RECEIVED,
    SENT,
    SYSTEM,
    ERROR
}