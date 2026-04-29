#pragma once
#include "../Module.h"
#include <windows.h>
#include <gdiplus.h>
#include <thread>
#include <atomic>
#include <string>

struct Vec2 { float x, y; };
struct Vec3 { double x, y, z; };

class ESP : public Module {
private:
    HWND overlayWindow = NULL;
    HWND mcWindow = NULL;
    ULONG_PTR gdiplusToken = 0;
    std::thread renderThread;
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

    static LRESULT CALLBACK OverlayProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void RenderLoop();
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
