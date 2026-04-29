#include "ESP.h"
#include "../../core/JNIHelper.h"
#include <iostream>
#include <cmath>
#include <string>

#pragma comment(lib, "gdiplus.lib")
using namespace Gdiplus;

ESP::ESP() : Module("ESP") {}

ESP::~ESP() {
    if (running) {
        running = false;
        if (renderThread.joinable()) renderThread.join();
    }
}

void ESP::OnEnable() {
    running = true;
    renderThread = std::thread(&ESP::RenderLoop, this);
    std::cout << "[MustyClient] Advanced ESP Enabled." << std::endl;
}

void ESP::OnDisable() {
    running = false;
    if (renderThread.joinable()) {
        renderThread.join();
    }
    std::cout << "[MustyClient] Advanced ESP Disabled." << std::endl;
}

LRESULT CALLBACK ESP::OverlayProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_ERASEBKGND) return 1;
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

bool ESP::WorldToScreen(Vec3 pos, Vec3 camPos, float* mv, float* p, Vec2& screen, int width, int height) {
    float x = (float)(pos.x - camPos.x);
    float y = (float)(pos.y - camPos.y);
    float z = (float)(pos.z - camPos.z);

    float viewX = mv[0] * x + mv[4] * y + mv[8] * z + mv[12];
    float viewY = mv[1] * x + mv[5] * y + mv[9] * z + mv[13];
    float viewZ = mv[2] * x + mv[6] * y + mv[10] * z + mv[14];
    float viewW = mv[3] * x + mv[7] * y + mv[11] * z + mv[15];

    float clipX = p[0] * viewX + p[4] * viewY + p[8] * viewZ + p[12] * viewW;
    float clipY = p[1] * viewX + p[5] * viewY + p[9] * viewZ + p[13] * viewW;
    float clipW = p[3] * viewX + p[7] * viewY + p[11] * viewZ + p[15] * viewW;

    if (clipW < 0.1f) return false;

    float ndcX = clipX / clipW;
    float ndcY = clipY / clipW;

    screen.x = (width / 2.0f) * (ndcX + 1.0f);
    screen.y = (height / 2.0f) * (1.0f - ndcY);
    return true;
}

void ESP::DrawCornerBox(Graphics& g, float x, float y, float w, float h, Color color) {
    Pen pen(color, 1.5f);
    Pen shadow(Color(255, 0, 0, 0), 2.5f);
    
    float lineW = w / 4.0f;
    float lineH = h / 4.0f;

    // Shadows
    g.DrawLine(&shadow, x, y, x + lineW, y);
    g.DrawLine(&shadow, x, y, x, y + lineH);
    g.DrawLine(&shadow, x + w - lineW, y, x + w, y);
    g.DrawLine(&shadow, x + w, y, x + w, y + lineH);
    g.DrawLine(&shadow, x, y + h, x + lineW, y + h);
    g.DrawLine(&shadow, x, y + h - lineH, x, y + h);
    g.DrawLine(&shadow, x + w - lineW, y + h, x + w, y + h);
    g.DrawLine(&shadow, x + w, y + h - lineH, x + w, y + h);

    // Main Lines
    g.DrawLine(&pen, x, y, x + lineW, y);
    g.DrawLine(&pen, x, y, x, y + lineH);
    g.DrawLine(&pen, x + w - lineW, y, x + w, y);
    g.DrawLine(&pen, x + w, y, x + w, y + lineH);
    g.DrawLine(&pen, x, y + h, x + lineW, y + h);
    g.DrawLine(&pen, x, y + h - lineH, x, y + h);
    g.DrawLine(&pen, x + w - lineW, y + h, x + w, y + h);
    g.DrawLine(&pen, x + w, y + h - lineH, x + w, y + h);
}

void ESP::DrawHealthBar(Graphics& g, float x, float y, float w, float h, float hp, float maxHp) {
    float hpPercent = hp / maxHp;
    if (hpPercent > 1.0f) hpPercent = 1.0f;
    if (hpPercent < 0.0f) hpPercent = 0.0f;

    int r = (int)(255.0f * (1.0f - hpPercent));
    int gr = (int)(255.0f * hpPercent);

    SolidBrush bgBrush(Color(200, 0, 0, 0));
    g.FillRectangle(&bgBrush, x - 1, y - 1, w + 2, h + 2);

    SolidBrush hpBrush(Color(255, r, gr, 0));
    float hpFillH = h * hpPercent;
    float hpFillY = y + (h - hpFillH);
    g.FillRectangle(&hpBrush, x, hpFillY, w, hpFillH);
}

void ESP::DrawSkeleton(Graphics& g, Vec3 feet, float bodyYaw, float height, Vec3 camPos, float* mv, float* p, int sW, int sH) {
    float cosY = std::cos(bodyYaw * (3.14159265f / 180.0f));
    float sinY = std::sin(bodyYaw * (3.14159265f / 180.0f));

    Vec3 rightOffset = { -cosY * 0.3, 0, -sinY * 0.3 };
    Vec3 leftOffset = { cosY * 0.3, 0, sinY * 0.3 };

    Vec3 head = { feet.x, feet.y + height, feet.z };
    Vec3 neck = { feet.x, feet.y + height - 0.4, feet.z };
    Vec3 pelvis = { feet.x, feet.y + height - 1.1, feet.z };

    Vec3 rShoulder = { neck.x + rightOffset.x, neck.y, neck.z + rightOffset.z };
    Vec3 lShoulder = { neck.x + leftOffset.x, neck.y, neck.z + leftOffset.z };
    Vec3 rHand = { rShoulder.x, rShoulder.y - 0.6, rShoulder.z };
    Vec3 lHand = { lShoulder.x, lShoulder.y - 0.6, lShoulder.z };

    Vec3 rHip = { pelvis.x + rightOffset.x * 0.5, pelvis.y, pelvis.z + rightOffset.z * 0.5 };
    Vec3 lHip = { pelvis.x + leftOffset.x * 0.5, pelvis.y, pelvis.z + leftOffset.z * 0.5 };
    Vec3 rFoot = { feet.x + rightOffset.x * 0.5, feet.y, feet.z + rightOffset.z * 0.5 };
    Vec3 lFoot = { feet.x + leftOffset.x * 0.5, feet.y, feet.z + leftOffset.z * 0.5 };

    Vec2 pHead, pNeck, pPelvis, pRS, pLS, pRH, pLH, pRHip, pLHip, pRFoot, pLFoot;

    if (WorldToScreen(head, camPos, mv, p, pHead, sW, sH) &&
        WorldToScreen(neck, camPos, mv, p, pNeck, sW, sH) &&
        WorldToScreen(pelvis, camPos, mv, p, pPelvis, sW, sH) &&
        WorldToScreen(rShoulder, camPos, mv, p, pRS, sW, sH) &&
        WorldToScreen(lShoulder, camPos, mv, p, pLS, sW, sH) &&
        WorldToScreen(rHand, camPos, mv, p, pRH, sW, sH) &&
        WorldToScreen(lHand, camPos, mv, p, pLH, sW, sH) &&
        WorldToScreen(rHip, camPos, mv, p, pRHip, sW, sH) &&
        WorldToScreen(lHip, camPos, mv, p, pLHip, sW, sH) &&
        WorldToScreen(rFoot, camPos, mv, p, pRFoot, sW, sH) &&
        WorldToScreen(lFoot, camPos, mv, p, pLFoot, sW, sH)) {

        Pen bonePen(Color(255, 255, 255, 255), 1.0f);
        
        g.DrawLine(&bonePen, pHead.x, pHead.y, pNeck.x, pNeck.y);
        g.DrawLine(&bonePen, pNeck.x, pNeck.y, pPelvis.x, pPelvis.y);
        
        g.DrawLine(&bonePen, pNeck.x, pNeck.y, pRS.x, pRS.y);
        g.DrawLine(&bonePen, pRS.x, pRS.y, pRH.x, pRH.y);
        g.DrawLine(&bonePen, pNeck.x, pNeck.y, pLS.x, pLS.y);
        g.DrawLine(&bonePen, pLS.x, pLS.y, pLH.x, pLH.y);
        
        g.DrawLine(&bonePen, pPelvis.x, pPelvis.y, pRHip.x, pRHip.y);
        g.DrawLine(&bonePen, pRHip.x, pRHip.y, pRFoot.x, pRFoot.y);
        g.DrawLine(&bonePen, pPelvis.x, pPelvis.y, pLHip.x, pLHip.y);
        g.DrawLine(&bonePen, pLHip.x, pLHip.y, pLFoot.x, pLFoot.y);
    }
}

void ESP::RenderLoop() {
    GdiplusStartupInput gdiplusStartupInput;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    mcWindow = FindWindowA("GLFW30", NULL);
    if (!mcWindow) mcWindow = FindWindowA("LWJGL", NULL);

    WNDCLASSEXA wc = { sizeof(WNDCLASSEXA), 0, OverlayProc, 0, 0, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, "MustyOverlay", NULL };
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

    JNIEnv* env = JNIHelper::env;
    if (!env) return;

    // Cache classes
    jclass mcClass = JNIHelper::FindClassBySignature("Lnet/minecraft/class_310;");
    jclass worldClass = JNIHelper::FindClassBySignature("Lnet/minecraft/class_638;");
    jclass entityClass = JNIHelper::FindClassBySignature("Lnet/minecraft/class_1297;");
    jclass livingClass = JNIHelper::FindClassBySignature("Lnet/minecraft/class_1309;");
    jclass rendererClass = JNIHelper::FindClassBySignature("Lnet/minecraft/class_757;");
    jclass cameraClass = JNIHelper::FindClassBySignature("Lnet/minecraft/class_4184;");
    jclass vec3dClass = JNIHelper::FindClassBySignature("Lnet/minecraft/class_243;");
    jclass matrixClass = JNIHelper::FindClassBySignature("Lorg/joml/Matrix4f;");

    if (!mcClass || !worldClass || !entityClass || !livingClass || !rendererClass || !cameraClass || !vec3dClass || !matrixClass) {
        std::cout << "[MustyClient] Failed to resolve obfuscated classes." << std::endl;
        return;
    }

    jfieldID instanceField = env->GetStaticFieldID(mcClass, "field_1700", "Lnet/minecraft/class_310;");
    jfieldID localPlayerField = env->GetFieldID(mcClass, "field_1724", "Lnet/minecraft/class_746;");
    jfieldID worldField = env->GetFieldID(mcClass, "field_1687", "Lnet/minecraft/class_638;");
    jfieldID rendererField = env->GetFieldID(mcClass, "field_1773", "Lnet/minecraft/class_757;");

    jfieldID playersField = env->GetFieldID(worldClass, "field_18226", "Ljava/util/List;");
    jclass listClass = env->FindClass("java/util/List");
    jmethodID listSize = env->GetMethodID(listClass, "size", "()I");
    jmethodID listGet = env->GetMethodID(listClass, "get", "(I)Ljava/lang/Object;");

    jfieldID entX = env->GetFieldID(entityClass, "field_6014", "D");
    jfieldID entY = env->GetFieldID(entityClass, "field_6036", "D");
    jfieldID entZ = env->GetFieldID(entityClass, "field_5969", "D");
    
    jmethodID getHealth = env->GetMethodID(livingClass, "method_6032", "()F");
    jmethodID getMaxHealth = env->GetMethodID(livingClass, "method_6063", "()F");
    jfieldID bodyYaw = env->GetFieldID(livingClass, "field_6283", "F"); // bodyYaw

    jfieldID camField = env->GetFieldID(rendererClass, "lunar$savedCamera", "Lnet/minecraft/class_4184;");
    jfieldID modelViewField = env->GetFieldID(rendererClass, "lunar$savedModelView$v1_19_3", "Lorg/joml/Matrix4f;");
    jfieldID projField = env->GetFieldID(rendererClass, "lunar$savedProjection$v1_19_3", "Lorg/joml/Matrix4f;");
    jfieldID camPosField = env->GetFieldID(cameraClass, "field_18712", "Lnet/minecraft/class_243;");

    const char* mNames[] = { "m00","m01","m02","m03", "m10","m11","m12","m13", "m20","m21","m22","m23", "m30","m31","m32","m33" };
    jfieldID matrixFields[16];
    for (int i = 0; i < 16; i++) matrixFields[i] = env->GetFieldID(matrixClass, mNames[i], "F");

    FontFamily fontFamily(L"Verdana");
    Font font(&fontFamily, 11, FontStyleBold, UnitPixel);
    StringFormat format;
    format.SetAlignment(StringAlignmentCenter);
    SolidBrush textBrush(Color(255, 255, 255, 255));
    SolidBrush shadowBrush(Color(255, 0, 0, 0));

    while (running) {
        GetWindowRect(mcWindow, &rect);
        int width = rect.right - rect.left;
        int height = rect.bottom - rect.top;
        MoveWindow(overlayWindow, rect.left, rect.top, width, height, true);

        HDC hdc = GetDC(overlayWindow);
        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP memBitmap = CreateCompatibleBitmap(hdc, width, height);
        HGDIOBJ oldBitmap = SelectObject(memDC, memBitmap);

        RECT clientRect = { 0, 0, width, height };
        HBRUSH bgBrush = CreateSolidBrush(RGB(0, 0, 0));
        FillRect(memDC, &clientRect, bgBrush);
        DeleteObject(bgBrush);

        Graphics g(memDC);
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        g.SetTextRenderingHint(TextRenderingHintAntiAlias);

        jobject mcInstance = env->GetStaticObjectField(mcClass, instanceField);
        if (mcInstance) {
            jobject localPlayer = env->GetObjectField(mcInstance, localPlayerField);
            jobject renderer = env->GetObjectField(mcInstance, rendererField);
            jobject world = env->GetObjectField(mcInstance, worldField);

            if (renderer && world) {
                jobject camera = env->GetObjectField(renderer, camField);
                jobject modelViewObj = env->GetObjectField(renderer, modelViewField);
                jobject projObj = env->GetObjectField(renderer, projField);
                jobject playersList = env->GetObjectField(world, playersField);

                if (camera && modelViewObj && projObj && playersList) {
                    jobject camPosObj = env->GetObjectField(camera, camPosField);
                    
                    // Fetch Camera Pos
                    jint fCount; jfieldID* fds;
                    JNIHelper::jvmti->GetClassFields(vec3dClass, &fCount, &fds);
                    jfieldID vecFields[3]; int vIdx = 0;
                    for (int i = 0; i < fCount; i++) {
                        char* sig; JNIHelper::jvmti->GetFieldName(vec3dClass, fds[i], nullptr, &sig, nullptr);
                        if (strcmp(sig, "D") == 0 && vIdx < 3) vecFields[vIdx++] = fds[i];
                        JNIHelper::jvmti->Deallocate((unsigned char*)sig);
                    }
                    JNIHelper::jvmti->Deallocate((unsigned char*)fds);

                    Vec3 camPos = {
                        env->GetDoubleField(camPosObj, vecFields[0]),
                        env->GetDoubleField(camPosObj, vecFields[1]),
                        env->GetDoubleField(camPosObj, vecFields[2])
                    };
                    env->DeleteLocalRef(camPosObj);

                    float mv[16], p[16];
                    for (int i = 0; i < 16; i++) {
                        mv[i] = env->GetFloatField(modelViewObj, matrixFields[i]);
                        p[i] = env->GetFloatField(projObj, matrixFields[i]);
                    }

                    jint size = env->CallIntMethod(playersList, listSize);
                    for (int i = 0; i < size; i++) {
                        jobject player = env->CallObjectMethod(playersList, listGet, i);
                        if (!player) continue;

                        if (localPlayer && env->IsSameObject(player, localPlayer)) {
                            env->DeleteLocalRef(player);
                            continue;
                        }

                        Vec3 feetPos = {
                            env->GetDoubleField(player, entX),
                            env->GetDoubleField(player, entY),
                            env->GetDoubleField(player, entZ)
                        };

                        Vec3 headPos = feetPos;
                        headPos.y += 2.0; // Approx height

                        Vec2 screenFeet, screenHead;
                        if (WorldToScreen(feetPos, camPos, mv, p, screenFeet, width, height) &&
                            WorldToScreen(headPos, camPos, mv, p, screenHead, width, height)) {

                            float boxHeight = screenFeet.y - screenHead.y;
                            float boxWidth = boxHeight / 2.0f;
                            float boxX = screenHead.x - boxWidth / 2.0f;
                            float boxY = screenHead.y;

                            // 1. Corner Box
                            DrawCornerBox(g, boxX, boxY, boxWidth, boxHeight, Color(255, 255, 255, 255));

                            // 2. Health Bar
                            float hp = 20.0f, maxHp = 20.0f;
                            if (getHealth && getMaxHealth) {
                                hp = env->CallFloatMethod(player, getHealth);
                                maxHp = env->CallFloatMethod(player, getMaxHealth);
                            }
                            DrawHealthBar(g, boxX - 6.0f, boxY, 3.0f, boxHeight, hp, maxHp);

                            // 3. Skeleton
                            float bYaw = 0.0f;
                            if (bodyYaw) bYaw = env->GetFloatField(player, bodyYaw);
                            DrawSkeleton(g, feetPos, bYaw, 1.8f, camPos, mv, p, width, height);

                            // 4. Nametag & Distance
                            double distance = std::sqrt(std::pow(camPos.x - feetPos.x, 2) + std::pow(camPos.y - feetPos.y, 2) + std::pow(camPos.z - feetPos.z, 2));
                            wchar_t textBuf[256];
                            swprintf_s(textBuf, L"Player [%.1fm] \u2764%.1f", distance, hp);
                            
                            PointF textPos(screenHead.x, boxY - 16.0f);
                            g.DrawString(textBuf, -1, &font, PointF(textPos.X + 1, textPos.Y + 1), &format, &shadowBrush);
                            g.DrawString(textBuf, -1, &font, textPos, &format, &textBrush);
                        }
                        env->DeleteLocalRef(player);
                    }
                }
                if (camera) env->DeleteLocalRef(camera);
                if (modelViewObj) env->DeleteLocalRef(modelViewObj);
                if (projObj) env->DeleteLocalRef(projObj);
                if (playersList) env->DeleteLocalRef(playersList);
            }
            if (renderer) env->DeleteLocalRef(renderer);
            if (world) env->DeleteLocalRef(world);
            if (localPlayer) env->DeleteLocalRef(localPlayer);
            env->DeleteLocalRef(mcInstance);
        }

        BitBlt(hdc, 0, 0, width, height, memDC, 0, 0, SRCCOPY);

        SelectObject(memDC, oldBitmap);
        DeleteObject(memBitmap);
        DeleteDC(memDC);
        ReleaseDC(overlayWindow, hdc);

        std::this_thread::sleep_for(std::chrono::milliseconds(8)); // ~120 FPS cap
    }

    DestroyWindow(overlayWindow);
    GdiplusShutdown(gdiplusToken);
}
