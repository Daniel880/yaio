//------
#define MIN_WIN_VER 0x0501

#ifndef WINVER
#	define WINVER			MIN_WIN_VER
#endif

#ifndef _WIN32_WINNT
#	define _WIN32_WINNT		MIN_WIN_VER 
#endif

#pragma warning(disable:4996) //_CRT_SECURE_NO_WARNINGS


#include <Windows.h>
#include <stdio.h>
#include <conio.h>
#include <signal.h>
#include <time.h>
#include <vector>
#include <d3d11.h>
#include <tchar.h>

#include "irsdk_defines.h"
#include "irsdk_client.h"
#include "serial.h"

// ImGui includes
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

// for timeBeginPeriod
#pragma comment(lib, "Winmm")
#pragma comment(lib, "d3d11")

// DirectX11 globals
static ID3D11Device*           g_Device = nullptr;
static ID3D11DeviceContext*    g_Context = nullptr;
static IDXGISwapChain*         g_SwapChain = nullptr;
static ID3D11RenderTargetView* g_RTV = nullptr;

// Graph data storage
const int GRAPH_HISTORY_SIZE = 1000;
std::vector<float> throttle_history;
std::vector<float> brake_history;
std::vector<bool> abs_history;

// Forward declarations
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

irsdkCVar Throttle("Throttle");
irsdkCVar Brake("Brake");

irsdkCVar RFbrakeLinePress("RFbrakeLinePress");
irsdkCVar BrakeABSactive("BrakeABSactive");


// Helper functions for DirectX11
bool CreateDeviceD3D(HWND hWnd) {
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 2;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    D3D_FEATURE_LEVEL level;
    const D3D_FEATURE_LEVEL levels[1] = { D3D_FEATURE_LEVEL_11_0 };
    if (D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
            levels, 1, D3D11_SDK_VERSION, &sd, &g_SwapChain, &g_Device, &level, &g_Context) != S_OK)
        return false;

    ID3D11Texture2D* backBuffer = nullptr;
    g_SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer);
    g_Device->CreateRenderTargetView(backBuffer, nullptr, &g_RTV);
    backBuffer->Release();
    return true;
}

void CleanupDeviceD3D() {
    if (g_RTV) { g_RTV->Release(); g_RTV = nullptr; }
    if (g_SwapChain) { g_SwapChain->Release(); g_SwapChain = nullptr; }
    if (g_Context) { g_Context->Release(); g_Context = nullptr; }
    if (g_Device) { g_Device->Release(); g_Device = nullptr; }
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) return 1;
    switch (msg) {
    case WM_SIZE:
        if (g_Device && wParam != SIZE_MINIMIZED) {
            if (g_RTV) { g_RTV->Release(); g_RTV = nullptr; }
            g_SwapChain->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, 0);
            ID3D11Texture2D* backBuffer = nullptr;
            g_SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer);
            g_Device->CreateRenderTargetView(backBuffer, nullptr, &g_RTV);
            backBuffer->Release();
        }
        return 0;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

void ex_program(int sig) 
{
	(void)sig;

	printf("recieved ctrl-c, exiting\n\n");

	timeEndPeriod(1);

	signal(SIGINT, SIG_DFL);
	exit(0);
}


int main(int argc, char *argv[])
{
	signal(SIGINT, ex_program);

	// Initialize data storage
	throttle_history.reserve(GRAPH_HISTORY_SIZE);
	brake_history.reserve(GRAPH_HISTORY_SIZE);
	abs_history.reserve(GRAPH_HISTORY_SIZE);

	// 1) Okno overlay (top-most, borderless, bottom half of screen)
	WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, _T("iRacingOverlay"), nullptr };
	RegisterClassEx(&wc);

	// Get screen dimensions for bottom half
	int screenWidth = GetSystemMetrics(SM_CXSCREEN);
	int screenHeight = GetSystemMetrics(SM_CYSCREEN);
	int halfHeight = screenHeight / 2;

	DWORD exStyle = WS_EX_TOPMOST | WS_EX_LAYERED; // WS_EX_TRANSPARENT toggle with F1
	HWND hwnd = CreateWindowEx(exStyle, wc.lpszClassName, _T("iRacing Overlay"),
		WS_POPUP, 0, halfHeight, screenWidth, halfHeight,
		nullptr, nullptr, wc.hInstance, nullptr);

	// Transparent background using color key (black = transparent)
	SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 0, LWA_COLORKEY);
	ShowWindow(hwnd, SW_SHOW);
	UpdateWindow(hwnd);

	// 2) DX11 + ImGui
	if (!CreateDeviceD3D(hwnd)) { CleanupDeviceD3D(); return 1; }
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.IniFilename = nullptr; // Disable ini file saving
	ImGui_ImplWin32_Init(hwnd);
	ImGui_ImplDX11_Init(g_Device, g_Context);
	ImGui::StyleColorsDark();

	// bump priority up so we get time from the sim
	SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
	timeBeginPeriod(1);

	// 3) Main loop
	bool clickThrough = false; // F1 toggle
	MSG msg{};
	while (msg.message != WM_QUIT) {
		while (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		
		// Toggle click-through with F1
		if (GetAsyncKeyState(VK_F1) & 1) {
			clickThrough = !clickThrough;
			LONG ex = GetWindowLong(hwnd, GWL_EXSTYLE);
			if (clickThrough) SetWindowLong(hwnd, GWL_EXSTYLE, ex | WS_EX_TRANSPARENT);
			else SetWindowLong(hwnd, GWL_EXSTYLE, ex & ~WS_EX_TRANSPARENT);
		}

		// Get iRacing data
		if (irsdkClient::instance().waitForData(16))
		{
			float throttle_val = Throttle.getFloat();
			float brake_val = Brake.getFloat();
			bool abs_active = BrakeABSactive.getBool();

			// Add to history (rolling buffer)
			if (throttle_history.size() >= GRAPH_HISTORY_SIZE)
			{
				throttle_history.erase(throttle_history.begin());
				brake_history.erase(brake_history.begin());
				abs_history.erase(abs_history.begin());
			}

			throttle_history.push_back(throttle_val);
			brake_history.push_back(brake_val);
			abs_history.push_back(abs_active);
		}

		ImGui_ImplDX11_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		// iRacing Data Overlay
		ImGui::SetNextWindowBgAlpha(0.9f);
		ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_FirstUseEver);
		ImGui::Begin("iRacing Data", nullptr,
			ImGuiWindowFlags_NoTitleBar);

		// Display current values
		ImGui::Text("Throttle: %.2f%%", throttle_history.empty() ? 0.0f : throttle_history.back() * 100.0f);
		ImGui::SameLine();
		ImGui::Text("   Brake: %.2f%%", brake_history.empty() ? 0.0f : brake_history.back() * 100.0f);
		ImGui::SameLine();
		ImGui::Text("   ABS: %s", abs_history.empty() ? "No" : (abs_history.back() ? "Yes" : "No"));
		ImGui::Text("F1: %s clicks", clickThrough ? "pass-through" : "capture");

		if (!throttle_history.empty() && !brake_history.empty())
		{
			// Calculate chart size based on available window space
			ImVec2 available = ImGui::GetContentRegionAvail();
			ImVec2 chart_size = ImVec2(available.x - 20, available.y - 20);
			
			// Custom plot overlay to show both throttle and brake
			if (ImGui::BeginChild("Chart", chart_size, true))
			{
				ImDrawList* draw_list = ImGui::GetWindowDrawList();
				ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
				ImVec2 canvas_size = ImGui::GetContentRegionAvail();
				
				// Draw chart background
				draw_list->AddRectFilled(canvas_pos, ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y), IM_COL32(20, 20, 20, 200));
				
				if (canvas_size.x > 0 && canvas_size.y > 0 && throttle_history.size() > 1)
				{
					float x_scale = canvas_size.x / (float)(throttle_history.size() - 1);
					
					// Draw throttle line (green)
					for (int i = 1; i < (int)throttle_history.size(); ++i)
					{
						ImVec2 p1 = ImVec2(canvas_pos.x + (i-1) * x_scale, canvas_pos.y + canvas_size.y - throttle_history[i-1] * canvas_size.y);
						ImVec2 p2 = ImVec2(canvas_pos.x + i * x_scale, canvas_pos.y + canvas_size.y - throttle_history[i] * canvas_size.y);
						draw_list->AddLine(p1, p2, IM_COL32(0, 255, 0, 255), 2.0f);
					}
					
					// Draw brake line (red/yellow based on ABS)
					for (int i = 1; i < (int)brake_history.size(); ++i)
					{
						ImVec2 p1 = ImVec2(canvas_pos.x + (i-1) * x_scale, canvas_pos.y + canvas_size.y - brake_history[i-1] * canvas_size.y);
						ImVec2 p2 = ImVec2(canvas_pos.x + i * x_scale, canvas_pos.y + canvas_size.y - brake_history[i] * canvas_size.y);
						
						bool current_abs = i < (int)abs_history.size() ? abs_history[i] : false;
						ImU32 brake_color = current_abs ? IM_COL32(255, 255, 0, 255) : IM_COL32(255, 0, 0, 255);
						draw_list->AddLine(p1, p2, brake_color, 2.0f);
					}
				}
			}
			ImGui::EndChild();
		}

		ImGui::End();

		ImGui::Render();
		const float clear[4] = {0,0,0,1}; // black background (transparent via color key)
		g_Context->OMSetRenderTargets(1, &g_RTV, nullptr);
		g_Context->ClearRenderTargetView(g_RTV, clear);
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
		g_SwapChain->Present(1, 0);
	}

	// 4) Cleanup
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
	CleanupDeviceD3D();
	DestroyWindow(hwnd);
	UnregisterClass(wc.lpszClassName, wc.hInstance);
	
	timeEndPeriod(1);
	return 0;
}
