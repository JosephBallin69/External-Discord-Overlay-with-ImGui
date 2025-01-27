#pragma once
#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_win32.h"
#include "ImGui/imgui_impl_dx11.h"

#include <d3d11.h>
#include <stdexcept>
#include <string>

#pragma comment(lib, "d3d11.lib")

namespace Rendering {
    namespace Core {

        ID3D11Device* Device = nullptr;
        ID3D11DeviceContext* Context = nullptr;
        IDXGISwapChain* SwapChain = nullptr;
        ID3D11RenderTargetView* RenderTargetView = nullptr;

        HANDLE DiscordFileMapping = nullptr;
        void* DiscordMappedAddress = nullptr;
        UINT FrameCount = 0;

        struct DiscordHeader {
            UINT Magic;
            UINT FrameCount;
            UINT NoClue;
            UINT Width;
            UINT Height;
            BYTE Buffer[1]; // BGRA8 format
        };

        void Initialize(HWND hWnd, UINT discordTargetProcessId) {
            // Set up DirectX 11
            DXGI_SWAP_CHAIN_DESC sd;
            ZeroMemory(&sd, sizeof(sd));
            sd.BufferCount = 2;
            sd.BufferDesc.Width = 0;
            sd.BufferDesc.Height = 0;
            sd.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
            sd.BufferDesc.RefreshRate.Numerator = 60;
            sd.BufferDesc.RefreshRate.Denominator = 1;
            sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
            sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
            sd.OutputWindow = hWnd;
            sd.SampleDesc.Count = 1;
            sd.SampleDesc.Quality = 0;
            sd.Windowed = TRUE;
            sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

            if (FAILED(D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &sd, &SwapChain, &Device, nullptr, &Context))) {
                throw std::runtime_error("Failed to initialize DirectX 11 device and swap chain.");
            }

            ID3D11Texture2D* backBuffer = nullptr;
            SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer);
            Device->CreateRenderTargetView(backBuffer, nullptr, &RenderTargetView);
            backBuffer->Release();

            ImGui::CreateContext();
            ImGui_ImplWin32_Init(hWnd);
            ImGui_ImplDX11_Init(Device, Context);

            //SetupStyle();

            std::string mappedFilename = "DiscordOverlay_Framebuffer_Memory_" + std::to_string(discordTargetProcessId);
            DiscordFileMapping = OpenFileMappingA(FILE_MAP_ALL_ACCESS, false, mappedFilename.c_str());
            if (!DiscordFileMapping) {
                throw std::runtime_error("Failed to connect to Discord's framebuffer.");
            }

            DiscordMappedAddress = MapViewOfFile(DiscordFileMapping, FILE_MAP_ALL_ACCESS, 0, 0, 0);
            if (!DiscordMappedAddress) {
                CloseHandle(DiscordFileMapping);
                throw std::runtime_error("Failed to map Discord's framebuffer.");
            }
        }

        void Cleanup() {
            ImGui_ImplDX11_Shutdown();
            ImGui::DestroyContext();

            if (RenderTargetView) { RenderTargetView->Release(); RenderTargetView = nullptr; }
            if (SwapChain) { SwapChain->Release(); SwapChain = nullptr; }
            if (Context) { Context->Release(); Context = nullptr; }
            if (Device) { Device->Release(); Device = nullptr; }

            if (DiscordMappedAddress) {
                UnmapViewOfFile(DiscordMappedAddress);
                DiscordMappedAddress = nullptr;
            }
            if (DiscordFileMapping) {
                CloseHandle(DiscordFileMapping);
                DiscordFileMapping = nullptr;
            }
        }

        void SendToDiscord(UINT width, UINT height) {
            if (!DiscordMappedAddress)
                return;

            auto* header = static_cast<DiscordHeader*>(DiscordMappedAddress);
            header->Width = width;
            header->Height = height;

            ID3D11Device* pDevice;
            SwapChain->GetDevice(__uuidof(ID3D11Device), reinterpret_cast<void**>(&pDevice));

            ID3D11DeviceContext* pContext;
            pDevice->GetImmediateContext(&pContext);

            ID3D11Texture2D* pBackBuffer;
            SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&pBackBuffer));

            D3D11_TEXTURE2D_DESC desc;

            pBackBuffer->GetDesc(&desc);
            D3D11_TEXTURE2D_DESC txtDesc;
            pBackBuffer->GetDesc(&txtDesc);
            txtDesc.Usage = D3D11_USAGE_STAGING;
            txtDesc.BindFlags = 0;
            txtDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

            ID3D11Texture2D* pBackBufferStaging;
            HRESULT status = pDevice->CreateTexture2D(&txtDesc, nullptr, &pBackBufferStaging);
            pContext->CopyResource(pBackBufferStaging, pBackBuffer);


            D3D11_MAPPED_SUBRESOURCE mappedResource;
            status = Context->Map(pBackBufferStaging, 0, D3D11_MAP_READ, 0, &mappedResource);

            memcpy(header->Buffer, mappedResource.pData, width * height * 4); 
            header->FrameCount = ++FrameCount;

            Context->Unmap(pBackBufferStaging, 0);
            pBackBufferStaging->Release();
            pBackBuffer->Release();
        }

        void BeginFrame() {
            ImGui_ImplDX11_NewFrame();
            ImGui_ImplWin32_NewFrame();
            ImGui::NewFrame();

        }

        void EndFrame(UINT width, UINT height) {
            ImGui::Render();

            Context->OMSetRenderTargets(1, &RenderTargetView, nullptr);
            float clearColor[4] = { 0.1f, 0.1f, 0.1f, 0.0f };
            Context->ClearRenderTargetView(RenderTargetView, clearColor);

            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

            SwapChain->Present(1, 0);
            SendToDiscord(width, height);
        }


    }

    void HandleInput() {
        ImGuiIO& io = ImGui::GetIO();
        POINT mousePos;
        GetCursorPos(&mousePos);
        io.MousePos = ImVec2(mousePos.x, mousePos.y);

        io.MouseDown[0] = (GetKeyState(VK_LBUTTON) & 0x8000) != 0;
        io.MouseDown[1] = (GetKeyState(VK_RBUTTON) & 0x8000) != 0;

    }

    void Initialize(HWND hWnd, UINT discordTargetProcessId) {
        Core::Initialize(hWnd, discordTargetProcessId);
    }

    void Cleanup() {
        Core::Cleanup();
    }

    void BeginFrame() {
        Core::BeginFrame();
    }

    void EndFrame(UINT width, UINT height) {
        Core::EndFrame(width, height);
    }

    void DrawMenu() {
        ImGui::ShowDemoWindow();
    }

    void DrawOverlay() {

    }
}