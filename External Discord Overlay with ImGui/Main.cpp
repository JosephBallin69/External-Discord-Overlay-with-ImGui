#include "Rendering.h"

int main() {
	HWND TargetWindow = FindWindowA("UnrealWindow", 0);

	int ProcessID = 0;
	GetWindowThreadProcessId(TargetWindow, reinterpret_cast<LPDWORD>(&ProcessID));

	Rendering::Initialize(TargetWindow, ProcessID);

	bool exit = false;

	float Width = GetSystemMetrics(SM_CXSCREEN);
	float Height = GetSystemMetrics(SM_CYSCREEN);

	while (!exit) {
		Rendering::HandleInput();
		Rendering::BeginFrame();

		ImGui::ShowDemoWindow();

		Rendering::EndFrame(Width, Height);

		if (GetAsyncKeyState(VK_END) & 0x8000) exit = true;
	}

}