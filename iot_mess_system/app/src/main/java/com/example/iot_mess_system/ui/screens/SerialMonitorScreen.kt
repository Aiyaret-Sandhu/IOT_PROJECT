package com.example.iot_mess_system.ui.screens

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.KeyboardActions
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Clear
import androidx.compose.material.icons.filled.Refresh
import androidx.compose.material.icons.filled.Send
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.input.ImeAction
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.example.iot_mess_system.viewmodel.ESP32ViewModel
import com.example.iot_mess_system.viewmodel.LogType
import com.example.iot_mess_system.ui.theme.LogColors

@Composable
fun SerialMonitorScreen(
    viewModel: ESP32ViewModel,
    modifier: Modifier = Modifier
) {
    val ipAddress by viewModel.ipAddress
    val port by viewModel.port
    val isConnected by viewModel.isConnected
    val connectionStatus by viewModel.connectionStatus
    val serialOutput = viewModel.serialOutput
    // We'll always auto-scroll but keep the variable in viewModel for future flexibility
    viewModel.autoScroll.value = true  // Force auto-scroll to be always true
    
    var command by remember { mutableStateOf("") }
    val scrollState = rememberLazyListState()

    // Auto-scroll to the bottom when new items are added (always enabled)
    LaunchedEffect(serialOutput.size) {
        if (serialOutput.isNotEmpty()) {
            scrollState.animateScrollToItem(serialOutput.size - 1)
        }
    }

    Column(
        modifier = modifier
            .fillMaxSize()
            .padding(16.dp)
    ) {
        // Connection settings card
        Card(
            modifier = Modifier
                .fillMaxWidth()
                .padding(bottom = 16.dp)
        ) {
            Column(
                modifier = Modifier
                    .padding(16.dp)
            ) {
                Text(
                    text = "ESP32 Connection",
                    style = MaterialTheme.typography.titleLarge
                )

                Spacer(modifier = Modifier.height(16.dp))

                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.spacedBy(8.dp)
                ) {
                    OutlinedTextField(
                        value = ipAddress,
                        onValueChange = { viewModel.ipAddress.value = it },
                        label = { Text("IP Address") },
                        modifier = Modifier.weight(2f),
                        keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Text),
                        enabled = !isConnected
                    )

                    OutlinedTextField(
                        value = port,
                        onValueChange = { viewModel.port.value = it },
                        label = { Text("Port") },
                        modifier = Modifier.weight(1f),
                        keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number),
                        enabled = !isConnected
                    )
                }

                Spacer(modifier = Modifier.height(16.dp))

                Row(
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(vertical = 8.dp),
                    horizontalArrangement = Arrangement.spacedBy(16.dp)
                ) {
                    Button(
                        onClick = { if (isConnected) viewModel.disconnect() else viewModel.connect() },
                        modifier = Modifier
                            .weight(1f)
                            .height(50.dp),
                        shape = RoundedCornerShape(12.dp),
                        colors = ButtonDefaults.buttonColors(
                            containerColor = if (isConnected) Color(0xFFD32F2F) else Color(0xFF1976D2), // Darker red for disconnect, blue for connect
                            contentColor = Color.White
                        ),
                        elevation = ButtonDefaults.elevatedButtonElevation(8.dp)
                    ) {
                        Icon(
                            imageVector = if (isConnected) Icons.Default.Clear else Icons.Default.Refresh, // Changed icon for "Connect"
                            contentDescription = null,
                            modifier = Modifier.size(20.dp),
                            tint = Color.White // Ensures icon is visible on the button
                        )
                        Spacer(modifier = Modifier.width(8.dp))
                        Text(
                            text = if (isConnected) "Disconnect" else "Connect",
                            style = MaterialTheme.typography.bodyMedium,
                            color = Color.White // Ensures text is visible
                        )
                    }

                    Button(
                        onClick = { viewModel.clearSerialOutput() },
                        modifier = Modifier
                            .weight(1f)
                            .height(50.dp),
                        shape = RoundedCornerShape(12.dp),
                        colors = ButtonDefaults.buttonColors(
                            containerColor = Color(0xFF455A64), // Dark gray for the "Clear" button
                            contentColor = Color.White
                        ),
                        elevation = ButtonDefaults.elevatedButtonElevation(8.dp)
                    ) {
                        Icon(
                            imageVector = Icons.Default.Clear,
                            contentDescription = "Clear",
                            modifier = Modifier.size(20.dp),
                            tint = Color.White // Ensures icon is visible on the button
                        )
                        Spacer(modifier = Modifier.width(8.dp))
                        Text(
                            text = "Clear",
                            style = MaterialTheme.typography.bodyMedium,
                            color = Color.White // Ensures text is visible
                        )
                    }
                }

                Spacer(modifier = Modifier.height(8.dp))

                // Status row without auto-scroll toggle
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Text(
                        text = "Status: ",
                        style = MaterialTheme.typography.bodyMedium
                    )
                    Text(
                        text = connectionStatus,
                        style = MaterialTheme.typography.bodyMedium,
                        color = if (isConnected) Color.Green else Color.Red
                    )
                    
                    // Removed the auto-scroll toggle
                }
            }
        }

        // Serial monitor output
        Card(
            modifier = Modifier
                .weight(1f)
                .fillMaxWidth()
        ) {
            Box(
                modifier = Modifier
                    .fillMaxSize()
                    .padding(8.dp)
                    .background(Color(0xFFF5F5F5))
            ) {
                LazyColumn(
                    state = scrollState,
                    modifier = Modifier
                        .fillMaxSize()
                        .padding(8.dp)
                ) {
                    items(serialOutput) { entry ->
                        val textColor = when (entry.type) {
                            LogType.RECEIVED -> LogColors.received
                            LogType.SENT -> LogColors.sent
                            LogType.SYSTEM -> LogColors.system
                            LogType.ERROR -> LogColors.error
                        }

                        Row(
                            modifier = Modifier
                                .fillMaxWidth()
                                .padding(vertical = 2.dp)
                        ) {
                            Text(
                                text = "[${entry.timestamp}] ",
                                color = LogColors.system,
                                fontFamily = FontFamily.Monospace,
                                fontSize = 12.sp
                            )
                            Text(
                                text = entry.message,
                                color = textColor,
                                fontFamily = FontFamily.Monospace,
                                fontSize = 14.sp
                            )
                        }
                    }
                }

                if (serialOutput.isEmpty()) {
                    Text(
                        text = "No data received yet",
                        modifier = Modifier.align(Alignment.Center),
                        color = Color.Gray
                    )
                }
            }
        }

        Spacer(modifier = Modifier.height(16.dp))

        // Command input
        Row(
            modifier = Modifier.fillMaxWidth(),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(8.dp)
        ) {
            OutlinedTextField(
                value = command,
                onValueChange = { command = it },
                label = { Text("Command") },
                modifier = Modifier.weight(1f).height(60.dp),
                enabled = isConnected,
                keyboardOptions = KeyboardOptions(
                    imeAction = ImeAction.Send
                ),
                keyboardActions = KeyboardActions(
                    onSend = {
                        if (command.isNotEmpty() && isConnected) {
                            viewModel.sendCommand(command)
                            command = ""
                        }
                    }
                ),
                trailingIcon = {
                    if (command.isNotEmpty()) {
                        IconButton(onClick = { command = "" }) {
                            Icon(Icons.Default.Clear, contentDescription = "Clear")
                        }
                    }
                }
            )

            Button(
                onClick = {
                    if (command.isNotEmpty()) {
                        viewModel.sendCommand(command)
                        command = ""
                    }
                },
                enabled = isConnected && command.isNotEmpty(),
                modifier = Modifier.height(50.dp),
                shape = RoundedCornerShape(4.dp) // Reduced border radius (default is 8dp)
            ) {
                Icon(
                    Icons.Default.Send, 
                    contentDescription = "Send",
                    modifier = Modifier.size(26.dp)
                )
                Spacer(modifier = Modifier.width(6.dp))
                Text(
                    "Send",
                    fontSize = 16.sp, // Increased text size
                )
            }
        }
    }
}