// Guarded independently of telemetry; no filesystem or UI access on NimBLE host.
static portMUX_TYPE calendar_lock=portMUX_INITIALIZER_UNLOCKED;
struct CalendarTransportStorage {
    WatchCalendarReceiver receiver;
    watch_calendar_week_t received;
};
static CalendarTransportStorage *calendar_storage=nullptr;
#define calendar_receiver (calendar_storage->receiver)
#define calendar_received (calendar_storage->received)
static bool calendar_received_ready=false,calendar_clear_pending=false;
static int64_t calendar_started=0;

static int calendar_access(ble_gatt_access_ctxt *context)
{
    if (!calendar_storage) return BLE_ATT_ERR_INSUFFICIENT_RES;
    if (context->op==BLE_GATT_ACCESS_OP_READ_CHR) {
        uint8_t request[10]={4,0}; // v4 adds Google authorization status; framing/schema stay v1/2.
        portENTER_CRITICAL(&calendar_lock);
        const uint32_t id=calendar_receiver.request,week=calendar_receiver.monday;
        portEXIT_CRITICAL(&calendar_lock);
        for (unsigned i=0;i<4;++i) { request[2+i]=uint8_t(id>>(8*i)); request[6+i]=uint8_t(week>>(8*i)); }
        return os_mbuf_append(context->om,request,sizeof(request)) ? BLE_ATT_ERR_INSUFFICIENT_RES : 0;
    }
    if (context->op!=BLE_GATT_ACCESS_OP_WRITE_CHR) return BLE_ATT_ERR_WRITE_NOT_PERMITTED;
    uint8_t frame[244]; const size_t n=OS_MBUF_PKTLEN(context->om);
    if (n<2 || n>sizeof(frame) || os_mbuf_copydata(context->om,0,n,frame)) return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    if (n==2 && frame[0]==1 && frame[1]==0x7f) { watch_ble_calendar_clear(); return 0; }
    const int64_t now=esp_timer_get_time();
    portENTER_CRITICAL(&calendar_lock);
    if (n>10 && !watch_calendar_le(frame+6,2)) calendar_started=now;
    if (now-calendar_started>120000000) calendar_receiver.reset();
    const bool final=calendar_receiver.complete(frame,n);
    if (final) calendar_received_ready=false;
    const bool accepted=now-calendar_started<=120000000 && calendar_receiver.accept(frame,n,calendar_received);
    if (accepted && final) calendar_received_ready=true;
    portEXIT_CRITICAL(&calendar_lock);
    memset(frame,0,sizeof(frame));
    if (accepted && final) ESP_LOGI(kTag,"Calendar snapshot accepted");
    return accepted ? 0 : BLE_ATT_ERR_VALUE_NOT_ALLOWED;
}
