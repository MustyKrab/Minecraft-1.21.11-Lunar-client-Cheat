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
    bool draggingSlider = false;

    static LRESULT CALLBACK OverlayProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void RenderLoop();
    bool WorldToScreen(Vec3 pos, Vec3 camPos, float* mv, float* p, Vec2& screen, int width, int height);
    
    void DrawProfessionalESP(Gdiplus::Graphics& g, float x, float y, float w, float h, float health, float maxHealth, int screenW, int screenH, const std::wstring& name, double distance);
    void DrawGUI(Gdiplus::Graphics& g, int mouseX, int mouseY, bool clickAction);

public:
    ESP();
    ~ESP();
    void OnEnable() override;
    void OnDisable() override;
};
