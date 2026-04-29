#include "ESP.h"
#include "../../core/JNIHelper.h"
#include "../ModuleManager.h"
#include <iostream>
#include <fstream>
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
    std::cout << "[MustyClient] ESP Enabled." << std::endl;
}

void ESP::OnDisable() {
    running = false;
    if (renderThread.joinable()) {
        renderThread.join();
    }
    std::cout << "[MustyClient] ESP Disabled." << std::endl;
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

void ESP::Draw3DBox(Graphics& g, Vec3 feet, float w, float h, Vec3 camPos, float* mv, float* p, int sW, int sH, Color color) {
    float hw = w / 2.0f;
    
    // 8 Corners of the bounding box
    Vec3 corners[8] = {
        {feet.x - hw, feet.y, feet.z - hw},
        {feet.x + hw, feet.y, feet.z - hw},
        {feet.x + hw, feet.y, feet.z + hw},
        {feet.x - hw, feet.y, feet.z + hw},
        {feet.x - hw, feet.y + h, feet.z - hw},
        {feet.x + hw, feet.y + h, feet.z - hw},
        {feet.x + hw, feet.y + h, feet.z + hw},
        {feet.x - hw, feet.y + h, feet.z + hw}
    };

    Vec2 s[8];
    bool valid[8];
    for (int i = 0; i < 8; i++) {
        valid[i] = WorldToScreen(corners[i], camPos, mv, p, s[i], sW, sH);
    }

    Pen pen(color, 1.5f);
    
    auto drawLineIfValid = [&](int i, int j) {
        if (valid[i] && valid[j]) {
            g.DrawLine(&pen, s[i].x, s[i].y, s[j].x, s[j].y);
        }
    };

    // Bottom rectangle
    drawLineIfValid(0, 1); drawLineIfValid(1, 2); drawLineIfValid(2, 3); drawLineIfValid(3, 0);
    // Top rectangle
    drawLineIfValid(4, 5); drawLineIfValid(5, 6); drawLineIfValid(6, 7); drawLineIfValid(7, 4);
    // Pillars
    drawLineIfValid(0, 4); drawLineIfValid(1, 5); drawLineIfValid(2, 6); drawLineIfValid(3, 7);
}

void ESP::DrawGUI(Graphics& g, int mouseX, int mouseY, bool clickAction) {
    // Main Window Background
    SolidBrush bg(Color(240, 25, 25, 25));
    g.FillRectangle(&bg, 100, 100, 400, 350);

    // Header
    SolidBrush header(Color(255, 15, 15, 15));
    g.FillRectangle(&header, 100, 100, 400, 40);

    FontFamily fontFamily(L"Verdana");
    Font titleFont(&fontFamily, 16, FontStyleBold, UnitPixel);
    Font modFont(&fontFamily, 14, FontStyleRegular, UnitPixel);
    SolidBrush textBrush(Color(255, 255, 255, 255));
    
    g.DrawString(L"Vape V4 (MustyClient Edition)", -1, &titleFont, PointF(115, 110), nullptr, &textBrush);

    // Draw Modules
    int y = 160;
    for (Module* mod : ModuleManager::GetModules()) {
        bool enabled = mod->IsEnabled();
        
        // Module Button Background
        SolidBrush modBg(enabled ? Color(255, 46, 204, 113) : Color(255, 45, 45, 45));
        g.FillRectangle(&modBg, 120, y, 360, 30);

        std::wstring wName(mod->GetName().begin(), mod->GetName().end());
        g.DrawString(wName.c_str(), -1, &modFont, PointF(135, y + 6), nullptr, &textBrush);

        // Handle Click
        if (clickAction && mouseX >= 120 && mouseX <= 480 && mouseY >= y && mouseY <= y + 30) {
            mod->Toggle();
        }
        
        y += 40;
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

    jclass mcClass = nullptr, worldClass = nullptr, entityClass = nullptr, livingEntityClass = nullptr;
    jclass rendererClass = nullptr, cameraClass = nullptr, vec3dClass = nullptr, matrixClass = nullptr;
    jclass textClass = nullptr;

    jint classCount = 0;
    jclass* classes = nullptr;
    JNIHelper::jvmti->GetLoadedClasses(&classCount, &classes);

    for (int i = 0; i < classCount; i++) {
        char* sig;
        JNIHelper::jvmti->GetClassSignature(classes[i], &sig, nullptr);
        if (sig) {
            if (strcmp(sig, "Lnet/minecraft/class_310;") == 0) mcClass = classes[i];
            else if (strcmp(sig, "Lnet/minecraft/class_638;") == 0) worldClass = classes[i];
            else if (strcmp(sig, "Lnet/minecraft/class_1297;") == 0) entityClass = classes[i];
            else if (strcmp(sig, "Lnet/minecraft/class_1309;") == 0) livingEntityClass = classes[i];
            else if (strcmp(sig, "Lnet/minecraft/class_757;") == 0) rendererClass = classes[i];
            else if (strcmp(sig, "Lnet/minecraft/class_4184;") == 0) cameraClass = classes[i];
            else if (strcmp(sig, "Lnet/minecraft/class_243;") == 0) vec3dClass = classes[i];
            else if (strcmp(sig, "Lorg/joml/Matrix4f;") == 0) matrixClass = classes[i];
            else if (strcmp(sig, "Lnet/minecraft/class_2561;") == 0) textClass = classes[i];
            JNIHelper::jvmti->Deallocate((unsigned char*)sig);
        }
    }
    JNIHelper::jvmti->Deallocate((unsigned char*)classes);

    if (!mcClass || !worldClass || !entityClass || !rendererClass || !cameraClass || !vec3dClass || !matrixClass) {
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

    jmethodID getNameMethod = env->GetMethodID(entityClass, "method_5477", "()Lnet/minecraft/class_2561;");
    jmethodID getStringMethod = nullptr;
    if (textClass) getStringMethod = env->GetMethodID(textClass, "method_10851", "()Ljava/lang/String;");

    jmethodID getHealth = nullptr, getMaxHealth = nullptr;
    if (livingEntityClass) {
        getHealth = env->GetMethodID(livingEntityClass, "method_6032", "()F");
        getMaxHealth = env->GetMethodID(livingEntityClass, "method_6063", "()F");
    }

    jfieldID camField = env->GetFieldID(rendererClass, "lunar$savedCamera", "Lnet/minecraft/class_4184;");
    jfieldID modelViewField = env->GetFieldID(rendererClass, "lunar$savedModelView$v1_19_3", "Lorg/joml/Matrix4f;");
    jfieldID projField = env->GetFieldID(rendererClass, "lunar$savedProjection$v1_19_3", "Lorg/joml/Matrix4f;");
    jfieldID camPosField = env->GetFieldID(cameraClass, "field_18712", "Lnet/minecraft/class_243;");

    jfieldID vec3dFields[3];
    int vecIdx = 0;
    jint fCount; jfieldID* fds;
    JNIHelper::jvmti->GetClassFields(vec3dClass, &fCount, &fds);
    for (int i = 0; i < fCount; i++) {
        char* sig;
        JNIHelper::jvmti->GetFieldName(vec3dClass, fds[i], nullptr, &sig, nullptr);
        if (strcmp(sig, "D") == 0 && vecIdx < 3) vec3dFields[vecIdx++] = fds[i];
        JNIHelper::jvmti->Deallocate((unsigned char*)sig);
    }
    JNIHelper::jvmti->Deallocate((unsigned char*)fds);

    const char* mNames[] = { "m00","m01","m02","m03", "m10","m11","m12","m13", "m20","m21","m22","m23", "m30","m31","m32","m33" };
    jfieldID matrixFields[16];
    for (int i = 0; i < 16; i++) matrixFields[i] = env->GetFieldID(matrixClass, mNames[i], "F");

    FontFamily fontFamily(L"Consolas");
    Font font(&fontFamily, 12, FontStyleBold, UnitPixel);
    StringFormat format;
    format.SetAlignment(StringAlignmentCenter);
    SolidBrush shadowBrush(Color(255, 0, 0, 0));
    SolidBrush textBrush(Color(255, 255, 255, 255));

    while (running) {
        GetWindowRect(mcWindow, &rect);
        int width = rect.right - rect.left;
        int height = rect.bottom - rect.top;
        MoveWindow(overlayWindow, rect.left, rect.top, width, height, true);

        // GUI Toggle Logic (Left Bracket '[')
        if (GetAsyncKeyState(VK_OEM_4) & 0x8000) {
            if (!insertPressed) {
                guiOpen = !guiOpen;
                insertPressed = true;
                long style = GetWindowLong(overlayWindow, GWL_EXSTYLE);
                if (guiOpen) {
                    SetWindowLong(overlayWindow, GWL_EXSTYLE, style & ~WS_EX_TRANSPARENT);
                    SetForegroundWindow(overlayWindow);
                } else {
                    SetWindowLong(overlayWindow, GWL_EXSTYLE, style | WS_EX_TRANSPARENT);
                }
            }
        } else {
            insertPressed = false;
        }

        // Mouse Logic
        POINT cursorPos;
        GetCursorPos(&cursorPos);
        ScreenToClient(overlayWindow, &cursorPos);
        bool isClicked = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
        bool clickAction = isClicked && !wasClicked;
        wasClicked = isClicked;

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
                    Vec3 camPos = {
                        env->GetDoubleField(camPosObj, vec3dFields[0]),
                        env->GetDoubleField(camPosObj, vec3dFields[1]),
                        env->GetDoubleField(camPosObj, vec3dFields[2])
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

                        // 1. Draw 3D Rectangular Prism (Vape Style)
                        Draw3DBox(g, feetPos, 0.6f, 1.8f, camPos, mv, p, width, height, Color(255, 46, 204, 113));

                        // 2. Nametag & Health
                        Vec3 headPos = { feetPos.x, feetPos.y + 2.0, feetPos.z };
                        Vec2 screenHead;
                        if (WorldToScreen(headPos, camPos, mv, p, screenHead, width, height)) {
                            float hp = 20.0f;
                            if (getHealth) hp = env->CallFloatMethod(player, getHealth);

                            double distance = sqrt(pow(camPos.x - feetPos.x, 2) + pow(camPos.y - feetPos.y, 2) + pow(camPos.z - feetPos.z, 2));

                            std::wstring playerName = L"Unknown";
                            if (getNameMethod && getStringMethod) {
                                jobject textObj = env->CallObjectMethod(player, getNameMethod);
                                if (textObj) {
                                    jstring nameStr = (jstring)env->CallObjectMethod(textObj, getStringMethod);
                                    if (nameStr) {
                                        const jchar* rawName = env->GetStringChars(nameStr, nullptr);
                                        jsize len = env->GetStringLength(nameStr);
                                        playerName = std::wstring((const wchar_t*)rawName, len);
                                        env->ReleaseStringChars(nameStr, rawName);
                                        env->DeleteLocalRef(nameStr);
                                    }
                                    env->DeleteLocalRef(textObj);
                                }
                            }

                            wchar_t textBuf[256];
                            swprintf_s(textBuf, L"%s [%.1fm] \u2764%.1f", playerName.c_str(), distance, hp);
                            
                            PointF textPos(screenHead.x, screenHead.y - 16.0f);
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

        // Draw GUI on top of everything if open
        if (guiOpen) {
            DrawGUI(g, cursorPos.x, cursorPos.y, clickAction);
        }

        BitBlt(hdc, 0, 0, width, height, memDC, 0, 0, SRCCOPY);

        SelectObject(memDC, oldBitmap);
        DeleteObject(memBitmap);
        DeleteDC(memDC);
        ReleaseDC(overlayWindow, hdc);

        std::this_thread::sleep_for(std::chrono::milliseconds(8));
    }

    DestroyWindow(overlayWindow);
    GdiplusShutdown(gdiplusToken);
}
