#pragma once
#include "../Module.h"
#include <windows.h>
#include <gdiplus.h>
#include <thread>
#include <atomic>

struct Vec2 { float x, y; };
struct Vec3 { double x, y, z; };

class ESP : public Module {
private:
    HWND overlayWindow = NULL;
    HWND mcWindow = NULL;
    ULONG_PTR gdiplusToken = 0;
    std::thread renderThread;
    std::atomic<bool> running{false};

    static LRESULT CALLBACK OverlayProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void RenderLoop();
    bool WorldToScreen(Vec3 pos, Vec3 camPos, float* mv, float* p, Vec2& screen, int width, int height);
    
    void DrawCornerBox(Gdiplus::Graphics& g, float x, float y, float w, float h, Gdiplus::Color color);
    void DrawHealthBar(Gdiplus::Graphics& g, float x, float y, float w, float h, float hp, float maxHp);
    void DrawSkeleton(Gdiplus::Graphics& g, Vec3 feet, float bodyYaw, float height, Vec3 camPos, float* mv, float* p, int sW, int sH);

public:
    ESP();
    ~ESP();
    void OnEnable() override;
    void OnDisable() override;
};
