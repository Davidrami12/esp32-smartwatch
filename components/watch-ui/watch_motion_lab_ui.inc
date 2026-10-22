static WatchMotionLab motion_lab;
static lv_obj_t *motion_canvas,*motion_readout,*motion_status,*motion_center,*motion_tabs[3];
static lv_obj_t *motion_accel_hint;
static bool motion_center_confirmed=false;
static uint32_t motion_centered_at=0;
static lv_timer_t *motion_timer;
static unsigned motion_mode=0;
static bool motion_demo=false;
static constexpr uint32_t motion_colors[]={0x6EDFD1,0xAB94F5,0xF5B96A};

static void motion_line(lv_layer_t *layer,float x1,float y1,float x2,float y2,uint32_t color,int width=2) {
    lv_draw_line_dsc_t line; lv_draw_line_dsc_init(&line);
    line.color=lv_color_hex(color); line.width=width;
    line.p1={static_cast<lv_value_precise_t>(x1),static_cast<lv_value_precise_t>(y1)};
    line.p2={static_cast<lv_value_precise_t>(x2),static_cast<lv_value_precise_t>(y2)};
    lv_draw_line(layer,&line);
}
static void motion_circle(lv_layer_t *layer,int x,int y,int radius,uint32_t color,int width=2) {
    lv_draw_arc_dsc_t arc; lv_draw_arc_dsc_init(&arc);
    arc.center={x,y}; arc.radius=radius; arc.width=width;
    arc.start_angle=0; arc.end_angle=360; arc.color=lv_color_hex(color); lv_draw_arc(layer,&arc);
}
static void motion_icon_draw(lv_event_t *event) {
    lv_area_t a; lv_obj_get_coords(static_cast<lv_obj_t *>(lv_event_get_target(event)),&a);
    auto *layer=lv_event_get_layer(event);
    const int x=(a.x1+a.x2)/2,y=(a.y1+a.y2)/2;
    const int points[][2]={{0,-17},{16,-8},{16,10},{0,19},{-16,10},{-16,-8},{0,1}};
    const unsigned edges[][2]={{0,1},{1,2},{2,3},{3,4},{4,5},{5,0},{5,6},{1,6},{3,6}};
    for (unsigned i=0;i<9;++i) motion_line(layer,x+points[edges[i][0]][0],y+points[edges[i][0]][1],
        x+points[edges[i][1]][0],y+points[edges[i][1]][1],motion_colors[i/3],2);
}
static void motion_draw(lv_event_t *event) {
    if (!motion_lab.fresh(lv_tick_get())) return;
    auto *obj=static_cast<lv_obj_t *>(lv_event_get_target(event));
    lv_area_t a; lv_obj_get_coords(obj,&a);
    auto *layer=lv_event_get_layer(event);
    const int cx=(a.x1+a.x2)/2,cy=(a.y1+a.y2)/2;
    const float roll=motion_lab.relative_roll(),pitch=motion_lab.relative_pitch();
    if (motion_mode==0) {
        float points[8][2];
        for (unsigned i=0;i<8;++i) {
            const float x=(i&1) ? 41.f : -41.f,y=(i&2) ? 41.f : -41.f,z=(i&4) ? 41.f : -41.f;
            const float xx=.82f*x+.57f*z,zz=-.57f*x+.82f*z;
            const float yy=cosf(pitch)*y-sinf(pitch)*zz,depth=sinf(pitch)*y+cosf(pitch)*zz;
            const float scale=240.f/(240.f+depth);
            points[i][0]=cx+(cosf(roll)*xx-sinf(roll)*yy)*scale;
            points[i][1]=cy+(sinf(roll)*xx+cosf(roll)*yy)*scale;
        }
        for (unsigned i=0;i<8;++i) for (unsigned axis=0;axis<3;++axis) {
            const unsigned j=i^(1u<<axis);
            if (i<j) motion_line(layer,points[i][0],points[i][1],points[j][0],points[j][1],motion_colors[axis],3);
        }
    } else if (motion_mode==1) {
        motion_circle(layer,cx,cy,70,design::Border);
        motion_circle(layer,cx,cy,36,design::Border,1);
        motion_line(layer,cx-74,cy,cx+74,cy,design::Border,1);
        motion_line(layer,cx,cy-74,cx,cy+74,design::Border,1);
        float bx=sinf(roll),by=sinf(pitch),length=sqrtf(bx*bx+by*by);
        if (length>1) { bx/=length; by/=length; }
        const bool level=fabsf(roll)<.035f && fabsf(pitch)<.035f;
        motion_circle(layer,cx+int(bx*54),cy+int(by*54),13,level ? 0xA8D98C : motion_colors[0],6);
    } else {
        const float scale=(lv_obj_get_height(obj)-24)/8.f;
        for (int g=-4;g<=4;g+=2) motion_line(layer,a.x1+8,cy-g*scale,a.x2-8,cy-g*scale,design::Border,1);
        for (unsigned n=1;n<motion_lab.count;++n) {
            const unsigned prev=(motion_lab.next+80-motion_lab.count+n-1)%80,cur=(prev+1)%80;
            for (unsigned axis=0;axis<3;++axis) {
                auto bound=[](float v) { return fmaxf(-4.f,fminf(4.f,v)); };
                motion_line(layer,a.x1+8+(n-1)*4,cy-bound(motion_lab.history[prev][axis])*scale,
                    a.x1+8+n*4,cy-bound(motion_lab.history[cur][axis])*scale,motion_colors[axis]);
            }
        }
    }
}
static void motion_tick(lv_timer_t *) {
    if (!display_active || current_screen!=WatchScreen::MotionLab) return;
    const uint32_t now=lv_tick_get();
    if (motion_demo) {
        const float t=now*.001f,r=.65f*sinf(t*.7f),p=.5f*cosf(t*.5f);
        motion_lab.sample(-sinf(p)*9.80665f,sinf(r)*cosf(p)*9.80665f,cosf(r)*cosf(p)*9.80665f,now);
    }
    const bool fresh=motion_lab.fresh(now);
    lv_label_set_text(motion_status,!fresh ? "MOTION SENSOR UNAVAILABLE" : motion_demo ? "SIMULATED MOTION / DEMO" : motion_mode==2 ? "LIVE ACCELERATION / X Y Z" : "GRAVITY TILT / NO COMPASS HEADING");
    if (!fresh) lv_label_set_text(motion_readout,"Waiting for a fresh sensor sample");
    else if (motion_mode==2) lv_label_set_text_fmt(motion_readout,"X %+.2f   Y %+.2f   Z %+.2f g",motion_lab.x,motion_lab.y,motion_lab.z);
    else lv_label_set_text_fmt(motion_readout,"ROLL %+.0f°    PITCH %+.0f°",motion_lab.relative_roll()*57.29578f,motion_lab.relative_pitch()*57.29578f);
    if (fresh && motion_mode!=2) lv_obj_remove_state(motion_center,LV_STATE_DISABLED);
    else lv_obj_add_state(motion_center,LV_STATE_DISABLED);
    if (motion_center_confirmed && uint32_t(now-motion_centered_at)>=1200) {
        motion_center_confirmed=false;
        lv_label_set_text(lv_obj_get_child(motion_center,0),"SET LEVEL / RECENTER");
    }
    lv_obj_invalidate(motion_canvas);
}
static void refresh_motion_lab() {
    if (!motion_timer) return;
    if (display_active && current_screen==WatchScreen::MotionLab) { lv_timer_resume(motion_timer); motion_tick(nullptr); }
    else lv_timer_pause(motion_timer);
}
static void motion_select(unsigned mode) {
    motion_mode=mode;
    motion_center_confirmed=false;
    lv_label_set_text(lv_obj_get_child(motion_center,0),"SET LEVEL / RECENTER");
    if (mode==2) {
        lv_obj_add_flag(motion_center,LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(motion_accel_hint,LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_remove_flag(motion_center,LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(motion_accel_hint,LV_OBJ_FLAG_HIDDEN);
    }
    for (unsigned i=0;i<3;++i) {
        lv_obj_set_style_border_color(motion_tabs[i],lv_color_hex(i==mode ? motion_colors[i] : design::Border),0);
        lv_obj_set_style_text_color(lv_obj_get_child(motion_tabs[i],0),lv_color_hex(i==mode ? motion_colors[i] : kWhite),0);
    }
    motion_tick(nullptr);
}
static void create_motion_lab_screen() {
    auto *screen=create_screen(WatchScreen::MotionLab,"Motion Lab");
    const char *names[]={"CUBE","LEVEL","ACCEL"};
    for (unsigned i=0;i<3;++i) {
        motion_tabs[i]=button(screen,names[i],104,design::TouchHeight,LV_ALIGN_TOP_MID,(int(i)-1)*114,74,[](lv_event_t *e) {
            for (unsigned n=0;n<3;++n) if (lv_event_get_target(e)==motion_tabs[n]) motion_select(n);
        });
        lv_obj_set_style_text_font(lv_obj_get_child(motion_tabs[i],0),design::caption(),0);
    }
    motion_status=label(screen,"MOTION SENSOR UNAVAILABLE",&lv_font_montserrat_14,kWhite,LV_ALIGN_TOP_MID,0,138);
    lv_obj_set_width(motion_status,design::ContentWidth);
    lv_label_set_long_mode(motion_status,LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(motion_status,LV_TEXT_ALIGN_CENTER,0);
    motion_canvas=lv_obj_create(screen); lv_obj_remove_style_all(motion_canvas);
    lv_obj_set_size(motion_canvas,336,158); lv_obj_align(motion_canvas,LV_ALIGN_TOP_MID,0,162);
    lv_obj_remove_flag(motion_canvas,LV_OBJ_FLAG_SCROLLABLE); lv_obj_remove_flag(motion_canvas,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(motion_canvas,motion_draw,LV_EVENT_DRAW_MAIN,nullptr);
    motion_readout=label(screen,"Waiting for a fresh sensor sample",&lv_font_montserrat_16,kWhite,LV_ALIGN_TOP_MID,0,328);
    lv_obj_set_width(motion_readout,design::ContentWidth);
    lv_label_set_long_mode(motion_readout,LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(motion_readout,LV_TEXT_ALIGN_CENTER,0);
    motion_accel_hint=label(screen,"+/-4 g RANGE / GRAVITY INCLUDED",design::caption(),kWhite,LV_ALIGN_TOP_MID,0,376);
    motion_center=button(screen,"SET LEVEL / RECENTER",264,design::TouchHeight,LV_ALIGN_TOP_MID,0,356,[](lv_event_t *) {
        if (motion_lab.center(lv_tick_get())) {
            motion_center_confirmed=true; motion_centered_at=lv_tick_get();
            lv_label_set_text(lv_obj_get_child(motion_center,0),"REFERENCE SET");
            motion_tick(nullptr);
        }
    });
    lv_obj_set_style_text_font(lv_obj_get_child(motion_center,0),design::caption(),0);
    motion_timer=lv_timer_create(motion_tick,40,nullptr);
    lv_timer_pause(motion_timer); motion_select(0);
}
