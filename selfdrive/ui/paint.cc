#include "selfdrive/ui/paint.h"

#include <algorithm>
#include <cassert>

#ifdef __APPLE__
#include <OpenGL/gl3.h>
#define NANOVG_GL3_IMPLEMENTATION
#define nvgCreate nvgCreateGL3
#else
#include <GLES3/gl3.h>
#define NANOVG_GLES3_IMPLEMENTATION
#define nvgCreate nvgCreateGLES3
#endif

#define NANOVG_GLES3_IMPLEMENTATION
#include <nanovg_gl.h>
#include <nanovg_gl_utils.h>

#include "selfdrive/common/timing.h"
#include "selfdrive/common/util.h"
#include "selfdrive/hardware/hw.h"

#include "selfdrive/ui/ui.h"
#include "selfdrive/ui/extras.h"

const int bdr_is = bdr_s;

static void ui_draw_text(const UIState *s, float x, float y, const char *string, float size, NVGcolor color, const char *font_name) {
  nvgFontFace(s->vg, font_name);
  nvgFontSize(s->vg, size);
  nvgFillColor(s->vg, color);
  nvgText(s->vg, x, y, string, NULL);
}

static void draw_chevron(UIState *s, float x, float y, float sz, NVGcolor fillColor, NVGcolor glowColor) {
  // glow
  float g_xo = sz/5;
  float g_yo = sz/10;
  nvgBeginPath(s->vg);
  nvgMoveTo(s->vg, x+(sz*1.35)+g_xo, y+sz+g_yo);
  nvgLineTo(s->vg, x, y-g_xo);
  nvgLineTo(s->vg, x-(sz*1.35)-g_xo, y+sz+g_yo);
  nvgClosePath(s->vg);
  nvgFillColor(s->vg, glowColor);
  nvgFill(s->vg);

  // chevron
  nvgBeginPath(s->vg);
  nvgMoveTo(s->vg, x+(sz*1.25), y+sz);
  nvgLineTo(s->vg, x, y);
  nvgLineTo(s->vg, x-(sz*1.25), y+sz);
  nvgClosePath(s->vg);
  nvgFillColor(s->vg, fillColor);
  nvgFill(s->vg);
}

static void ui_draw_circle_image(const UIState *s, int center_x, int center_y, int radius, const char *image, NVGcolor color, float img_alpha) {
  nvgBeginPath(s->vg);
  nvgCircle(s->vg, center_x, center_y, radius);
  nvgFillColor(s->vg, color);
  nvgFill(s->vg);
  const int img_size = radius * 1.5;
  ui_draw_image(s, {center_x - (img_size / 2), center_y - (img_size / 2), img_size, img_size}, image, img_alpha);
}

static void ui_draw_circle_image(const UIState *s, int center_x, int center_y, int radius, const char *image, bool active) {
  float bg_alpha = active ? 0.3f : 0.1f;
  float img_alpha = active ? 1.0f : 0.15f;
  ui_draw_circle_image(s, center_x, center_y, radius, image, nvgRGBA(0, 0, 0, (255 * bg_alpha)), img_alpha);
}

static void draw_lead(UIState *s, const cereal::RadarState::LeadData::Reader &lead_data, const vertex_data &vd) {
  // Draw lead car indicator
  auto [x, y] = vd;

  float fillAlpha = 0;
  float speedBuff = 10.;
  float leadBuff = 40.;
  float d_rel = lead_data.getDRel();
  float v_rel = lead_data.getVRel();
  if (d_rel < leadBuff) {
    fillAlpha = 255*(1.0-(d_rel/leadBuff));
    if (v_rel < 0) {
      fillAlpha += 255*(-1*(v_rel/speedBuff));
    }
    fillAlpha = (int)(fmin(fillAlpha, 255));
  }

  float sz = std::clamp((25 * 30) / (d_rel / 3 + 30), 15.0f, 30.0f) * 2.35;
  x = std::clamp(x, 0.f, s->viz_rect.right() - sz / 2);
  y = std::fmin(s->viz_rect.bottom() - sz * .6, y);
  draw_chevron(s, x, y, sz, nvgRGBA(201, 34, 49, fillAlpha), COLOR_YELLOW);
}

static void ui_draw_line(UIState *s, const line_vertices_data &vd, NVGcolor *color, NVGpaint *paint) {
  if (vd.cnt == 0) return;

  const vertex_data *v = &vd.v[0];
  nvgBeginPath(s->vg);
  nvgMoveTo(s->vg, v[0].x, v[0].y);
  for (int i = 1; i < vd.cnt; i++) {
    nvgLineTo(s->vg, v[i].x, v[i].y);
  }
  nvgClosePath(s->vg);
  if (color) {
    nvgFillColor(s->vg, *color);
  } else if (paint) {
    nvgFillPaint(s->vg, *paint);
  }
  nvgFill(s->vg);
}

static void draw_frame(UIState *s) {
  glBindVertexArray(s->frame_vao);
  mat4 *out_mat = &s->rear_frame_mat;
  glActiveTexture(GL_TEXTURE0);

  if (s->last_frame) {
    glBindTexture(GL_TEXTURE_2D, s->texture[s->last_frame->idx]->frame_tex);
    if (!Hardware::EON()) {
      // this is handled in ion on QCOM
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, s->last_frame->width, s->last_frame->height,
                   0, GL_RGB, GL_UNSIGNED_BYTE, s->last_frame->addr);
    }
  }

  glUseProgram(s->gl_shader->prog);
  glUniform1i(s->gl_shader->getUniformLocation("uTexture"), 0);
  glUniformMatrix4fv(s->gl_shader->getUniformLocation("uTransform"), 1, GL_TRUE, out_mat->v);

  assert(glGetError() == GL_NO_ERROR);
  glEnableVertexAttribArray(0);
  glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, (const void *)0);
  glDisableVertexAttribArray(0);
  glBindVertexArray(0);
}

static void ui_draw_vision_lane_lines(UIState *s) {
  const UIScene &scene = s->scene;
  NVGpaint track_bg;
  if (!scene.end_to_end) {
    // paint lanelines
    for (int i = 0; i < std::size(scene.lane_line_vertices); i++) {
      NVGcolor color = nvgRGBAf(1.0, 1.0, 1.0, scene.lane_line_probs[i]);
      ui_draw_line(s, scene.lane_line_vertices[i], &color, nullptr);
    }

    // paint road edges
    for (int i = 0; i < std::size(scene.road_edge_vertices); i++) {
      NVGcolor color = nvgRGBAf(1.0, 0.0, 0.0, std::clamp<float>(1.0 - scene.road_edge_stds[i], 0.0, 1.0));
      ui_draw_line(s, scene.road_edge_vertices[i], &color, nullptr);
    }
    track_bg = nvgLinearGradient(s->vg, s->fb_w, s->fb_h, s->fb_w, s->fb_h * .4,
                                          COLOR_WHITE, COLOR_WHITE_ALPHA(0));
  } else {
    track_bg = nvgLinearGradient(s->vg, s->fb_w, s->fb_h, s->fb_w, s->fb_h * .4,
                                          COLOR_RED, COLOR_RED_ALPHA(0));
  }
  // paint path
  ui_draw_line(s, scene.track_vertices, nullptr, &track_bg);
}

// Draw all world space objects.
static void ui_draw_world(UIState *s) {
  // Don't draw on top of sidebar
  nvgScissor(s->vg, s->viz_rect.x, s->viz_rect.y, s->viz_rect.w, s->viz_rect.h);

  // Draw lane edges and vision/mpc tracks
  ui_draw_vision_lane_lines(s);

  // Draw lead indicators if openpilot is handling longitudinal
  if (s->scene.longitudinal_control) {
    auto radar_state = (*s->sm)["radarState"].getRadarState();
    auto lead_one = radar_state.getLeadOne();
    auto lead_two = radar_state.getLeadTwo();
    if (lead_one.getStatus()) {
      draw_lead(s, lead_one, s->scene.lead_vertices[0]);
    }
    if (lead_two.getStatus() && (std::abs(lead_one.getDRel() - lead_two.getDRel()) > 3.0)) {
      draw_lead(s, lead_two, s->scene.lead_vertices[1]);
    }
  }
  nvgResetScissor(s->vg);
}

static void bb_ui_draw_basic_info(UIState *s)
{
  const UIScene *scene = &s->scene;
  char str[1024];

  auto controls_state = (*s->sm)["controlsState"].getControlsState();
  auto car_params = (*s->sm)["carParams"].getCarParams();
  auto live_params = (*s->sm)["liveParameters"].getLiveParameters();

  snprintf(str, sizeof(str), "SR(%.2f) SRC(%.2f) SAD(%.2f) AO(%.2f/%.2f)", controls_state.getSteerRatio(),
                                                      controls_state.getSteerRateCost(),
                                                      controls_state.getSteerActuatorDelay(),
                                                      live_params.getAngleOffsetDeg(),
                                                      live_params.getAngleOffsetAverageDeg());

  int x = s->viz_rect.x + (bdr_s * 2);
  int y = s->viz_rect.bottom() - 10;
  nvgBeginPath(s->vg);
  nvgRect(s->vg, x-40, y-27, 950, 50);
  NVGcolor squareColor = nvgRGBA(34, 139, 34, 200);
  nvgFillColor(s->vg, squareColor);
  nvgFill(s->vg);
  const NVGcolor textColor2 = COLOR_WHITE_ALPHA(254);
  nvgTextAlign(s->vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
  ui_draw_text(s, x, y, str, 20 * 2.3, textColor2, "sans-regular");
}

static int bb_ui_draw_measurev(UIState *s, const char* bb_value,
    int bb_x, int bb_y, NVGcolor bb_valueColor, int bb_valueFontSize) {

  //print value
  nvgFontFace(s->vg, "sans-regular");
  nvgFontSize(s->vg, bb_valueFontSize*2.3);
  nvgFillColor(s->vg, bb_valueColor);
  nvgText(s->vg, bb_x, bb_y, bb_value, NULL);
  return (int)(0);
}

static int bb_ui_draw_measurel(UIState *s, const char* bb_label,
    int bb_x, int bb_y, NVGcolor bb_labelColor, int bb_labelFontSize) {

  //print label
  nvgFontFace(s->vg, "sans-regular");
  nvgFontSize(s->vg, bb_labelFontSize*2.3);
  nvgFillColor(s->vg, bb_labelColor);
  nvgText(s->vg, bb_x, bb_y, bb_label, NULL);
  return (int)(0);
}

static void bb_ui_draw_debug(UIState *s)
{
    const UIScene *scene = &s->scene;
    char str[1024];
    char val_str[16];
    char lab_str[16];
    NVGcolor val_color = nvgRGBA(255, 255, 255, 200);
    NVGcolor lab_color = nvgRGBA(255, 255, 255, 200);
    int value_fontSize=20;
    int label_fontSize=20;

    int y = 20;
    const int height = 55;

    nvgTextAlign(s->vg, NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE);

    const int text_x = 260; //*s->viz_rect.centerX() + s->viz_rect.w * 10 / 55;*

    auto controls_state = (*s->sm)["controlsState"].getControlsState();
    auto car_control = (*s->sm)["carControl"].getCarControl();
    auto device_state = (*s->sm)["deviceState"].getDeviceState();

    int longControlState = (int)controls_state.getLongControlState();
    float vPid = controls_state.getVPid();
    float upAccelCmd = controls_state.getUpAccelCmd();
    float uiAccelCmd = controls_state.getUiAccelCmd();
    float ufAccelCmd = controls_state.getUfAccelCmd();
    float gas = car_control.getActuators().getGas();
    float brake = car_control.getActuators().getBrake();

    const char* long_state[] = {"off", "pid", "stopping", "starting"};

    const NVGcolor textColor = COLOR_WHITE;
    const NVGcolor textColor2 = COLOR_GREEN_ALPHA(250);

    y += height;
    snprintf(str, sizeof(str), "State: %s", long_state[longControlState]);
    ui_draw_text(s, text_x, y, str, 20 * 2.3, textColor2, "sans-regular");

    y += height;
    snprintf(str, sizeof(str), "P: %.3f", upAccelCmd);
    ui_draw_text(s, text_x, y, str, 20 * 2.3, textColor, "sans-regular");

    y += height;
    snprintf(str, sizeof(str), "I: %.3f", uiAccelCmd);
    ui_draw_text(s, text_x, y, str, 20 * 2.3, textColor, "sans-regular");

    y += height;
    snprintf(str, sizeof(str), "F: %.3f", ufAccelCmd);
    ui_draw_text(s, text_x, y, str, 20 * 2.3, textColor, "sans-regular");

    y += height;
    snprintf(str, sizeof(str), "vPid: %.3f(%.1f)", vPid, vPid * 3.6f);
    ui_draw_text(s, text_x-210, y, str, 20 * 2.3, textColor2, "sans-regular");

    y += height;
    snprintf(str, sizeof(str), "Gas: %.3f", gas);
    ui_draw_text(s, text_x-210, y, str, 20 * 2.3, textColor, "sans-regular");

    y += height;
    snprintf(str, sizeof(str), "Brake: %.3f", brake);
    ui_draw_text(s, text_x-210, y, str, 20 * 2.3, textColor, "sans-regular");
    y += height;

    //Cpu 온도
    float cpuTemp = 0;
    auto cpuList = device_state.getCpuTempC();

    if(cpuList.size() > 0)
    {
        for(int i = 0; i < cpuList.size(); i++)
            cpuTemp += cpuList[i];
        cpuTemp /= cpuList.size();
    }

    // temp is alway in C * 10
    snprintf(val_str, sizeof(val_str), "%.1f°", cpuTemp);
    bb_ui_draw_measurev(s, val_str, text_x-40, y, val_color, value_fontSize);
    bb_ui_draw_measurel(s, "CPU온도:", text_x-210, y, lab_color, label_fontSize);
    y += height;

    //차간거리
    auto radar_state = (*s->sm)["radarState"].getRadarState();
    auto lead_one = radar_state.getLeadOne();

    if (lead_one.getStatus()) {

      // lead car relative distance is always in meters
      snprintf(val_str, sizeof(val_str), "%.1fm", lead_one.getDRel());
    }
    else {
      snprintf(val_str, sizeof(val_str), "--");
    }
    bb_ui_draw_measurev(s, val_str, text_x-40, y, val_color, value_fontSize);
    bb_ui_draw_measurel(s, "차간거리:", text_x-210, y, lab_color, label_fontSize);
    y += height;

    // GPS 정확도
    auto gps_ext = s->scene.gps_ext;
    float verticalAccuracy = gps_ext.getVerticalAccuracy();
    float gpsAltitude = gps_ext.getAltitude();
    float gpsAccuracy = gps_ext.getAccuracy();

    if(verticalAccuracy == 0 || verticalAccuracy > 100)
        gpsAltitude = 99.99;

    if (gpsAccuracy > 100)
      gpsAccuracy = 99.99;
    else if (gpsAccuracy == 0)
      gpsAccuracy = 99.8;

    snprintf(val_str, sizeof(val_str), "%.2fm", gpsAccuracy);
    bb_ui_draw_measurev(s, val_str, text_x-40, y, val_color, value_fontSize);
    bb_ui_draw_measurel(s, "GPS거리:", text_x-210, y, lab_color, label_fontSize);
    y += height;

    //엔진 RPM
    if(s->scene.engineRPM == 0) {
      val_color = nvgRGBA(255, 255, 255, 200);
      snprintf(val_str, sizeof(val_str), "OFF");
    }
    else {
      snprintf(val_str, sizeof(val_str), "%d", (s->scene.engineRPM));
    }
    bb_ui_draw_measurev(s, val_str, text_x-30, y, val_color, value_fontSize);
    bb_ui_draw_measurel(s, "엔진RPM:", text_x-210, y, lab_color, label_fontSize);
    y += height;

    //현재 조향각
    float angleSteers = controls_state.getAngleSteers();
    val_color = nvgRGBA(255, 255, 255, 200);

    // steering is in degrees
    snprintf(val_str, sizeof(val_str), "%.1f °", angleSteers);
    bb_ui_draw_measurev(s, val_str, text_x-80, y, val_color, value_fontSize);
    bb_ui_draw_measurel(s, "핸들각:", text_x-210, y, lab_color, label_fontSize);
    y += height;

    //필요 조향각
    auto carControl = (*s->sm)["carControl"].getCarControl();
    if (carControl.getEnabled()) {
      auto actuators = carControl.getActuators();
      float steeringAngleDeg  = actuators.getSteeringAngleDeg();
      val_color = nvgRGBA(255, 255, 255, 200);

      // steering is in degrees
      snprintf(val_str, sizeof(val_str), "%.1f °", steeringAngleDeg);
    }
    else {
      snprintf(val_str, sizeof(val_str), "--");
    }
    bb_ui_draw_measurev(s, val_str, text_x-80, y, val_color, value_fontSize);
    bb_ui_draw_measurel(s, "경로각:", text_x-210, y, lab_color, label_fontSize);
}

static void ui_draw_vision_brake(UIState *s) {
  const UIScene *scene = &s->scene;

  const int radius = 96;
  const int center_x = s->viz_rect.x + radius + (bdr_s * 2) + radius*2 + 60;
  const int center_y = s->viz_rect.bottom() - footer_h / 2;

  auto car_state = (*s->sm)["carState"].getCarState();
  bool brake_valid = car_state.getBrakeLights();
  float brake_img_alpha = brake_valid ? 1.0f : 0.15f;
  float brake_bg_alpha = brake_valid ? 0.3f : 0.1f;
  NVGcolor brake_bg = nvgRGBA(0, 0, 0, (255 * brake_bg_alpha));

  ui_draw_circle_image(s, center_x, center_y, radius, "brake", brake_bg, brake_img_alpha);
}

static void ui_draw_vision_autohold(UIState *s) {
  const UIScene *scene = &s->scene;

  const int radius = 96;
  const int center_x = s->viz_rect.x + radius + (bdr_s * 2) + (radius*2 + 60) * 2;
  const int center_y = s->viz_rect.bottom() - footer_h / 2;

  auto car_state = (*s->sm)["carState"].getCarState();
  bool autohold_valid = car_state.getAutoHoldActivated();

  float brake_img_alpha = autohold_valid ? 1.0f : 0.15f;
  float brake_bg_alpha = autohold_valid ? 0.3f : 0.1f;
  NVGcolor brake_bg = nvgRGBA(0, 0, 0, (255 * brake_bg_alpha));

  ui_draw_circle_image(s, center_x, center_y, radius,
        "autohold_active",
        brake_bg, brake_img_alpha);
}

static void ui_draw_vision_maxspeed(UIState *s) {
  const int SET_SPEED_NA = 255;
  float maxspeed = (*s->sm)["controlsState"].getControlsState().getVCruise();
  const bool is_cruise_set = maxspeed != 0 && maxspeed != SET_SPEED_NA;
  if (is_cruise_set && !s->scene.is_metric) { maxspeed *= 0.6225; }

  const Rect rect = {s->viz_rect.x + (bdr_s * 2), int(s->viz_rect.y + (bdr_s * 1.5)), 184, 202};
  ui_fill_rect(s->vg, rect, COLOR_BLACK_ALPHA(100), 30.);
  ui_draw_rect(s->vg, rect, COLOR_WHITE_ALPHA(100), 10, 20.);

  nvgTextAlign(s->vg, NVG_ALIGN_CENTER | NVG_ALIGN_BASELINE);
  ui_draw_text(s, rect.centerX(), 118, "MAX", 19 * 2.5, COLOR_WHITE_ALPHA(is_cruise_set ? 200 : 100), "sans-regular");
  if (is_cruise_set) {
    const std::string maxspeed_str = std::to_string((int)std::nearbyint(maxspeed));
    ui_draw_text(s, rect.centerX(), 212, maxspeed_str.c_str(), 48 * 2.5, COLOR_WHITE, "sans-bold");
  } else {
    ui_draw_text(s, rect.centerX(), 212, "N/A", 32 * 2.5, COLOR_WHITE_ALPHA(100), "sans-semibold");
  }
}

static void ui_draw_vision_speed(UIState *s) {
  const float speed = std::max(0.0, (*s->sm)["carState"].getCarState().getVEgo() * (s->scene.is_metric ? 3.6 : 2.2369363));
  const std::string speed_str = std::to_string((int)std::nearbyint(speed));
  nvgTextAlign(s->vg, NVG_ALIGN_CENTER | NVG_ALIGN_BASELINE);
  NVGcolor color = s->scene.car_state.getBrakeLights() ? nvgRGBA(255, 66, 66, 255) : COLOR_WHITE;
  ui_draw_text(s, s->viz_rect.centerX(), 190, speed_str.c_str(), 80 * 2.5, color, "sans-bold");
  ui_draw_text(s, s->viz_rect.centerX(), 270, s->scene.is_metric ? "km/h" : "mph", 24 * 2.5, COLOR_WHITE_ALPHA(200), "sans-regular");
}

static void ui_draw_vision_event(UIState *s) {
  const UIScene *scene = &s->scene;
  const int viz_event_w = 220;
  const int viz_event_x = s->viz_rect.right() - (viz_event_w + bdr_s*2);
  const int viz_event_y = s->viz_rect.y + (bdr_s*1.5)+25;
  if (s->scene.controls_state.getDecelForModel() && s->scene.controls_state.getEnabled()) {
    // draw winding road sign
    const int img_turn_size = 160*1.5*0.82;
    const int img_turn_x = viz_event_x-(img_turn_size/4)+80;
    const int img_turn_y = viz_event_y+bdr_is-45;
    float img_turn_alpha = 1.0f;
    nvgBeginPath(s->vg);
    NVGpaint imgPaint = nvgImagePattern(s->vg, img_turn_x, img_turn_y,
      img_turn_size, img_turn_size, 0, s->images["trafficSign_turn"], img_turn_alpha);
    nvgRect(s->vg, img_turn_x, img_turn_y, img_turn_size, img_turn_size);
    nvgFillPaint(s->vg, imgPaint);
    nvgFill(s->vg);
  } else {
    // draw steering wheel
    const int bg_wheel_size = 96;
    const int bg_wheel_x = viz_event_x + (viz_event_w-bg_wheel_size);
    const int bg_wheel_y = viz_event_y + (bg_wheel_size/2);
    const int img_wheel_size = bg_wheel_size*1.5;
    const int img_wheel_x = bg_wheel_x-(img_wheel_size/2);
    const int img_wheel_y = bg_wheel_y-45;
    const float img_rotation = s->scene.angleSteers/180*3.141592;
    float img_wheel_alpha = 0.1f;
    bool is_engaged = (s->status == STATUS_ENGAGED) && !s->scene.controls_state.getSteerOverride();
    bool is_warning = (s->status == STATUS_WARNING);
    bool is_engageable = s->scene.controls_state.getEngageable();
    if (is_engaged || is_warning || is_engageable) {
      nvgBeginPath(s->vg);
      nvgCircle(s->vg, bg_wheel_x, (bg_wheel_y + (bdr_is*1.5)), bg_wheel_size);
      if (is_engaged) {
        nvgFillColor(s->vg, nvgRGBA(23, 134, 68, 255));
      } else if (is_warning) {
        nvgFillColor(s->vg, nvgRGBA(218, 111, 37, 255));
      } else if (is_engageable) {
        nvgFillColor(s->vg, nvgRGBA(23, 51, 73, 255));
      }
      nvgFill(s->vg);
      img_wheel_alpha = 1.0f;
    }
    nvgSave(s->vg);
    nvgTranslate(s->vg,bg_wheel_x,(bg_wheel_y + (bdr_s*1.5)));
    nvgRotate(s->vg,-img_rotation);
    nvgBeginPath(s->vg);
    NVGpaint imgPaint = nvgImagePattern(s->vg, img_wheel_x-bg_wheel_x, img_wheel_y-(bg_wheel_y + (bdr_s*1.5)),
        img_wheel_size, img_wheel_size, 0, s->images["wheel"], img_wheel_alpha);
    nvgRect(s->vg, img_wheel_x-bg_wheel_x, img_wheel_y-(bg_wheel_y + (bdr_s*1.5)), img_wheel_size, img_wheel_size);
    nvgFillPaint(s->vg, imgPaint);
    nvgFill(s->vg);
    nvgRestore(s->vg);
  }
}

static void ui_draw_vision_face(UIState *s) {
  const int radius = 96;
  const int center_x = s->viz_rect.x + radius + (bdr_s * 2);
  const int center_y = s->viz_rect.bottom() - footer_h / 2;
  ui_draw_circle_image(s, center_x, center_y, radius, "driver_face", s->scene.dm_active);
}

static void ui_draw_vision_header(UIState *s) {
  NVGpaint gradient = nvgLinearGradient(s->vg, s->viz_rect.x,
                        s->viz_rect.y+(header_h-(header_h/2.5)),
                        s->viz_rect.x, s->viz_rect.y+header_h,
                        nvgRGBAf(0,0,0,0.45), nvgRGBAf(0,0,0,0));

  ui_fill_rect(s->vg, {s->viz_rect.x, s->viz_rect.y, s->viz_rect.w, header_h}, gradient);

  ui_draw_vision_maxspeed(s);
  ui_draw_vision_speed(s);
  ui_draw_vision_event(s);
  bb_ui_draw_basic_info(s);
  bb_ui_draw_debug(s);
  ui_draw_extras(s);
}

static void ui_draw_vision_frame(UIState *s) {
  // Draw video frames
  glEnable(GL_SCISSOR_TEST);
  glViewport(s->video_rect.x, s->video_rect.y, s->video_rect.w, s->video_rect.h);
  glScissor(s->viz_rect.x, s->viz_rect.y, s->viz_rect.w, s->viz_rect.h);
  draw_frame(s);
  glDisable(GL_SCISSOR_TEST);

  glViewport(0, 0, s->fb_w, s->fb_h);
}

static void ui_draw_vision(UIState *s) {
  const UIScene *scene = &s->scene;
  // Draw augmented elements
  if (scene->world_objects_visible) {
    ui_draw_world(s);
  }
  // Set Speed, Current Speed, Status/Events
  ui_draw_vision_header(s);
  if ((*s->sm)["controlsState"].getControlsState().getAlertSize() == cereal::ControlsState::AlertSize::NONE) {
    ui_draw_vision_face(s);
  }
  ui_draw_vision_brake(s);
  ui_draw_vision_autohold(s);
}

static void ui_draw_background(UIState *s) {
  const QColor &color = bg_colors[s->status];
  glClearColor(color.redF(), color.greenF(), color.blueF(), 1.0);
  glClear(GL_STENCIL_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
}

void ui_draw(UIState *s, int w, int h) {
  s->viz_rect = Rect{bdr_s, bdr_s, w - 2 * bdr_s, h - 2 * bdr_s};

  const bool draw_vision = s->scene.started && s->vipc_client->connected;

  // GL drawing functions
  ui_draw_background(s);
  if (draw_vision) {
    ui_draw_vision_frame(s);
  }
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glViewport(0, 0, s->fb_w, s->fb_h);

  // NVG drawing functions - should be no GL inside NVG frame
  nvgBeginFrame(s->vg, s->fb_w, s->fb_h, 1.0f);

  if (draw_vision) {
    ui_draw_vision(s);
  }

  nvgEndFrame(s->vg);
  glDisable(GL_BLEND);
}

void ui_draw_image(const UIState *s, const Rect &r, const char *name, float alpha) {
  nvgBeginPath(s->vg);
  NVGpaint imgPaint = nvgImagePattern(s->vg, r.x, r.y, r.w, r.h, 0, s->images.at(name), alpha);
  nvgRect(s->vg, r.x, r.y, r.w, r.h);
  nvgFillPaint(s->vg, imgPaint);
  nvgFill(s->vg);
}

void ui_draw_rect(NVGcontext *vg, const Rect &r, NVGcolor color, int width, float radius) {
  nvgBeginPath(vg);
  radius > 0 ? nvgRoundedRect(vg, r.x, r.y, r.w, r.h, radius) : nvgRect(vg, r.x, r.y, r.w, r.h);
  nvgStrokeColor(vg, color);
  nvgStrokeWidth(vg, width);
  nvgStroke(vg);
}

static inline void fill_rect(NVGcontext *vg, const Rect &r, const NVGcolor *color, const NVGpaint *paint, float radius) {
  nvgBeginPath(vg);
  radius > 0 ? nvgRoundedRect(vg, r.x, r.y, r.w, r.h, radius) : nvgRect(vg, r.x, r.y, r.w, r.h);
  if (color) nvgFillColor(vg, *color);
  if (paint) nvgFillPaint(vg, *paint);
  nvgFill(vg);
}
void ui_fill_rect(NVGcontext *vg, const Rect &r, const NVGcolor &color, float radius) {
  fill_rect(vg, r, &color, nullptr, radius);
}
void ui_fill_rect(NVGcontext *vg, const Rect &r, const NVGpaint &paint, float radius) {
  fill_rect(vg, r, nullptr, &paint, radius);
}

static const char frame_vertex_shader[] =
#ifdef NANOVG_GL3_IMPLEMENTATION
  "#version 150 core\n"
#else
  "#version 300 es\n"
#endif
  "in vec4 aPosition;\n"
  "in vec4 aTexCoord;\n"
  "uniform mat4 uTransform;\n"
  "out vec4 vTexCoord;\n"
  "void main() {\n"
  "  gl_Position = uTransform * aPosition;\n"
  "  vTexCoord = aTexCoord;\n"
  "}\n";

static const char frame_fragment_shader[] =
#ifdef NANOVG_GL3_IMPLEMENTATION
  "#version 150 core\n"
#else
  "#version 300 es\n"
#endif
  "precision mediump float;\n"
  "uniform sampler2D uTexture;\n"
  "in vec4 vTexCoord;\n"
  "out vec4 colorOut;\n"
  "void main() {\n"
  "  colorOut = texture(uTexture, vTexCoord.xy);\n"
#ifdef QCOM
  "  vec3 dz = vec3(0.0627f, 0.0627f, 0.0627f);\n"
  "  colorOut.rgb = ((vec3(1.0f, 1.0f, 1.0f) - dz) * colorOut.rgb / vec3(1.0f, 1.0f, 1.0f)) + dz;\n"
#endif
  "}\n";

static const mat4 device_transform = {{
  1.0,  0.0, 0.0, 0.0,
  0.0,  1.0, 0.0, 0.0,
  0.0,  0.0, 1.0, 0.0,
  0.0,  0.0, 0.0, 1.0,
}};

void ui_nvg_init(UIState *s) {
  // init drawing

  // on EON, we enable MSAA
  s->vg = Hardware::EON() ? nvgCreate(0) : nvgCreate(NVG_ANTIALIAS | NVG_STENCIL_STROKES | NVG_DEBUG);
  assert(s->vg);

  // init fonts
  std::pair<const char *, const char *> fonts[] = {
      {"sans-regular", "../assets/fonts/opensans_regular.ttf"},
      {"sans-semibold", "../assets/fonts/opensans_semibold.ttf"},
      {"sans-bold", "../assets/fonts/opensans_bold.ttf"},
  };
  for (auto [name, file] : fonts) {
    int font_id = nvgCreateFont(s->vg, name, file);
    assert(font_id >= 0);
  }

  // init images
  std::vector<std::pair<const char *, const char *>> images = {
    {"wheel", "../assets/img_chffr_wheel.png"},
    {"driver_face", "../assets/img_driver_face.png"},
    {"brake", "../assets/img_brake_disc.png"},
    {"autohold_active", "../assets/img_autohold_active.png"},
  };
  for (auto [name, file] : images) {
    s->images[name] = nvgCreateImage(s->vg, file, 1);
    assert(s->images[name] != 0);
  }

  // init gl
  s->gl_shader = std::make_unique<GLShader>(frame_vertex_shader, frame_fragment_shader);
  GLint frame_pos_loc = glGetAttribLocation(s->gl_shader->prog, "aPosition");
  GLint frame_texcoord_loc = glGetAttribLocation(s->gl_shader->prog, "aTexCoord");

  glViewport(0, 0, s->fb_w, s->fb_h);

  glDisable(GL_DEPTH_TEST);

  assert(glGetError() == GL_NO_ERROR);

  float x1 = 1.0, x2 = 0.0, y1 = 1.0, y2 = 0.0;
  const uint8_t frame_indicies[] = {0, 1, 2, 0, 2, 3};
  const float frame_coords[4][4] = {
    {-1.0, -1.0, x2, y1}, //bl
    {-1.0,  1.0, x2, y2}, //tl
    { 1.0,  1.0, x1, y2}, //tr
    { 1.0, -1.0, x1, y1}, //br
  };

  glGenVertexArrays(1, &s->frame_vao);
  glBindVertexArray(s->frame_vao);
  glGenBuffers(1, &s->frame_vbo);
  glBindBuffer(GL_ARRAY_BUFFER, s->frame_vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(frame_coords), frame_coords, GL_STATIC_DRAW);
  glEnableVertexAttribArray(frame_pos_loc);
  glVertexAttribPointer(frame_pos_loc, 2, GL_FLOAT, GL_FALSE,
                        sizeof(frame_coords[0]), (const void *)0);
  glEnableVertexAttribArray(frame_texcoord_loc);
  glVertexAttribPointer(frame_texcoord_loc, 2, GL_FLOAT, GL_FALSE,
                        sizeof(frame_coords[0]), (const void *)(sizeof(float) * 2));
  glGenBuffers(1, &s->frame_ibo);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, s->frame_ibo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(frame_indicies), frame_indicies, GL_STATIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindVertexArray(0);

  ui_resize(s, s->fb_w, s->fb_h);
}

void ui_resize(UIState *s, int width, int height) {
  s->fb_w = width;
  s->fb_h = height;

  auto intrinsic_matrix = s->wide_camera ? ecam_intrinsic_matrix : fcam_intrinsic_matrix;

  float zoom = ZOOM / intrinsic_matrix.v[0];

  if (s->wide_camera) {
    zoom *= 0.5;
  }

  s->video_rect = Rect{bdr_s, bdr_s, s->fb_w - 2 * bdr_s, s->fb_h - 2 * bdr_s};
  float zx = zoom * 2 * intrinsic_matrix.v[2] / s->video_rect.w;
  float zy = zoom * 2 * intrinsic_matrix.v[5] / s->video_rect.h;

  const mat4 frame_transform = {{
    zx, 0.0, 0.0, 0.0,
    0.0, zy, 0.0, -y_offset / s->video_rect.h * 2,
    0.0, 0.0, 1.0, 0.0,
    0.0, 0.0, 0.0, 1.0,
  }};

  s->rear_frame_mat = matmul(device_transform, frame_transform);

  // Apply transformation such that video pixel coordinates match video
  // 1) Put (0, 0) in the middle of the video
  nvgTranslate(s->vg, s->video_rect.x + s->video_rect.w / 2, s->video_rect.y + s->video_rect.h / 2 + y_offset);

  // 2) Apply same scaling as video
  nvgScale(s->vg, zoom, zoom);

  // 3) Put (0, 0) in top left corner of video
  nvgTranslate(s->vg, -intrinsic_matrix.v[2], -intrinsic_matrix.v[5]);

  nvgCurrentTransform(s->vg, s->car_space_transform);
  nvgResetTransform(s->vg);
}
