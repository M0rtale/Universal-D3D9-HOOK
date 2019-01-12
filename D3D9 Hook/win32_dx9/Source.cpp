#include <d3d9.h>
#include "pattern.h"
#include "detours.h"
#include "d3dx9.h"

using hook_fn = void(*)();

uintptr_t d3d9_device_new = 0;
uintptr_t return_address = 0;

ID3DXLine* d3dLine;
LPD3DXFONT pFont;

HWND gameWindow;
LPRECT gameWindowRect;

bool PressedKeys[256] = {};

// lpClassName = MSCTFIME U
//

//HOOKING
void HookFunction(PVOID *oFunction, PVOID pDetour)
{
	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	DetourAttach(oFunction, pDetour);
	DetourTransactionCommit();
}
void UnhookFunction(PVOID *oFunction, PVOID pDetour)
{
	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	DetourDetach(oFunction, pDetour);
	DetourTransactionCommit();
}

void DrawString(int x, int y, DWORD color, LPD3DXFONT g_pFont, const char *fmt, ...)
{
	char buf[1024] = { 0 };
	va_list va_alist;
	RECT FontPos = { x, y, x + 120, y + 16 };
	va_start(va_alist, fmt);
	vsprintf_s(buf, fmt, va_alist);
	va_end(va_alist);
	g_pFont->DrawTextA(NULL, buf, -1, &FontPos, DT_NOCLIP, color);
}

void DrawLine(float x, float y, float xx, float yy, D3DCOLOR color)
{
	D3DXVECTOR2 dLine[2];

	d3dLine->SetWidth(1.f);

	dLine[0].x = x;
	dLine[0].y = y;

	dLine[1].x = xx;
	dLine[1].y = yy;

	d3dLine->Draw(dLine, 2, color);

}

void DrawBox(float x, float y, float width, float height, D3DCOLOR color)
{
	D3DXVECTOR2 points[5];
	points[0] = D3DXVECTOR2(x, y);
	points[1] = D3DXVECTOR2(x + width, y);
	points[2] = D3DXVECTOR2(x + width, y + height);
	points[3] = D3DXVECTOR2(x, y + height);
	points[4] = D3DXVECTOR2(x, y);
	d3dLine->SetWidth(1);
	d3dLine->Draw(points, 5, color);
}

__declspec(naked) void cfg_hook()
{
	__asm {
		push eax
		mov eax, [esp + 4]
		cmp eax, [return_address] //; endscene return addr
		jne not_endscene
		mov[d3d9_device_new], ebx
		sub[d3d9_device_new], 4
		not_endscene:
		pop eax
			ret
	}
}

hook_fn patch(const uintptr_t address, hook_fn function)
{
	DWORD protection = 0;
	const auto original = hook_fn(*reinterpret_cast<uintptr_t*>(address));
	if (VirtualProtect(LPVOID(address), sizeof(uintptr_t), PAGE_READWRITE, &protection) == FALSE)
		return nullptr;
	*reinterpret_cast<uintptr_t*>(address) = uintptr_t(function);
	VirtualProtect(LPVOID(address), sizeof(uintptr_t), protection, &protection);
	return original;
}

inline void redirect_stdout()
{
	freopen_s(reinterpret_cast<FILE**>(stdout), "CONOUT$", "w", stdout);
}

uintptr_t get_return_address(const HMODULE d3d_module)
{
	constexpr auto return_addr_pattern = "8B CF FF D6 8B 4D 08 8B 41 3C FF 40 08 83 4D FC FF";

	MODULEINFO moduleinfo = { nullptr };
	if (GetModuleInformation(GetCurrentProcess(), d3d_module, &moduleinfo, sizeof(moduleinfo)) == FALSE)
		return 0;

	return uintptr_t(FindPattern(PBYTE(moduleinfo.lpBaseOfDll), moduleinfo.SizeOfImage, return_addr_pattern));
}

/* EndScene defs */
typedef long(__stdcall *EndScene_t)(IDirect3DDevice9*);
EndScene_t o_EndScene = nullptr;

/* BeginScene defs */
typedef long(__stdcall *BeginScene_t)(IDirect3DDevice9*);
BeginScene_t o_BeginScene = nullptr;

/* Present defs */
typedef long(__stdcall *Present_t)(IDirect3DDevice9*, RECT*, RECT*, HWND, RGNDATA*);
Present_t o_Present = nullptr;

/* Reset defs */
typedef long(__stdcall *Reset_t)(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*);
Reset_t o_Reset = nullptr;

/* WndProc defs*/
LONG_PTR o_WndProc = NULL;

static bool bOnce = false;

static int x = 50;
static int y = 50;

HRESULT __stdcall EndScene_h(IDirect3DDevice9 *pDevice)
{
	//printf("EndScene_h honk \n");
	pDevice->SetRenderState(D3DRS_COLORWRITEENABLE, 0xFFFFFFFF);

	DrawString(x, y, D3DCOLOR_ARGB(255, 255, 0, 0), pFont, "BOIHOOK");

	return o_EndScene(pDevice);
}

HRESULT __stdcall Reset_h(IDirect3DDevice9 *pDevice, D3DPRESENT_PARAMETERS *pPresentationParameters)
{
	//printf("Reset_h honk \n");
	return o_Reset(pDevice, pPresentationParameters);
}

HRESULT __stdcall Present_h(IDirect3DDevice9 *pDevice, RECT* pSourceRect, RECT* pDestRect, HWND hDestWindowOverride, RGNDATA* pDirtyRegion)
{
	return o_Present(pDevice, pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);
}

LRESULT __stdcall WndProc_h(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	//Helpers::Log("Wndproc called");

	switch (uMsg) // record window message
	{
	case WM_LBUTTONDOWN:
		 PressedKeys[VK_LBUTTON] = true;
		break;
	case WM_LBUTTONUP:
		 PressedKeys[VK_LBUTTON] = false;
		break;
	case WM_RBUTTONDOWN:
		 PressedKeys[VK_RBUTTON] = true;
		break;
	case WM_RBUTTONUP:
		 PressedKeys[VK_RBUTTON] = false;
		break;
	case WM_MBUTTONDOWN:
		 PressedKeys[VK_MBUTTON] = true;
		break;
	case WM_MBUTTONUP:
		 PressedKeys[VK_MBUTTON] = false;
		break;
	case WM_XBUTTONDOWN:
	{
		UINT button = GET_XBUTTON_WPARAM(wParam);
		if (button == XBUTTON1)
		{
			 PressedKeys[VK_XBUTTON1] = true;
		}
		else if (button == XBUTTON2)
		{
			 PressedKeys[VK_XBUTTON2] = true;
		}
		break;
	}
	case WM_XBUTTONUP:
	{
		UINT button = GET_XBUTTON_WPARAM(wParam);
		if (button == XBUTTON1)
		{
			 PressedKeys[VK_XBUTTON1] = false;
		}
		else if (button == XBUTTON2)
		{
			 PressedKeys[VK_XBUTTON2] = false;
		}
		break;
	}
	case WM_KEYDOWN:
		 PressedKeys[wParam] = true;
		break;
	case WM_KEYUP:
		 PressedKeys[wParam] = false;
		break;
	default:
		break;
	}

	return CallWindowProcW((WNDPROC)o_WndProc, hWnd, uMsg, wParam, lParam);
}

HRESULT __stdcall BeginScene_h(IDirect3DDevice9* pDevice)
{
	//printf("BeginScene_h honk \n");
	//setup fonts, and lines
	D3DXCreateFontA(pDevice, 25, 0, FW_HEAVY, 1, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Arial", &pFont);
	D3DXCreateLine(pDevice, &d3dLine);

	if (!bOnce)
	{
		D3DDEVICE_CREATION_PARAMETERS pPara;
		pDevice->GetCreationParameters(&pPara);
		gameWindow = pPara.hFocusWindow;

		//hook wndproc and window size
		o_WndProc = SetWindowLongPtr(gameWindow, GWLP_WNDPROC, (LONG_PTR)WndProc_h);
		GetWindowRect(gameWindow, gameWindowRect);

		char gameName[256];
		GetClassName(gameWindow, gameName, sizeof(gameName - 2));
		printf("%s\n", gameName);
		bOnce = true;
	}

	return o_BeginScene(pDevice);
}

uintptr_t guard_function;
hook_fn original_fn;

bool find_device(const HMODULE d3d_module)
{
	// get cfg return address
	return_address = get_return_address(d3d_module);

	if (return_address == 0) {
		printf("could not find return address\n");
		return false;
	}

	printf("return address: 0x%08X\n", return_address);

	if (ImageNtHeader(d3d_module) == nullptr)
	{
		printf("d3d9 module is not a valid pe image!\n");
		return false;
	}

	// get load config directory entry
	ULONG directory_size = 0;
	const auto load_config = PIMAGE_LOAD_CONFIG_DIRECTORY(ImageDirectoryEntryToData(d3d_module, TRUE, IMAGE_DIRECTORY_ENTRY_LOAD_CONFIG, &directory_size));

	if (load_config == nullptr)
	{
		printf("could not find IMAGE_DIRECTORY_ENTRY_LOAD_CONFIG\n");
		return false;
	}

	printf("load config: 0x%p\n", load_config);

	guard_function = load_config->GuardCFCheckFunctionPointer;

	printf("guard function: 0x%08X\n", guard_function);

	// replace original function with ours
	original_fn = patch(guard_function, cfg_hook);

	if (original_fn == nullptr)
	{
		printf("could not patch GuardCFCheckFunctionPointer\n");
		return false;
	}

	printf("original function: 0x%p\n", original_fn);

	while (true)
	{
		if (HIBYTE(GetAsyncKeyState(VK_DELETE)))
		{
			break;
		}

		// wait until d3d device is found
		if (d3d9_device_new != 0)
		{
			printf("d3d9 device: 0x%I32X\n", d3d9_device_new);

			DWORD_PTR* pDeviceVT = reinterpret_cast<DWORD_PTR*>(d3d9_device_new);
			pDeviceVT = reinterpret_cast<DWORD_PTR*>(pDeviceVT[0]);

			IDirect3DDevice9* ref = nullptr;

			o_EndScene = reinterpret_cast<EndScene_t>(pDeviceVT[42]);
			o_BeginScene = reinterpret_cast<BeginScene_t>(pDeviceVT[41]);
			o_Reset = reinterpret_cast<Reset_t>(pDeviceVT[16]);
			o_Present = reinterpret_cast<Present_t>(pDeviceVT[17]);


			HookFunction(reinterpret_cast<PVOID*>(&o_Reset), Reset_h);
			HookFunction(reinterpret_cast<PVOID*>(&o_Present), Present_h);
			HookFunction(reinterpret_cast<PVOID*>(&o_BeginScene), BeginScene_h);
			HookFunction(reinterpret_cast<PVOID*>(&o_EndScene), EndScene_h);

			Beep(650, 50);
			break;
		}

		using namespace std::chrono_literals;
		std::this_thread::sleep_for(50ms);
	}

	// restor original function
	//patch(guard_function, original_fn);

	return true;
}

DWORD WINAPI initialize(void* mod)
{

	const auto current_module = static_cast<HMODULE>(mod);
	const auto d3d9_module = GetModuleHandle(TEXT("d3d9.dll"));
	// don't allocate a console if there's already one
	const auto allocate_console = GetConsoleWindow() == nullptr;

	if (allocate_console) {
		AllocConsole();
		redirect_stdout();
	}

	if (d3d9_module != nullptr) {
		if (find_device(d3d9_module))
			printf("success\n");
		else
			printf("fail!\n");
	}
	else
	{
		printf("could not find d3d9 module\n");
	}

	while (!GetAsyncKeyState(VK_DELETE))
	{
		Sleep(5);
	}

	using namespace std::chrono_literals;
	std::this_thread::sleep_for(500ms);

	if (allocate_console) {
		redirect_stdout();
		FreeConsole();
	}

	UnhookFunction(reinterpret_cast<PVOID*>(&o_EndScene), EndScene_h);
	UnhookFunction(reinterpret_cast<PVOID*>(&o_Reset), Reset_h);
	UnhookFunction(reinterpret_cast<PVOID*>(&o_Present), Present_h);
	UnhookFunction(reinterpret_cast<PVOID*>(&o_BeginScene), BeginScene_h);

	patch(guard_function, original_fn);

	// unload
	FreeLibraryAndExitThread(current_module, 0);
	//return 0;
}

// ReSharper disable once CppInconsistentNaming
BOOL WINAPI DllMain(const HMODULE mod, const ULONG reason, PVOID)
{
	if (reason == DLL_PROCESS_ATTACH)
	{
		const auto thread = CreateThread(nullptr, 0, initialize, mod, 0, nullptr);
		CloseHandle(thread);
	}
	return TRUE;
}