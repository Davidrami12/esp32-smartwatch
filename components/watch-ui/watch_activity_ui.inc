static lv_obj_t *activity_ring,*activity_percent,*activity_selection,*activity_chart;
static lv_obj_t *activity_columns[7],*activity_bars[7],*activity_days[7];
static watch_ui_activity_day_t activity_history[7]={}; // Newest first, same platform contract.
static bool activity_history_valid=false;
static unsigned activity_selected=0;
static uint32_t activity_scale=8000;

static uint32_t activity_day_steps(unsigned index) { return index ? activity_history[index].steps : today_steps; }
static void activity_goal_line(lv_event_t *event)
{
    if (!activity_history_valid) return;
    lv_area_t a; lv_obj_get_coords(activity_chart,&a);
    const int y=a.y1+60-static_cast<int>(uint64_t(daily_step_goal)*52/activity_scale);
    auto *layer=lv_event_get_layer(event);
    for (int x=a.x1+4;x<a.x2-4;x+=12) {
        lv_draw_line_dsc_t line; lv_draw_line_dsc_init(&line);
        line.color=lv_color_hex(0x646464); line.width=1;
        line.p1={static_cast<lv_value_precise_t>(x),static_cast<lv_value_precise_t>(y)};
        line.p2={static_cast<lv_value_precise_t>(x+6),static_cast<lv_value_precise_t>(y)};
        lv_draw_line(layer,&line);
    }
}
static void refresh_activity()
{
    if (!activity_ring || !display_active || current_screen!=WatchScreen::Activity) return;
    const uint64_t progress=uint64_t(today_steps)*1000/daily_step_goal;
    lv_arc_set_value(activity_ring,progress>1000 ? 1000 : static_cast<int>(progress));
    const uint64_t percent=(uint64_t(today_steps)*100+daily_step_goal/2)/daily_step_goal;
    if (percent>999) lv_label_set_text(activity_percent,">999% OF GOAL");
    else lv_label_set_text_fmt(activity_percent,"%lu%% OF GOAL",static_cast<unsigned long>(percent));
    activity_scale=daily_step_goal;
    if (activity_history_valid) for (unsigned i=0;i<7;++i) {
        const uint32_t steps=activity_day_steps(i);
        if (steps>activity_scale) activity_scale=steps;
    }
    static const char *weekdays[]={"SUN","MON","TUE","WED","THU","FRI","SAT"};
    for (unsigned i=0;i<7;++i) {
        const unsigned day=6-i; // Oldest left, Today right.
        const uint32_t steps=activity_day_steps(day);
        const int height=activity_history_valid ? static_cast<int>(uint64_t(steps)*52/activity_scale) : 0;
        lv_obj_set_size(activity_bars[i],22,height>1 ? height : 2);
        lv_obj_set_pos(activity_bars[i],11,60-(height>1 ? height : 2));
        lv_obj_set_style_bg_color(activity_bars[i],lv_color_hex(!steps || !activity_history_valid ? design::Border : day==0 ? design::Activity : 0x7F858D),0);
        lv_obj_set_style_bg_opa(activity_bars[i],activity_history_valid ? LV_OPA_COVER : LV_OPA_TRANSP,0);
        lv_label_set_text(activity_days[i],activity_history_valid ? weekdays[activity_history[day].weekday%7] : "--");
        lv_obj_set_style_text_color(activity_days[i],lv_color_hex(day==0 ? design::Activity : kMuted),0);
        lv_obj_set_style_border_color(activity_columns[i],lv_color_hex(activity_history_valid && day==activity_selected ? design::Activity : 0x000000),0);
    }
    if (!activity_history_valid) lv_label_set_text(activity_selection,"7 DAYS / HISTORY UNAVAILABLE");
    else lv_label_set_text_fmt(activity_selection,"%s%s / %lu STEPS",weekdays[activity_history[activity_selected].weekday%7],
        activity_selected==0 ? " / TODAY" : "",static_cast<unsigned long>(activity_day_steps(activity_selected)));
    lv_obj_invalidate(activity_chart);
}

static void create_activity_screen()
{
    auto *screen=create_screen(WatchScreen::Activity,"Activity");
    activity_ring=lv_arc_create(screen); lv_obj_set_size(activity_ring,144,144);
    lv_obj_align(activity_ring,LV_ALIGN_TOP_MID,0,74);
    lv_arc_set_rotation(activity_ring,270); lv_arc_set_bg_angles(activity_ring,0,360);
    lv_arc_set_range(activity_ring,0,1000); lv_arc_set_value(activity_ring,0);
    lv_obj_remove_style(activity_ring,nullptr,LV_PART_KNOB);
    lv_obj_remove_flag(activity_ring,LV_OBJ_FLAG_CLICKABLE); lv_obj_remove_flag(activity_ring,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_arc_width(activity_ring,8,LV_PART_MAIN); lv_obj_set_style_arc_width(activity_ring,8,LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(activity_ring,lv_color_hex(design::Border),LV_PART_MAIN);
    lv_obj_set_style_arc_color(activity_ring,lv_color_hex(design::Activity),LV_PART_INDICATOR);
    label(screen,"TODAY / STEPS",&lv_font_montserrat_14,kMuted,LV_ALIGN_TOP_MID,0,104);
    activity_steps_label=label(screen,"0",&lv_font_montserrat_26,kWhite,LV_ALIGN_TOP_MID,0,122);
    activity_percent=label(screen,"0% OF GOAL",&lv_font_montserrat_14,design::Activity,LV_ALIGN_TOP_MID,0,170);
    goal_minus=button(screen,"-",56,56,LV_ALIGN_TOP_LEFT,design::Margin,226,step_goal_changed_cb);
    activity_goal_label=label(screen,"Goal 8,000",design::caption(),kWhite,LV_ALIGN_TOP_MID,0,246);
    lv_obj_set_width(activity_goal_label,210); lv_obj_set_style_text_align(activity_goal_label,LV_TEXT_ALIGN_CENTER,0);
    lv_label_set_long_mode(activity_goal_label,LV_LABEL_LONG_DOT);
    goal_plus=button(screen,"+",56,56,LV_ALIGN_TOP_RIGHT,-design::Margin,226,step_goal_changed_cb);
    activity_selection=label(screen,"7 DAYS / TAP A DAY",design::caption(),kWhite,LV_ALIGN_TOP_MID,0,290);
    lv_obj_set_width(activity_selection,346); lv_obj_set_style_text_align(activity_selection,LV_TEXT_ALIGN_CENTER,0);
    lv_label_set_long_mode(activity_selection,LV_LABEL_LONG_DOT);
    activity_chart=lv_obj_create(screen); lv_obj_remove_style_all(activity_chart);
    lv_obj_set_size(activity_chart,346,94); lv_obj_align(activity_chart,LV_ALIGN_TOP_MID,0,316);
    lv_obj_remove_flag(activity_chart,LV_OBJ_FLAG_SCROLLABLE); lv_obj_remove_flag(activity_chart,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(activity_chart,activity_goal_line,LV_EVENT_DRAW_MAIN,nullptr);
    for (unsigned i=0;i<7;++i) {
        auto *column=activity_columns[i]=lv_obj_create(activity_chart); lv_obj_remove_style_all(column);
        lv_obj_set_size(column,44,94); lv_obj_set_pos(column,4+int(i)*49,0);
        lv_obj_set_style_radius(column,8,0); lv_obj_set_style_border_width(column,1,0);
        lv_obj_remove_flag(column,LV_OBJ_FLAG_SCROLLABLE); lv_obj_add_flag(column,LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(column,[](lv_event_t *e) {
            if (!activity_history_valid) return;
            for (unsigned n=0;n<7;++n) if (lv_event_get_target(e)==activity_columns[n]) activity_selected=6-n;
            refresh_activity();
        },LV_EVENT_CLICKED,nullptr);
        auto *bar=activity_bars[i]=lv_obj_create(column); lv_obj_remove_style_all(bar);
        lv_obj_set_style_radius(bar,4,0);
        lv_obj_remove_flag(bar,LV_OBJ_FLAG_CLICKABLE); lv_obj_remove_flag(bar,LV_OBJ_FLAG_SCROLLABLE);
        activity_days[i]=label(column,"--",&lv_font_montserrat_14,kMuted,LV_ALIGN_TOP_MID,0,74);
        lv_obj_remove_flag(activity_days[i],LV_OBJ_FLAG_CLICKABLE);
    }
    refresh_step_goal();
}
