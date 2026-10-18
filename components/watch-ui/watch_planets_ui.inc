static lv_obj_t *planets_map, *planets_empty;
static lv_obj_t *planet_dots[8];
static lv_obj_t *planet_labels[8];
static const char *planet_names[]={"Mercury","Venus","Earth","Mars","Jupiter","Saturn","Uranus","Neptune"};
static constexpr uint32_t planet_colors[] = {0xA8A29E,0xE8C78C,0x6DBFFF,0xEB8065,0xDAB38A,0xE6D29B,0x8EDDDC,0x738FFF};
static void position_planet_names() {
    const bool valid=astronomy_clock_valid && watch_planets::valid_clock(astronomy_utc);
    for (auto *text : planet_labels) {
        if (valid) lv_obj_remove_flag(text,LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(text,LV_OBJ_FLAG_HIDDEN);
    }
    if (!valid) return;
    lv_obj_update_layout(planets_map);
    // Edge-constrained outer planets get first choice; otherwise Neptune can
    // be pushed far away by labels already occupying its only available side.
    for (int p=7;p>=0;--p) {
        auto *text=planet_labels[p],*dot=planet_dots[p];
        const int cx=lv_obj_get_x(dot)+lv_obj_get_width(dot)/2;
        const int cy=lv_obj_get_y(dot)+lv_obj_get_height(dot)/2;
        const int width=lv_obj_get_width(text),height=lv_obj_get_height(text);
        int best_x=4,best_y=4,best_score=1000000;
        // Small bounded search on entry/wake, not a redraw-time layout loop.
        for (int offset : {0,-18,18,-36,36,-54,54,-72,72}) {
            for (int side : {1,-1}) {
                int x=side>0 ? cx+13 : cx-13-width,y=cy-height/2+offset;
                if (x<4 || x+width>lv_obj_get_width(planets_map)-4 || y<4 || y+height>lv_obj_get_height(planets_map)-4) continue;
                int score=abs(offset)*3+(side<0 ? 3 : 0);
                for (unsigned previous=p+1;previous<8;++previous) {
                    auto *other=planet_labels[previous];
                    const int ox=lv_obj_get_x(other),oy=lv_obj_get_y(other);
                    if (x<ox+lv_obj_get_width(other)+3 && x+width+3>ox && y<oy+lv_obj_get_height(other)+3 && y+height+3>oy) score+=10000;
                }
                for (auto *planet : planet_dots) {
                    const int px=lv_obj_get_x(planet)+lv_obj_get_width(planet)/2;
                    const int py=lv_obj_get_y(planet)+lv_obj_get_height(planet)/2;
                    if (x<px+11 && x+width>px-11 && y<py+11 && y+height>py-11) score+=1000;
                }
                if (score<best_score) { best_score=score; best_x=x; best_y=y; }
            }
        }
        lv_obj_set_pos(text,best_x,best_y);
        lv_obj_update_layout(text);
    }
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
        arc.center={cx,cy}; arc.radius=34+p*19; arc.width=1;
        arc.start_angle=0; arc.end_angle=360; arc.color=lv_color_hex(0x282B35);
        lv_draw_arc(layer,&arc);
    }
    // Small static outward-fading glow, drawn only with the existing map.
    // No animation, timer or runtime blur.
    for (int radius=27;radius>13;--radius) {
        lv_draw_rect_dsc_t glow; lv_draw_rect_dsc_init(&glow);
        glow.bg_color=lv_color_hex(0xFFD65A);
        glow.bg_opa=static_cast<lv_opa_t>(5+(27-radius)*3);
        glow.radius=LV_RADIUS_CIRCLE;
        lv_area_t bounds={cx-radius,cy-radius,cx+radius,cy+radius};
        lv_draw_rect(layer,&glow,&bounds);
    }
    astronomy_circle(layer,cx,cy,13,design::Weather);
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
            const int radius=34+p*19;
            lv_obj_align(planet_dots[p],LV_ALIGN_CENTER,int(lround(cos(pos.longitude)*radius)),int(lround(-sin(pos.longitude)*radius)));
        }
    }
    position_planet_names();
    lv_obj_invalidate(planets_map);
}
static void create_planets_screen() {
    auto *screen=create_screen(WatchScreen::Planets,"Planets");
    planets_map=lv_obj_create(screen); lv_obj_remove_style_all(planets_map);
    lv_obj_set_size(planets_map,404,354); lv_obj_align(planets_map,LV_ALIGN_TOP_MID,0,64);
    lv_obj_remove_flag(planets_map,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(planets_map,planets_draw,LV_EVENT_DRAW_MAIN,nullptr);
    for (unsigned p=0;p<8;++p) {
        auto *dot=planet_dots[p]=lv_obj_create(planets_map); lv_obj_remove_style_all(dot);
        lv_obj_set_size(dot,44,44); lv_obj_remove_flag(dot,LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_remove_flag(dot,LV_OBJ_FLAG_CLICKABLE);
        auto *disc=lv_obj_create(dot); lv_obj_remove_style_all(disc);
        lv_obj_set_size(disc,p==4 || p==5 ? 22 : 18,p==4 || p==5 ? 22 : 18);
        lv_obj_center(disc); lv_obj_remove_flag(disc,static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
        lv_obj_set_style_radius(disc,LV_RADIUS_CIRCLE,0);
        lv_obj_set_style_bg_color(disc,lv_color_hex(planet_colors[p]),0); lv_obj_set_style_bg_opa(disc,LV_OPA_COVER,0);
        if (p==2) { lv_obj_set_style_outline_color(disc,lv_color_hex(kWhite),0); lv_obj_set_style_outline_width(disc,2,0); lv_obj_set_style_outline_pad(disc,3,0); }
    }
    for (unsigned p=0;p<8;++p) {
        auto *text=planet_labels[p]=label(planets_map,planet_names[p],design::caption(),planet_colors[p],LV_ALIGN_TOP_LEFT,0,0);
        lv_obj_remove_flag(text,LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_bg_opa(text,LV_OPA_TRANSP,0);
        lv_obj_add_flag(text,LV_OBJ_FLAG_HIDDEN);
    }
    planets_empty=label(screen,"Clock required\nConnect Wi-Fi to sync time",design::body(),kMuted,LV_ALIGN_CENTER,0,0);
    lv_obj_set_style_text_align(planets_empty,LV_TEXT_ALIGN_CENTER,0);
}
