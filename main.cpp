// xenon_ie: Internet Explorer for Xbox 360 UI, rebuilt on xenosworks xsdk.
// Layout of this file: xsdk_network_limits (needs <xsdk/net.h> first, per xsdk docs),
// then URL helpers, the app, and main().
#include <xsdk/net.h>  // XsdkNetworkLimits — keep this include before the definition
extern "C" const XsdkNetworkLimits xsdk_network_limits = { 16, 8, 32, 32 };

#include "addressbar.h"
#include "browser.h"
#include "gfx.h"
#include "hub.h"
#include "input.h"
#include "keyboard.h"
#include "store.h"
#include "theme.h"

#include <xsdk/log.h>
#include <xsdk/splash.h>
#include <xsdk/xam.h>

#include <string>
#include <memory> // Heap allocation ke liye zaroori hai

namespace {

enum class State { Hub, Page, Edit };

std::string normalize_url(std::string s)
{
    const auto is_alnum = [](unsigned char c) {
        return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
    };
    while (!s.empty() && s.front() == ' ')
        s.erase(s.begin());
    while (!s.empty() && s.back() == ' ')
        s.pop_back();
    if (s.empty())
        return "";
    if (s.rfind("http://", 0) == 0 || s.rfind("https://", 0) == 0)
        return s;
    const bool host_like = s.find(' ') == std::string::npos && s.find('.') != std::string::npos;
    if (host_like) {
        if (s.rfind("www.", 0) != 0)
            return "http://www." + s;
        return "http://" + s;
    }
    // Not a URL: run it as a Bing search, like the real browser's suggestion tile.
    std::string q;
    const char* hex = "0123456789ABCDEF";
    for (unsigned char c : s) {
        if (is_alnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            q += static_cast<char>(c);
        } else {
            q += '%';
            q += hex[c >> 4];
            q += hex[c & 15];
        }
    }
    return "http://www.bing.com/search?q=" + q;
}

void hint(float& x, uint32_t color, const char* btn, const char* text)
{
    gfx::text(x, 694, 1, color, btn);
    x += gfx::text_width(btn, 1) + 6;
    gfx::text(x, 694, 1, theme::kTextDim, text);
    x += gfx::text_width(text, 1) + 30;
}

void draw_about()
{
    gfx::fill_rect(430, 250, 420, 200, theme::kFieldBorder);
    gfx::fill_rect(434, 254, 412, 192, theme::kTopBarBg);
    gfx::text(480, 292, 2, theme::kText, "Xenon IE");
    gfx::text(480, 336, 1, theme::kTextDim, "Internet Explorer for Xbox 360 UI");
    gfx::text(480, 360, 1, theme::kTextDim, "built with xenosworks xsdk");
    gfx::text(480, 396, 1, theme::kText, "Press any button to close");
}

int run()
{
    // FIX: Moved all large objects to Heap (std::make_unique) to prevent Stack Overflow
    auto store = std::make_unique<Store>();
    store->load();
    
    auto browser = std::make_unique<Browser>();
    auto bar = std::make_unique<AddressBar>(*store);
    auto keyboard = std::make_unique<Keyboard>();
    auto hub = std::make_unique<Hub>();
    auto pad = std::make_unique<Pad>();

    State state = State::Hub;
    std::string flash;
    int flash_frames = 0;
    bool about = false;
    bool quit = false;

    for (;;) {
        pad->update();
        if (pad->hit(INPUT_GAMEPAD_BACK))
            break;

        const bool sysui = xsdk_system_ui_open() != 0;  // poll once a frame
        std::string nav_to;

        if (about) {
            if (pad->hit(INPUT_GAMEPAD_A) || pad->hit(INPUT_GAMEPAD_B)
                || pad->hit(INPUT_GAMEPAD_X) || pad->hit(INPUT_GAMEPAD_Y))
                about = false;
        } else if (!sysui) {
            switch (state) {
            case State::Hub: {
                const HubAction a = hub->update(*pad, *store, browser->has_page());
                if (a.hide_hub)
                    state = State::Page;
                if (!a.navigate.empty())
                    nav_to = a.navigate;
                if (pad->hit(INPUT_GAMEPAD_Y)) {
                    state = State::Edit;
                    bar->begin_edit(browser->url());
                    keyboard->show();
                }
                break;
            }
            case State::Page: {
                const std::string link = browser->update(*pad);
                if (!link.empty())
                    nav_to = link;
                const AddressBar::Result r = bar->update(*pad, !link.empty());
                switch (r) {
                case AddressBar::Result::Back: browser->back(); break;
                case AddressBar::Result::Forward: browser->forward(); break;
                case AddressBar::Result::Reload: browser->reload(); break;
                case AddressBar::Result::ToggleFav:
                    if (browser->has_page()) {
                        if (store->is_favorite(browser->url())) {
                            for (size_t i = 0; i < store->favorites().size(); i++)
                                if (store->favorites()[i].url == browser->url())
                                    store->remove_favorite(i);
                            flash = "Removed from favorites";
                        } else {
                            store->add_favorite(browser->title(), browser->url());
                            flash = "Added to favorites";
                        }
                        flash_frames = 120;
                    }
                    break;
                case AddressBar::Result::About: about = true; break;
                case AddressBar::Result::Exit: quit = true; break;
                case AddressBar::Result::Edit:
                    state = State::Edit;
                    bar->begin_edit(browser->url());
                    keyboard->show();
                    break;
                default: break;
                }
                if (pad->hit(INPUT_GAMEPAD_Y)) {
                    state = State::Edit;
                    bar->begin_edit(browser->url());
                    keyboard->show();
                }
                if (pad->hit(INPUT_GAMEPAD_B)) {
                    if (browser->can_back())
                        browser->back();
                    else
                        state = State::Hub;
                }
                break;
            }
            case State::Edit: {
                if (pad->hit(INPUT_GAMEPAD_B)) {
                    bar->end_edit();
                    keyboard->hide();
                    state = browser->has_page() ? State::Page : State::Hub;
                    break;
                }
                if (bar->suggestions_focused()) {
                    if (pad->nav_x())
                        bar->move_suggestion(pad->nav_x());
                    if (pad->nav_y() > 0) {
                        bar->suggestions_focus(false);
                        keyboard->reset_focus();
                    }
                    if (pad->hit(INPUT_GAMEPAD_A))
                        nav_to = bar->suggestion_url(0);
                } else if (keyboard->update(*pad, *bar)) {
                    bar->suggestions_focus(true);
                }
                if (bar->enter_pending()) {
                    bar->clear_enter();
                    nav_to = bar->text();
                }
                break;
            }
            }
            if (!nav_to.empty()) {
                const std::string target = normalize_url(nav_to);
                if (!target.empty()) {
                    browser->navigate(target);
                    store->save();
                    state = State::Page;
                    bar->end_edit();
                    keyboard->hide();
                }
            }
        }
        if (quit)
            break;

        // Draw every frame, even while the Guide is open (xsdk presents XAM UI under us).
        const uint32_t addr = gfx::frame_begin();
        gfx::fill_rect(0, 0, gfx::width(), gfx::height(), theme::kBg);
        switch (state) {
        case State::Page:
            browser->draw(680);
            bar->draw_display(browser->url(), browser->can_back(), browser->can_forward(),
                browser->has_page() && store->is_favorite(browser->url()));
            break;
        case State::Hub:
            if (browser->has_page() && !browser->loading())
                browser->draw(680);
            hub->draw(*store);
            bar->draw_display(browser->url(), browser->can_back(), browser->can_forward(),
                browser->has_page() && store->is_favorite(browser->url()));
            break;
        case State::Edit:
            if (browser->has_page() && !browser->loading())
                browser->draw(theme::kKeyY - 8);
            bar->draw_edit();
            keyboard->draw();
            break;
        }
        if (about)
            draw_about();
        if (flash_frames > 0 && !flash.empty()) {
            const float w = gfx::text_width(flash, 1) + 24;
            gfx::fill_rect((gfx::width() - w) / 2, 640, w, 32, theme::kTopBarBg);
            gfx::text((gfx::width() - w) / 2 + 12, 648, 1, theme::kText, flash.c_str());
            flash_frames--;
        }
        float hx = 24;
        switch (state) {
        case State::Hub:
            hint(hx, theme::kHintA, "A", "Select");
            if (browser->has_page())
                hint(hx, theme::kHintB, "B", "Hide Web Hub");
            hint(hx, theme::kHintX, "X", hub->edit_mode() ? "Done" : "Edit");
            hint(hx, theme::kHintY, "Y", "Address");
            break;
        case State::Page:
            hint(hx, theme::kHintA, "A", "Open Link");
            hint(hx, theme::kHintB, "B", browser->can_back() ? "Back" : "Web Hub");
            hint(hx, theme::kHintX, "X",
                store->is_favorite(browser->url()) ? "Unfavorite" : "Favorite");
            hint(hx, theme::kHintY, "Y", "Address");
            break;
        case State::Edit:
            hint(hx, theme::kHintA, "A", bar->suggestions_focused() ? "Go" : "Type");
            hint(hx, theme::kHintB, "B", "Cancel");
            break;
        }
        gfx::frame_end(addr);
    }
    store->save();
    return 0;
}

} // namespace

int main()
{
    xsdk_log_write("xenon_ie: Internet Explorer for Xbox 360 UI on xsdk; Back ends the title");
    
    const NTSTATUS splash = xsdk_splash_show(800);
    if (!NT_SUCCESS(splash))
        xsdk_log_write("xenon_ie: splash failed: 0x%08X", (unsigned)splash);
        
    if (!gfx::open()) {
        xsdk_log_write("xenon_ie: gfx::open() FAILED! Exiting.");
        return 1;
    }
    
    xsdk_log_write("xenon_ie: gfx::open() SUCCESS. Starting run loop.");
    
    const int status = run();
    
    xsdk_log_write("xenon_ie: run loop exited with status %d", status);
    gfx::close();
    
    return status;
}
