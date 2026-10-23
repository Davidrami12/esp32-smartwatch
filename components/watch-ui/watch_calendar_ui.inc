static watch_ui_calendar_request_cb_t calendar_request_cb=nullptr;
struct CalendarCacheStorage { watch_calendar_week_t weeks[6]; };
static CalendarCacheStorage *calendar_cache_storage=nullptr;
#define calendar_cache (calendar_cache_storage->weeks)
static unsigned calendar_cache_next=0,calendar_selected_day=0;
static uint32_t calendar_selected_week=0,calendar_requested_at=0;
static bool calendar_dirty=true,calendar_pending=false,calendar_was_connected=false,calendar_failed=false;
static lv_obj_t *calendar_week_label,*calendar_status,*calendar_day_headers[7],*calendar_list;
static uint32_t calendar_rendered_week=0;
static bool calendar_scroll_to_day=true;
static lv_obj_t *calendar_previous,*calendar_next,*calendar_today,*calendar_detail_text;
static uint32_t calendar_month=0,calendar_month_requested=0;
static unsigned calendar_month_index=0;
static bool calendar_month_sync=false,calendar_month_connected=false,calendar_month_invalidated=false,calendar_month_failed=false;
static lv_obj_t *calendar_month_heading,*calendar_month_status,*calendar_cells[42],*calendar_cell_dates[42],*calendar_cell_bars[42][3],*calendar_cell_more[42];
static constexpr uint32_t CalendarRestDayColor=0xC1CED8;
static void refresh_calendar_month();
static void calendar_month_start();
static void create_calendar_month_screen();

static uint32_t calendar_today_date()
{
    if (!astronomy_clock_valid) return 0;
    const auto d=astronomy::madrid_date(astronomy_utc);
    return uint32_t(d.year*10000+d.month*100+d.day);
}
static const watch_calendar_week_t *calendar_cached()
{
    for (const auto &week : calendar_cache) if (week.monday==calendar_selected_week) return &week;
    return nullptr;
}
static void calendar_request()
{
    calendar_month_sync=false;
    calendar_pending=calendar_request_cb && bluetooth_state==WATCH_UI_BLUETOOTH_CONNECTED;
    calendar_failed=false;
    calendar_requested_at=lv_tick_get();
    if (calendar_request_cb && calendar_selected_week) calendar_request_cb(calendar_selected_week);
}
static void calendar_select_today()
{
    const uint32_t today=calendar_today_date();
    if (!watch_calendar_valid_date(today)) return;
    calendar_selected_week=watch_calendar_monday(today);
    calendar_selected_day=unsigned(watch_calendar_day(today)-watch_calendar_day(calendar_selected_week));
    calendar_scroll_to_day=true;
    calendar_dirty=true; calendar_request(); refresh_calendar();
}
static void open_calendar_cb(lv_event_t *)
{
    if (!calendar_selected_week) calendar_select_today();
    if (!calendar_month) calendar_month=calendar_today_date()/100*100+1;
    calendar_month_start();
    navigate_to(WatchScreen::CalendarMonth);
}
static void calendar_move(int delta)
{
    if (!calendar_selected_week) return;
    const uint32_t next=watch_calendar_date(watch_calendar_day(calendar_selected_week)+delta*7);
    if (!watch_calendar_valid_date(next) || !watch_calendar_valid_date(watch_calendar_date(watch_calendar_day(next)+6))) return;
    calendar_selected_week=next; calendar_selected_day=0; calendar_scroll_to_day=true;
    calendar_dirty=true; calendar_request(); refresh_calendar();
}
static void calendar_detail_cb(lv_event_t *event)
{
    const auto *week=calendar_cached(); const unsigned code=unsigned(reinterpret_cast<uintptr_t>(lv_event_get_user_data(event)));
    const unsigned index=code&255,day=code>>8;
    if (!week || index>=week->count || day>6 || !(week->events[index].days&(1u<<day))) return;
    calendar_selected_day=day;
    const auto &e=week->events[index];
    const uint32_t date=watch_calendar_date(watch_calendar_day(calendar_selected_week)+calendar_selected_day);
    lv_label_set_text_fmt(calendar_detail_text,"%s\n\n%02lu/%02lu/%lu / MADRID\n%s\n\n%s%s",e.title,
        static_cast<unsigned long>(date%100),static_cast<unsigned long>(date/100%100),static_cast<unsigned long>(date/10000),e.time,e.location[0] ? "LOCATION\n" : "",e.location);
    lv_obj_scroll_to_y(lv_obj_get_parent(calendar_detail_text),0,LV_ANIM_OFF);
    navigate_to(WatchScreen::CalendarDetail);
}
static void refresh_calendar(bool force)
{
    if (current_screen==WatchScreen::CalendarMonth) { refresh_calendar_month(); return; }
    if (!calendar_status || !display_active || current_screen!=WatchScreen::Calendar) return;
    const bool connected=bluetooth_state==WATCH_UI_BLUETOOTH_CONNECTED;
    const bool reconnected=connected && !calendar_was_connected;
    calendar_was_connected=connected;
    if (reconnected && calendar_selected_week) calendar_request();
    if (!calendar_selected_week && calendar_today_date()) { calendar_select_today(); return; }
    if (calendar_pending && lv_tick_elaps(calendar_requested_at)>130000) calendar_pending=false;
    const auto *week=calendar_cached();
    const char *status=!calendar_selected_week ? "WAITING FOR WATCH DATE" : calendar_pending && connected ? (week ? "SYNCING / CACHED" : "SYNCING WITH PHONE") :
        !connected ? (week && week->status==CalendarReady ? "OFFLINE / CACHED" : "NOT SYNCED / CONNECT PHONE") :
        !week ? "NOT SYNCED / TAP WEEK RANGE TO RETRY" : week->status==CalendarOff ? "ENABLE CALENDAR IN PHONE APP" :
        week->status==CalendarPermission ? "ALLOW CALENDAR ACCESS ON PHONE" : week->status==CalendarGoogleAuth ? "CONNECT GOOGLE IN PHONE APP" : week->status==CalendarNoSelection ? "SELECT CALENDARS ON PHONE" :
        week->status==CalendarError ? "SYNC FAILED / TAP WEEK RANGE TO RETRY" : week->truncated ? "FIRST 32 EVENTS / MADRID TIME" : "SYNCED / MADRID TIME";
    if (week && week->status==CalendarReady && !calendar_pending) {
        const uint32_t age=astronomy_utc>week->fetched_at ? uint32_t((astronomy_utc-week->fetched_at)/60) : 0;
        lv_label_set_text_fmt(calendar_status,"%s / %lum old",!connected ? "OFFLINE / CACHED" : calendar_failed ? "SYNC FAILED / CACHED" : week->truncated ? "32 EVENTS MAX" : "SYNCED",static_cast<unsigned long>(age));
    } else lv_label_set_text(calendar_status,status);
    if (!calendar_dirty && !force) return;
    calendar_dirty=false;
    const int64_t start=calendar_selected_week ? watch_calendar_day(calendar_selected_week) : 0;
    const uint32_t end=watch_calendar_date(start+6);
    if (calendar_selected_week) {
        lv_label_set_text_fmt(calendar_week_label,"%02lu/%02lu - %02lu/%02lu\n%lu",static_cast<unsigned long>(calendar_selected_week%100),
            static_cast<unsigned long>(calendar_selected_week/100%100),static_cast<unsigned long>(end%100),static_cast<unsigned long>(end/100%100),static_cast<unsigned long>(calendar_selected_week/10000));
        if (calendar_selected_week/10000!=end/10000) lv_label_set_text_fmt(calendar_week_label,"%02lu/%02lu - %02lu/%02lu\n%lu - %lu",
            static_cast<unsigned long>(calendar_selected_week%100),static_cast<unsigned long>(calendar_selected_week/100%100),static_cast<unsigned long>(end%100),static_cast<unsigned long>(end/100%100),
            static_cast<unsigned long>(calendar_selected_week/10000),static_cast<unsigned long>(end/10000));
    }
    else lv_label_set_text(calendar_week_label,"WEEK / --");
    const int32_t previous_scroll=lv_obj_get_scroll_y(calendar_list);
    const bool select_day=calendar_scroll_to_day || calendar_rendered_week!=calendar_selected_week;
    lv_obj_clean(calendar_list); memset(calendar_day_headers,0,sizeof(calendar_day_headers));
    if (week && week->status==CalendarReady) {
        const char *names[]={"Monday","Tuesday","Wednesday","Thursday","Friday","Saturday","Sunday"};
        int y=0,selected_y=0;
        for (unsigned d=0;d<7;++d) {
            const uint32_t date=watch_calendar_date(start+d);
            if (d==calendar_selected_day) selected_y=y;
            const bool rest_day=d>=5 || (week->holiday_days&(1u<<d));
            auto *heading=calendar_day_headers[d]=label(calendar_list,"",watch_font_18(),rest_day ? CalendarRestDayColor : kWhite,LV_ALIGN_TOP_LEFT,0,y);
            lv_label_set_text_fmt(heading,"%s  %02lu/%02lu",names[d],static_cast<unsigned long>(date%100),static_cast<unsigned long>(date/100%100));
            lv_obj_set_width(heading,330); lv_label_set_long_mode(heading,LV_LABEL_LONG_DOT);
            y+=30; unsigned visible=0;
            for (unsigned n=0;n<week->count;++n) {
                const auto &e=week->events[n]; if (!(e.days&(1u<<d))) continue;
                auto *row=button(calendar_list,"",330,82,LV_ALIGN_TOP_LEFT,0,y,nullptr);
                lv_obj_add_event_cb(row,calendar_detail_cb,LV_EVENT_CLICKED,reinterpret_cast<void *>(uintptr_t(n|(d<<8))));
                lv_obj_set_style_border_color(row,lv_color_hex(e.color),0); lv_obj_set_style_border_width(row,2,0);
                auto *time=label(row,e.time,design::caption(),kWhite,LV_ALIGN_TOP_LEFT,10,8);
                auto *title=label(row,e.title,watch_font_18(),kWhite,LV_ALIGN_TOP_LEFT,10,34);
                for (auto *text : {time,title}) { lv_obj_set_width(text,306); lv_label_set_long_mode(text,LV_LABEL_LONG_DOT); }
                y+=90; ++visible;
            }
            if (!visible) {
                label(calendar_list,week->truncated ? "No events in limited sync" : "No events",design::caption(),kMuted,LV_ALIGN_TOP_LEFT,8,y);
                y+=28;
            }
            y+=14;
        }
        // Allow even Sunday's heading to align to the top when selected.
        auto *tail=lv_obj_create(calendar_list); lv_obj_remove_style_all(tail);
        lv_obj_set_size(tail,1,1); lv_obj_set_pos(tail,0,y+lv_obj_get_height(calendar_list)-1);
        lv_obj_remove_flag(tail,static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE));
        lv_obj_update_layout(calendar_list);
        lv_obj_scroll_to_y(calendar_list,select_day ? selected_y : previous_scroll,LV_ANIM_OFF);
        calendar_scroll_to_day=false;
    } else {
        label(calendar_list,"Your weekly agenda will appear here",design::caption(),kMuted,LV_ALIGN_TOP_MID,0,52);
        lv_obj_scroll_to_y(calendar_list,0,LV_ANIM_OFF);
    }
    calendar_rendered_week=calendar_selected_week;
}
static void create_calendar_screens()
{
#ifdef ESP_PLATFORM
    calendar_cache_storage=static_cast<CalendarCacheStorage *>(heap_caps_calloc(1,sizeof(CalendarCacheStorage),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
#else
    calendar_cache_storage=new CalendarCacheStorage{};
#endif
    LV_ASSERT_MALLOC(calendar_cache_storage);
    auto *screen=create_screen(WatchScreen::Calendar,"Weekly agenda");
    calendar_previous=button(screen,LV_SYMBOL_LEFT,56,56,LV_ALIGN_TOP_LEFT,32,74,[](lv_event_t *) { calendar_move(-1); });
    calendar_next=button(screen,LV_SYMBOL_RIGHT,56,56,LV_ALIGN_TOP_RIGHT,-32,74,[](lv_event_t *) { calendar_move(1); });
    calendar_today=button(screen,"WEEK / --",210,56,LV_ALIGN_TOP_MID,0,74,[](lv_event_t *) { calendar_request(); refresh_calendar(); });
    calendar_week_label=lv_obj_get_child(calendar_today,0);
    lv_obj_set_style_text_font(calendar_week_label,design::caption(),0);
    lv_obj_set_style_text_align(calendar_week_label,LV_TEXT_ALIGN_CENTER,0);
    calendar_status=label(screen,"NOT SYNCED",&lv_font_montserrat_12,kMuted,LV_ALIGN_TOP_MID,0,142);
    lv_obj_set_width(calendar_status,346); lv_obj_set_style_text_align(calendar_status,LV_TEXT_ALIGN_CENTER,0);
    lv_label_set_long_mode(calendar_status,LV_LABEL_LONG_DOT);
    calendar_list=lv_obj_create(screen); lv_obj_remove_style_all(calendar_list);
    lv_obj_set_size(calendar_list,346,238); lv_obj_align(calendar_list,LV_ALIGN_TOP_MID,0,174);
    lv_obj_set_scroll_dir(calendar_list,LV_DIR_VER); lv_obj_set_scrollbar_mode(calendar_list,LV_SCROLLBAR_MODE_AUTO);
    auto *detail=create_screen(WatchScreen::CalendarDetail,"Event details");
    auto *scroll=lv_obj_create(detail); lv_obj_remove_style_all(scroll); lv_obj_set_size(scroll,346,330);
    lv_obj_align(scroll,LV_ALIGN_TOP_MID,0,76); lv_obj_set_scroll_dir(scroll,LV_DIR_VER);
    calendar_detail_text=label(scroll,"",watch_font_18(),kWhite,LV_ALIGN_TOP_LEFT,0,0);
    lv_obj_set_width(calendar_detail_text,330); lv_label_set_long_mode(calendar_detail_text,LV_LABEL_LONG_WRAP);
    create_calendar_month_screen();
}

// Six bounded weekly snapshots cover every visible cell, including neighbouring
// month dates. Requests are serialized through the existing low-priority channel.
static int64_t calendar_month_start_day()
{
    return watch_calendar_day(watch_calendar_monday(calendar_month));
}
static const watch_calendar_week_t *calendar_find_week(uint32_t monday)
{
    for (const auto &week : calendar_cache) if (week.monday==monday) return &week;
    return nullptr;
}
static void calendar_month_start()
{
    calendar_pending=false; calendar_month_requested=0; calendar_month_index=0;
    calendar_month_failed=false;
    calendar_month_sync=watch_calendar_valid_date(calendar_month) && calendar_request_cb && bluetooth_state==WATCH_UI_BLUETOOTH_CONNECTED;
}
static void calendar_month_move(int delta)
{
    if (!watch_calendar_valid_date(calendar_month)) return;
    int year=int(calendar_month/10000),month=int(calendar_month/100%100)+delta;
    if (month<1) { month=12; --year; } else if (month>12) { month=1; ++year; }
    const uint32_t date=uint32_t(year*10000+month*100+1);
    if (!watch_calendar_valid_date(date)) return;
    calendar_month=date; calendar_month_start(); refresh_calendar_month();
}
static void calendar_month_open_day(unsigned index)
{
    if (!watch_calendar_valid_date(calendar_month)) return;
    const uint32_t date=watch_calendar_date(calendar_month_start_day()+index);
    if (!watch_calendar_valid_date(date)) return;
    calendar_selected_week=watch_calendar_monday(date);
    calendar_selected_day=unsigned(watch_calendar_day(date)-watch_calendar_day(calendar_selected_week));
    calendar_scroll_to_day=true;
    calendar_dirty=true; calendar_request(); navigate_to(WatchScreen::Calendar);
}
static void refresh_calendar_month()
{
    if (!calendar_month_status || !display_active || current_screen!=WatchScreen::CalendarMonth) return;
    const bool connected=bluetooth_state==WATCH_UI_BLUETOOTH_CONNECTED;
    if (!watch_calendar_valid_date(calendar_month)) {
        const auto today=calendar_today_date();
        if (!watch_calendar_valid_date(today)) { lv_label_set_text(calendar_month_status,"Last refresh -- / waiting for date"); return; }
        calendar_month=today/100*100+1; calendar_month_start();
    }
    if (connected && !calendar_month_connected) calendar_month_start();
    calendar_month_connected=connected;
    if (!connected) { calendar_month_sync=false; calendar_pending=false; }
    if (calendar_pending && lv_tick_elaps(calendar_requested_at)>130000) { calendar_pending=false; calendar_month_sync=false; calendar_month_failed=true; }
    if (calendar_month_sync && !calendar_pending) {
        if (calendar_month_index==6) calendar_month_sync=false;
        else {
            calendar_month_requested=watch_calendar_date(calendar_month_start_day()+7*calendar_month_index++);
            if (watch_calendar_valid_date(calendar_month_requested)) {
                calendar_pending=true; calendar_requested_at=lv_tick_get();
                calendar_request_cb(calendar_month_requested);
            } else calendar_month_sync=false;
        }
    }
    static const char *months[]={"January","February","March","April","May","June","July","August","September","October","November","December"};
    lv_label_set_text_fmt(calendar_month_heading,"%s %lu",months[calendar_month/100%100-1],static_cast<unsigned long>(calendar_month/10000));
    unsigned ready=0; bool limited=false; watch_calendar_status_t issue=CalendarReady;
    uint32_t oldest=0;
    for (unsigned w=0;w<6;++w) {
        const auto *week=calendar_find_week(watch_calendar_date(calendar_month_start_day()+w*7));
        if (week) {
            if (week->status==CalendarReady) {
                ++ready;
                if (week->fetched_at && (!oldest || week->fetched_at<oldest)) oldest=week->fetched_at;
            } else issue=week->status;
            limited|=week->truncated;
        }
    }
    const char *status=issue==CalendarOff ? " / sharing off" : issue==CalendarPermission ? " / allow access on phone" : issue==CalendarGoogleAuth ? " / connect Google on phone" :
        issue==CalendarNoSelection ? " / select calendars" : issue==CalendarError || calendar_month_failed ? " / refresh failed" :
        calendar_month_sync ? " / refreshing" : !connected ? " / offline" :
        ready<6 ? " / partial" : limited ? " / limited" : "";
    if (oldest && astronomy_clock_valid && astronomy_utc>=oldest) {
        const uint32_t minutes=uint32_t((astronomy_utc-oldest)/60);
        const uint32_t age=minutes>=1440 ? minutes/1440 : minutes>=60 ? minutes/60 : minutes;
        lv_label_set_text_fmt(calendar_month_status,"Last refresh %lu%s ago%s",static_cast<unsigned long>(age),minutes>=1440 ? "d" : minutes>=60 ? "h" : "m",status);
    } else lv_label_set_text_fmt(calendar_month_status,"Last refresh --%s",status);
    for (unsigned i=0;i<42;++i) {
        const uint32_t date=watch_calendar_date(calendar_month_start_day()+i);
        const auto *week=calendar_find_week(watch_calendar_monday(date));
        const bool known=week && week->status==CalendarReady;
        const bool rest_day=i%7>=5 || (known && (week->holiday_days&(1u<<(i%7))));
        const bool in_month=date/100==calendar_month/100;
        lv_label_set_text_fmt(calendar_cell_dates[i],"%lu",static_cast<unsigned long>(date%100));
        lv_obj_set_style_text_font(calendar_cell_dates[i],in_month ? &lv_font_montserrat_14 : &lv_font_montserrat_12,0);
        lv_obj_set_style_text_color(calendar_cell_dates[i],lv_color_hex(!in_month ? 0x454C53 : rest_day ? CalendarRestDayColor : kWhite),0);
        lv_obj_set_style_bg_color(calendar_cells[i],lv_color_hex(!in_month ? 0x060606 : rest_day ? 0x101418 : 0x000000),0);
        const bool today=date==calendar_today_date();
        lv_obj_set_style_border_color(calendar_cells[i],lv_color_hex(today ? 0xF1F4F6 : !in_month ? 0x15191C : 0x35414A),0);
        lv_obj_set_style_border_width(calendar_cells[i],today ? 2 : 1,0);
        lv_obj_set_style_text_color(calendar_cell_more[i],lv_color_hex(!in_month ? 0x454C53 : rest_day ? CalendarRestDayColor : kWhite),0);
        unsigned count=0;
        if (known) for (unsigned e=0;e<week->count;++e) if (week->events[e].days&(1u<<(i%7))) {
            if (count<3) lv_obj_set_style_bg_color(calendar_cell_bars[i][count],lv_color_hex(week->events[e].color),0);
            ++count;
        }
        for (unsigned b=0;b<3;++b) {
            lv_obj_set_style_bg_opa(calendar_cell_bars[i][b],in_month ? LV_OPA_COVER : LV_OPA_20,0);
            if (b>=count) lv_obj_add_flag(calendar_cell_bars[i][b],LV_OBJ_FLAG_HIDDEN);
            else lv_obj_remove_flag(calendar_cell_bars[i][b],LV_OBJ_FLAG_HIDDEN);
        }
        lv_label_set_text(calendar_cell_more[i],count>3 || (known && week->truncated) ? "+" : "");
    }
}
static void create_calendar_month_screen()
{
    auto *screen=create_screen(WatchScreen::CalendarMonth,"Calendar");
    button(screen,LV_SYMBOL_LEFT,56,56,LV_ALIGN_TOP_LEFT,32,74,[](lv_event_t *) { calendar_month_move(-1); });
    button(screen,LV_SYMBOL_RIGHT,56,56,LV_ALIGN_TOP_RIGHT,-32,74,[](lv_event_t *) { calendar_month_move(1); });
    auto *week=button(screen,"",210,56,LV_ALIGN_TOP_MID,0,74,[](lv_event_t *) {
        const uint32_t today=calendar_today_date();
        if (!watch_calendar_valid_date(today)) return;
        calendar_month=today/100*100+1;
        calendar_month_start(); refresh_calendar_month();
    });
    calendar_month_heading=lv_obj_get_child(week,0);
    lv_obj_set_style_text_font(calendar_month_heading,&lv_font_montserrat_22,0);
    lv_obj_set_width(calendar_month_heading,202);
    lv_obj_set_style_text_align(calendar_month_heading,LV_TEXT_ALIGN_CENTER,0);
    lv_label_set_long_mode(calendar_month_heading,LV_LABEL_LONG_DOT);
    static const char *names[]={"Mo","Tu","We","Th","Fr","Sa","Su"};
    for (unsigned i=0;i<7;++i) label(screen,names[i],&lv_font_montserrat_14,i>=5 ? CalendarRestDayColor : kMuted,LV_ALIGN_TOP_LEFT,39+int(i)*50,138);
    for (unsigned i=0;i<42;++i) {
        auto *cell=calendar_cells[i]=lv_button_create(screen);
        lv_obj_remove_style_all(cell); lv_obj_set_size(cell,48,36); lv_obj_set_pos(cell,31+int(i%7)*50,160+int(i/7)*37);
        lv_obj_set_style_bg_opa(cell,LV_OPA_COVER,0); lv_obj_set_style_border_width(cell,1,0); lv_obj_set_style_radius(cell,3,0);
        lv_obj_remove_flag(cell,LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_event_cb(cell,[](lv_event_t *e) { calendar_month_open_day(unsigned(reinterpret_cast<uintptr_t>(lv_event_get_user_data(e)))); },LV_EVENT_CLICKED,reinterpret_cast<void *>(uintptr_t(i)));
        calendar_cell_dates[i]=label(cell,"",&lv_font_montserrat_14,kWhite,LV_ALIGN_TOP_LEFT,4,1);
        calendar_cell_more[i]=label(cell,"",&lv_font_montserrat_14,kWhite,LV_ALIGN_TOP_RIGHT,-2,0);
        for (unsigned b=0;b<3;++b) {
            auto *bar=calendar_cell_bars[i][b]=lv_obj_create(cell); lv_obj_remove_style_all(bar);
            lv_obj_set_size(bar,32,2); lv_obj_set_pos(bar,5,21+int(b)*4); lv_obj_set_style_bg_opa(bar,LV_OPA_COVER,0);
            lv_obj_remove_flag(bar,LV_OBJ_FLAG_CLICKABLE); lv_obj_remove_flag(bar,LV_OBJ_FLAG_SCROLLABLE);
        }
    }
    calendar_month_status=label(screen,"Last refresh --",&lv_font_montserrat_12,kMuted,LV_ALIGN_TOP_MID,0,390);
    lv_obj_set_width(calendar_month_status,346); lv_label_set_long_mode(calendar_month_status,LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(calendar_month_status,LV_TEXT_ALIGN_CENTER,0);
    lv_obj_set_height(calendar_month_status,24);
    lv_obj_add_flag(calendar_month_status,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(calendar_month_status,[](lv_event_t *) { calendar_month_start(); refresh_calendar_month(); },LV_EVENT_CLICKED,nullptr);
}

static void calendar_icon_draw(lv_event_t *event)
{
    lv_area_t a; lv_obj_get_coords(static_cast<lv_obj_t *>(lv_event_get_target(event)),&a);
    auto *layer=lv_event_get_layer(event);
    lv_draw_rect_dsc_t d; lv_draw_rect_dsc_init(&d);
    d.bg_opa=LV_OPA_TRANSP; d.border_width=2; d.border_color=lv_color_hex(0xA8C5DA); d.radius=4;
    lv_area_t body={a.x1+3,a.y1+6,a.x1+36,a.y1+37}; lv_draw_rect(layer,&d,&body);
    d.bg_opa=LV_OPA_COVER; d.bg_color=lv_color_hex(0xA8C5DA); d.border_width=0; d.radius=1;
    lv_area_t header={a.x1+4,a.y1+8,a.x1+35,a.y1+14}; lv_draw_rect(layer,&d,&header);
    for (int x : {11,28}) {
        lv_area_t ring={a.x1+x,a.y1+2,a.x1+x+2,a.y1+10}; lv_draw_rect(layer,&d,&ring);
    }
    for (unsigned i=0;i<6;++i) {
        const int x=a.x1+10+int(i%3)*8,y=a.y1+21+int(i/3)*8;
        lv_area_t day={x,y,x+3,y+3}; lv_draw_rect(layer,&d,&day);
    }
}
