#include "stdafx.h"
#include "Window.h"

/**
 * Windows message loop.
 */
LRESULT CALLBACK WndProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam ) {
	PAINTSTRUCT ps;
    HDC hdc;

    switch( message ) {
        case WM_PAINT:
            hdc = BeginPaint( hWnd, &ps );
            EndPaint( hWnd, &ps );
            break;
        case WM_DESTROY:
            PostQuitMessage( 0 );
            break;
        default:
            return DefWindowProc( hWnd, message, wParam, lParam );
    }
    return 0;
}

namespace Window
{

/**
 * Create a new window.
*/
HRESULT Create( LONG width, LONG height, HINSTANCE& instance, HWND& window, LPCWSTR title ) {
    
	// Register the window class
	WNDCLASSEX wcex;
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = instance;
	wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = NULL;
	wcex.lpszClassName = L"WindowClass";
	wcex.hIcon = nullptr;
	wcex.hIconSm = nullptr;

	if (!RegisterClassEx(&wcex)) {
		throw std::runtime_error("Failed to register window!");
	}

	// Get the desktop resolution
	RECT desktop;
	const HWND hDesktop = GetDesktopWindow();
	GetWindowRect(hDesktop, &desktop);

	int x = (desktop.right - width) / 2;
	int y = (desktop.bottom - height) / 2;

	// Create the window
	RECT rc = { 0, 0, width, height };
	AdjustWindowRect( &rc, WS_OVERLAPPEDWINDOW, FALSE );
	window = CreateWindow(wcex.lpszClassName, title, WS_OVERLAPPEDWINDOW, x, 0, (rc.right - rc.left), (rc.bottom - rc.top), NULL, NULL, instance, NULL);
	if (!window) return E_FAIL;

	// Set the window icon
	HANDLE hIcon = LoadImageA(GetModuleHandle(NULL), "nvidia.ico", IMAGE_ICON, 0, 0, LR_DEFAULTSIZE | LR_LOADFROMFILE);
	SendMessage(window, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
	
	// Show the window
	ShowWindow(window, SW_SHOWDEFAULT);
	UpdateWindow(window);

	return S_OK;
}

}