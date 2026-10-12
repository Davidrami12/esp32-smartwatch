package dev.esp32.smartwatch.activity

import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.animation.core.tween
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.clickable
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.rememberScrollState
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.semantics.*
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import java.time.Instant
import java.time.format.DateTimeFormatter
import java.util.Locale

private val mint = Color(0xFFB5DEC9)
private val number = java.text.NumberFormat.getIntegerInstance(Locale.ENGLISH)
private val dateFormat = DateTimeFormatter.ofPattern("EEE, dd MMM yyyy", Locale.ENGLISH)
private val updateFormat = DateTimeFormatter.ofPattern("dd MMM, HH:mm:ss z", Locale.ENGLISH).withZone(ActivityDashboardModel.WATCH_ZONE)

@Composable
fun ActivityDashboardScreen(state: ActivityDashboardState, canRefresh: Boolean, back: () -> Unit, refresh: () -> Unit) {
    Scaffold(containerColor = Color.Black) { padding ->
        LazyColumn(Modifier.fillMaxSize().padding(padding), contentPadding = PaddingValues(20.dp),
            verticalArrangement = Arrangement.spacedBy(16.dp)) {
            item {
                Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween, verticalAlignment = Alignment.CenterVertically) {
                    TextButton(onClick = back) { Text("‹  Companion") }
                    TextButton(onClick = refresh, enabled = canRefresh) { Text("Refresh") }
                }
                Text("Activity", fontSize = 38.sp, fontWeight = FontWeight.Bold)
                Text(if (state.live) "●  LIVE · WATCH" else if (state.cache != null) "○  CACHED · NOT LIVE" else "○  WAITING FOR WATCH",
                    color = if (state.live) mint else MaterialTheme.colorScheme.onSurfaceVariant, style = MaterialTheme.typography.labelMedium)
                state.cache?.let { cache ->
                    Text("Last update · ${updateFormat.format(Instant.ofEpochMilli(cache.updatedAt))}", style = MaterialTheme.typography.bodySmall)
                }
                Text("Watch calendar · CET / CEST", style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
            }
            state.error?.let { error -> item { Text(error, color = MaterialTheme.colorScheme.error) } }
            val summary = state.summary
            if (summary == null) {
                item { DashboardCard {
                    Text("Your activity starts here", style = MaterialTheme.typography.titleLarge)
                    Text("Connect your paired watch to load real daily totals and its saved goal. No Health Connect read permission is needed.")
                } }
            } else {
                item { DashboardCard {
                    Text("TODAY · ${summary.today.format(DateTimeFormatter.ofPattern("dd MMM",Locale.ENGLISH))}", color = MaterialTheme.colorScheme.onSurfaceVariant)
                    Text(number.format(summary.todaySteps), fontSize = 58.sp, fontWeight = FontWeight.Bold)
                    Text("steps", style = MaterialTheme.typography.titleMedium)
                    if (!summary.todayRecorded) Text("No watch record for today in this snapshot. Displayed as zero, not a live measurement.", style = MaterialTheme.typography.bodySmall)
                    if (summary.goal == null) {
                        Text("Watch goal unavailable · install goal-enabled firmware.", style = MaterialTheme.typography.bodySmall)
                    } else {
                        val progress by animateFloatAsState(summary.visualProgress, tween(350), label = "Daily goal progress")
                        LinearProgressIndicator(progress = { progress }, modifier = Modifier.fillMaxWidth().height(6.dp), color = mint)
                        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
                            Text("Goal ${number.format(summary.goal)}", style = MaterialTheme.typography.bodyMedium)
                            Text(String.format(Locale.ENGLISH,"%.1f%%",summary.percentage), color = mint)
                        }
                        Text(if (summary.remaining == 0L) "Daily goal reached" else "${number.format(summary.remaining)} steps to go",
                            style = MaterialTheme.typography.titleMedium)
                        state.cache?.goalUpdatedAt?.let { Text("Goal read from watch · ${updateFormat.format(Instant.ofEpochMilli(it))}",
                            style = MaterialTheme.typography.labelSmall, color = MaterialTheme.colorScheme.onSurfaceVariant) }
                    }
                } }
                item { DashboardCard {
                    Text("Last seven days", style = MaterialTheme.typography.titleLarge)
                    ActivityBarChart(summary)
                } }
                item { Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(12.dp)) {
                    Card(Modifier.weight(1f)) { Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                        Text("7-DAY TOTAL", style = MaterialTheme.typography.labelMedium)
                        Text(number.format(summary.total), style = MaterialTheme.typography.headlineSmall, fontWeight = FontWeight.Bold)
                        Text("steps", style = MaterialTheme.typography.bodySmall)
                    } }
                    Card(Modifier.weight(1f)) { Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                        Text("DAILY AVERAGE", style = MaterialTheme.typography.labelMedium)
                        Text(String.format(Locale.ENGLISH,"%,.1f",summary.average), style = MaterialTheme.typography.headlineSmall, fontWeight = FontWeight.Bold)
                        Text("over 7 calendar days", style = MaterialTheme.typography.bodySmall)
                    } }
                } }
                item { Text("Missing dates display zero in this chart only. These are watch daily totals, not precise walking timestamps. Health Connect is an export destination, never this dashboard's data source.",
                    style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant) }
            }
        }
    }
}

@Composable
private fun DashboardCard(content: @Composable ColumnScope.() -> Unit) {
    Card(Modifier.fillMaxWidth()) { Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(12.dp), content = content) }
}

@Composable
private fun ActivityBarChart(summary: ActivitySummary) {
    var selectedDate by rememberSaveable { mutableStateOf<String?>(null) }
    val selected = summary.bars.firstOrNull { it.date.toString() == selectedDate } ?: summary.bars.last()
    val maxSteps = maxOf(1L, summary.bars.maxOf { it.steps })
    BoxWithConstraints(Modifier.fillMaxWidth()) {
        val width = maxOf(maxWidth,336.dp) // Seven accessible 48dp hit targets; scroll on narrow devices.
        Row(Modifier.horizontalScroll(rememberScrollState()).width(width)) {
            summary.bars.forEach { bar ->
                val today = bar.date == summary.today
                val active = bar.date == selected.date
                val height by animateFloatAsState(bar.steps.toFloat()/maxSteps, tween(350), label = "Daily bar height")
                val color = if (today) mint else if (active) Color.White else Color(0xFF777777)
                Column(Modifier.weight(1f).clickable(role = Role.Button) { selectedDate = bar.date.toString() }
                    .semantics(mergeDescendants = true) {
                        contentDescription = "${bar.date.format(dateFormat)}, ${bar.steps} steps${if (today) ", today" else ""}${if (!bar.recorded) ", no record" else ""}"
                        this.selected = active
                    }, horizontalAlignment = Alignment.CenterHorizontally, verticalArrangement = Arrangement.spacedBy(8.dp)) {
                    Text(number.format(bar.steps), fontSize = 10.sp, maxLines = 1, overflow = TextOverflow.Ellipsis)
                    Canvas(Modifier.fillMaxWidth().height(150.dp).padding(horizontal = 8.dp)) {
                        val barHeight = size.height*height
                        if (bar.steps == 0L) drawLine(color,Offset(0f,size.height-1f),Offset(size.width,size.height-1f),2.dp.toPx())
                        else drawRoundRect(color, Offset(0f,size.height-barHeight), Size(size.width,barHeight), CornerRadius(5.dp.toPx()))
                    }
                    Text(bar.date.dayOfWeek.getDisplayName(java.time.format.TextStyle.SHORT,Locale.ENGLISH), style = MaterialTheme.typography.labelSmall,
                        color = if (today) mint else Color.White)
                    Text(if (today) "TODAY" else if (active) "●" else " ", fontSize = 9.sp, color = color)
                }
            }
        }
    }
    HorizontalDivider(color = Color(0xFF333333))
    Text(selected.date.format(dateFormat), style = MaterialTheme.typography.titleMedium)
    Text("${number.format(selected.steps)} steps" + if (!selected.recorded) " · no record" else "", style = MaterialTheme.typography.bodyLarge)
}
