#include <time.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>

static void ui_draw_extras_limit_speed(UIState *s)
{
    const UIScene *scene = &s->scene;

    int limit_speed = s->scene.controls_state.getRoadLimitSpeed();
    int left_dist = s->scene.controls_state.getRoadLimitSpeedLeftDist();

    if(limit_speed > 10 && left_dist > 0)
    {
        int w = 180;
        int h = 180;
        int x = (s->viz_rect.x + (bdr_s*2)) + 400;
        int y = 50;
        char str[32];

        nvgBeginPath(s->vg);
        nvgRoundedRect(s->vg, x, y, w, h, 185);
        nvgStrokeColor(s->vg, nvgRGBA(255, 0, 0, 200));
        nvgStrokeWidth(s->vg, 15);
        nvgStroke(s->vg);

        NVGcolor fillColor = nvgRGBA(0, 0, 0, 50);
        nvgFillColor(s->vg, fillColor);
        nvgFill(s->vg);

        nvgFillColor(s->vg, nvgRGBA(255, 255, 255, 250));

        nvgFontSize(s->vg, 110);
        nvgFontFace(s->vg, "sans-bold");
        nvgTextAlign(s->vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);

        snprintf(str, sizeof(str), "%d", limit_speed);
        nvgText(s->vg, x+w/2, y+h/2, str, NULL);

        nvgBeginPath(s->vg);
        nvgRect(s->vg, x+w/2-100, y+h-40, 190, 80);
        NVGcolor squareColor = nvgRGBA(255, 0, 0, 200);
        nvgFillColor(s->vg, squareColor);
        nvgFill(s->vg);
        nvgFillColor(s->vg, nvgRGBA(255, 255, 255, 250));

        nvgFontSize(s->vg, 74);
        nvgFontFace(s->vg, "sans-bold");
        nvgTextAlign(s->vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);

        if(left_dist >= 1000)
            snprintf(str, sizeof(str), "%.1fkm", left_dist / 1000.f);
        else
            snprintf(str, sizeof(str), "%dm", left_dist);

        nvgText(s->vg, x+w/2, y+h, str, NULL);
    }
}

static void ui_draw_extras(UIState *s)
{
    ui_draw_extras_limit_speed(s);
}
