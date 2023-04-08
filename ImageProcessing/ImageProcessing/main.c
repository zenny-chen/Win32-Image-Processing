﻿// ImageProcessing.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define _USE_MATH_DEFINES
#include <math.h>

#include <Windows.h>
#include <CommCtrl.h>
#include <wincodecsdk.h>


static const char s_appName[] = "Image Processing";
static HWND s_browseButton = NULL;
static HWND s_imagePathEdit = NULL;
static HWND s_loadImageButton = NULL;
static HBITMAP s_hBitmap = NULL;
static HDC s_hMemDC = NULL;

static inline void ClearBitmapResources(void)
{
    if (s_hBitmap != NULL)
    {
        DeleteObject(s_hBitmap);
        s_hBitmap = NULL;
    }
    if (s_hMemDC != NULL)
    {
        DeleteDC(s_hMemDC);
        s_hMemDC = NULL;
    }
}

enum
{
    WINDOW_WIDTH = 800,
    WINDOW_HEIGHT = 640
};

// Window message process procedure
static LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
    {
        RECT windowRect;
        GetWindowRect(hWnd, &windowRect);
        //SetWindowLongA(hWnd, GWL_STYLE, GetWindowLongA(hWnd, GWL_STYLE) & ~WS_MINIMIZEBOX);
        SetWindowLongA(hWnd, GWL_STYLE, GetWindowLongA(hWnd, GWL_STYLE) & ~WS_MAXIMIZEBOX);
        SetWindowLongA(hWnd, GWL_STYLE, GetWindowLongA(hWnd, GWL_STYLE) & ~WS_SIZEBOX);
        break;
    }

    case WM_CLOSE:
        PostQuitMessage(0);
        break;

    case WM_COMMAND:
    {
        const DWORD wmId = LOWORD(wParam);
        HINSTANCE hInstance = GetModuleHandleA(NULL);

        if ((HWND)lParam == s_browseButton)
        {
            const DWORD notifCode = HIWORD(wParam);
            if (notifCode == BN_CLICKED)
            {
                char filePath[MAX_PATH] = { '\0' };
                char fileTitle[MAX_PATH] = { '\0' };

                OPENFILENAMEA ofn = {
                    .lStructSize = sizeof(ofn),
                    .hwndOwner = hWnd,
                    .hInstance = hInstance,
                    .lpstrFilter = "Image File(*.bmp,*.tif,*.tiff)\0*.bmp;*.tif;*.tiff;\0\0",
                    .nFilterIndex = 1,
                    .lpstrFile = filePath,
                    .nMaxFile = sizeof(filePath),
                    .lpstrFileTitle = fileTitle,
                    .nMaxFileTitle = sizeof(fileTitle),
                    .Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_EXPLORER
                };
                if (GetOpenFileNameA(&ofn))
                {
                    SetWindowTextA(s_imagePathEdit, filePath);
                    UpdateWindow(hWnd);
                }
            }
        }
        else if ((HWND)lParam == s_loadImageButton)
        {
            const DWORD notifCode = HIWORD(wParam);
            if (notifCode == BN_CLICKED)
            {
                char filePath[MAX_PATH] = { '\0' };
                int len = GetWindowTextA(s_imagePathEdit, filePath, sizeof(filePath));
                if (len < 5 || len >= MAX_PATH) break;

                char* dotPos = strrchr(filePath, '.');
                if (dotPos == NULL || strlen(dotPos) > 7) break;

                enum { IMAGE_TYPE_NONE, IMAGE_TYPE_BMP, IMAGE_TYPE_TIFF } imageType = IMAGE_TYPE_NONE;

                if (strcmp(dotPos, ".bmp") == 0) {
                    imageType = IMAGE_TYPE_BMP;
                }
                else if (strcmp(dotPos, ".tif") == 0 || strcmp(dotPos, ".tiff") == 0) {
                    imageType = IMAGE_TYPE_TIFF;
                }

                if (imageType == IMAGE_TYPE_NONE) break;

                ClearBitmapResources();

                if (imageType == IMAGE_TYPE_BMP) {
                    s_hBitmap = LoadImageA(NULL, filePath, IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
                }
                else
                {
                    // Transfer ASCII-based file path to wide-char string buffer
                    WCHAR filePathW[MAX_PATH] = { L'\0' };
                    const int wideStrLen = MultiByteToWideChar(CP_ACP, 0, filePath, -1, NULL, 0);
                    MultiByteToWideChar(CP_ACP, 0, filePath, -1, filePathW, wideStrLen);

                    IWICImagingFactory2* pFactory = NULL;
                    IWICBitmapDecoder* pBitmapDecoder = NULL;
                    IWICBitmapFrameDecode* pBitmapFrame = NULL;
                    BYTE* pixelBuffer = NULL;
                    do
                    {
                        // Create the COM imaging factory
                        HRESULT hr = CoCreateInstance(
                            &CLSID_WICImagingFactory2,
                            NULL,
                            CLSCTX_INPROC_SERVER,
                            &IID_IWICImagingFactory2,
                            &pFactory);
                        if (hr != S_OK) break;

                        hr = pFactory->lpVtbl->CreateDecoderFromFilename(pFactory, filePathW, NULL, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &pBitmapDecoder);
                        if (hr != S_OK) break;

                        UINT frameCount = 0;
                        hr = pBitmapDecoder->lpVtbl->GetFrameCount(pBitmapDecoder, &frameCount);
                        if (hr != S_OK) break;

                        // Create the decoded bitmap frame
                        hr = pBitmapDecoder->lpVtbl->GetFrame(pBitmapDecoder, frameCount - 1, &pBitmapFrame);
                        if (hr != S_OK) break;

                        UINT width = 0, height = 0;
                        hr = pBitmapFrame->lpVtbl->GetSize(pBitmapFrame, &width, &height);
                        if (hr != S_OK) break;

                        // Get the frame pixel format
                        WICPixelFormatGUID pixelFormat = { 0 };
                        hr = pBitmapFrame->lpVtbl->GetPixelFormat(pBitmapFrame, &pixelFormat);
                        if (hr != S_OK) break;

                        UINT nComponents = 0;
                        if (IsEqualGUID(&pixelFormat, &GUID_WICPixelFormat24bppBGR) || IsEqualGUID(&pixelFormat, &GUID_WICPixelFormat24bppRGB)) {
                            nComponents = 3;
                        }
                        else if (IsEqualGUID(&pixelFormat, &GUID_WICPixelFormat32bppRGBA) || IsEqualGUID(&pixelFormat, &GUID_WICPixelFormat32bppBGRA)) {
                            nComponents = 4;
                        }
                        else
                        {
                            MessageBoxA(hWnd, "The pixel format is not supported!", "Attention", MB_OK);
                            break;
                        }

                        // Create the pixel buffer and copy the pixels
                        const UINT nBytesPerRow = width * nComponents;
                        const UINT bufferSize = nBytesPerRow * height;
                        pixelBuffer = calloc(bufferSize, 1);
                        hr = pBitmapFrame->lpVtbl->CopyPixels(pBitmapFrame, NULL, nBytesPerRow, bufferSize, pixelBuffer);
                        if (hr != S_OK) break;

                        // Create the BITMAP instance
                        s_hBitmap = CreateBitmap(width, height, 1U, nComponents * 8U, pixelBuffer);
                    }
                    while (false);

                    if (pixelBuffer != NULL) {
                        free(pixelBuffer);
                    }
                    if (pBitmapFrame != NULL) {
                        pBitmapFrame->lpVtbl->Release(pBitmapFrame);
                    }
                    if (pBitmapDecoder != NULL) {
                        pBitmapDecoder->lpVtbl->Release(pBitmapDecoder);
                    }
                    if (pFactory != NULL) {
                        pFactory->lpVtbl->Release(pFactory);
                    }
                }
            }
        }
    }
    break;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        // TODO: 在此处添加使用 hdc 的任何绘图代码...
        if (s_hBitmap != NULL)
        {
            if (s_hMemDC == NULL) {
                s_hMemDC = CreateCompatibleDC(hdc);
            }
            
            HBITMAP hOldBitmap = SelectObject(s_hMemDC, s_hBitmap);

            BITMAP bmp = { 0 };
            GetObject(s_hBitmap, sizeof(BITMAP), &bmp);
            const int x = (WINDOW_WIDTH - bmp.bmWidth) / 2;
            const int y = (WINDOW_HEIGHT - bmp.bmHeight) / 2;
            BitBlt(hdc, x, y, bmp.bmWidth, bmp.bmHeight, s_hMemDC, 0, 0, SRCCOPY);

            SelectObject(s_hMemDC, hOldBitmap);
            RECT rect = { 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT };
            InvalidateRect(hWnd, &rect, TRUE);
            UpdateWindow(hWnd);
        }

        EndPaint(hWnd, &ps);
    }

    case WM_ERASEBKGND:
        return 1;

    case WM_SIZE:
        UpdateWindow(hWnd);
        break;

    case WM_KEYDOWN:
        switch (wParam)
        {
        case VK_ESCAPE:
            PostQuitMessage(0);
            break;
        case VK_LEFT:
            break;
        case VK_RIGHT:
            break;
        case VK_SPACE:
            break;
        }
        return 0;

    default:
        break;
    }

    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

// Initialize the current app instance and return the handle of the window created.
static HWND InitInstance(void)
{
    HINSTANCE hInstance = GetModuleHandleA(NULL);

    WNDCLASSEXA win_class;
    // Initialize the window class structure:
    win_class.cbSize = sizeof(WNDCLASSEX);
    win_class.style = CS_HREDRAW | CS_VREDRAW;
    win_class.lpfnWndProc = WndProc;
    win_class.cbClsExtra = 0;
    win_class.cbWndExtra = 0;
    win_class.hInstance = hInstance;
    win_class.hIcon = LoadIconA(NULL, IDI_APPLICATION);
    win_class.hCursor = LoadCursorA(NULL, IDC_ARROW);
    win_class.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
    win_class.lpszMenuName = NULL;
    win_class.lpszClassName = s_appName;
    win_class.hIconSm = LoadIconA(NULL, IDI_WINLOGO);
    // Register window class:
    if (!RegisterClassExA(&win_class))
    {
        // It didn't work, so try to give a useful error:
        puts("Unexpected error trying to start the application!\n");
        exit(1);
    }

    // Create window with the registered class:
    RECT wr = { 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT };
    AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);

    HWND hWnd = CreateWindowExA(
        0,                                                      // extra style
        s_appName,                                              // class name
        s_appName,                                              // app name
        WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_SYSMENU,              // window style
        CW_USEDEFAULT, CW_USEDEFAULT,                               // x, y coords
        WINDOW_WIDTH,                                               // width
        WINDOW_HEIGHT,                                              // height
        NULL,                                                   // handle to parent
        NULL,                                                       // handle to menu
        hInstance,                                              // hInstance
        NULL);

    if (hWnd == NULL)
    {
        // It didn't work, so try to give a useful error:
        puts("Cannot create a window in which to draw!");
        exit(-1);
    }

    return hWnd;
}

int main(int argc, const char* argv[])
{
    const HRESULT res = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (res != S_OK)
    {
        puts("The COM library was not initialized successfully!");
        return -1;
    }

    HWND hWnd = InitInstance();

    puts("Initializing the app...");

    // Initialize other controls
    HINSTANCE hInstance = GetModuleHandleA(NULL);

    s_browseButton = CreateWindowExA(
        0,
        WC_BUTTON,          // Predefined class;
        "Browse",        // Button text 
        WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,  // Styles 
        10,                         // x position 
        10,                         // y position 
        70,                     // Button width
        30,                     // Button height
        hWnd,                   // Parent window
        NULL,                   // No menu.
        hInstance,
        NULL);                  // Pointer not needed.

    s_imagePathEdit = CreateWindowExA(
        0,
        WC_EDIT,
        "",
        WS_VISIBLE | WS_CHILD | WS_BORDER | ES_LEFT | ES_AUTOHSCROLL | SS_CENTERIMAGE,
        90,
        12,
        400,
        26,
        hWnd,
        NULL,
        hInstance,
        NULL);

    s_loadImageButton = CreateWindowExA(
        0,
        WC_BUTTON,          // Predefined class;
        "Load Image",       // Button text 
        WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,  // Styles 
        500,                        // x position 
        10,                         // y position 
        90,                     // Button width
        30,                     // Button height
        hWnd,                   // Parent window
        NULL,                   // No menu.
        hInstance,
        NULL);                  // Pointer not needed.

    UpdateWindow(hWnd);

    // main message loop
    MSG msg = { 0 };
    bool done = false;

    while (!done)
    {
        PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE);

        // check for a quit message
        if (msg.message == WM_QUIT) {
            done = true;  // if found, quit app
        }
        else {
            // Translate and dispatch to event queue
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
        RedrawWindow(hWnd, NULL, NULL, RDW_INTERNALPAINT);
    }

    ClearBitmapResources();

    if (hWnd != NULL)
    {
        DestroyWindow(hWnd);
        hWnd = NULL;
    }

    CoUninitialize();
}

// 运行程序: Ctrl + F5 或调试 >“开始执行(不调试)”菜单
// 调试程序: F5 或调试 >“开始调试”菜单

// 入门使用技巧: 
//   1. 使用解决方案资源管理器窗口添加/管理文件
//   2. 使用团队资源管理器窗口连接到源代码管理
//   3. 使用输出窗口查看生成输出和其他消息
//   4. 使用错误列表窗口查看错误
//   5. 转到“项目”>“添加新项”以创建新的代码文件，或转到“项目”>“添加现有项”以将现有代码文件添加到项目
//   6. 将来，若要再次打开此项目，请转到“文件”>“打开”>“项目”并选择 .sln 文件
