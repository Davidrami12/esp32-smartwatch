// On-device, settings-based guidance. No invented drain or runtime estimates.
static lv_obj_t *battery_saver_summary, *battery_saver_list, *battery_saver_details[4];
static lv_obj_t *battery_saver_cards[4];
static int battery_saver_percent=-1;
static bool battery_saver_charging=false;

static void refresh_battery_saver()
{
    if (!battery_saver_summary || !display_active || current_screen!=WatchScreen::BatterySaver) return;
    if (battery_saver_percent<0) lv_label_set_text(battery_saver_summary,"Battery level unavailable");
    else lv_label_set_text_fmt(battery_saver_summary,"%d%% / %s",battery_saver_percent,
        battery_saver_charging ? "CHARGING" : battery_saver_percent<=20 ? "LOW BATTERY" : "ON BATTERY");
    lv_obj_set_style_text_color(battery_saver_summary,lv_color_hex(battery_saver_percent>=0 && battery_saver_percent<=20 && !battery_saver_charging ? design::BatteryCritical : 0xA8D98C),0);
    lv_label_set_text_fmt(battery_saver_details[0],"%u%% brightness. %s",current_brightness,
        current_brightness>30 ? "Try 30% or lower indoors.\nA dimmer screen uses less power." : "Already low.\nKeep it comfortable to read.");
    if (!display_timeout_ms) lv_label_set_text(battery_saver_details[1],"Screen timeout is Never.\nChoose 5 or 10 seconds to reduce\nscreen-on time.");
    else lv_label_set_text_fmt(battery_saver_details[1],"%lu-second timeout.\n%s",static_cast<unsigned long>(display_timeout_ms/1000),
        display_timeout_ms>10000 ? "Try 5 or 10 seconds to reduce\nscreen-on time." : "Already short. 5 seconds saves\nmore screen-on time.");
    lv_label_set_text(battery_saver_details[2],wifi_state==WATCH_UI_WIFI_CONNECTED ?
        "Connected. Turn off when not needed.\nClock and weather updates need\nWi-Fi connectivity." : wifi_state==WATCH_UI_WIFI_CONNECTING ?
        "Connecting. If no network is nearby,\nturn off Wi-Fi to avoid retries.\nClock/weather updates will pause." :
        "No connection reported.\nKeep Wi-Fi off when not needed.\nClock/weather updates need Wi-Fi.");
    lv_label_set_text(battery_saver_details[3],bluetooth_state==WATCH_UI_BLUETOOTH_DISABLED ?
        "Bluetooth is disabled.\nPhone features are unavailable.\nEnable it when you need them." :
        "Bluetooth prevents light sleep\non this build. Turning it off stops\nphone, media and assistant links.");
}

static void create_battery_saver_screen()
{
    auto *screen=create_screen(WatchScreen::BatterySaver,"Battery saver");
    battery_saver_summary=label(screen,"Battery level unavailable",design::body(),0xA8D98C,LV_ALIGN_TOP_MID,0,76);
    label(screen,"SETTINGS INSIGHTS / SCROLL FOR TIPS",design::caption(),kMuted,LV_ALIGN_TOP_MID,0,104);
    battery_saver_list=lv_obj_create(screen); lv_obj_remove_style_all(battery_saver_list);
    lv_obj_set_size(battery_saver_list,design::ContentWidth,280);
    lv_obj_align(battery_saver_list,LV_ALIGN_TOP_MID,0,130);
    lv_obj_set_flex_flow(battery_saver_list,LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(battery_saver_list,12,0);
    lv_obj_set_scroll_dir(battery_saver_list,LV_DIR_VER);
    const char *names[]={"BRIGHTNESS  " LV_SYMBOL_RIGHT,"SCREEN TIMEOUT  " LV_SYMBOL_RIGHT,"WI-FI  " LV_SYMBOL_RIGHT,"BLUETOOTH  " LV_SYMBOL_RIGHT};
    static WatchScreen targets[]={WatchScreen::Brightness,WatchScreen::Timeout,WatchScreen::Wifi,WatchScreen::Bluetooth};
    for (unsigned i=0;i<4;++i) {
        auto *card=battery_saver_cards[i]=button(battery_saver_list,names[i],design::ContentWidth,132,LV_ALIGN_TOP_LEFT,0,0,nullptr);
        lv_obj_add_event_cb(card,[](lv_event_t *event) {
            battery_saver_return=true;
            navigate_to(*static_cast<const WatchScreen *>(lv_event_get_user_data(event)));
        },LV_EVENT_CLICKED,&targets[i]);
        lv_obj_set_flex_grow(card,0);
        auto *title=lv_obj_get_child(card,0);
        lv_obj_align(title,LV_ALIGN_TOP_LEFT,16,12);
        lv_obj_set_style_text_font(title,design::caption(),0);
        lv_obj_set_style_text_color(title,lv_color_hex(0xA8D98C),0);
        auto *detail=battery_saver_details[i]=label(card,"",design::caption(),kWhite,LV_ALIGN_TOP_LEFT,16,40);
        lv_obj_set_width(detail,design::ContentWidth-32);
        lv_label_set_long_mode(detail,LV_LABEL_LONG_WRAP);
        lv_obj_remove_flag(detail,LV_OBJ_FLAG_CLICKABLE);
    }
    auto *note=label(battery_saver_list,"Guidance, not measured app drain.\nNo battery-life estimate or automatic\nradio changes.",design::caption(),kMuted,LV_ALIGN_TOP_LEFT,0,0);
    lv_obj_set_width(note,design::ContentWidth);
}
