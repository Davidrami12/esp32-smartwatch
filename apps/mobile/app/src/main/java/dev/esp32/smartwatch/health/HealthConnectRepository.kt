package dev.esp32.smartwatch.health

import android.content.Context
import androidx.health.connect.client.HealthConnectClient
import androidx.health.connect.client.permission.HealthPermission
import androidx.health.connect.client.records.StepsRecord
import androidx.health.connect.client.records.metadata.Device
import androidx.health.connect.client.records.metadata.Metadata

/** The only SDK boundary. WRITE_STEPS only: never reads another app's health data. */
class HealthConnectRepository(context: Context) : HealthStepsWriter {
    private val context = context.applicationContext
    fun availability() = HealthConnectClient.getSdkStatus(context)
    private fun client(): HealthConnectClient = HealthConnectClient.getOrCreate(context)
    override suspend fun permitted() = availability() == HealthConnectClient.SDK_AVAILABLE &&
        client().permissionController.getGrantedPermissions().containsAll(PERMISSIONS)
    override suspend fun insert(item: StepSyncItem, revision: StepSyncRevision) {
        if (item.day.count == 0L) {
            // StepsRecord requires >=1. Delete only our exact client ID for a true zero correction.
            client().deleteRecords(StepsRecord::class, emptyList(), listOf(item.id))
        } else client().insertRecords(listOf(record(item, revision)))
    }
    companion object {
        val PERMISSIONS = setOf(HealthPermission.getWritePermission(StepsRecord::class))
        internal fun record(item: StepSyncItem, revision: StepSyncRevision): StepsRecord {
            val day = item.day
            return StepsRecord(
                count = day.count, startTime = day.start, endTime = day.end,
                startZoneOffset = day.startOffset, endZoneOffset = day.endOffset,
                metadata = Metadata.autoRecorded(clientRecordId = item.id, clientRecordVersion = revision.version,
                    device = Device(type = Device.TYPE_WATCH, manufacturer = "Waveshare", model = "ESP32-S3 Touch AMOLED 2.06"))
            )
        }
    }
}
