// Shared LVGL presentation, included inside watch_ui.cpp's private namespace.
namespace astronomy = watch_astronomy;
static int64_t astronomy_utc = 0;
static bool astronomy_clock_valid = false, astronomy_moon_view = false;
static unsigned astronomy_day_offset = 0;
static astronomy::Sun astronomy_sun;
static astronomy::Moon astronomy_moon;
static lv_obj_t *astronomy_date_label, *astronomy_preview_label, *astronomy_graph;
static lv_obj_t *astronomy_first_title, *astronomy_second_title, *astronomy_first_value, *astronomy_second_value;
static lv_obj_t *astronomy_info, *astronomy_next, *astronomy_empty, *astronomy_previous_day, *astronomy_next_day;
static lv_obj_t *astronomy_sun_tab, *astronomy_moon_tab;

static void astronomy_circle(lv_layer_t *layer, int x, int y, int radius, uint32_t color)
{
    lv_draw_rect_dsc_t dsc; lv_draw_rect_dsc_init(&dsc);
    dsc.bg_color = lv_color_hex(color); dsc.bg_opa = LV_OPA_COVER; dsc.radius = LV_RADIUS_CIRCLE;
    lv_area_t bounds = {x-radius,y-radius,x+radius,y+radius};
    lv_draw_rect(layer,&dsc,&bounds);
}

static void astronomy_icon_draw(lv_event_t *event)
{
    auto *object = static_cast<lv_obj_t *>(lv_event_get_target(event));
    lv_area_t area; lv_obj_get_coords(object,&area);
    auto *layer = lv_event_get_layer(event);
    astronomy_circle(layer,area.x1+10,area.y1+15,9,design::Weather);
    astronomy_circle(layer,area.x1+29,area.y1+12,10,kWhite);
    astronomy_circle(layer,area.x1+33,area.y1+8,9,kSurface);
}

static void astronomy_tab_icon_draw(lv_event_t *event)
{
    auto *tab=static_cast<lv_obj_t *>(lv_event_get_target(event));
    lv_area_t area; lv_obj_get_coords(tab,&area);
    auto *layer=lv_event_get_layer(event);
    const int cx=(area.x1+area.x2)/2,cy=(area.y1+area.y2)/2;
    if (tab==astronomy_sun_tab) {
        astronomy_circle(layer,cx,cy,8,design::Weather);
        for (int ray=0;ray<8;++ray) {
            const double angle=ray*6.283185307179586/8;
            lv_draw_line_dsc_t line; lv_draw_line_dsc_init(&line);
            line.color=lv_color_hex(design::Weather); line.width=2;
            line.round_start=line.round_end=1;
            line.p1={static_cast<lv_value_precise_t>(cx+lround(cos(angle)*12)),static_cast<lv_value_precise_t>(cy+lround(sin(angle)*12))};
            line.p2={static_cast<lv_value_precise_t>(cx+lround(cos(angle)*15)),static_cast<lv_value_precise_t>(cy+lround(sin(angle)*15))};
            lv_draw_line(layer,&line);
        }
    } else {
        // Draw only the crescent, leaving the selected-tab surface untouched.
        for (int y=-13;y<=13;++y) {
            const int half=int(sqrt(double(169-y*y)));
            int left=-half,right=half;
            const int inner_y=y+5;
            if (abs(inner_y)<=12) {
                const int cut=5-int(sqrt(double(144-inner_y*inner_y)));
                if (cut<=right) right=cut-1;
            }
            if (right<left) continue;
            lv_draw_rect_dsc_t row; lv_draw_rect_dsc_init(&row);
            row.bg_color=lv_color_hex(design::Timeout); row.bg_opa=LV_OPA_COVER;
            lv_area_t bounds={cx+left,cy+y,cx+right,cy+y};
            lv_draw_rect(layer,&row,&bounds);
        }
    }
}

static void astronomy_graph_draw(lv_event_t *event)
{
    if (!astronomy_clock_valid) return;
    lv_area_t area; lv_obj_get_coords(astronomy_graph,&area);
    auto *layer = lv_event_get_layer(event);
    const int cx = (area.x1+area.x2)/2, cy = (area.y1+area.y2)/2;
    if (astronomy_moon_view) {
        constexpr int radius = 46;
        const double phase = astronomy_moon.phase, curve = cos(phase*6.283185307179586);
        for (int dy = -radius; dy <= radius; ++dy) {
            const int half = int(sqrt(double(radius*radius-dy*dy)));
            lv_draw_rect_dsc_t row; lv_draw_rect_dsc_init(&row);
            row.bg_opa = LV_OPA_COVER; row.bg_color = lv_color_hex(0x202438);
            lv_area_t bounds = {cx-half,cy+dy,cx+half,cy+dy};
            lv_draw_rect(layer,&row,&bounds);
            const int x0 = phase <= .5 ? int(ceil(half*curve)) : -half;
            const int x1 = phase <= .5 ? half : int(floor(-half*curve));
            if (x1 > x0) {
                bounds.x1 = cx+x0; bounds.x2 = cx+x1;
                row.bg_color = lv_color_hex(0xE5E6EF);
                lv_draw_rect(layer,&row,&bounds);
                // Fixed decorative craters: clip every scanline to the lit
                // span, preserving both the lunar limb and phase terminator.
                struct Crater { int x, y, radius; };
                static constexpr Crater craters[] = {
                    {-20,-18,6}, {9,-6,5}, {-8,17,8}, {17,23,4}
                };
                row.bg_color = lv_color_hex(0xA5A6AF);
                for (const auto &crater : craters) {
                    const int offset = dy-crater.y;
                    if (abs(offset) > crater.radius) continue;
                    const int span = int(sqrt(double(crater.radius*crater.radius-offset*offset)));
                    const int left = crater.x-span > x0 ? crater.x-span : x0;
                    const int right = crater.x+span < x1 ? crater.x+span : x1;
                    if (left > right) continue;
                    lv_area_t crater_row = {cx+left,cy+dy,cx+right,cy+dy};
                    lv_draw_rect(layer,&row,&crater_row);
                }
            }
        }
        return;
    }
    const int horizon = area.y2-12, radius = 83;
    lv_draw_arc_dsc_t arc; lv_draw_arc_dsc_init(&arc);
    arc.center = {cx,horizon}; arc.radius = radius; arc.width = 2;
    arc.start_angle = 180; arc.end_angle = 360; arc.color = lv_color_hex(design::Border);
    lv_draw_arc(layer,&arc);
    lv_draw_line_dsc_t line; lv_draw_line_dsc_init(&line);
    line.color = lv_color_hex(design::Secondary); line.width = 1;
    line.p1 = {static_cast<lv_value_precise_t>(area.x1+12),static_cast<lv_value_precise_t>(horizon)};
    line.p2 = {static_cast<lv_value_precise_t>(area.x2-12),static_cast<lv_value_precise_t>(horizon)};
    lv_draw_line(layer,&line);
    for (int angle = 0; angle <= 180; angle += 30) {
        const double a = angle*3.141592653589793/180;
        line.p1 = {static_cast<lv_value_precise_t>(cx-cos(a)*radius),static_cast<lv_value_precise_t>(horizon-sin(a)*radius)};
        line.p2 = {static_cast<lv_value_precise_t>(cx-cos(a)*(radius-7)),static_cast<lv_value_precise_t>(horizon-sin(a)*(radius-7))};
        lv_draw_line(layer,&line);
    }
    // Schematic time-of-day arc, not a calibrated sky map. No moving marker on
    // preview dates, nor for polar day/night where rise/set do not exist.
    if (!astronomy_day_offset && astronomy_sun.rise && astronomy_utc >= astronomy_sun.rise && astronomy_utc <= astronomy_sun.set) {
        const double fraction = double(astronomy_utc-astronomy_sun.rise)/(astronomy_sun.set-astronomy_sun.rise);
        const double a = fraction*3.141592653589793;
        astronomy_circle(layer,cx-int(cos(a)*radius),horizon-int(sin(a)*radius),7,design::Weather);
    }
}

static void astronomy_time_text(char *text, size_t size, int64_t utc)
{
    if (!utc) { snprintf(text,size,"--:--"); return; }
    const int minute = astronomy::madrid_minutes(utc);
    snprintf(text,size,"%02d:%02d",minute/60,minute%60);
}

static void render_astronomy()
{
    if (!astronomy_graph || !display_active || current_screen != WatchScreen::Astronomy) return;
    for (auto *tab : {astronomy_sun_tab,astronomy_moon_tab}) lv_obj_remove_state(tab,LV_STATE_DISABLED);
    lv_obj_add_state(astronomy_moon_view ? astronomy_moon_tab : astronomy_sun_tab,LV_STATE_DISABLED);
    const auto value_color=lv_color_hex(astronomy_moon_view ? design::Timeout : design::Weather);
    lv_obj_set_style_text_color(astronomy_first_value,value_color,0);
    lv_obj_set_style_text_color(astronomy_second_value,value_color,0);
    lv_obj_t *content[] = {astronomy_graph,astronomy_first_title,astronomy_second_title,
        astronomy_first_value,astronomy_second_value,astronomy_info,astronomy_next};
    for (auto *object : content) {
        if (astronomy_clock_valid) lv_obj_remove_flag(object,LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(object,LV_OBJ_FLAG_HIDDEN);
    }
    if (!astronomy_clock_valid) {
        lv_obj_remove_flag(astronomy_empty,LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(astronomy_date_label,"MADRID");
        lv_label_set_text(astronomy_preview_label,"Clock unavailable");
        lv_obj_add_state(astronomy_previous_day,LV_STATE_DISABLED);
        lv_obj_add_state(astronomy_next_day,LV_STATE_DISABLED);
        return;
    }
    lv_obj_add_flag(astronomy_empty,LV_OBJ_FLAG_HIDDEN);
    if (astronomy_day_offset == 0) lv_obj_add_state(astronomy_previous_day,LV_STATE_DISABLED);
    else lv_obj_remove_state(astronomy_previous_day,LV_STATE_DISABLED);
    if (astronomy_day_offset == 6) lv_obj_add_state(astronomy_next_day,LV_STATE_DISABLED);
    else lv_obj_remove_state(astronomy_next_day,LV_STATE_DISABLED);
    const auto date = astronomy::add_days(astronomy::madrid_date(astronomy_utc),astronomy_day_offset);
    lv_label_set_text_fmt(astronomy_date_label,"MADRID · %02d/%02d",date.day,date.month);
    if (astronomy_day_offset) lv_label_set_text_fmt(astronomy_preview_label,"+%u %s · local time",astronomy_day_offset,astronomy_day_offset == 1 ? "day" : "days");
    else lv_label_set_text(astronomy_preview_label,"TODAY · local time");
    astronomy_sun = astronomy::sun(date,astronomy_utc);
    // Preview the same Madrid civil time, not now + N*86400 across a DST change.
    const int local_minute = astronomy::madrid_minutes(astronomy_utc);
    int64_t shown = astronomy::day_number(date)*86400+local_minute*60-3600;
    shown = astronomy::day_number(date)*86400+local_minute*60-astronomy::madrid_offset(shown);
    astronomy_moon = astronomy::moon(astronomy_day_offset ? shown : astronomy_utc);
    char first[32], second[32], noon[12];
    if (astronomy_moon_view) {
        lv_label_set_text(astronomy_first_title,"ILLUMINATION");
        lv_label_set_text(astronomy_second_title,"DAYS TO FULL");
        lv_label_set_text_fmt(astronomy_first_value,"%.0f%%",astronomy_moon.illumination);
        lv_label_set_text_fmt(astronomy_second_value,"%u",unsigned(ceil(double(astronomy_moon.next_full-(astronomy_day_offset ? shown : astronomy_utc))/86400)));
        lv_label_set_text(astronomy_info,astronomy_moon.name);
        const auto next_new = astronomy::madrid_date(astronomy_moon.next_new);
        lv_label_set_text_fmt(astronomy_next,"New moon %02d/%02d · approximate",next_new.day,next_new.month);
    } else {
        lv_label_set_text(astronomy_first_title,"SUNRISE");
        lv_label_set_text(astronomy_second_title,"SUNSET");
        astronomy_time_text(first,sizeof(first),astronomy_sun.rise);
        astronomy_time_text(second,sizeof(second),astronomy_sun.set);
        lv_label_set_text(astronomy_first_value,first); lv_label_set_text(astronomy_second_value,second);
        astronomy_time_text(noon,sizeof(noon),astronomy_sun.noon);
        if (astronomy_sun.polar_day || astronomy_sun.polar_night) {
            lv_label_set_text(astronomy_info,astronomy_sun.polar_day ? "Polar day" : "Polar night");
            lv_label_set_text(astronomy_next,"No sunrise/sunset on this date");
        } else {
            const int daylight = int((astronomy_sun.set-astronomy_sun.rise+30)/60);
            lv_label_set_text_fmt(astronomy_info,"Day %dh %02dm · Noon %s",daylight/60,daylight%60,noon);
            if (astronomy_day_offset) lv_label_set_text(astronomy_next,"Date preview · estimated sun times");
            else {
                const bool before_rise = astronomy_utc < astronomy_sun.rise;
                const bool before_set = astronomy_utc < astronomy_sun.set;
                const auto tomorrow = astronomy::sun(astronomy::add_days(date,1),astronomy_utc);
                const int64_t next = before_rise ? astronomy_sun.rise : before_set ? astronomy_sun.set : tomorrow.rise;
                const int minutes = int((next-astronomy_utc+59)/60);
                lv_label_set_text_fmt(astronomy_next,"%s in %dh %02dm",before_rise || !before_set ? "Sunrise" : "Sunset",minutes/60,minutes%60);
            }
        }
    }
    lv_obj_invalidate(astronomy_graph);
}

static void refresh_astronomy() { render_astronomy(); }
static void astronomy_tab_cb(lv_event_t *event)
{
    astronomy_moon_view = lv_event_get_target(event) == astronomy_moon_tab;
    render_astronomy();
}
static void astronomy_day_cb(lv_event_t *event)
{
    if (!astronomy_clock_valid) return;
    if (lv_event_get_target(event) == astronomy_next_day && astronomy_day_offset < 6) ++astronomy_day_offset;
    else if (lv_event_get_target(event) == astronomy_previous_day && astronomy_day_offset) --astronomy_day_offset;
    render_astronomy();
}

static void create_astronomy_screen()
{
    auto *screen = create_screen(WatchScreen::Astronomy,"Sun & Moon");
    astronomy_sun_tab = button(screen,"",160,56,LV_ALIGN_TOP_MID,-88,80,astronomy_tab_cb);
    astronomy_moon_tab = button(screen,"",160,56,LV_ALIGN_TOP_MID,88,80,astronomy_tab_cb);
    lv_obj_add_event_cb(astronomy_sun_tab,astronomy_tab_icon_draw,LV_EVENT_DRAW_MAIN,nullptr);
    lv_obj_add_event_cb(astronomy_moon_tab,astronomy_tab_icon_draw,LV_EVENT_DRAW_MAIN,nullptr);
    lv_obj_set_style_text_color(astronomy_sun_tab,lv_color_hex(design::Weather),0);
    lv_obj_set_style_text_color(astronomy_moon_tab,lv_color_hex(design::Timeout),0);
    // Keep category colors on the captions even when the selected tab uses
    // the disabled-state background (whose inherited text color is grey).
    lv_obj_set_style_text_color(lv_obj_get_child(astronomy_sun_tab,0),lv_color_hex(design::Weather),0);
    lv_obj_set_style_text_color(lv_obj_get_child(astronomy_moon_tab,0),lv_color_hex(design::Timeout),0);
    astronomy_previous_day = button(screen,LV_SYMBOL_LEFT,56,56,LV_ALIGN_TOP_LEFT,32,144,astronomy_day_cb);
    astronomy_next_day = button(screen,LV_SYMBOL_RIGHT,56,56,LV_ALIGN_TOP_RIGHT,-32,144,astronomy_day_cb);
    lv_obj_set_style_radius(astronomy_previous_day,LV_RADIUS_CIRCLE,0);
    lv_obj_set_style_radius(astronomy_next_day,LV_RADIUS_CIRCLE,0);
    astronomy_date_label = label(screen,"MADRID",design::body(),kWhite,LV_ALIGN_TOP_MID,0,148);
    astronomy_preview_label = label(screen,"",design::caption(),kMuted,LV_ALIGN_TOP_MID,0,174);
    astronomy_graph = lv_obj_create(screen); lv_obj_remove_style_all(astronomy_graph);
    lv_obj_set_size(astronomy_graph,design::ContentWidth,104); lv_obj_align(astronomy_graph,LV_ALIGN_TOP_MID,0,206);
    lv_obj_remove_flag(astronomy_graph,static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
    lv_obj_add_event_cb(astronomy_graph,astronomy_graph_draw,LV_EVENT_DRAW_MAIN,nullptr);
    astronomy_empty = label(screen,"Waiting for clock\nConnect Wi-Fi to sync time",design::body(),kMuted,LV_ALIGN_TOP_MID,0,230);
    lv_obj_set_style_text_align(astronomy_empty,LV_TEXT_ALIGN_CENTER,0);
    astronomy_first_title = label(screen,"SUNRISE",design::caption(),kMuted,LV_ALIGN_TOP_MID,-88,316);
    astronomy_second_title = label(screen,"SUNSET",design::caption(),kMuted,LV_ALIGN_TOP_MID,88,316);
    astronomy_first_value = label(screen,"--:--",design::title(),design::Weather,LV_ALIGN_TOP_MID,-88,336);
    astronomy_second_value = label(screen,"--:--",design::title(),design::Weather,LV_ALIGN_TOP_MID,88,336);
    astronomy_info = label(screen,"",design::caption(),kWhite,LV_ALIGN_TOP_MID,0,376);
    astronomy_next = label(screen,"",design::caption(),kMuted,LV_ALIGN_TOP_MID,0,396);
    for (auto *object : {astronomy_info,astronomy_next}) {
        lv_obj_set_width(object,design::ContentWidth);
        lv_obj_set_style_text_align(object,LV_TEXT_ALIGN_CENTER,0);
        lv_label_set_long_mode(object,LV_LABEL_LONG_DOT);
    }
}
