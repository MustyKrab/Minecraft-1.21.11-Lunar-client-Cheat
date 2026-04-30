#pragma once
#include "../Module.h"
#include <windows.h>
#include <gdiplus.h>
#include <thread>
#include <atomic>
#include <string>
#include <vector>
#include <mutex>

struct Vec2 { float x, y; };
struct Vec3 { double x, y, z; };

// FOX FIX: Struct to cache player data so we don't spam JNI in the render loop
struct PlayerData {
    Vec3 feetPos;
    float health;
    float maxHealth;
    std::wstring name;
};

class ESP : public Module {
private:
    HWND overlayWindow = NULL;
    HWND mcWindow = NULL;
    ULONG_PTR gdiplusToken = 0;
    
    std::thread renderThread;
    std::thread updateThread; // FOX FIX: Separate thread for JNI data gathering
    std::atomic<bool> running{false};

    bool guiOpen = false;
    bool insertPressed = false;
    bool wasClicked = false;
    bool wasRightClicked = false;
    
    bool draggingSlider = false;
    bool draggingAimSlider = false;
    bool draggingEspRangeSlider = false;
    bool draggingReachSlider = false;
    bool draggingAcMinSlider = false;
    bool draggingAcMaxSlider = false;

    float espRange = 100.0f;

    // FOX FIX: Cached data for smooth rendering
    std::vector<PlayerData> cachedPlayers;
    std::mutex dataMutex;
    Vec3 cachedCamPos;
    float cachedMv[16];
    float cachedP[16];

    static LRESULT CALLBACK OverlayProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void RenderLoop();
    void UpdateDataLoop(); // FOX FIX: New loop for JNI calls
    
    bool WorldToScreen(Vec3 pos, Vec3 camPos, float* mv, float* p, Vec2& screen, int width, int height);
    void Draw3DBox(Gdiplus::Graphics& g, Vec3 feet, float w, float h, Vec3 camPos, float* mv, float* p, int sW, int sH, Gdiplus::Color color);
    void DrawProfessionalESP(Gdiplus::Graphics& g, float x, float y, float w, float h, float health, float maxHealth, int screenW, int screenH, const std::wstring& name, double distance, bool drawTracer);
    void DrawGUI(Gdiplus::Graphics& g, int mouseX, int mouseY, bool clickAction, bool rightClickAction);

public:
    ESP();
    ~ESP();
    void OnEnable() override;
    void OnDisable() override;
};