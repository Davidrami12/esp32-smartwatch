static lv_obj_t *planets_map, *planets_empty;
static lv_obj_t *planet_dots[8];
static lv_obj_t *planet_name;
static int selected_planet=-1;
static const char *planet_names[]={"Mercury","Venus","Earth","Mars","Jupiter","Saturn","Uranus","Neptune"};
static constexpr uint32_t planet_colors[] = {0xA8A29E,0xE8C78C,0x6DBFFF,0xEB8065,0xDAB38A,0xE6D29B,0x8EDDDC,0x738FFF};
static void position_planet_name() {
    if (!planet_name) return;
    if (selected_planet<0 || !astronomy_clock_valid || !watch_planets::valid_clock(astronomy_utc)) {
        lv_obj_add_flag(planet_name,LV_OBJ_FLAG_HIDDEN); return;
    }
    lv_label_set_text(planet_name,planet_names[selected_planet]);
    lv_obj_set_style_text_color(planet_name,lv_color_hex(planet_colors[selected_planet]),0);
    lv_obj_remove_flag(planet_name,LV_OBJ_FLAG_HIDDEN);
    lv_obj_update_layout(planets_map);
    auto *dot=planet_dots[selected_planet];
    const int cx=lv_obj_get_x(dot)+lv_obj_get_width(dot)/2;
    const int cy=lv_obj_get_y(dot)+lv_obj_get_height(dot)/2;
    const int width=lv_obj_get_width(planet_name),height=lv_obj_get_height(planet_name);
    int x=cx+12,y=cy-height/2;
    if (x+width>lv_obj_get_width(planets_map)-4) x=cx-12-width;
    if (x<4) x=4;
    if (y<4) y=4;
    if (y+height>lv_obj_get_height(planets_map)-4) y=lv_obj_get_height(planets_map)-4-height;
    lv_obj_set_pos(planet_name,x,y);
}
static void planet_select_cb(lv_event_t *event) {
    selected_planet=int(reinterpret_cast<intptr_t>(lv_event_get_user_data(event)));
    position_planet_name();
}
static void planets_icon_draw(lv_event_t *event) {
    auto *object=static_cast<lv_obj_t *>(lv_event_get_target(event));
    lv_area_t area; lv_obj_get_coords(object,&area);
    auto *layer=lv_event_get_layer(event);
    const int cx=(area.x1+area.x2)/2,cy=(area.y1+area.y2)/2;
    auto ring_half=[&](int first,int last) {
        for (int step=first;step<last;++step) {
            const double a=step*6.283185307179586/32,b=(step+1)*6.283185307179586/32;
            auto point=[&](double t) {
                const double x=18*cos(t),y=6*sin(t);
                return lv_point_precise_t{static_cast<lv_value_precise_t>(cx+.90*x+.44*y),static_cast<lv_value_precise_t>(cy-.44*x+.90*y)};
            };
            lv_draw_line_dsc_t line; lv_draw_line_dsc_init(&line);
            line.color=lv_color_hex(0x8AC4FF); line.width=2;
            line.p1=point(a); line.p2=point(b); lv_draw_line(layer,&line);
        }
    };
    ring_half(16,32);
    astronomy_circle(layer,cx,cy,10,design::Bluetooth);
    ring_half(0,16);
}
static void planets_draw(lv_event_t *event) {
    if (!astronomy_clock_valid || !watch_planets::valid_clock(astronomy_utc)) return;
    lv_area_t area; lv_obj_get_coords(planets_map,&area);
    auto *layer = lv_event_get_layer(event);
    const int cx=(area.x1+area.x2)/2,cy=(area.y1+area.y2)/2;
    for (int p=0;p<8;++p) {
        lv_draw_arc_dsc_t arc; lv_draw_arc_dsc_init(&arc);
        arc.center={cx,cy}; arc.radius=32+p*18; arc.width=1;
        arc.start_angle=0; arc.end_angle=360; arc.color=lv_color_hex(0x282B35);
        lv_draw_arc(layer,&arc);
    }
    astronomy_circle(layer,cx,cy,8,design::Weather);
}
static void refresh_planets() {
    if (!planets_map || !display_active || current_screen != WatchScreen::Planets) return;
    const bool valid=astronomy_clock_valid && watch_planets::valid_clock(astronomy_utc);
    for (auto *dot : planet_dots) {
        if (valid) lv_obj_remove_flag(dot,LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(dot,LV_OBJ_FLAG_HIDDEN);
    }
    if (!valid) {
        lv_obj_remove_flag(planets_empty,LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(planets_empty,LV_OBJ_FLAG_HIDDEN);
        for (unsigned p=0;p<8;++p) {
            const auto pos=watch_planets::position(p,astronomy_utc);
            const int radius=32+p*18;
            lv_obj_align(planet_dots[p],LV_ALIGN_CENTER,int(lround(cos(pos.longitude)*radius)),int(lround(-sin(pos.longitude)*radius)));
        }
    }
    position_planet_name();
    lv_obj_invalidate(planets_map);
}
static void create_planets_screen() {
    auto *screen=create_screen(WatchScreen::Planets,"Planets");
    planets_map=lv_obj_create(screen); lv_obj_remove_style_all(planets_map);
    lv_obj_set_size(planets_map,390,340); lv_obj_align(planets_map,LV_ALIGN_TOP_MID,0,76);
    lv_obj_remove_flag(planets_map,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(planets_map,planets_draw,LV_EVENT_DRAW_MAIN,nullptr);
    for (unsigned p=0;p<8;++p) {
        auto *dot=planet_dots[p]=lv_obj_create(planets_map); lv_obj_remove_style_all(dot);
        lv_obj_set_size(dot,28,28); lv_obj_remove_flag(dot,LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(dot,LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(dot,planet_select_cb,LV_EVENT_CLICKED,reinterpret_cast<void *>(intptr_t(p)));
        auto *disc=lv_obj_create(dot); lv_obj_remove_style_all(disc);
        lv_obj_set_size(disc,p==4 || p==5 ? 12 : 9,p==4 || p==5 ? 12 : 9);
        lv_obj_center(disc); lv_obj_remove_flag(disc,static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
        lv_obj_set_style_radius(disc,LV_RADIUS_CIRCLE,0);
        lv_obj_set_style_bg_color(disc,lv_color_hex(planet_colors[p]),0); lv_obj_set_style_bg_opa(disc,LV_OPA_COVER,0);
        if (p==2) { lv_obj_set_style_outline_color(disc,lv_color_hex(kWhite),0); lv_obj_set_style_outline_width(disc,2,0); lv_obj_set_style_outline_pad(disc,3,0); }
    }
    planet_name=label(planets_map,"",design::caption(),kWhite,LV_ALIGN_TOP_LEFT,0,0);
    lv_obj_remove_flag(planet_name,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(planet_name,lv_color_hex(0x000000),0);
    lv_obj_set_style_bg_opa(planet_name,LV_OPA_80,0);
    lv_obj_add_flag(planet_name,LV_OBJ_FLAG_HIDDEN);
    planets_empty=label(screen,"Clock required\nConnect Wi-Fi to sync time",design::body(),kMuted,LV_ALIGN_CENTER,0,0);
    lv_obj_set_style_text_align(planets_empty,LV_TEXT_ALIGN_CENTER,0);
}
