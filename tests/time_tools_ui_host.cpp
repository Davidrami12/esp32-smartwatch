// Include production UI so this headless integration test can inspect its private screens.
#include "../components/watch-ui/watch_ui.cpp"
#include <assert.h>
#include <initializer_list>
#include "../apps/watch/main/watch_ble_control_protocol.h"
#include "../apps/watch/main/watch_notification_protocol.h"
#include "../apps/watch/main/watch_ble_security_policy.h"
#include "../apps/watch/main/watch_media_protocol.h"
#include <vector>
#include "../components/watch-ui/watch_voice_result.h"
#include "../components/watch-ui/watch_touch_gate.h"

static void notification_protocol_tests()
{
    WatchNotificationAssembler assembler;
    watch_notification_t item = {};
    std::vector<uint8_t> message(16, 0);
    const char *fields[] = {"demo.app", "Demo", "David", "Hello there"};
    for (unsigned f = 0; f < 4; ++f) {
        const size_t size = strlen(fields[f]); message[8 + 2*f] = size;
        message.insert(message.end(), fields[f], fields[f] + size);
    }
    auto frame = [&](size_t offset) {
        std::vector<uint8_t> result = {1, 1, 7, 0, static_cast<uint8_t>(offset), static_cast<uint8_t>(offset >> 8),
            static_cast<uint8_t>(message.size()), static_cast<uint8_t>(message.size() >> 8)};
        const size_t end = offset + 12 < message.size() ? offset + 12 : message.size();
        result.insert(result.end(), message.begin() + offset, message.begin() + end); return result;
    };
    for (size_t offset = 0; offset < message.size(); offset += 12) {
        const auto bytes = frame(offset);
        const int result = assembler.accept(bytes.data(), bytes.size(), 100, item);
        assert(result == (offset + 12 >= message.size() ? 1 : 0));
    }
    assert(strcmp(item.app, "Demo") == 0 && strcmp(item.title, "David") == 0 && strcmp(item.body, "Hello there") == 0);
    auto bytes = frame(0);
    assert(assembler.accept(bytes.data(), bytes.size(), UINT32_MAX - 1, item) == 0);
    auto next = frame(12);
    assert(assembler.accept(next.data(), next.size(), 2, item) == 0); // Tick wrap.
    assembler.reset();
    assert(assembler.accept(next.data(), next.size(), 0, item) == -1); // No START.
    assert(assembler.accept(bytes.data(), bytes.size(), 0, item) == 0);
    assert(assembler.accept(next.data(), next.size(), 6000, item) == -1); // Timeout.
    bytes[0] = 9; assert(assembler.accept(bytes.data(), bytes.size(), 0, item) == -1);
    bytes[0] = 1; bytes[6] = 1; bytes[7] = 3; // Total 769 exceeds cap.
    assert(assembler.accept(bytes.data(), bytes.size(), 0, item) == -1);
    assert(assembler.accept(nullptr, 20, 0, item) == -1);
    for (size_t length : {0u, 8u, 21u, 100u}) assert(assembler.accept(bytes.data(), length, 0, item) == -1);
    message[8] = 97; // Malformed package size / exact structure.
    for (size_t offset = 0; offset < message.size(); offset += 12) {
        const auto part = frame(offset);
        const int result = assembler.accept(part.data(), part.size(), 0, item);
        assert(result == (offset + 12 >= message.size() ? -1 : 0));
    }
    WatchNotificationQueue queue;
    for (unsigned i = 0; i < 7; ++i) { item.timestamp_ms = i; queue.push(item); }
    assert(queue.size() == 5);
    for (unsigned i = 2; i < 7; ++i) { assert(queue.pop(item)); assert(item.timestamp_ms == i); }
    assert(!queue.pop(item));
}

static unsigned brightness_calls, commit_calls;
static uint8_t committed;
static unsigned goal_calls;
static unsigned voice_calls[4];
static bool voice_test_action(uint8_t action) { assert(action >= 1 && action <= 3); ++voice_calls[action]; return true; }
static unsigned game_save_calls, game_force_calls;
static uint32_t game_saved_best;
static bool game_best_test(uint32_t best, bool force)
{
    ++game_save_calls; if (force) ++game_force_calls; game_saved_best = best; return true;
}
static bool goal_save_ok = true;
static bool goal_test(uint32_t) { ++goal_calls; return goal_save_ok; }
static void brightness_test(uint8_t) { ++brightness_calls; }
static void commit_test(uint8_t value) { ++commit_calls; committed = value; }

static lv_obj_t *find_button(lv_obj_t *screen, const char *text)
{
    for (uint32_t i = 0; i < lv_obj_get_child_count(screen); ++i) {
        lv_obj_t *object = lv_obj_get_child(screen, i);
        if (!lv_obj_check_type(object, &lv_button_class) || !lv_obj_get_child_count(object)) continue;
        lv_obj_t *child = lv_obj_get_child(object, 0);
        if (lv_obj_check_type(child, &lv_label_class) && strcmp(lv_label_get_text(child), text) == 0) return object;
        if (lv_obj_check_type(child,&lv_label_class) && lv_obj_get_child_count(object) == 2) {
            auto *title = lv_obj_get_child(object,1);
            if (lv_obj_check_type(title,&lv_label_class)) {
                char combined[64]; snprintf(combined,sizeof(combined),"%s  %s",lv_label_get_text(child),lv_label_get_text(title));
                if (!strcmp(combined,text)) return object;
            }
        }
    }
    assert(false && "Button not found");
    return nullptr;
}
static void check_touch_targets(lv_obj_t *parent)
{
    if (lv_obj_check_type(parent,&lv_button_class)) {
        assert(lv_obj_get_width(parent) >= design::TouchHeight);
        assert(lv_obj_get_height(parent) >= design::TouchHeight);
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(parent); ++i) check_touch_targets(lv_obj_get_child(parent,i));
}

int main()
{
    static_assert(sizeof(mochi_halo_pixels) == mochi_halo_size*mochi_halo_size*2,"Fixed-size RGB565 halo");
    {
        auto pixel = [](unsigned x, unsigned y) {
            const auto at = (y*mochi_halo_size+x)*2;
            return unsigned(mochi_halo_pixels[at]) | (unsigned(mochi_halo_pixels[at+1])<<8);
        };
        assert(pixel(88,88) == 0); // Black face interior.
        assert(pixel(0,0) == 0); // No rectangular glow at image corners.
        assert(pixel(144,88) != 0); // Bright ring, same radius as before.
        assert((pixel(160,88) & 31) >= 2); // Glow now extends beyond the previous image.
        assert(pixel(175,88) == 0); // Fade to black before the asset boundary.
    }
    {
        WatchOffscreenHold hold;
        assert(!hold.sample(false,true,100,100,0));
        assert(!hold.sample(false,true,104,102,999));
        assert(hold.sample(false,true,104,102,1000));
        assert(!hold.sample(false,true,104,102,3000)); // One activation per press.
        assert(!hold.sample(true,false,104,102,3010));
        assert(!hold.sample(true,true,100,100,4000));
        assert(!hold.sample(false,true,100,100,6000)); // Press began awake.
        hold.sample(false,false,100,100,6010);
        hold.sample(false,true,100,100,7000);
        assert(!hold.sample(false,true,113,100,7500));
        assert(!hold.sample(false,true,100,100,9000)); // Moving back cannot rearm.
        hold.sample(false,false,100,100,9010);
        hold.sample(false,true,100,100,10000);
        assert(!hold.sample(true,true,100,100,10500)); // Double tap/external wake cancels.
        assert(!hold.sample(true,true,100,100,12000));
        hold.sample(false,false,100,100,12010);
        hold.sample(false,true,100,100,UINT32_MAX-499);
        assert(hold.sample(false,true,100,100,500)); // Tick wraparound.
    }
    {
        WatchTouchGate gate;
        auto sample = gate.sample(false,true); assert(!sample.allow && sample.wake_press);
        sample = gate.sample(false,true); assert(!sample.allow && !sample.wake_press);
        assert(!gate.sample(false,false).allow);
        sample = gate.sample(false,true); assert(!sample.allow && sample.wake_press);
        assert(!gate.sample(true,true).allow); // Second tap wakes, but cannot click or swipe.
        assert(!gate.sample(true,false).allow); // Its release cannot click either.
        assert(gate.sample(true,true).allow); // A fresh, awake touch works.
        gate.sleep();
        assert(!gate.sample(false,true).wake_press); // Held finger is not another tap.
        assert(!gate.sample(false,false).allow);
        assert(gate.sample(false,true).wake_press);
        assert(!gate.sample(true,true).allow); // Also safe if awakened externally.
    }
    for (auto option : {WatchScreen::Wifi, WatchScreen::Bluetooth, WatchScreen::Brightness, WatchScreen::Timeout}) {
        assert(navigation_animation(WatchScreen::Settings,option) == LV_SCR_LOAD_ANIM_MOVE_RIGHT);
        assert(navigation_animation(option,WatchScreen::Settings) == LV_SCR_LOAD_ANIM_MOVE_LEFT);
    }
    assert(navigation_animation(WatchScreen::Launcher,WatchScreen::Voice) == LV_SCR_LOAD_ANIM_MOVE_LEFT);
    assert(navigation_animation(WatchScreen::Voice,WatchScreen::Launcher) == LV_SCR_LOAD_ANIM_MOVE_RIGHT);
    {
        WatchMediaAssembler assembler; watch_media_t media;
        uint8_t frame[] = {1, 2, 1, 0, 0, 0, 12, 0, 1, 1, 7, 0, 99, 0, 0, 0, 0, 0, 0, 0};
        assert(assembler.accept(frame, sizeof(frame), 0, media) == 1);
        assert(media.state == 1 && media.actions == 7 && media.session == 99);
        frame[9] = 0; assert(assembler.accept(frame, sizeof(frame), 0, media) == -1); // unavailable can't carry session/actions.
        frame[10] = 0; frame[12] = 0;
        assert(assembler.accept(frame, sizeof(frame), 0, media) == 1);
        assert(media.state == 0);
        frame[0] = 2; assert(assembler.accept(frame, sizeof(frame), 0, media) == -1);
        frame[0] = 1; frame[6] = 253; assert(assembler.accept(frame, sizeof(frame), 0, media) == -1);
        assert(assembler.accept(nullptr, 20, 0, media) == -1);
        frame[6] = 12;
        assert(assembler.accept(frame, 10, 0, media) == 0);
        frame[4] = 2;
        assert(assembler.accept(frame, sizeof(frame), 6000, media) == -1); // timeout/out of order.
        const uint8_t first[] = {1, 2, 2, 0, 0, 0, 23, 0, 1, 2, 2, 0, 99, 0, 0, 0, 5, 0, 6, 0};
        const uint8_t second[] = {1, 2, 2, 0, 12, 0, 23, 0, 'T', 'r', 'a', 'c', 'k', 'A', 'r', 't', 'i', 's', 't'};
        assert(assembler.accept(first, sizeof(first), 0, media) == 0);
        assert(assembler.accept(second, sizeof(second), 1, media) == 1);
        assert(strcmp(media.title, "Track") == 0 && strcmp(media.artist, "Artist") == 0);
        uint8_t volume_frame[] = {1, 2, 3, 0, 0, 0, 12, 0, 2, 1, 31, 0, 99, 0, 0, 0, 0, 0, 0, 0};
        assert(assembler.accept(volume_frame, sizeof(volume_frame), 2, media) == 1);
        assert(media.actions == 31);
        volume_frame[8] = 1; // Legacy schema cannot claim volume.
        assert(assembler.accept(volume_frame, sizeof(volume_frame), 3, media) == -1);
        volume_frame[8] = 2; volume_frame[10] = 32;
        assert(assembler.accept(volume_frame, sizeof(volume_frame), 4, media) == -1);
    }
    assert(watch_ble_key_is_strong(true, true, true, 16, true));
    assert(!watch_ble_key_is_strong(false, true, true, 16, true));
    assert(!watch_ble_key_is_strong(true, false, true, 16, true));
    assert(!watch_ble_key_is_strong(true, true, false, 16, true));
    assert(!watch_ble_key_is_strong(true, true, true, 7, true));
    assert(!watch_ble_key_is_strong(true, true, true, 16, false));
    assert(watch_ble_can_pair(0, false, false));
    assert(!watch_ble_can_pair(1, false, false));
    assert(!watch_ble_can_pair(2, false, false));
    assert(!watch_ble_can_pair(0, true, false));
    assert(!watch_ble_can_pair(0, false, true));
    notification_protocol_tests();
    lv_init();
    lv_display_create(410, 502);
    watch_ui_create();
    const uint32_t spanish_glyphs[] = {0x00a1, 0x00bf, 0x00c1, 0x00c9, 0x00cd, 0x00d1, 0x00d3, 0x00da,
        0x00dc, 0x00e1, 0x00e9, 0x00ed, 0x00f1, 0x00f3, 0x00fa, 0x00fc};
    const lv_font_t *spanish_fonts[] = {design::caption(), watch_font_18(), watch_font_26(), watch_font_30()};
    for (const auto *font : spanish_fonts) {
        for (const auto codepoint : spanish_glyphs) {
            lv_font_glyph_dsc_t glyph = {};
            assert(lv_font_get_glyph_dsc(font, &glyph, codepoint, 0));
            assert(!glyph.is_placeholder && glyph.box_w > 0 && glyph.box_h > 0);
        }
        lv_font_glyph_dsc_t ascii = {};
        assert(lv_font_get_glyph_dsc(font, &ascii, 'A', 0) && !ascii.is_placeholder);
        lv_font_glyph_dsc_t icon = {};
        assert(lv_font_get_glyph_dsc(font, &icon, 0xf04b, 0) && !icon.is_placeholder);
    }
    {
        watch_ui_set_voice_callback(voice_test_action);
        const auto recordings = voice_calls[1];
        watch_ui_set_display_active(false);
        assert(!watch_ui_open_voice());
        watch_ui_set_display_active(true);
        assert(watch_ui_open_voice() && current_screen == WatchScreen::Voice);
        assert(lv_screen_active() == screen_for(WatchScreen::Voice)); // Immediate, no intermediate Home animation.
        assert(!voice_held && voice_calls[1] == recordings);
        navigate_to(WatchScreen::Home);
    }
    watch_ui_configure_2048(100, 42, game_best_test);
    open_game_cb(nullptr); assert(current_screen == WatchScreen::Game2048);
    assert(game_2048.started && game_2048.best == 100 && !game_2048.score);
    assert(lv_obj_has_flag(game_board, LV_OBJ_FLAG_GESTURE_BUBBLE));
    assert(!lv_obj_has_flag(game_tiles[0], LV_OBJ_FLAG_SCROLLABLE));
    memset(game_2048.board, 0, sizeof(game_2048.board)); game_2048.board[0] = game_2048.board[1] = 2;
    process_swipe(LV_DIR_LEFT); assert(game_2048.score == 4 && game_2048.board[0] == 4);
    const lv_dir_t directions[] = {LV_DIR_RIGHT, LV_DIR_TOP, LV_DIR_BOTTOM};
    for (auto direction : directions) {
        memset(game_2048.board, 0, sizeof(game_2048.board)); game_2048.board[5] = 2;
        const uint32_t moves = game_2048.moves;
        process_swipe(direction); assert(current_screen == WatchScreen::Game2048 && game_2048.moves == moves + 1);
    }
    uint32_t preserved[16]; memcpy(preserved, game_2048.board, sizeof(preserved));
    watch_ui_set_display_active(false); process_swipe(LV_DIR_LEFT);
    assert(!memcmp(preserved, game_2048.board, sizeof(preserved)));
    watch_ui_set_display_active(true);
    watch_ui_show_notification("Phone", "Incoming", "While playing"); assert(current_screen == WatchScreen::Notification);
    watch_ui_hide_notification(); assert(current_screen == WatchScreen::Game2048);
    assert(!memcmp(preserved, game_2048.board, sizeof(preserved)));
    lv_obj_send_event(find_button(screen_for(WatchScreen::Game2048), "NEW GAME"), LV_EVENT_CLICKED, nullptr);
    assert(game_dialog == GameDialog::NewGame);
    process_swipe(LV_DIR_RIGHT); assert(current_screen == WatchScreen::Game2048);
    assert(!memcmp(preserved, game_2048.board, sizeof(preserved)));
    watch_ui_show_notification("Phone", "Incoming", "During confirmation"); watch_ui_hide_notification();
    assert(game_dialog == GameDialog::NewGame);
    lv_obj_send_event(game_dialog_left, LV_EVENT_CLICKED, nullptr);
    assert(game_dialog == GameDialog::None && !memcmp(preserved, game_2048.board, sizeof(preserved)));
    game_new_cb(nullptr); lv_obj_send_event(game_dialog_right, LV_EVENT_CLICKED, nullptr);
    assert(game_2048.score == 0 && game_2048.best == 100 && game_2048.moves == 0);
    memset(game_2048.board, 0, sizeof(game_2048.board)); game_2048.board[0] = game_2048.board[1] = 1024;
    process_swipe(LV_DIR_LEFT); assert(game_dialog == GameDialog::Win && game_saved_best == 2048);
    lv_obj_send_event(game_dialog_left, LV_EVENT_CLICKED, nullptr);
    assert(game_dialog == GameDialog::None && game_2048.won && !game_2048.win_pending);
    const uint32_t no_moves[16] = {2,4,2,4,4,2,4,2,2,4,2,4,4,2,4,2};
    memcpy(game_2048.board, no_moves, sizeof(no_moves));
    process_swipe(LV_DIR_RIGHT); assert(game_dialog == GameDialog::Over && current_screen == WatchScreen::Game2048);
    lv_obj_send_event(game_dialog_right, LV_EVENT_CLICKED, nullptr); assert(current_screen == WatchScreen::Launcher);
    assert(game_save_calls && game_force_calls && game_saved_best == 2048);
    open_game_cb(nullptr); assert(game_dialog == GameDialog::Over); // Opening does not reset.
    lv_obj_send_event(game_dialog_left, LV_EVENT_CLICKED, nullptr);
    assert(!game_2048.game_over && game_2048.best == 2048 && game_2048.score == 0);
    back_cb(nullptr); assert(current_screen == WatchScreen::Launcher);
    back_cb(nullptr); assert(current_screen == WatchScreen::Home);
    watch_ui_set_media(1, 7, 99, "A readable track", "Artist");
    static unsigned media_commands = 0;
    watch_ui_set_media_callback([](uint8_t command, uint32_t token) { assert(command == 2 && token == 99); ++media_commands; return true; });
    lv_obj_send_event(media_toggle, LV_EVENT_CLICKED, nullptr); assert(media_commands == 1);
    open_media_cb(nullptr); assert(current_screen == WatchScreen::Media);
    assert(media_record_spinning && lv_anim_get(media_record, rotate_media_record) != nullptr);
    watch_ui_set_display_active(false);
    assert(!media_record_spinning && lv_anim_get(media_record, rotate_media_record) == nullptr);
    watch_ui_set_display_active(true); assert(media_record_spinning);
    assert(strcmp(lv_label_get_text(media_title), "A readable track") == 0);
    assert(strcmp(lv_label_get_text(media_artist), "Artist: Artist") == 0);
    assert(lv_obj_get_style_text_align(media_title, LV_PART_MAIN) == LV_TEXT_ALIGN_CENTER);
    lv_obj_update_layout(media_title);
    assert(lv_obj_get_y(media_artist) == lv_obj_get_y(media_title) + lv_obj_get_height(media_title) + 8);
    assert(lv_obj_get_style_text_font(media_title, LV_PART_MAIN) == watch_font_30());
    watch_ui_set_media(1, 7, 99, "A deliberately long song name that spans many lines and must remain safely bounded above the playback controls even with extra descriptive words", "An artist name that is also deliberately long to exercise wrapping");
    lv_obj_update_layout(media_artist);
    assert(lv_obj_get_height(media_title) <= 80 && lv_obj_get_height(media_artist) <= 48);
    assert(lv_obj_get_y(media_artist) + lv_obj_get_height(media_artist) + 8 <= lv_obj_get_y(media_toggle));
    assert(lv_obj_get_y(media_toggle) + lv_obj_get_height(media_toggle) + 8 <= lv_obj_get_y(media_volume_up));
    auto *media_back = lv_obj_get_child(screen_for(WatchScreen::Media), 1);
    assert(lv_obj_get_y(media_volume_up) + lv_obj_get_height(media_volume_up) + 16 <= lv_obj_get_y(media_back));
    assert(lv_obj_get_y(media_volume_down) + lv_obj_get_height(media_volume_down) + 16 <= lv_obj_get_y(media_back));
    assert(lv_obj_get_y(media_artist) == lv_obj_get_y(media_title) + lv_obj_get_height(media_title) + 8);
    watch_ui_set_media(1, 7, 99, "A readable track", "Artist");
    assert(lv_obj_get_style_radius(media_previous, LV_PART_MAIN) == LV_RADIUS_CIRCLE);
    assert(lv_obj_has_state(media_volume_up, LV_STATE_DISABLED));
    watch_ui_set_media(1, 31, 99, "A readable track", "Artist");
    assert(!lv_obj_has_state(media_volume_up, LV_STATE_DISABLED));
    assert(!lv_obj_has_state(media_volume_down, LV_STATE_DISABLED));
    watch_ui_set_media(1, 7, 99, "A readable track", "Artist");
    assert(strcmp(lv_label_get_text(lv_obj_get_child(media_toggle, 0)), LV_SYMBOL_PAUSE) == 0);
    assert(!lv_obj_has_state(media_next, LV_STATE_DISABLED));
    watch_ui_set_media(2, 2, 99, "Track", "Artist");
    assert(!media_record_spinning && lv_anim_get(media_record, rotate_media_record) == nullptr);
    assert(strcmp(lv_label_get_text(lv_obj_get_child(media_toggle, 0)), LV_SYMBOL_PLAY) == 0);
    assert(lv_obj_has_state(media_next, LV_STATE_DISABLED));
    watch_ui_set_media(0, 0, 0, "", "");
    assert(!media_record_spinning);
    assert(lv_obj_has_state(media_toggle, LV_STATE_DISABLED));
    back_cb(nullptr); assert(current_screen == WatchScreen::Launcher); navigate_to(WatchScreen::Home);
    static unsigned security_answers = 0;
    watch_ui_set_security_callbacks([](uint32_t token, bool accept) { assert(token == 77); assert(!accept); ++security_answers; return true; },
                                    []() { return true; });
    watch_ui_show_pairing(77, 123);
    assert(current_screen == WatchScreen::Pairing);
    assert(strcmp(lv_label_get_text(pairing_code), "000123") == 0);
    back_cb(nullptr); assert(security_answers == 1); // Back explicitly rejects, never implicitly accepts.
    watch_ui_hide_pairing(); assert(current_screen == WatchScreen::Home);
    watch_ui_set_security_status("Trusted phone / bonded", true);
    assert(!lv_obj_has_state(forget_phone_button, LV_STATE_DISABLED));
    navigate_to(WatchScreen::ForgetPhone); back_cb(nullptr);
    assert(current_screen == WatchScreen::Bluetooth);
    watch_ui_set_security_status("No paired phone", false);
    assert(lv_obj_has_state(forget_phone_button, LV_STATE_DISABLED));
    navigate_to(WatchScreen::Home);
    navigate_to(WatchScreen::Settings);
    watch_ui_show_notification("Demo", "David", "Hello there");
    assert(current_screen == WatchScreen::Notification);
    assert(strcmp(lv_label_get_text(notification_app), "Demo") == 0);
    assert(strcmp(lv_label_get_text(notification_body), "Hello there") == 0);
    watch_ui_show_notification("Next", "Title", "Body");
    back_cb(nullptr);
    assert(current_screen == WatchScreen::Settings);
    back_cb(nullptr);
    uint32_t decoded = 0;
    uint8_t bytes[4] = {};
    for (unsigned i = 0; i <= 255; ++i) {
        bytes[0] = i;
        assert(watch_ble_decode_control(watch_ble_control_t::Brightness, bytes, 1, decoded) == (i >= 10 && i <= 100));
        assert(watch_ble_decode_control(watch_ble_control_t::WifiEnabled, bytes, 1, decoded) == (i <= 1));
    }
    for (uint32_t value : {0u, 5000u, 10000u, 30000u, 60000u, 1u, 9999u, UINT32_MAX}) {
        for (unsigned i = 0; i < 4; ++i) bytes[i] = value >> (8 * i);
        const bool valid = value == 0 || value == 5000 || value == 10000 || value == 30000 || value == 60000;
        assert(watch_ble_decode_control(watch_ble_control_t::ScreenTimeout, bytes, 4, decoded) == valid);
        assert(decoded == value);
    }
    for (auto control : {watch_ble_control_t::Brightness, watch_ble_control_t::ScreenTimeout, watch_ble_control_t::WifiEnabled}) {
        for (size_t length : {0u, 2u, 3u, 5u}) assert(!watch_ble_decode_control(control, bytes, length, decoded));
        assert(!watch_ble_decode_control(control, nullptr, watch_ble_control_length(control), decoded));
    }
    watch_ui_set_bluetooth_state(WATCH_UI_BLUETOOTH_CONNECTED);
    assert(strcmp(lv_label_get_text(bluetooth_status_label), LV_SYMBOL_BLUETOOTH " ON") == 0);
    assert(strcmp(lv_label_get_text(bluetooth_detail_label), "CONNECTED") == 0);
    watch_ui_set_bluetooth_state(WATCH_UI_BLUETOOTH_ADVERTISING);
    assert(strcmp(lv_label_get_text(bluetooth_status_label), LV_SYMBOL_BLUETOOTH " READY") == 0);
    watch_ui_set_bluetooth_state(WATCH_UI_BLUETOOTH_DISABLED);
    assert(strcmp(lv_label_get_text(bluetooth_status_label), LV_SYMBOL_BLUETOOTH " OFF") == 0);
    assert(navigation_animation(WatchScreen::Home, WatchScreen::Settings) == LV_SCR_LOAD_ANIM_MOVE_RIGHT);
    assert(navigation_animation(WatchScreen::Settings, WatchScreen::Home) == LV_SCR_LOAD_ANIM_MOVE_LEFT);
    assert(navigation_animation(WatchScreen::Home, WatchScreen::Launcher) == LV_SCR_LOAD_ANIM_MOVE_LEFT);
    assert(navigation_animation(WatchScreen::Launcher, WatchScreen::Home) == LV_SCR_LOAD_ANIM_MOVE_RIGHT);
    assert(navigation_animation(WatchScreen::Settings, WatchScreen::Wifi) == LV_SCR_LOAD_ANIM_MOVE_RIGHT);
    assert(navigation_animation(WatchScreen::Wifi, WatchScreen::Settings) == LV_SCR_LOAD_ANIM_MOVE_LEFT);
    assert(navigation_animation(WatchScreen::Launcher, WatchScreen::Timer) == LV_SCR_LOAD_ANIM_MOVE_LEFT);
    assert(navigation_animation(WatchScreen::Timer, WatchScreen::Launcher) == LV_SCR_LOAD_ANIM_MOVE_RIGHT);
    lv_obj_t *apps = find_button(screen_for(WatchScreen::Home), LV_SYMBOL_LIST "  APPS");
    lv_obj_t *settings = find_button(screen_for(WatchScreen::Home), LV_SYMBOL_SETTINGS "  SETTINGS");
    lv_obj_update_layout(screen_for(WatchScreen::Home));
    assert(lv_obj_get_x(settings) < lv_obj_get_x(apps));
    for (auto *nav : {settings, apps}) {
        assert(lv_obj_get_width(nav) == 136 && lv_obj_get_height(nav) == design::TouchHeight);
        assert(lv_obj_get_style_radius(nav,LV_PART_MAIN) == design::TouchHeight/2);
        assert(lv_obj_get_style_border_width(nav, LV_PART_MAIN) == design::BorderWidth);
        assert(lv_obj_get_style_bg_opa(nav, LV_PART_MAIN) == LV_OPA_COVER);
        assert(lv_color_eq(lv_obj_get_style_text_color(lv_obj_get_child(nav, 0), LV_PART_MAIN), lv_color_hex(kWhite)));
        lv_obj_add_state(nav, LV_STATE_PRESSED);
        assert(lv_obj_get_style_bg_opa(nav, LV_PART_MAIN) == LV_OPA_COVER);
        assert(lv_obj_get_style_border_width(nav, LV_PART_MAIN) == design::BorderWidth);
        lv_obj_remove_state(nav, LV_STATE_PRESSED);
    }
    lv_obj_send_event(settings, LV_EVENT_CLICKED, nullptr);
    assert(current_screen == WatchScreen::Settings);
    back_cb(nullptr);
    lv_obj_send_event(apps, LV_EVENT_CLICKED, nullptr);
    assert(current_screen == WatchScreen::Launcher);
    back_cb(nullptr);
    assert(weather_icon_color(WATCH_UI_WEATHER_SUNNY) == design::Weather);
    assert(weather_icon_color(WATCH_UI_WEATHER_RAIN) == design::WeatherRain);
    assert(weather_icon_color(WATCH_UI_WEATHER_CLOUDY) == design::WeatherCloudy);
    assert(weather_icon_color(WATCH_UI_WEATHER_THUNDERSTORM) == design::WeatherStorm);
    assert(weather_icon_color(WATCH_UI_WEATHER_WINDY) == design::WeatherWind);
    for (const auto parent : {WatchScreen::Launcher, WatchScreen::Settings}) {
        navigate_to(parent);
        lv_obj_send_event(find_button(screen_for(parent), LV_SYMBOL_HOME "  HOME"), LV_EVENT_CLICKED, nullptr);
        assert(current_screen == WatchScreen::Home);
    }
    for (const auto child : {WatchScreen::Activity, WatchScreen::Timer, WatchScreen::Stopwatch, WatchScreen::Weather,
                             WatchScreen::Wifi, WatchScreen::Bluetooth, WatchScreen::Brightness, WatchScreen::Timeout}) {
        navigate_to(child);
        lv_obj_send_event(find_button(screen_for(child), LV_SYMBOL_LEFT "  BACK"), LV_EVENT_CLICKED, nullptr);
        const bool settings_child = child == WatchScreen::Wifi || child == WatchScreen::Bluetooth ||
            child == WatchScreen::Brightness || child == WatchScreen::Timeout;
        assert(current_screen == (settings_child ? WatchScreen::Settings : WatchScreen::Launcher));
    }
    back_cb(nullptr);
    watch_ui_set_brightness_callback(brightness_test);
    watch_ui_set_brightness_committed_callback(commit_test);
    watch_ui_set_settings(50, 30000);
    watch_ui_set_step_goal_callback(goal_test);
    watch_ui_set_steps(5421);
    assert(strcmp(lv_label_get_text(steps_goal_label), "Goal 8,000") == 0);
    assert(lv_bar_get_value(steps_progress) == 5421);
    lv_obj_send_event(goal_plus, LV_EVENT_CLICKED, nullptr);
    assert(daily_step_goal == 9000 && goal_calls == 1);
    assert(strcmp(lv_label_get_text(activity_goal_label), "Goal 9,000") == 0);
    goal_save_ok = false;
    lv_obj_send_event(goal_plus, LV_EVENT_CLICKED, nullptr);
    assert(daily_step_goal == 9000 && goal_calls == 2);
    assert(strcmp(lv_label_get_text(activity_goal_label), "Save failed / retry") == 0);
    goal_save_ok = true;
    watch_ui_set_step_goal(1000);
    assert(lv_obj_has_state(goal_minus, LV_STATE_DISABLED));
    assert(lv_bar_get_value(steps_progress) == 1000);
    lv_obj_send_event(goal_minus, LV_EVENT_CLICKED, nullptr);
    assert(daily_step_goal == 1000 && goal_calls == 2);
    watch_ui_set_step_goal(30000);
    assert(lv_obj_has_state(goal_plus, LV_STATE_DISABLED));
    lv_obj_send_event(goal_plus, LV_EVENT_CLICKED, nullptr);
    assert(daily_step_goal == 30000 && goal_calls == 2);
    watch_ui_set_step_goal(0); watch_ui_set_step_goal(3500);
    assert(daily_step_goal == 30000);
    watch_ui_set_steps(UINT32_MAX);
    assert(lv_bar_get_value(steps_progress) == 30000);
    watch_ui_set_steps(0);
    assert(lv_bar_get_value(steps_progress) == 0);
    watch_ui_set_step_goal(8000);
    lv_obj_send_event(goal_minus, LV_EVENT_CLICKED, nullptr);
    assert(daily_step_goal == 7000 && goal_calls == 3);
    watch_ui_set_step_goal(8000);
    assert(strcmp(lv_label_get_text(settings_brightness_value), "50%") == 0);
    assert(strcmp(lv_label_get_text(settings_timeout_value), "30 sec") == 0);
    lv_obj_t *plus = find_button(screen_for(WatchScreen::Brightness), "+");
    lv_obj_t *minus = find_button(screen_for(WatchScreen::Brightness), "-");
    lv_obj_send_event(plus, LV_EVENT_CLICKED, nullptr);
    assert(current_brightness == 55 && committed == 55 && brightness_calls == 1 && commit_calls == 1);
    assert(strcmp(lv_label_get_text(settings_brightness_value), "55%") == 0);
    lv_obj_send_event(minus, LV_EVENT_CLICKED, nullptr);
    assert(current_brightness == 50 && committed == 50);
    watch_ui_set_settings(100, 30000);
    unsigned before = commit_calls;
    lv_obj_send_event(plus, LV_EVENT_CLICKED, nullptr);
    assert(current_brightness == 100 && commit_calls == before);
    watch_ui_set_settings(10, 30000);
    lv_obj_send_event(minus, LV_EVENT_CLICKED, nullptr);
    assert(current_brightness == 10 && commit_calls == before);
    lv_dropdown_set_selected(screen_timeout_dropdown, 4);
    lv_obj_send_event(screen_timeout_dropdown, LV_EVENT_VALUE_CHANGED, nullptr);
    assert(strcmp(lv_label_get_text(settings_timeout_value), "Never") == 0);

    open_settings_cb(nullptr); open_wifi_cb(nullptr); back_cb(nullptr);
    assert(current_screen == WatchScreen::Settings);
    back_cb(nullptr); assert(current_screen == WatchScreen::Home);
    open_settings_cb(nullptr);
    auto *sound_tile=find_button(screen_for(WatchScreen::Settings),"VOLUME");
    auto *info_button=find_button(screen_for(WatchScreen::Settings),"i");
    assert(info_button);
    lv_obj_update_layout(screen_for(WatchScreen::Settings));
    assert(lv_obj_get_width(info_button)==64 && lv_obj_get_height(info_button)==64);
    assert(lv_obj_get_x(info_button)==(410-64)/2 && lv_obj_get_y(info_button)==214);
    lv_obj_send_event(info_button,LV_EVENT_CLICKED,nullptr);
    assert(current_screen==WatchScreen::Hardware);
    auto *hardware_screen=screen_for(WatchScreen::Hardware);
    auto *hardware_list=lv_obj_get_child(hardware_screen,-1);
    lv_obj_update_layout(hardware_screen);
    assert(lv_obj_get_child_count(hardware_list)==7); // Seven hardware groups.
    assert(lv_obj_get_scroll_bottom(hardware_list)>0);
    assert(lv_obj_get_height(hardware_list)==280);
    lv_obj_scroll_to_y(hardware_list,10000,LV_ANIM_OFF);
    assert(lv_obj_get_scroll_y(hardware_list)>0);
    back_cb(nullptr);
    assert(current_screen==WatchScreen::Settings);
    lv_obj_update_layout(screen_for(WatchScreen::Settings));
    assert(lv_obj_get_width(sound_tile)==108 && lv_obj_get_height(sound_tile)==108);
    assert(lv_obj_get_y(sound_tile)+lv_obj_get_height(sound_tile)<=420);
    lv_obj_send_event(sound_tile,LV_EVENT_CLICKED,nullptr);
    assert(current_screen==WatchScreen::Sound);
    watch_ui_set_volume(40,[](uint8_t percent) { return percent<=75; });
    assert(strcmp(lv_label_get_text(settings_volume_value),"40%")==0);
    assert(!lv_obj_has_state(sound_slider,LV_STATE_DISABLED));
    lv_obj_send_event(sound_slider,LV_EVENT_PRESSED,nullptr);
    lv_slider_set_value(sound_slider,75,LV_ANIM_OFF);
    lv_obj_send_event(sound_slider,LV_EVENT_RELEASED,nullptr);
    assert(lv_slider_get_value(sound_slider)==75 && sound_volume_percent==75);
    assert(strcmp(lv_label_get_text(settings_volume_value),"75%")==0);
    lv_obj_send_event(sound_slider,LV_EVENT_PRESSED,nullptr);
    lv_slider_set_value(sound_slider,100,LV_ANIM_OFF);
    lv_obj_send_event(sound_slider,LV_EVENT_RELEASED,nullptr);
    assert(lv_slider_get_value(sound_slider)==75 && sound_volume_percent==75);
    watch_ui_set_media(0,0,0,nullptr,nullptr);
    assert(strcmp(lv_label_get_text(settings_volume_value),"75%")==0);
    assert(!lv_obj_has_state(sound_slider,LV_STATE_DISABLED));
    watch_ui_set_volume(60,nullptr);
    assert(lv_dropdown_get_selected(sound_gain)==4);
    assert(lv_color_eq(lv_obj_get_style_bg_color(sound_gain,LV_PART_MAIN),lv_obj_get_style_bg_color(screen_timeout_dropdown,LV_PART_MAIN)));
    assert(lv_obj_get_style_text_font(sound_gain,LV_PART_MAIN)==lv_obj_get_style_text_font(screen_timeout_dropdown,LV_PART_MAIN));
    assert(lv_color_eq(lv_obj_get_style_bg_color(lv_dropdown_get_list(sound_gain),LV_PART_MAIN),lv_obj_get_style_bg_color(lv_dropdown_get_list(screen_timeout_dropdown),LV_PART_MAIN)));
    watch_ui_set_microphone_gain(24,[](uint8_t gain) { return gain==30; });
    lv_dropdown_set_selected(sound_gain,5);
    lv_obj_send_event(sound_gain,LV_EVENT_VALUE_CHANGED,nullptr);
    assert(sound_gain_db==30);
    lv_dropdown_set_selected(sound_gain,7);
    lv_obj_send_event(sound_gain,LV_EVENT_VALUE_CHANGED,nullptr);
    assert(sound_gain_db==30 && lv_dropdown_get_selected(sound_gain)==5);
    watch_ui_set_microphone_gain(43,nullptr);
    assert(sound_gain_db==30);
    watch_ui_set_microphone_gain(24,nullptr);
    back_cb(nullptr); assert(current_screen==WatchScreen::Settings);
    back_cb(nullptr); assert(current_screen==WatchScreen::Home);
    open_settings_cb(nullptr);
    auto *saver_tile=find_button(screen_for(WatchScreen::Settings),"BATTERY");
    lv_obj_update_layout(screen_for(WatchScreen::Settings));
    assert(lv_obj_get_width(saver_tile)==108 && lv_obj_get_height(saver_tile)==108 && lv_obj_get_y(saver_tile)+108<=420);
    const char *settings_tiles[]={"WI-FI","BLUETOOTH","BRIGHTNESS","TIMEOUT","VOLUME","BATTERY"};
    for (unsigned i=0;i<6;++i) {
        auto *tile=find_button(screen_for(WatchScreen::Settings),settings_tiles[i]);
        assert(lv_obj_get_width(tile)==108 && lv_obj_get_height(tile)==108);
        assert(lv_obj_get_y(tile)==72+int(i/2)*120);
        const int offsets[]={-62,62,-118,118,-62,62};
        assert(lv_obj_get_x(tile)==(410-108)/2+offsets[i]);
    }
    lv_obj_send_event(saver_tile,LV_EVENT_CLICKED,nullptr);
    assert(current_screen==WatchScreen::BatterySaver);
    watch_ui_set_battery(-1,false);
    assert(strstr(lv_label_get_text(battery_saver_summary),"unavailable"));
    watch_ui_set_battery(15,false);
    assert(strstr(lv_label_get_text(battery_saver_summary),"LOW BATTERY"));
    watch_ui_set_battery(15,true);
    assert(strstr(lv_label_get_text(battery_saver_summary),"CHARGING"));
    watch_ui_set_settings(80,0);
    assert(strstr(lv_label_get_text(battery_saver_details[0]),"Try 30%"));
    assert(strstr(lv_label_get_text(battery_saver_details[1]),"Never"));
    watch_ui_set_settings(20,5000);
    assert(strstr(lv_label_get_text(battery_saver_details[0]),"Already low"));
    assert(strstr(lv_label_get_text(battery_saver_details[1]),"Already short"));
    for (unsigned i=0;i<4;++i) {
        const WatchScreen target[]={WatchScreen::Brightness,WatchScreen::Timeout,WatchScreen::Wifi,WatchScreen::Bluetooth};
        lv_obj_send_event(battery_saver_cards[i],LV_EVENT_CLICKED,nullptr);
        assert(current_screen==target[i]);
        back_cb(nullptr); assert(current_screen==WatchScreen::BatterySaver && !battery_saver_return);
    }
    watch_ui_set_settings(10,0);
    back_cb(nullptr); assert(current_screen==WatchScreen::Settings);
    back_cb(nullptr); assert(current_screen==WatchScreen::Home);
    open_launcher_cb(nullptr); open_timer_cb(nullptr);
    assert(pomodoro_duration()==25*60000);
    assert(strcmp(lv_label_get_text(timer_status),"READY WHEN YOU ARE")==0);
    assert(strcmp(lv_label_get_text(pomodoro_phase),"FOCUS TIME")==0);
    auto *skip_phase=find_button(screen_for(WatchScreen::Timer),"SKIP PHASE");
    lv_obj_update_layout(screen_for(WatchScreen::Timer));
    assert(lv_obj_get_y(skip_phase)+lv_obj_get_height(skip_phase)<=414);
    assert(lv_obj_get_width(pomodoro_start)==304);
    countdown.elapsed_ms=pomodoro_duration()/2;
    refresh_time_tools();
    assert(lv_arc_get_value(pomodoro_progress)==500);
    assert(strcmp(lv_label_get_text(timer_value),"12:30")==0);
    timer_reset_cb(nullptr);
    timer_control_cb(nullptr);
    lv_tick_inc(1000); time_tools_tick(nullptr);
    assert(strcmp(lv_label_get_text(timer_value), "24:59") == 0);
    assert(lv_arc_get_value(pomodoro_progress)==0);
    assert(!lv_obj_has_flag(pomodoro_progress,LV_OBJ_FLAG_CLICKABLE));
    timer_control_cb(nullptr);
    lv_tick_inc(5000); time_tools_tick(nullptr);
    assert(countdown.elapsed_ms == 1000 && !countdown.running);
    timer_control_cb(nullptr); stopwatch_control_cb(nullptr);
    watch_ui_set_display_active(false);
    lv_tick_inc(1499000); time_tools_tick(nullptr);
    assert(!countdown.running && countdown.elapsed_ms == 1500000);
    assert(stopwatch.running && stopwatch.elapsed_ms == 1499000);
    // Expiry while IDLE must not redraw until wake.
    assert(strcmp(lv_label_get_text(timer_value), "24:59") == 0);
    watch_ui_set_display_active(true);
    assert(strcmp(lv_label_get_text(timer_value), "00:00") == 0);
    assert(lv_arc_get_value(pomodoro_progress)==1000);
    assert(strstr(lv_label_get_text(timer_status), "DONE"));
    timer_control_cb(nullptr);
    assert(pomodoro_break && pomodoro_completed==1 && countdown.running && pomodoro_duration()==300000);
    assert(strcmp(lv_label_get_text(pomodoro_phase),"SHORT BREAK")==0);
    assert(lv_arc_get_value(pomodoro_progress)==0);
    assert(lv_color_eq(lv_obj_get_style_arc_color(pomodoro_progress,LV_PART_INDICATOR),lv_color_hex(0x86C978)));
    for (unsigned i=1;i<4;++i) { pomodoro_next_cb(nullptr); pomodoro_next_cb(nullptr); }
    assert(pomodoro_break && pomodoro_completed==4 && pomodoro_duration()==900000);
    pomodoro_next_cb(nullptr);
    assert(!pomodoro_break && pomodoro_completed==0 && !countdown.running);
    timer_reset_cb(nullptr);
    assert(countdown.elapsed_ms==0 && pomodoro_duration()==1500000);
    assert(lv_arc_get_value(pomodoro_progress)==0);
    back_cb(nullptr); open_stopwatch_cb(nullptr);
    assert(strcmp(lv_label_get_text(stopwatch_value), "00:24:59.0") == 0);
    stopwatch_control_cb(nullptr); stopwatch_reset_cb(nullptr);
    assert(stopwatch.elapsed_ms == 0 && !stopwatch.running);
    back_cb(nullptr); assert(current_screen == WatchScreen::Launcher);
    watch_ui_forecast_day_t forecast[7] = {};
    forecast[0] = {true, 20261003, 6, 12, 24, WATCH_UI_WEATHER_SUNNY};
    watch_ui_set_forecast(forecast, false, 45);
    assert(strcmp(lv_label_get_text(weather_range_label), "H 24\xC2\xB0 / L 12\xC2\xB0") == 0);
    open_weather_cb(nullptr);
    assert(strcmp(lv_label_get_text(forecast_names[0]), "TODAY 03/10") == 0);
    assert(strcmp(lv_label_get_text(forecast_values[0]), "24 / 12") == 0);
    assert(strstr(lv_label_get_text(forecast_status), "OFFLINE / CACHED"));
    assert(strcmp(lv_label_get_text(forecast_values[1]), "-- / --") == 0);
    watch_ui_set_forecast(nullptr, true, 0);
    assert(strcmp(lv_label_get_text(weather_range_label), "H -- / L --") == 0);
    assert(strcmp(lv_label_get_text(forecast_values[0]), "-- / --") == 0);
    back_cb(nullptr); assert(current_screen == WatchScreen::Launcher);
    watch_ui_set_voice_callback(voice_test_action);
    open_voice_cb(nullptr); assert(current_screen == WatchScreen::Voice);
    watch_ui_set_voice(0,0,0,nullptr);
    assert(!lv_obj_has_flag(voice_face,LV_OBJ_FLAG_HIDDEN));
    assert(!lv_obj_has_flag(voice_face,LV_OBJ_FLAG_CLICKABLE));
    assert(strcmp(lv_label_get_text(voice_name),"Mochi") == 0);
    assert(!lv_obj_has_flag(voice_name,LV_OBJ_FLAG_HIDDEN));
    assert(strcmp(lv_label_get_text(lv_obj_get_child(voice_hold,0)),"HOLD TO TALK") == 0);
    assert(lv_obj_has_flag(voice_clear,LV_OBJ_FLAG_HIDDEN));
    assert(lv_obj_has_flag(voice_details,LV_OBJ_FLAG_HIDDEN));
    assert(lv_obj_has_flag(voice_level,LV_OBJ_FLAG_HIDDEN));
    lv_obj_update_layout(screen_for(WatchScreen::Voice));
    const int idle_hold_y = lv_obj_get_y(voice_hold);
    assert(lv_obj_get_height(voice_hold) == 72);
    for (uint8_t busy_state : {1,2,3,6,7}) {
        watch_ui_set_voice(busy_state,1000,0,nullptr);
        assert(lv_obj_has_flag(voice_clear,LV_OBJ_FLAG_HIDDEN) == (busy_state != 2));
        assert(strcmp(lv_label_get_text(lv_obj_get_child(voice_clear,0)),"CANCEL") == 0);
        if (busy_state == 2 || busy_state == 6) {
            lv_obj_update_layout(screen_for(WatchScreen::Voice));
            assert(lv_obj_get_y(voice_hold) == idle_hold_y && lv_obj_get_height(voice_hold) == 72);
            assert(!lv_obj_has_flag(voice_level,LV_OBJ_FLAG_HIDDEN));
            assert(!lv_obj_has_flag(voice_details,LV_OBJ_FLAG_HIDDEN));
        }
    }
    for (uint8_t finished_state : {5,8}) {
        watch_ui_set_voice(finished_state,1000,0,nullptr);
        assert(lv_obj_has_flag(voice_clear,LV_OBJ_FLAG_HIDDEN) == (finished_state == 8));
        assert(lv_obj_has_flag(voice_hold,LV_OBJ_FLAG_HIDDEN) == (finished_state == 8));
    }
    watch_ui_set_voice(0,0,0,nullptr);
    auto *ai_tile = find_button(screen_for(WatchScreen::Launcher),"AI ASSISTANT");
    auto *timer_tile = find_button(screen_for(WatchScreen::Launcher),"POMODORO");
    assert(lv_color_eq(lv_obj_get_style_bg_color(ai_tile,LV_PART_MAIN),lv_obj_get_style_bg_color(timer_tile,LV_PART_MAIN)));
    assert(lv_color_eq(lv_obj_get_style_border_color(ai_tile,LV_PART_MAIN),lv_obj_get_style_border_color(timer_tile,LV_PART_MAIN)));
    for (uint32_t i = 0; i < lv_obj_get_child_count(ai_tile); ++i) {
        auto *child = lv_obj_get_child(ai_tile,i);
        if (lv_obj_check_type(child,&lv_label_class)) assert(strcmp(lv_label_get_text(child),"HOLD TO TALK") != 0);
    }
    assert(!lv_obj_has_flag(voice_microphone,LV_OBJ_FLAG_HIDDEN));
    assert(lv_color_eq(lv_obj_get_style_bg_color(voice_hold,LV_PART_MAIN),lv_color_hex(0xC4A7FF)));
    assert(lv_obj_get_child(screen_for(WatchScreen::Voice),-1) == voice_name);
    {
        const bool blinking = voice_face_blink;
        watch_ui_set_display_active(false);
        voice_face_tick(nullptr);
        assert(voice_face_blink == blinking); // No hidden-screen animation work.
        watch_ui_set_display_active(true);
    }
    lv_obj_update_layout(screen_for(WatchScreen::Voice));
    assert(lv_obj_get_width(voice_face) == 176);
    assert(lv_obj_get_y(voice_face)+lv_obj_get_height(voice_face)/2+58+8 <= lv_obj_get_y(voice_details));
    assert(lv_obj_get_y(voice_name) == 66 && lv_obj_get_y(voice_face) == 64);
    for (uint8_t state : {1,2,3,6,7,8,5,0}) {
        watch_ui_set_voice(state,1000,0,nullptr);
        lv_obj_update_layout(screen_for(WatchScreen::Voice));
        assert(lv_obj_get_y(voice_face)==64 && lv_obj_get_x(voice_face)==117);
        assert(lv_obj_get_y(voice_name)==66);
    }
    assert(lv_obj_get_y(voice_hold)+lv_obj_get_height(voice_hold)+8 <= lv_obj_get_y(voice_clear));
    lv_obj_send_event(voice_hold,LV_EVENT_PRESSED,nullptr); assert(voice_held && voice_calls[1] == 1);
    lv_obj_send_event(voice_hold,LV_EVENT_RELEASED,nullptr); assert(!voice_held && voice_calls[2] == 1);
    watch_ui_set_voice(3,2500,50,nullptr);
    watch_ui_set_voice(6,2500,0,nullptr); assert(lv_obj_has_state(voice_hold,LV_STATE_DISABLED));
    watch_ui_set_voice(0,0,0,nullptr);
    assert(!lv_label_get_text(voice_status)[0]);
    assert(!lv_label_get_text(voice_note)[0]);
    lv_obj_send_event(voice_hold,LV_EVENT_PRESSED,nullptr);
    const unsigned cancels = voice_calls[3];
    lv_obj_send_event(voice_hold,LV_EVENT_PRESS_LOST,nullptr); assert(!voice_held && voice_calls[3] == cancels+1);
    lv_obj_send_event(voice_hold,LV_EVENT_PRESSED,nullptr); watch_ui_set_display_active(false);
    assert(!voice_held && voice_calls[3] == cancels+2); watch_ui_set_display_active(true);
    lv_obj_send_event(voice_hold,LV_EVENT_PRESSED,nullptr); watch_ui_show_notification("Phone","Test","Interrupt capture");
    assert(voice_held && voice_calls[3] == cancels+2 && current_screen == WatchScreen::Voice);
    lv_obj_send_event(voice_hold,LV_EVENT_RELEASED,nullptr);
    watch_ui_hide_notification(); assert(current_screen == WatchScreen::Voice);
    lv_obj_send_event(voice_hold,LV_EVENT_PRESSED,nullptr); back_cb(nullptr);
    assert(!voice_held && voice_calls[3] == cancels+3 && current_screen == WatchScreen::Launcher);
    watch_ui_set_voice_callback(nullptr);
    WatchVoiceResult ai_result;
    const char *spanish = "¿Qué tal?";
    assert(ai_result.accept(1,0,strlen(spanish),reinterpret_cast<const uint8_t *>(spanish),3));
    assert(!ai_result.accept(1,4,strlen(spanish),reinterpret_cast<const uint8_t *>(spanish)+3,1));
    assert(ai_result.accept(1,3,strlen(spanish),reinterpret_cast<const uint8_t *>(spanish)+3,strlen(spanish)-3));
    assert(ai_result.complete() && strcmp(ai_result.transcription,spanish) == 0);
    const uint8_t malformed[] = {0xc0,0x80};
    assert(!ai_result.accept(2,0,2,malformed,2));
    assert(!ai_result.accept(2,0,3073,reinterpret_cast<const uint8_t *>("x"),1));
    watch_ui_set_voice_text(spanish,"Una respuesta larga\ncon varias líneas.");
    watch_ui_set_voice(8,0,0,nullptr);
    assert(strcmp(lv_label_get_text(voice_transcription),spanish) == 0);
    assert(strcmp(lv_label_get_text(voice_answer),"Una respuesta larga\ncon varias líneas.") == 0);
    assert(strcmp(lv_label_get_text(voice_question_heading),"QUESTION:") == 0);
    assert(strcmp(lv_label_get_text(voice_answer_heading),"ANSWER:") == 0);
    assert(lv_obj_has_flag(voice_hold,LV_OBJ_FLAG_HIDDEN));
    assert(lv_obj_has_flag(voice_clear,LV_OBJ_FLAG_HIDDEN));
    lv_obj_update_layout(screen_for(WatchScreen::Voice));
    auto *answer_box=static_cast<lv_obj_t *>(lv_obj_get_user_data(voice_answer));
    assert(lv_obj_get_height(answer_box)==184);
    assert(lv_obj_has_flag(voice_status,LV_OBJ_FLAG_HIDDEN));
    assert(lv_obj_has_flag(voice_happy,LV_OBJ_FLAG_HIDDEN));
    assert(!lv_obj_has_flag(voice_face,LV_OBJ_FLAG_HIDDEN));
    // The halo bitmap includes black outer margins; preserve clearance from
    // the visible ring without reserving the entire transparent-looking box.
    assert(lv_obj_get_y(voice_face)+lv_obj_get_height(voice_face)/2+58+8<=lv_obj_get_y(answer_box));
    auto *answer_back=find_button(screen_for(WatchScreen::Voice),LV_SYMBOL_LEFT "  BACK");
    assert(lv_obj_get_y(answer_box)+lv_obj_get_height(answer_box)+8<=lv_obj_get_y(answer_back));
    assert(!lv_obj_has_flag(static_cast<lv_obj_t *>(lv_obj_get_user_data(voice_answer)),LV_OBJ_FLAG_HIDDEN));
    ai_result.clear(); assert(!ai_result.transcription[0] && !ai_result.answer[0]);
    assert(watch_voice_feedback::percent(0,0) == 0);
    assert(watch_voice_feedback::percent(2000,8000) == 25);
    assert(watch_voice_feedback::percent(7999,8000) == 99);
    assert(watch_voice_feedback::percent(9000,8000) == 100);
    watch_ui_set_voice_text("","");
    watch_ui_set_voice(6,2000,90,nullptr);
    watch_ui_set_voice_progress(2000,8000);
    assert(strstr(lv_label_get_text(voice_status),"25%"));
    assert(lv_bar_get_value(voice_level) == 25);
    assert(strstr(lv_label_get_text(voice_details),"2000 / 8000"));
    watch_ui_set_voice(2,2000,73,nullptr);
    assert(strstr(lv_label_get_text(voice_status),"release to send"));
    assert(lv_bar_get_value(voice_level) == 73); // Microphone level is not transfer percent.
    watch_ui_set_voice(7,2000,0,nullptr);
    assert(!strcmp(lv_label_get_text(voice_status),"Thinking..."));
    assert(!strchr(lv_label_get_text(voice_status),'%'));
    assert(lv_obj_has_flag(voice_waiting,LV_OBJ_FLAG_HIDDEN));
    assert(!lv_obj_has_flag(voice_face,LV_OBJ_FLAG_HIDDEN));
    assert(!lv_obj_has_flag(voice_name,LV_OBJ_FLAG_HIDDEN));
    assert(lv_obj_has_flag(static_cast<lv_obj_t *>(lv_obj_get_user_data(voice_answer)),LV_OBJ_FLAG_HIDDEN));
    assert(lv_obj_has_flag(voice_hold,LV_OBJ_FLAG_HIDDEN));
    assert(lv_obj_has_flag(voice_details,LV_OBJ_FLAG_HIDDEN));
    assert(lv_obj_has_flag(voice_clear,LV_OBJ_FLAG_HIDDEN));
    assert(!strcmp(lv_label_get_text(lv_obj_get_child(voice_clear,0)),"CANCEL"));
    watch_ui_set_voice_text("","AI disabled");
    watch_ui_set_voice(5,0,0,watch_voice_feedback::message("AI_DISABLED"));
    assert(strcmp(lv_label_get_text(voice_status),"AI disabled") == 0);
    assert(strstr(lv_label_get_text(voice_answer),"Enable AI"));
    assert(strcmp(watch_voice_feedback::message("AI_SIGN_IN"),"Sign in on your phone") == 0);
    assert(strcmp(watch_voice_feedback::message("unknown"),"Connection error") == 0);
    watch_ui_set_bluetooth_state(WATCH_UI_BLUETOOTH_DISABLED);
    watch_ui_set_voice(3,2000,0,nullptr);
    assert(strcmp(lv_label_get_text(voice_status),"Phone disconnected") == 0);
    open_voice_cb(nullptr); process_swipe(LV_DIR_TOP); assert(current_screen == WatchScreen::Voice);
    process_swipe(LV_DIR_RIGHT); assert(current_screen == WatchScreen::Launcher);
    assert(voice_state == 0 && !voice_held && !lv_label_get_text(voice_answer)[0]);
    navigate_to(WatchScreen::Bluetooth); lv_obj_update_layout(screen_for(WatchScreen::Bluetooth));
    auto *bluetooth_back = find_button(screen_for(WatchScreen::Bluetooth),LV_SYMBOL_LEFT "  BACK");
    {
        auto *home = screen_for(WatchScreen::Home);
        lv_obj_update_layout(home);
        auto *settings = find_button(home,LV_SYMBOL_SETTINGS "  SETTINGS");
        auto *apps = find_button(home,LV_SYMBOL_LIST "  APPS");
        assert(lv_obj_get_width(settings) == 136 && lv_obj_get_width(apps) == 136);
        assert(lv_obj_get_height(settings) == 56 && lv_obj_get_height(apps) == 56);
        assert(lv_obj_get_x(apps)-lv_obj_get_x(settings)-lv_obj_get_width(settings) == 16);
        assert(lv_obj_get_y(settings) == lv_obj_get_y(apps));
    }
    for (auto *screen : screens) {
        lv_obj_update_layout(screen);
        for (uint32_t i = 0; i < lv_obj_get_child_count(screen); ++i) {
            auto *object = lv_obj_get_child(screen,i);
            if (!lv_obj_check_type(object,&lv_button_class) || !lv_obj_get_child_count(object)) continue;
            auto *text = lv_obj_get_child(object,0);
            if (lv_obj_check_type(text,&lv_label_class) && lv_obj_get_child_count(object) == 2 &&
                (!strcmp(lv_label_get_text(text),LV_SYMBOL_LEFT) ||
                 !strcmp(lv_label_get_text(text),LV_SYMBOL_HOME))) {
                assert(lv_obj_get_width(object) == design::BackWidth);
                assert(lv_obj_get_height(object) >= design::TouchHeight);
                assert(lv_obj_get_style_radius(object,LV_PART_MAIN) == design::TouchHeight/2);
                auto *title = lv_obj_get_child(object,1);
                assert(lv_obj_get_x(title)-lv_obj_get_x(text)-lv_obj_get_width(text) == 8);
                const int group_center_twice = lv_obj_get_x(text)+lv_obj_get_x(title)+lv_obj_get_width(title);
                assert(group_center_twice >= lv_obj_get_width(object)-2 && group_center_twice <= lv_obj_get_width(object)+2);
            }
        }
    }
    assert(lv_obj_get_y(bluetooth_action)+lv_obj_get_height(bluetooth_action)+8 <= lv_obj_get_y(forget_phone_button));
    assert(lv_obj_get_y(forget_phone_button)+lv_obj_get_height(forget_phone_button)+8 <= lv_obj_get_y(bluetooth_back));
    lv_obj_add_state(bluetooth_action,LV_STATE_DISABLED);
    assert(lv_color_eq(lv_obj_get_style_text_color(bluetooth_action_label,LV_PART_MAIN),lv_color_hex(design::Disabled)));
    {
        find_button(screen_for(WatchScreen::Launcher),"SUN & MOON");
        watch_ui_set_astronomy_time(0,false);
        open_astronomy_cb(nullptr);
        assert(current_screen == WatchScreen::Astronomy);
        for (auto *arrow : {astronomy_previous_day,astronomy_next_day}) {
            assert(lv_obj_get_style_radius(arrow,LV_PART_MAIN) == LV_RADIUS_CIRCLE);
        }
        assert(!lv_obj_has_flag(astronomy_empty,LV_OBJ_FLAG_HIDDEN));
        assert(lv_obj_has_state(astronomy_next_day,LV_STATE_DISABLED));
        const int64_t noon = astronomy::day_number({2026,10,24})*86400+12*3600;
        watch_ui_set_astronomy_time(noon,true);
        assert(lv_obj_has_flag(astronomy_empty,LV_OBJ_FLAG_HIDDEN));
        assert(!strcmp(lv_label_get_text(astronomy_first_title),"SUNRISE"));
        assert(strcmp(lv_label_get_text(astronomy_first_value),"--:--"));
        lv_obj_send_event(astronomy_next_day,LV_EVENT_CLICKED,nullptr);
        assert(astronomy_day_offset == 1 && strstr(lv_label_get_text(astronomy_date_label),"25/10"));
        lv_obj_send_event(astronomy_moon_tab,LV_EVENT_CLICKED,nullptr);
        assert(lv_color_eq(lv_obj_get_style_text_color(astronomy_first_value,LV_PART_MAIN),lv_color_hex(design::Timeout)));
        assert(lv_color_eq(lv_obj_get_style_text_color(astronomy_second_value,LV_PART_MAIN),lv_color_hex(design::Timeout)));
        assert(astronomy_moon_view && !strcmp(lv_label_get_text(astronomy_first_title),"ILLUMINATION"));
        for (auto *tab : {astronomy_sun_tab,astronomy_moon_tab}) {
            assert(strcmp(lv_label_get_text(lv_obj_get_child(tab,0)),"")==0);
            const auto color = lv_color_hex(tab == astronomy_sun_tab ? design::Weather : design::Timeout);
            assert(lv_color_eq(lv_obj_get_style_text_color(lv_obj_get_child(tab,0),LV_PART_MAIN),color));
        }
        lv_obj_send_event(astronomy_sun_tab,LV_EVENT_CLICKED,nullptr);
        assert(lv_color_eq(lv_obj_get_style_text_color(astronomy_first_value,LV_PART_MAIN),lv_color_hex(design::Weather)));
        assert(lv_color_eq(lv_obj_get_style_text_color(astronomy_second_value,LV_PART_MAIN),lv_color_hex(design::Weather)));
        assert(lv_obj_has_state(astronomy_sun_tab,LV_STATE_DISABLED));
        assert(lv_color_eq(lv_obj_get_style_text_color(lv_obj_get_child(astronomy_sun_tab,0),LV_PART_MAIN),lv_color_hex(design::Weather)));
        for (int i = 0; i < 9; ++i) lv_obj_send_event(astronomy_next_day,LV_EVENT_CLICKED,nullptr);
        assert(astronomy_day_offset == 6 && lv_obj_has_state(astronomy_next_day,LV_STATE_DISABLED));
        for (int i = 0; i < 9; ++i) lv_obj_send_event(astronomy_previous_day,LV_EVENT_CLICKED,nullptr);
        assert(!astronomy_day_offset && lv_obj_has_state(astronomy_previous_day,LV_STATE_DISABLED));
        watch_ui_set_display_active(false);
        watch_ui_set_astronomy_time(noon+86400,true);
        assert(strstr(lv_label_get_text(astronomy_date_label),"24/10")); // No asleep redraw.
        watch_ui_set_display_active(true);
        assert(strstr(lv_label_get_text(astronomy_date_label),"25/10"));
        back_cb(nullptr); assert(current_screen == WatchScreen::Launcher);
        watch_ui_set_astronomy_time(noon+2*86400,true);
        assert(strstr(lv_label_get_text(astronomy_date_label),"25/10")); // Hidden app stays quiet.
        open_astronomy_cb(nullptr);
        assert(strstr(lv_label_get_text(astronomy_date_label),"26/10"));
        watch_ui_show_notification("Phone","Test","Astronomy interruption");
        watch_ui_hide_notification(); assert(current_screen == WatchScreen::Astronomy);
        lv_obj_update_layout(screen_for(WatchScreen::Astronomy));
        auto *back = find_button(screen_for(WatchScreen::Astronomy),LV_SYMBOL_LEFT "  BACK");
        assert(lv_obj_get_y(astronomy_next)+lv_obj_get_height(astronomy_next) <= lv_obj_get_y(back));
        back_cb(nullptr);
    }
    for (auto *screen : screens) { lv_obj_update_layout(screen); check_touch_targets(screen); }
    {
        assert(!watch_planets::valid_clock(0));
        for (unsigned p=0;p<8;++p) {
            const auto pos=watch_planets::position(p,946728000);
            assert(isfinite(pos.longitude) && pos.longitude>=0 && pos.longitude<6.284);
            assert(pos.distance_au>watch_planets::elements[p].a*.75);
            assert(pos.distance_au<watch_planets::elements[p].a*1.25);
        }
        // J2000 Earth/Moon barycentre ~100.38 degrees, radius ~0.9833 AU.
        auto earth=watch_planets::position(2,946728000);
        assert(fabs(earth.longitude*180/3.141592653589793-100.38)<.1);
        assert(fabs(earth.distance_au-.9833)<.001);
        find_button(screen_for(WatchScreen::Launcher),"PLANETS");
        watch_ui_set_astronomy_time(0,false);
        open_planets_cb(nullptr);
        assert(current_screen==WatchScreen::Planets && lv_obj_has_flag(planet_dots[0],LV_OBJ_FLAG_HIDDEN));
        watch_ui_set_astronomy_time(1792324800,true);
        assert(!lv_obj_has_flag(planet_dots[0],LV_OBJ_FLAG_HIDDEN));
        assert(lv_obj_has_flag(planets_empty,LV_OBJ_FLAG_HIDDEN));
        lv_obj_update_layout(screen_for(WatchScreen::Planets));
        assert(lv_obj_get_height(planets_map)==354 && lv_obj_get_width(planets_map)==404);
        for (unsigned p=0;p<8;++p) {
            assert(!lv_obj_has_flag(planet_dots[p],LV_OBJ_FLAG_CLICKABLE));
            auto *planet_name=planet_labels[p];
            assert(lv_obj_get_style_bg_opa(planet_name,LV_PART_MAIN)==LV_OPA_TRANSP);
            assert(!lv_obj_has_flag(planet_name,LV_OBJ_FLAG_HIDDEN));
            assert(lv_obj_get_width(planet_dots[p])==44 && lv_obj_get_height(planet_dots[p])==44);
            lv_obj_update_layout(planets_map);
            assert(!strcmp(lv_label_get_text(planet_name),planet_names[p]));
            assert(lv_color_eq(lv_obj_get_style_text_color(planet_name,LV_PART_MAIN),lv_color_hex(planet_colors[p])));
            assert(lv_obj_get_x(planet_name)>=0 && lv_obj_get_y(planet_name)>=0);
            assert(lv_obj_get_x(planet_name)+lv_obj_get_width(planet_name)<=lv_obj_get_width(planets_map));
            assert(lv_obj_get_y(planet_name)+lv_obj_get_height(planet_name)<=lv_obj_get_height(planets_map));
            if (p==7) {
                const int label_y=lv_obj_get_y(planet_name)+lv_obj_get_height(planet_name)/2;
                const int planet_y=lv_obj_get_y(planet_dots[p])+lv_obj_get_height(planet_dots[p])/2;
                assert(abs(label_y-planet_y)<=18);
            }
        }
        back_cb(nullptr); assert(current_screen==WatchScreen::Launcher);
    }
    {
        const char *names[]={"ACTIVITY","POMODORO","STOPWATCH","WEATHER","MEDIA","2048","SUN & MOON","PLANETS","AI ASSISTANT"};
        auto *launcher=screen_for(WatchScreen::Launcher);
        lv_obj_update_layout(launcher);
        for (unsigned i=0;i<9;++i) {
            auto *tile=find_button(launcher,names[i]);
            assert(lv_obj_get_width(tile)==108 && lv_obj_get_height(tile)==100);
            assert(lv_obj_get_y(tile)==92+int(i/3)*110);
            assert(lv_obj_get_x(tile)==lv_obj_get_x(find_button(launcher,names[i%3])));
            assert(lv_obj_get_y(lv_obj_get_child(tile,0))==70);
        }
    }
    {
        open_voice_cb(nullptr);
        watch_ui_set_voice_text("Question","Answer");
        watch_ui_set_voice_callback(voice_test_action);
        watch_ui_set_voice(8,0,0,nullptr);
        const unsigned before=voice_calls[3];
        watch_ui_show_notification("Phone","Message","Incoming notification");
        assert(current_screen==WatchScreen::Notification && voice_state==8);
        assert(voice_calls[3]==before && !strcmp(lv_label_get_text(voice_answer),"Answer"));
        watch_ui_hide_notification();
        assert(current_screen==WatchScreen::Voice && voice_state==8);
        watch_ui_set_voice(7,0,0,nullptr);
        watch_ui_show_notification("Phone","Message","Processing interruption");
        assert(voice_state==7 && voice_calls[3]==before);
        watch_ui_set_voice_text("New question","New answer");
        watch_ui_set_voice(8,0,0,nullptr);
        watch_ui_hide_notification();
        assert(current_screen==WatchScreen::Voice && voice_state==8);
        assert(!strcmp(lv_label_get_text(voice_answer),"New answer"));
        watch_ui_set_voice(2,1000,0,nullptr);
        voice_held=true;
        watch_ui_show_notification("Phone","Message","Recording interruption");
        assert(current_screen==WatchScreen::Voice && voice_held && voice_notification_pending);
        voice_held=false;
        watch_ui_set_voice(6,1000,0,nullptr);
        assert(current_screen==WatchScreen::Notification && voice_state==6 && voice_calls[3]==before);
        watch_ui_hide_notification();
        assert(current_screen==WatchScreen::Voice);
        back_cb(nullptr);
        assert(voice_calls[3]==before+1 && voice_state==0);
    }
    #if LV_USE_SYSMON && LV_USE_PERF_MONITOR
    {
        auto *layer=lv_display_get_layer_sys(lv_display_get_default());
        bool compact_found=false;
        for (uint32_t i=0;i<lv_obj_get_child_count(layer);++i) {
            auto *object=lv_obj_get_child(layer,i);
            if (!lv_obj_check_type(object,&lv_label_class) || lv_obj_has_flag(object,LV_OBJ_FLAG_HIDDEN)) continue;
            const char *text=lv_label_get_text(object);
            if (!strstr(text,"FPS")) continue;
            assert(strstr(text," FPS | ") && strstr(text,"% CPU | ") && !strchr(text,',') && !strchr(text,'(') && !strchr(text,'\n'));
            assert(lv_obj_get_style_align(object,LV_PART_MAIN)==LV_ALIGN_BOTTOM_MID);
            compact_found=true;
        }
        assert(compact_found);
    }
    #endif
    puts("PASS: UI/navigation/time tools, 2048, media, Spanish fonts, voice privacy routing and bounded Spanish AI results");
}
