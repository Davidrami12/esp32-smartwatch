// Original LVGL adaptation inspired by app-pixels Weather v1.0.1 (MIT).
// See docs/third-party/app-pixels-weather-LICENSE.txt. No Arduino/SD/network code reused.
static watch_weather_dashboard_t weather_dashboard={};
static bool weather_dashboard_online=false;
static uint32_t weather_dashboard_age=0;
static lv_obj_t *wx_status,*wx_temp,*wx_condition,*wx_icon,*wx_stats[3],*wx_grid,*wx_days[3];
static lv_obj_t *wx_day_icons[3],*wx_day_high[3],*wx_day_low[3];
static watch_ui_forecast_day_t wx_icon_data[4]={};

static lv_obj_t *wx_text(lv_obj_t *parent,const char *text,const lv_font_t *font,uint32_t color,int x,int y,int width)
{
    auto *obj=label(parent,text,font,color,LV_ALIGN_TOP_LEFT,x,y);
    lv_obj_set_width(obj,width); lv_label_set_long_mode(obj,LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(obj,LV_TEXT_ALIGN_CENTER,0);
    lv_obj_remove_flag(obj,LV_OBJ_FLAG_CLICKABLE);
    return obj;
}
static void wx_rule(lv_obj_t *parent,int x,int y,int width,int height)
{
    auto *line=lv_obj_create(parent); lv_obj_remove_style_all(line);
    lv_obj_set_pos(line,x,y); lv_obj_set_size(line,width,height);
    lv_obj_set_style_bg_color(line,lv_color_hex(0x30343A),0); lv_obj_set_style_bg_opa(line,LV_OPA_COVER,0);
    lv_obj_remove_flag(line,LV_OBJ_FLAG_CLICKABLE); lv_obj_remove_flag(line,LV_OBJ_FLAG_SCROLLABLE);
}
static lv_obj_t *wx_make_icon(lv_obj_t *parent,int x,int y,int size,unsigned index)
{
    auto *icon=lv_obj_create(parent); lv_obj_remove_style_all(icon);
    lv_obj_set_size(icon,size,size); lv_obj_set_pos(icon,x,y);
    lv_obj_remove_flag(icon,LV_OBJ_FLAG_CLICKABLE); lv_obj_remove_flag(icon,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(icon,weather_draw_cb,LV_EVENT_DRAW_MAIN,&wx_icon_data[index]);
    return icon;
}
static void create_weather_dashboard()
{
    auto *screen=create_screen(WatchScreen::Weather,"Madrid weather");
    wx_status=wx_text(screen,"Loading weather...",design::caption(),kMuted,32,76,346);
    wx_icon=wx_make_icon(screen,52,104,64,3);
    wx_condition=wx_text(screen,"--",design::caption(),design::Weather,24,174,120);
    wx_temp=wx_text(screen,"--°C",&lv_font_montserrat_48,design::Weather,144,102,234);
    wx_text(screen,"CURRENT / °C",design::caption(),kMuted,144,174,234);
    const char *names[]={"WIND","HUMIDITY","RAIN 1h"};
    for (unsigned i=0;i<3;++i) {
        wx_text(screen,names[i],design::caption(),kMuted,32+int(i)*116,202,114);
        wx_stats[i]=wx_text(screen,"--",&lv_font_montserrat_16,kWhite,32+int(i)*116,224,114);
    }
    wx_rule(screen,32,250,346,1);
    wx_grid=button(screen,"",346,128,LV_ALIGN_TOP_MID,0,258,[](lv_event_t *) { navigate_to(WatchScreen::WeatherWeek); });
    lv_obj_set_style_border_width(wx_grid,0,0); lv_obj_set_style_bg_opa(wx_grid,LV_OPA_TRANSP,0);
    for (unsigned d=0;d<3;++d) {
        if (d) wx_rule(wx_grid,d*115,0,1,128);
        wx_days[d]=wx_text(wx_grid,"--",design::caption(),d ? kMuted : kWhite,d*115+2,0,111);
        wx_rule(wx_grid,d*115+4,23,107,1);
        wx_day_icons[d]=wx_make_icon(wx_grid,d*115+40,30,36,d);
        wx_text(wx_grid,"MAX",&lv_font_montserrat_14,kMuted,d*115+8,80,36);
        wx_text(wx_grid,"MIN",&lv_font_montserrat_14,kMuted,d*115+8,106,36);
        wx_day_high[d]=wx_text(wx_grid,"--",&lv_font_montserrat_20,kWhite,d*115+44,76,64);
        wx_day_low[d]=wx_text(wx_grid,"--",&lv_font_montserrat_20,0x9BCBFF,d*115+44,102,64);
    }
    wx_text(screen,"Open-Meteo / tap grid for 7 days",design::caption(),kMuted,32,394,346);
    render_weather_dashboard();
}
static void render_weather_dashboard()
{
    if (!wx_status) return;
    const auto &data=weather_dashboard;
    lv_label_set_text_fmt(wx_status,"%s / %lum old",!weather_dashboard_online ? "OFFLINE / CACHED" :
        weather_dashboard_age>=35 ? "STALE / CACHED" : "MADRID",static_cast<unsigned long>(weather_dashboard_age));
    if (!data.available) lv_label_set_text(wx_status,weather_dashboard_online ? "Current weather unavailable" : "Weather unavailable / offline");
    const char *conditions[]={"SUNNY","CLOUDY","RAIN","STORM","WINDY"};
    const char *directions[]={"N","NE","E","SE","S","SW","W","NW"};
    if (data.available) {
        lv_label_set_text_fmt(wx_temp,"%.1f°C",data.temperature_c);
        const unsigned condition=data.condition<5 ? data.condition : 1;
        lv_obj_set_style_text_color(wx_temp,lv_color_hex(weather_icon_color(static_cast<watch_ui_weather_condition_t>(condition))),0);
        lv_label_set_text(wx_condition,conditions[condition]);
        wx_icon_data[3].condition=static_cast<watch_ui_weather_condition_t>(condition);
        lv_obj_remove_flag(wx_icon,LV_OBJ_FLAG_HIDDEN); lv_obj_invalidate(wx_icon);
        lv_label_set_text_fmt(wx_stats[0],"%.0f km/h %s",data.wind_kmh,directions[(static_cast<unsigned>((data.wind_degrees+22.5f)/45.f))%8]);
        lv_label_set_text_fmt(wx_stats[1],"%.0f%%",data.humidity_percent);
        if (data.rain_available) lv_label_set_text_fmt(wx_stats[2],"%.1f mm",data.rain_mm);
        else lv_label_set_text(wx_stats[2],"-- mm");
    } else {
        lv_label_set_text(wx_temp,"--°C"); lv_label_set_text(wx_condition,"NO DATA");
        lv_obj_add_flag(wx_icon,LV_OBJ_FLAG_HIDDEN);
        for (auto *stat : wx_stats) lv_label_set_text(stat,"--");
    }
    static const char *weekdays[]={"SUN","MON","TUE","WED","THU","FRI","SAT"};
    for (unsigned d=0;d<3;++d) {
        lv_label_set_text(wx_days[d],d==0 ? "TODAY" : forecast_days[d].date ? weekdays[forecast_days[d].weekday%7] : "--");
        const auto &day=forecast_days[d];
        if (day.available) {
            wx_icon_data[d].condition=day.condition;
            lv_label_set_text_fmt(wx_day_high[d],"%.0f°",day.high_c);
            lv_label_set_text_fmt(wx_day_low[d],"%.0f°",day.low_c);
            lv_obj_remove_flag(wx_day_icons[d],LV_OBJ_FLAG_HIDDEN); lv_obj_invalidate(wx_day_icons[d]);
        } else {
            lv_label_set_text(wx_day_high[d],"--"); lv_label_set_text(wx_day_low[d],"--");
            lv_obj_add_flag(wx_day_icons[d],LV_OBJ_FLAG_HIDDEN);
        }
    }
}
