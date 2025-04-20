package com.example.iot_mess_system.network

import android.util.Log
import androidx.compose.runtime.mutableStateOf
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.flow
import kotlinx.coroutines.withContext
import java.io.BufferedReader
import java.io.InputStreamReader
import java.io.PrintWriter
import java.net.InetSocketAddress
import java.net.Socket
import java.net.SocketException

class WiFiConnectionManager {
    private val TAG = "WiFiConnectionManager"

    private var socket: Socket? = null
    private var reader: BufferedReader? = null
    private var writer: PrintWriter? = null

    val isConnected = mutableStateOf(false)
    val connectionStatus = mutableStateOf("Disconnected")

    suspend fun connect(ipAddress: String, port: Int): Boolean = withContext(Dispatchers.IO) {
        try {
            connectionStatus.value = "Connecting to $ipAddress:$port..."
            socket = Socket()
            val socketAddress = InetSocketAddress(ipAddress, port)
            socket!!.connect(socketAddress, 5000) // 5 second timeout

            reader = BufferedReader(InputStreamReader(socket!!.getInputStream()))
            writer = PrintWriter(socket!!.getOutputStream(), true)

            isConnected.value = true
            connectionStatus.value = "Connected to $ipAddress:$port"
            Log.d(TAG, "Connected to $ipAddress:$port")
            true
        } catch (e: Exception) {
            Log.e(TAG, "Connection error: ${e.message}", e)
            connectionStatus.value = "Connection failed: ${e.message}"
            false
        }
    }

    suspend fun disconnect() = withContext(Dispatchers.IO) {
        try {
            writer?.close()
            reader?.close()
            socket?.close()
        } catch (e: Exception) {
            Log.e(TAG, "Disconnect error: ${e.message}")
        } finally {
            socket = null
            reader = null
            writer = null
            isConnected.value = false
            connectionStatus.value = "Disconnected"
        }
    }

    suspend fun sendCommand(command: String): Boolean = withContext(Dispatchers.IO) {
        try {
            if (isConnected.value && writer != null) {
                writer?.println(command)
                writer?.flush() // Make sure to flush the writer to send the data immediately
                Log.d(TAG, "Command sent: $command")
                true
            } else {
                Log.e(TAG, "Cannot send command: not connected")
                false
            }
        } catch (e: Exception) {
            Log.e(TAG, "Send command error: ${e.message}")
            isConnected.value = false
            connectionStatus.value = "Connection error: ${e.message}"
            false
        }
    }

    fun receiveData(): Flow<String> = flow {
    try {
        val reader = reader ?: return@flow
        val buffer = CharArray(1024)
        var lineBuffer = StringBuilder()

        while (isConnected.value) {
            val bytesRead = withContext(Dispatchers.IO) {
                try {
                    reader.read(buffer)
                } catch (e: Exception) {
                    Log.e(TAG, "Error reading from socket: ${e.message}")
                    -1
                }
            }

            if (bytesRead == -1) {
                // Connection closed
                Log.d(TAG, "Connection closed by server")
                isConnected.value = false
                connectionStatus.value = "Connection closed by server"
                break
            }

            if (bytesRead > 0) {
                val data = String(buffer, 0, bytesRead)
                Log.d(TAG, "Raw received data ($bytesRead bytes): $data")
                
                // Process the received data character by character
                for (c in data) {
                    if (c == '\n') {
                        // End of line reached, emit the complete line
                        val line = lineBuffer.toString().replace("\r", "")
                        if (line.isNotEmpty()) {
                            Log.d(TAG, "Emitting complete line: $line")
                            emit(line)
                        }
                        lineBuffer = StringBuilder() // Reset buffer for next line
                    } else {
                        // Add character to buffer
                        lineBuffer.append(c)
                    }
                }
                
                // If there's any incomplete line in the buffer and it's not just whitespace
                // also emit it as a "partial" line for real-time feedback
                val remainingText = lineBuffer.toString().trim()
                if (remainingText.isNotEmpty()) {
                    Log.d(TAG, "Emitting partial line: $remainingText")
                    emit("$remainingText (...)")
                    // Note: we don't clear the buffer here because this data is incomplete
                }
            }
            
            // Small delay to prevent overwhelming the system
            delay(10)
        }
    } catch (e: Exception) {
        Log.e(TAG, "Receive data error: ${e.message}", e)
        isConnected.value = false
        connectionStatus.value = "Connection error: ${e.message}"
    }
}
}