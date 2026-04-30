#include "ESP.h"
#include "../../core/JNIHelper.h"
#include "../ModuleManager.h"
#include "../combat/Killaura.h"
#include "../combat/Aimbot.h"
#include "../combat/Reach.h"
#include "../combat/AutoClicker.h"
#include "../TeleportAura.h"
#include "../FakeLag.h"
#include "../render/XRay.h"
#include <iostream>
#include <fstream>
#include <cmath>
#include <string>

#pragma comment(lib, "gdiplus.lib")
using namespace Gdiplus;

// ── slider drag state ────────────────────────────────────────────────────────
static bool draggingKaReachSlider    = false;
static bool draggingKaFovSlider      = false;
static bool draggingTaReachSlider    = false;
static bool draggingFlChokeSlider    = false;
static bool draggingAimSmoothSlider  = false;
static bool draggingAcMinSlider      = false;
static bool draggingAcMaxSlider      = false;
static bool draggingEspRangeSlider   = false;
static bool draggingReachSlider      = false;

// ── cached GDI+ objects (created once, reused every frame) ──────────────────
// Pens
static Pen*        s_blackPen4      = nullptr;   // 4px black outline
static Pen*        s_espPen2        = nullptr;   // 2px green ESP
static Pen*        s_tracerBlack4   = nullptr;
static Pen*        s_tracerWhite2   = nullptr;
// Brushes
static SolidBrush* s_bgBarBrush     = nullptr;   // health bar background
static SolidBrush* s_shadowBrush    = nullptr;
static SolidBrush* s_textBrush      = nullptr;
static SolidBrush* s_guiBg          = nullptr;
static SolidBrush* s_guiHeader      = nullptr;
static SolidBrush* s_cbBg           = nullptr;
static SolidBrush* s_cbFill         = nullptr;
static SolidBrush* s_sliderBg       = nullptr;
static SolidBrush* s_sliderFill     = nullptr;
// Fonts
static FontFamily* s_consolasFam    = nullptr;
static Font*       s_espFont        = nullptr;   // Consolas 12 Bold
static FontFamily* s_verdanaFam     = nullptr;
static Font*       s_titleFont      = nullptr;   // Verdana 16 Bold
static Font*       s_modFont        = nullptr;   // Verdana 14 Regular
// String format
static StringFormat* s_centerFmt   = nullptr;

static bool s_gdipObjectsInit = false;

static void InitGdipObjects() {
    if (s_gdipObjectsInit) return;

    s_blackPen4    = new Pen(Color(255, 0, 0, 0),    4.0f);
    s_espPen2      = new Pen(Color(255, 46, 204, 113), 2.0f);
    s_tracerBlack4 = new Pen(Color(255, 0, 0, 0),    4.0f);
    s_tracerWhite2 = new Pen(Color(150, 255, 255, 255), 2.0f);

    s_bgBarBrush   = new SolidBrush(Color(255, 0, 0, 0));
    s_shadowBrush  = new SolidBrush(Color(255, 0, 0, 0));
    s_textBrush    = new SolidBrush(Color(255, 255, 255, 255));
    s_guiBg        = new SolidBrush(Color(240, 25, 25, 25));
    s_guiHeader    = new SolidBrush(Color(255, 15, 15, 15));
    s_cbBg         = new SolidBrush(Color(255, 45, 45, 45));
    s_cbFill       = new SolidBrush(Color(255, 46, 204, 113));
    s_sliderBg     = new SolidBrush(Color(255, 45, 45, 45));
    s_sliderFill   = new SolidBrush(Color(255, 46, 204, 113));

    s_consolasFam  = new FontFamily(L"Consolas");
    s_espFont      = new Font(s_consolasFam, 12, FontStyleBold,    UnitPixel);
    s_verdanaFam   = new FontFamily(L"Verdana");
    s_titleFont    = new Font(s_verdanaFam,  16, FontStyleBold,    UnitPixel);
    s_modFont      = new Font(s_verdanaFam,  14, FontStyleRegular, UnitPixel);

    s_centerFmt    = new StringFormat();
    s_centerFmt->SetAlignment(StringAlignmentCenter);

    s_gdipObjectsInit = true;
}

static void FreeGdipObjects() {
    delete s_blackPen4;    s_blackPen4    = nullptr;
    delete s_espPen2;      s_espPen2      = nullptr;
    delete s_tracerBlack4; s_tracerBlack4 = nullptr;
    delete s_tracerWhite2; s_tracerWhite2 = nullptr;
    delete s_bgBarBrush;   s_bgBarBrush   = nullptr;
    delete s_shadowBrush;  s_shadowBrush  = nullptr;
    delete s_textBrush;    s_textBrush    = nullptr;
    delete s_guiBg;        s_guiBg        = nullptr;
    delete s_guiHeader;    s_guiHeader    = nullptr;
    delete s_cbBg;         s_cbBg         = nullptr;
    delete s_cbFill;       s_cbFill       = nullptr;
    delete s_sliderBg;     s_sliderBg     = nullptr;
    delete s_sliderFill;   s_sliderFill   = nullptr;
    delete s_espFont;      s_espFont      = nullptr;
    delete s_consolasFam;  s_consolasFam  = nullptr;
    delete s_titleFont;    s_titleFont    = nullptr;
    delete s_modFont;      s_modFont      = nullptr;
    delete s_verdanaFam;   s_verdanaFam   = nullptr;
    delete s_centerFmt;    s_centerFmt    = nullptr;
    s_gdipObjectsInit = false;
}

// ── persistent back-buffer (avoid CreateCompatibleBitmap every frame) ────────
static HDC     s_memDC      = nullptr;
static HBITMAP s_memBitmap  = nullptr;
static int     s_bufW       = 0;
static int     s_bufH       = 0;

static void EnsureBackBuffer(HDC hdc, int w, int h) {
    if (w == s_bufW && h == s_bufH && s_memDC) return;
    if (s_memBitmap) { DeleteObject(s_memBitmap); s_memBitmap = nullptr; }
    if (s_memDC)     { DeleteDC(s_memDC);         s_memDC     = nullptr; }
    s_memDC     = CreateCompatibleDC(hdc);
    s_memBitmap = CreateCompatibleBitmap(hdc, w, h);
    SelectObject(s_memDC, s_memBitmap);
    s_bufW = w; s_bufH = h;
}

// ── ctor / dtor ──────────────────────────────────────────────────────────────
ESP::ESP() : Module("ESP") {}

ESP::~ESP() {
    if (running) {
        running = false;
        if (renderThread.joinable()) {
            if (std::this_thread::get_id() == renderThread.get_id())
                renderThread.detach();
            else
                renderThread.join();
        }
        if (updateThread.joinable()) {
            if (std::this_thread::get_id() == updateThread.get_id())
                updateThread.detach();
            else
                updateThread.join();
        }
    }
}

void ESP::OnEnable() {
    if (running) return;
    running = true;
    renderThread = std::thread(&ESP::RenderLoop,      this);
    updateThread = std::thread(&ESP::UpdateDataLoop,  this);
}

void ESP::OnDisable() {
    running = false;
    if (renderThread.joinable()) {
        if (std::this_thread::get_id() == renderThread.get_id())
            renderThread.detach();
        else
            renderThread.join();
    }
    if (updateThread.joinable()) {
        if (std::this_thread::get_id() == updateThread.get_id())
            updateThread.detach();
        else
            updateThread.join();
    }
}

LRESULT CALLBACK ESP::OverlayProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_ERASEBKGND) return 1;
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// ── WorldToScreen ─────────────────────────────────────────────────────────────
bool ESP::WorldToScreen(Vec3 pos, Vec3 camPos, float* mv, float* p, Vec2& screen, int width, int height) {
    float x = (float)(pos.x - camPos.x);
    float y = (float)(pos.y - camPos.y);
    float z = (float)(pos.z - camPos.z);

    float vX = mv[0]*x + mv[4]*y + mv[8]*z  + mv[12];
    float vY = mv[1]*x + mv[5]*y + mv[9]*z  + mv[13];
    float vZ = mv[2]*x + mv[6]*y + mv[10]*z + mv[14];
    float vW = mv[3]*x + mv[7]*y + mv[11]*z + mv[15];

    float cX = p[0]*vX + p[4]*vY + p[8]*vZ  + p[12]*vW;
    float cY = p[1]*vX + p[5]*vY + p[9]*vZ  + p[13]*vW;
    float cW = p[3]*vX + p[7]*vY + p[11]*vZ + p[15]*vW;

    if (cW < 0.1f) return false;

    float ndcX = cX / cW;
    float ndcY = cY / cW;
    screen.x = (width  / 2.0f) * (ndcX + 1.0f);
    screen.y = (height / 2.0f) * (1.0f - ndcY);
    return true;
}

// ── Draw3DBox ─────────────────────────────────────────────────────────────────
// Per-call colour pen is the only allocation here; everything else is cached.
void ESP::Draw3DBox(Graphics& g, Vec3 feet, float w, float h, Vec3 camPos, float* mv, float* p, int sW, int sH, Color color) {
    float hw = w / 2.0f;
    Vec3 corners[8] = {
        {feet.x-hw, feet.y,   feet.z-hw}, {feet.x+hw, feet.y,   feet.z-hw},
        {feet.x+hw, feet.y,   feet.z+hw}, {feet.x-hw, feet.y,   feet.z+hw},
        {feet.x-hw, feet.y+h, feet.z-hw}, {feet.x+hw, feet.y+h, feet.z-hw},
        {feet.x+hw, feet.y+h, feet.z+hw}, {feet.x-hw, feet.y+h, feet.z+hw}
    };

    Vec2 s[8]; bool valid[8];
    for (int i = 0; i < 8; i++)
        valid[i] = WorldToScreen(corners[i], camPos, mv, p, s[i], sW, sH);

    // Only allocate the colour pen — outline pen is cached
    Pen colorPen(color, 2.0f);

    auto line = [&](int i, int j) {
        if (!valid[i] || !valid[j]) return;
        g.DrawLine(s_blackPen4, s[i].x, s[i].y, s[j].x, s[j].y);
        g.DrawLine(&colorPen,   s[i].x, s[i].y, s[j].x, s[j].y);
    };

    line(0,1); line(1,2); line(2,3); line(3,0);
    line(4,5); line(5,6); line(6,7); line(7,4);
    line(0,4); line(1,5); line(2,6); line(3,7);
}

// ── DrawProfessionalESP ───────────────────────────────────────────────────────
void ESP::DrawProfessionalESP(Graphics& g, float x, float y, float w, float h,
                               float health, float maxHealth,
                               int screenW, int screenH,
                               const std::wstring& name, double distance, bool drawTracer) {
    if (maxHealth <= 0) maxHealth = 20.0f;
    float hpPct = std::max(0.0f, std::min(1.0f, health / maxHealth));

    // Health bar — only the fill brush needs a per-call colour
    float barX = x - 8.0f, barY = y - 1.0f, barW = 6.0f, barH = h + 2.0f;
    g.FillRectangle(s_bgBarBrush, barX, barY, barW, barH);

    int r = (int)(255.0f * (1.0f - hpPct));
    int gr= (int)(255.0f * hpPct);
    SolidBrush hpBrush(Color(255, r, gr, 0));
    float fillH = h * hpPct;
    g.FillRectangle(&hpBrush, barX + 1.0f, y + (h - fillH), barW - 2.0f, fillH);

    if (drawTracer) {
        g.DrawLine(s_tracerBlack4, (REAL)(screenW/2), (REAL)screenH, x + w/2, y + h);
        g.DrawLine(s_tracerWhite2, (REAL)(screenW/2), (REAL)screenH, x + w/2, y + h);
    }

    wchar_t textBuf[256];
    swprintf_s(textBuf, L"%ls [%.1fm]", name.c_str(), distance);

    PointF textPos(x + w / 2.0f, y - 16.0f);
    g.DrawString(textBuf, -1, s_espFont, PointF(textPos.X+1, textPos.Y+1), s_centerFmt, s_shadowBrush);
    g.DrawString(textBuf, -1, s_espFont, textPos,                           s_centerFmt, s_textBrush);
}

// ── DrawGUI ───────────────────────────────────────────────────────────────────
void ESP::DrawGUI(Graphics& g, int mouseX, int mouseY, bool clickAction, bool rightClickAction) {
    int totalHeight = 60;
    for (Module* mod : ModuleManager::GetModules()) {
        if (!mod) continue;
        totalHeight += 35;
        if (mod->IsExpanded()) {
            const std::string& n = mod->GetName();
            if      (n == "XRay")          totalHeight += 9*25 + 10;
            else if (n == "Killaura")       totalHeight += 2*40 + 10;
            else if (n == "TeleportAura")   totalHeight += 40  + 10;
            else if (n == "Aimbot")         totalHeight += 40  + 10;
            else if (n == "AutoClicker")    totalHeight += 2*40 + 25 + 10;
            else if (n == "ESP")            totalHeight += 40  + 10;
            else if (n == "Reach")          totalHeight += 40  + 10;
            else if (n == "FakeLag")        totalHeight += 40  + 10;
            else                            totalHeight += 40  + 10;
        }
    }

    g.FillRectangle(s_guiBg,     100, 100, 400, totalHeight);
    g.FillRectangle(s_guiHeader, 100, 100, 400, 40);
    g.DrawString(L"Musty Client", -1, s_titleFont, PointF(115, 110), nullptr, s_textBrush);

    // ── lambdas use cached brushes ────────────────────────────────────────────
    auto DrawCheckbox = [&](const wchar_t* label, bool& value, int cx, int cy) {
        g.FillRectangle(s_cbBg, cx, cy, 15, 15);
        if (value) g.FillRectangle(s_cbFill, cx+2, cy+2, 11, 11);
        g.DrawString(label, -1, s_modFont, PointF((float)(cx+25), (float)cy), nullptr, s_textBrush);
        if (clickAction && mouseX >= cx && mouseX <= cx+200 && mouseY >= cy && mouseY <= cy+15)
            value = !value;
        return 25;
    };

    auto DrawSlider = [&](const wchar_t* label, float& value, float mn, float mx, bool& dragging, int cx, int cy) {
        int sW = 340, sH = 10;
        float pct = std::max(0.0f, std::min(1.0f, (value - mn) / (mx - mn)));

        wchar_t buf[64];
        swprintf_s(buf, L"%ls: %.2f", label, value);
        g.DrawString(buf, -1, s_modFont, PointF((float)cx, (float)cy), nullptr, s_textBrush);
        cy += 18;

        g.FillRectangle(s_sliderBg,   cx, cy, sW, sH);
        g.FillRectangle(s_sliderFill, cx, cy, (int)(sW * pct), sH);

        if (clickAction && mouseX >= cx && mouseX <= cx+sW && mouseY >= cy && mouseY <= cy+sH)
            dragging = true;
        if (dragging) {
            if (!(GetAsyncKeyState(VK_LBUTTON) & 0x8000)) {
                dragging = false;
            } else {
                float np = (float)(mouseX - cx) / sW;
                value = mn + std::max(0.0f, std::min(1.0f, np)) * (mx - mn);
            }
        }
        return 40;
    };

    int y = 150;
    for (Module* mod : ModuleManager::GetModules()) {
        if (!mod) continue;
        bool enabled = mod->IsEnabled();

        SolidBrush modBg(enabled ? Color(255, 46, 204, 113) : Color(255, 45, 45, 45));
        g.FillRectangle(&modBg, 120, y, 360, 30);

        std::string nameStr = mod->GetName();
        std::wstring wName(nameStr.begin(), nameStr.end());
        g.DrawString(wName.c_str(), -1, s_modFont, PointF(135, (float)(y+6)), nullptr, s_textBrush);
        g.DrawString(mod->IsExpanded() ? L"v" : L">", -1, s_modFont, PointF(460, (float)(y+6)), nullptr, s_textBrush);

        bool hovered = (mouseX >= 120 && mouseX <= 480 && mouseY >= y && mouseY <= y+30);
        if (clickAction      && hovered) mod->Toggle();
        if (rightClickAction && hovered) mod->SetExpanded(!mod->IsExpanded());

        y += 35;

        if (mod->IsExpanded()) {
            if (nameStr == "XRay") {
                XRay* xray = (XRay*)mod;
                if (xray) {
                    y += DrawCheckbox(L"Diamond Ore",    xray->showDiamond,     130, y);
                    y += DrawCheckbox(L"Gold Ore",       xray->showGold,        130, y);
                    y += DrawCheckbox(L"Iron Ore",       xray->showIron,        130, y);
                    y += DrawCheckbox(L"Emerald Ore",    xray->showEmerald,     130, y);
                    y += DrawCheckbox(L"Ancient Debris", xray->showNetherite,   130, y);
                    y += DrawCheckbox(L"Chest & Barrels",xray->showChests,      130, y);
                    y += DrawCheckbox(L"Ender Chests",   xray->showEnderChests, 130, y);
                    y += DrawCheckbox(L"Spawners",       xray->showSpawners,    130, y);
                    y += DrawCheckbox(L"Hoppers",        xray->showHoppers,     130, y);
                }
            } else if (nameStr == "Killaura") {
                Killaura* ka = (Killaura*)mod;
                if (ka) {
                    float r = ka->GetReach(), fov = ka->GetFOV();
                    y += DrawSlider(L"Reach", r,   3.0f,   6.0f, draggingKaReachSlider, 130, y);
                    y += DrawSlider(L"FOV",   fov, 10.0f, 360.0f, draggingKaFovSlider,  130, y);
                    ka->SetReach(r); ka->SetFOV(fov);
                }
            } else if (nameStr == "TeleportAura") {
                TeleportAura* ta = (TeleportAura*)mod;
                if (ta) {
                    float r = ta->GetReach();
                    y += DrawSlider(L"Teleport Reach", r, 5.0f, 100.0f, draggingTaReachSlider, 130, y);
                    ta->SetReach(r);
                }
            } else if (nameStr == "FakeLag") {
                FakeLag* fl = (FakeLag*)mod;
                if (fl) {
                    float limit = (float)fl->GetChokeLimit();
                    y += DrawSlider(L"Choke Limit", limit, 1.0f, 20.0f, draggingFlChokeSlider, 130, y);
                    fl->SetChokeLimit((int)limit);
                }
            } else if (nameStr == "Aimbot") {
                Aimbot* aim = (Aimbot*)mod;
                if (aim) {
                    float s = aim->GetSmoothSpeed();
                    y += DrawSlider(L"Smooth Speed", s, 0.01f, 0.50f, draggingAimSmoothSlider, 130, y);
                    aim->SetSmoothSpeed(s);
                }
            } else if (nameStr == "AutoClicker") {
                AutoClicker* ac = (AutoClicker*)mod;
                if (ac) {
                    float minCps = ac->GetMinCps(), maxCps = ac->GetMaxCps();
                    bool  jitter = ac->IsJitterEnabled();
                    y += DrawSlider(L"Min CPS", minCps, 1.0f, 20.0f, draggingAcMinSlider, 130, y);
                    y += DrawSlider(L"Max CPS", maxCps, 1.0f, 20.0f, draggingAcMaxSlider, 130, y);
                    y += DrawCheckbox(L"Jitter", jitter, 130, y);
                    ac->SetMinCps(minCps); ac->SetMaxCps(maxCps); ac->SetJitter(jitter);
                }
            } else if (nameStr == "ESP") {
                y += DrawSlider(L"ESP Range", espRange, 10.0f, 200.0f, draggingEspRangeSlider, 130, y);
            } else if (nameStr == "Reach") {
                Reach* rm = (Reach*)mod;
                if (rm) {
                    float r = rm->GetReach();
                    y += DrawSlider(L"Reach Distance", r, 3.0f, 6.0f, draggingReachSlider, 130, y);
                    rm->SetReach(r);
                }
            }
            y += 10;
        }
    }
}

// ── UpdateDataLoop ────────────────────────────────────────────────────────────
void ESP::UpdateDataLoop() {
    JNIEnv* env = nullptr;
    jint getEnvStat = JNIHelper::vm->GetEnv((void**)&env, JNI_VERSION_1_8);
    if (getEnvStat == JNI_EDETACHED) {
        if (JNIHelper::vm->AttachCurrentThread((void**)&env, nullptr) != JNI_OK) return;
    } else if (getEnvStat == JNI_EVERSION) {
        return;
    }

    JNIHelper::env = env;

    auto checkEx = [&](JNIEnv* e) {
        if (e->ExceptionCheck()) { e->ExceptionClear(); return true; }
        return false;
    };

    // ── class lookup with retry ───────────────────────────────────────────────
    jclass mcClass = nullptr, worldClass = nullptr, entityClass = nullptr, livingEntityClass = nullptr;
    jclass rendererClass = nullptr, cameraClass = nullptr, vec3dClass = nullptr, matrixClass = nullptr;
    jclass textClass = nullptr;

    int retries = 0;
    while (retries < 100 && running) {
        mcClass           = JNIHelper::FindClassSafe("Lnet/minecraft/class_310;",    "net/minecraft/client/MinecraftClient");
        worldClass        = JNIHelper::FindClassSafe("Lnet/minecraft/class_638;",    "net/minecraft/client/world/ClientWorld");
        entityClass       = JNIHelper::FindClassSafe("Lnet/minecraft/class_1297;",   "net/minecraft/entity/Entity");
        livingEntityClass = JNIHelper::FindClassSafe("Lnet/minecraft/class_1309;",   "net/minecraft/entity/LivingEntity");
        rendererClass     = JNIHelper::FindClassSafe("Lnet/minecraft/class_757;",    "net/minecraft/client/render/GameRenderer");
        cameraClass       = JNIHelper::FindClassSafe("Lnet/minecraft/class_4184;",   "net/minecraft/client/render/Camera");
        vec3dClass        = JNIHelper::FindClassSafe("Lnet/minecraft/class_243;",    "net/minecraft/util/math/Vec3d");
        matrixClass       = JNIHelper::FindClassSafe("Lorg/joml/Matrix4f;",          "org/joml/Matrix4f");
        textClass         = JNIHelper::FindClassSafe("Lnet/minecraft/class_2561;",   "net/minecraft/text/Text");

        if (mcClass && worldClass && entityClass && rendererClass && cameraClass && vec3dClass && matrixClass) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        retries++;
    }

    if (!mcClass || !worldClass || !entityClass || !rendererClass || !cameraClass || !vec3dClass || !matrixClass) {
        if (getEnvStat == JNI_EDETACHED) JNIHelper::vm->DetachCurrentThread();
        return;
    }

    // ── field / method IDs ────────────────────────────────────────────────────
    jfieldID instanceField     = JNIHelper::GetStaticFieldSafe(mcClass,       "field_1700",  "Lnet/minecraft/class_310;",  "instance");
    jfieldID localPlayerField  = JNIHelper::GetFieldSafe(mcClass,             "field_1724",  "Lnet/minecraft/class_746;",  "player");
    jfieldID worldField        = JNIHelper::GetFieldSafe(mcClass,             "field_1687",  "Lnet/minecraft/class_638;",  "world");
    jfieldID rendererField     = JNIHelper::GetFieldSafe(mcClass,             "field_1773",  "Lnet/minecraft/class_757;",  "gameRenderer");
    jfieldID playersField      = JNIHelper::GetFieldSafe(worldClass,          "field_18226", "Ljava/util/List;",           "players");

    jclass    listClass = env->FindClass("java/util/List");
    jmethodID listSize  = env->GetMethodID(listClass, "size", "()I");
    jmethodID listGet   = env->GetMethodID(listClass, "get",  "(I)Ljava/lang/Object;");

    jfieldID entX = JNIHelper::GetFieldSafe(entityClass, "field_6014", "D", "x");
    jfieldID entY = JNIHelper::GetFieldSafe(entityClass, "field_6036", "D", "y");
    jfieldID entZ = JNIHelper::GetFieldSafe(entityClass, "field_5969", "D", "z");

    jmethodID getNameMethod   = JNIHelper::GetMethodSafe(entityClass,       "method_5477",  "()Lnet/minecraft/class_2561;", "getName");
    jmethodID getStringMethod = textClass ? JNIHelper::GetMethodSafe(textClass, "method_10851", "()Ljava/lang/String;",      "getString") : nullptr;

    jmethodID getHealth = nullptr, getMaxHealth = nullptr;
    if (livingEntityClass) {
        getHealth    = JNIHelper::GetMethodSafe(livingEntityClass, "method_6032", "()F", "getHealth");
        getMaxHealth = JNIHelper::GetMethodSafe(livingEntityClass, "method_6063", "()F", "getMaxHealth");
    }

    jfieldID camField       = JNIHelper::GetFieldSafe(rendererClass, "lunar$savedCamera",                      "Lnet/minecraft/class_4184;", "camera");
    jfieldID modelViewField = JNIHelper::GetFieldSafe(rendererClass, "lunar$savedModelView$v1_19_3",            "Lorg/joml/Matrix4f;",        "modelView");
    jfieldID projField      = JNIHelper::GetFieldSafe(rendererClass, "lunar$savedProjection$v1_19_3",           "Lorg/joml/Matrix4f;",        "projection");
    jfieldID camPosField    = JNIHelper::GetFieldSafe(cameraClass,   "field_18712",                             "Lnet/minecraft/class_243;",  "pos");

    // Vec3d double fields via JVMTI
    jfieldID vec3dFields[3]; int vecIdx = 0;
    jint fCount; jfieldID* fds;
    JNIHelper::jvmti->GetClassFields(vec3dClass, &fCount, &fds);
    for (int i = 0; i < fCount; i++) {
        char* sig;
        JNIHelper::jvmti->GetFieldName(vec3dClass, fds[i], nullptr, &sig, nullptr);
        if (strcmp(sig, "D") == 0 && vecIdx < 3) vec3dFields[vecIdx++] = fds[i];
        JNIHelper::jvmti->Deallocate((unsigned char*)sig);
    }
    JNIHelper::jvmti->Deallocate((unsigned char*)fds);

    if (vecIdx < 3) {
        if (getEnvStat == JNI_EDETACHED) JNIHelper::vm->DetachCurrentThread();
        return;
    }

    // Matrix4f fields
    const char* mNames[] = { "m00","m01","m02","m03","m10","m11","m12","m13","m20","m21","m22","m23","m30","m31","m32","m33" };
    jfieldID matrixFields[16]; bool matrixValid = true;
    for (int i = 0; i < 16; i++) {
        matrixFields[i] = env->GetFieldID(matrixClass, mNames[i], "F");
        if (checkEx(env) || !matrixFields[i]) matrixValid = false;
    }

    if (!instanceField || !localPlayerField || !worldField || !rendererField ||
        !playersField   || !listSize        || !listGet    || !entX          ||
        !entY           || !entZ            || !camField   || !modelViewField ||
        !projField      || !camPosField     || !matrixValid) {
        if (getEnvStat == JNI_EDETACHED) JNIHelper::vm->DetachCurrentThread();
        return;
    }

    // ── data loop ─────────────────────────────────────────────────────────────
    while (running) {
        if (env->PushLocalFrame(128) < 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

        jobject mcInstance = env->GetStaticObjectField(mcClass, instanceField);
        if (checkEx(env) || !mcInstance) {
            env->PopLocalFrame(nullptr);
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

        jobject localPlayer = env->GetObjectField(mcInstance, localPlayerField); checkEx(env);
        jobject renderer    = env->GetObjectField(mcInstance, rendererField);    checkEx(env);
        jobject world       = env->GetObjectField(mcInstance, worldField);       checkEx(env);

        if (renderer && world) {
            jobject camera       = env->GetObjectField(renderer, camField);        checkEx(env);
            jobject modelViewObj = env->GetObjectField(renderer, modelViewField);  checkEx(env);
            jobject projObj      = env->GetObjectField(renderer, projField);       checkEx(env);
            jobject playersList  = env->GetObjectField(world, playersField);       checkEx(env);

            if (camera && modelViewObj && projObj && playersList) {
                jobject camPosObj = env->GetObjectField(camera, camPosField); checkEx(env);

                Vec3 tempCamPos = {0, 0, 0};
                if (camPosObj) {
                    tempCamPos = {
                        env->GetDoubleField(camPosObj, vec3dFields[0]),
                        env->GetDoubleField(camPosObj, vec3dFields[1]),
                        env->GetDoubleField(camPosObj, vec3dFields[2])
                    };
                    checkEx(env);
                }

                float tempMv[16], tempP[16];
                for (int i = 0; i < 16; i++) {
                    tempMv[i] = env->GetFloatField(modelViewObj, matrixFields[i]);
                    tempP[i]  = env->GetFloatField(projObj,      matrixFields[i]);
                }
                checkEx(env);

                std::vector<PlayerData> tempPlayers;
                jint size = env->CallIntMethod(playersList, listSize);
                if (!checkEx(env)) {
                    for (int i = 0; i < size; i++) {
                        jobject player = env->CallObjectMethod(playersList, listGet, i);
                        if (checkEx(env) || !player) continue;

                        if (localPlayer && env->IsSameObject(player, localPlayer)) {
                            env->DeleteLocalRef(player); continue;
                        }

                        Vec3 feetPos = {
                            env->GetDoubleField(player, entX),
                            env->GetDoubleField(player, entY),
                            env->GetDoubleField(player, entZ)
                        };
                        checkEx(env);

                        double dx = tempCamPos.x - feetPos.x;
                        double dy = tempCamPos.y - feetPos.y;
                        double dz = tempCamPos.z - feetPos.z;
                        double distance = std::sqrt(dx*dx + dy*dy + dz*dz);

                        if (distance <= espRange) {
                            float hp = 20.0f, maxHp = 20.0f;
                            if (getHealth && getMaxHealth) {
                                hp    = env->CallFloatMethod(player, getHealth);    checkEx(env);
                                maxHp = env->CallFloatMethod(player, getMaxHealth); checkEx(env);
                            }

                            std::wstring playerName = L"Player";
                            if (distance < 50.0 && getNameMethod && getStringMethod) {
                                jobject textObj = env->CallObjectMethod(player, getNameMethod);
                                if (!checkEx(env) && textObj) {
                                    jstring nameStr = (jstring)env->CallObjectMethod(textObj, getStringMethod);
                                    if (!checkEx(env) && nameStr) {
                                        const jchar* raw = env->GetStringChars(nameStr, nullptr);
                                        jsize len        = env->GetStringLength(nameStr);
                                        playerName = std::wstring((const wchar_t*)raw, len);
                                        env->ReleaseStringChars(nameStr, raw);
                                    }
                                }
                            }
                            tempPlayers.push_back({feetPos, hp, maxHp, playerName});
                        }
                        env->DeleteLocalRef(player);
                    }
                }

                {
                    std::lock_guard<std::mutex> lock(dataMutex);
                    cachedPlayers = tempPlayers;
                    cachedCamPos  = tempCamPos;
                    memcpy(cachedMv, tempMv, sizeof(tempMv));
                    memcpy(cachedP,  tempP,  sizeof(tempP));
                }
            }
        }

        env->PopLocalFrame(nullptr);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    if (getEnvStat == JNI_EDETACHED) JNIHelper::vm->DetachCurrentThread();
}

// ── RenderLoop ────────────────────────────────────────────────────────────────
void ESP::RenderLoop() {
    GdiplusStartupInput gdiplusStartupInput;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    InitGdipObjects();

    mcWindow = FindWindowA("GLFW30", NULL);
    if (!mcWindow) mcWindow = FindWindowA("LWJGL", NULL);

    WNDCLASSEXA wc = { sizeof(WNDCLASSEXA), 0, OverlayProc, 0, 0,
                       GetModuleHandle(NULL), NULL, NULL, NULL, NULL, "MustyOverlay", NULL };
    RegisterClassExA(&wc);

    RECT rect;
    GetWindowRect(mcWindow, &rect);

    overlayWindow = CreateWindowExA(
        WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED | WS_EX_TOOLWINDOW,
        "MustyOverlay", "Musty ESP",
        WS_POPUP,
        rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top,
        NULL, NULL, wc.hInstance, NULL
    );

    SetLayeredWindowAttributes(overlayWindow, RGB(0, 0, 0), 255, LWA_COLORKEY);
    ShowWindow(overlayWindow, SW_SHOW);

    // Black background brush — created once
    HBRUSH bgBrush = CreateSolidBrush(RGB(0, 0, 0));

    while (running) {
        MSG msg;
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        GetWindowRect(mcWindow, &rect);
        int width  = rect.right  - rect.left;
        int height = rect.bottom - rect.top;

        if (width <= 0 || height <= 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
            continue;
        }

        MoveWindow(overlayWindow, rect.left, rect.top, width, height, false); // false = no repaint flicker

        // ── GUI toggle ────────────────────────────────────────────────────────
        if (GetAsyncKeyState(VK_OEM_4) & 0x8000) {
            if (!insertPressed) {
                guiOpen = !guiOpen;
                insertPressed = true;
                LONG_PTR style = GetWindowLongPtrA(overlayWindow, GWL_EXSTYLE);
                if (guiOpen) {
                    SetWindowLongPtrA(overlayWindow, GWL_EXSTYLE, style & ~WS_EX_TRANSPARENT);
                    SetForegroundWindow(overlayWindow);
                } else {
                    SetWindowLongPtrA(overlayWindow, GWL_EXSTYLE, style | WS_EX_TRANSPARENT);
                }
            }
        } else {
            insertPressed = false;
        }

        POINT cursorPos = {0, 0};
        GetCursorPos(&cursorPos);
        ScreenToClient(overlayWindow, &cursorPos);

        bool isClicked       = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
        bool clickAction     = isClicked && !wasClicked;
        wasClicked           = isClicked;

        bool isRightClicked  = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
        bool rightClickAction= isRightClicked && !wasRightClicked;
        wasRightClicked      = isRightClicked;

        // ── back-buffer (persistent, only reallocated on resize) ──────────────
        HDC hdc = GetDC(overlayWindow);
        EnsureBackBuffer(hdc, width, height);

        RECT clientRect = { 0, 0, width, height };
        FillRect(s_memDC, &clientRect, bgBrush);

        {
            Graphics g(s_memDC);
            g.SetSmoothingMode(SmoothingModeAntiAlias);
            g.SetTextRenderingHint(TextRenderingHintAntiAlias);

            // ── snapshot shared data ──────────────────────────────────────────
            std::vector<PlayerData> playersToRender;
            Vec3  camPos;
            float mv[16], p[16];
            {
                std::lock_guard<std::mutex> lock(dataMutex);
                playersToRender = cachedPlayers;
                camPos          = cachedCamPos;
                memcpy(mv, cachedMv, sizeof(mv));
                memcpy(p,  cachedP,  sizeof(p));
            }

            // ── player ESP ────────────────────────────────────────────────────
            for (const auto& player : playersToRender) {
                Vec3 headPos = player.feetPos;
                headPos.y += 2.0;

                Vec2 screenFeet, screenHead;
                if (WorldToScreen(player.feetPos, camPos, mv, p, screenFeet, width, height) &&
                    WorldToScreen(headPos,         camPos, mv, p, screenHead, width, height)) {

                    float boxH = screenFeet.y - screenHead.y;
                    float boxW = boxH / 2.0f;
                    float boxX = screenHead.x - boxW / 2.0f;
                    float boxY = screenHead.y;

                    Draw3DBox(g, player.feetPos, 0.6f, 1.8f, camPos, mv, p, width, height, Color(255, 46, 204, 113));

                    double dx = camPos.x - player.feetPos.x;
                    double dy = camPos.y - player.feetPos.y;
                    double dz = camPos.z - player.feetPos.z;
                    DrawProfessionalESP(g, boxX, boxY, boxW, boxH,
                                        player.health, player.maxHealth,
                                        width, height,
                                        player.name, std::sqrt(dx*dx + dy*dy + dz*dz), true);
                }
            }

            // ── XRay blocks ───────────────────────────────────────────────────
            XRay* xray = (XRay*)ModuleManager::GetModule("XRay");
            if (xray && xray->IsEnabled()) {
                std::vector<XRayBlock> blocks = xray->GetFoundBlocks();
                for (const auto& block : blocks) {
                    Vec3 blockPos = { (double)block.x + 0.5, (double)block.y + 0.5, (double)block.z + 0.5 };
                    double dx = camPos.x - blockPos.x;
                    double dy = camPos.y - blockPos.y;
                    double dz = camPos.z - blockPos.z;
                    if (std::sqrt(dx*dx + dy*dy + dz*dz) <= espRange)
                        Draw3DBox(g, {(double)block.x, (double)block.y, (double)block.z},
                                  1.0f, 1.0f, camPos, mv, p, width, height,
                                  Color(255, block.r, block.g, block.b));
                }
            }

            if (guiOpen)
                DrawGUI(g, cursorPos.x, cursorPos.y, clickAction, rightClickAction);
        }

        BitBlt(hdc, 0, 0, width, height, s_memDC, 0, 0, SRCCOPY);
        ReleaseDC(overlayWindow, hdc);

        std::this_thread::sleep_for(std::chrono::milliseconds(8)); // ~120 fps cap
    }

    DeleteObject(bgBrush);
    FreeGdipObjects();

    // Free persistent back-buffer
    if (s_memBitmap) { DeleteObject(s_memBitmap); s_memBitmap = nullptr; }
    if (s_memDC)     { DeleteDC(s_memDC);         s_memDC     = nullptr; }
    s_bufW = s_bufH = 0;

    DestroyWindow(overlayWindow);
    GdiplusShutdown(gdiplusToken);
}
