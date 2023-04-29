// ImageProcessing.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
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

#include "cross_complex.h"

static const char s_appName[] = "Image Processing";
static HWND s_browseButton = NULL;
static HWND s_imagePathEdit = NULL;
static HWND s_loadImageButton = NULL;
static HWND s_generateImageButton = NULL;
static HWND s_optionComboBox = NULL;
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
    WINDOW_WIDTH = 960,
    WINDOW_HEIGHT = 640
};

// Open the file browser dialog
static void BrowseFileOpenDialog(HINSTANCE hInstance, HWND hWnd)
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

// Load an image file (Currently supports BMP and TIFF)
static void LoadImageFile(HWND hWnd)
{
    char filePath[MAX_PATH] = { '\0' };
    int len = GetWindowTextA(s_imagePathEdit, filePath, sizeof(filePath));
    if (len < 5 || len >= MAX_PATH) return;

    char* dotPos = strrchr(filePath, '.');
    if (dotPos == NULL || strlen(dotPos) > 7) return;

    enum { IMAGE_TYPE_NONE, IMAGE_TYPE_BMP, IMAGE_TYPE_TIFF } imageType = IMAGE_TYPE_NONE;

    if (strcmp(dotPos, ".bmp") == 0) {
        imageType = IMAGE_TYPE_BMP;
    }
    else if (strcmp(dotPos, ".tif") == 0 || strcmp(dotPos, ".tiff") == 0) {
        imageType = IMAGE_TYPE_TIFF;
    }

    // Image type not supported
    if (imageType == IMAGE_TYPE_NONE) return;

    ClearBitmapResources();

    if (imageType == IMAGE_TYPE_BMP) {
        s_hBitmap = LoadImageA(NULL, filePath, IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
    }
    else
    {
        // ==== Use WIC to load a TIFF image file ====
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

// Clear previous bitmap using the background (white) color
static void ClearPreviousBitmap(void)
{
    if (s_hBitmap == NULL) return;

    // Get the current bitmap size
    BITMAP bmp = { 0 };
    GetObjectA(s_hBitmap, sizeof(BITMAP), &bmp);

    ClearBitmapResources();

    // BITMAP uses BGRA8888 format by default,
    // So each pixel occupies 4 bytes
    unsigned* pixelBuffer = calloc(bmp.bmWidth * bmp.bmHeight, 4);
    if (pixelBuffer == NULL) return;

    for (int row = 0; row < bmp.bmHeight; ++row)
    {
        const unsigned color = 0xffff'ffffU;
        for (int col = 0; col < bmp.bmWidth; ++col) {
            pixelBuffer[row * bmp.bmWidth + col] = color;
        }
    }

    // Create the new BITMAP instance
    s_hBitmap = CreateBitmap(bmp.bmWidth, bmp.bmHeight, 1U, 4U * 8U, pixelBuffer);

    free(pixelBuffer);
}

// Generate a custom bitmap (with red, green and blue strips)
static void GenerateCustomBitmap(void)
{
    ClearBitmapResources();

    const int width = 480, height = 480;
    // BITMAP uses BGRA8888 format by default,
    // So each pixel occupies 4 bytes
    unsigned* pixelBuffer = calloc(width * height, 4);
    if (pixelBuffer == NULL) return;

    for (int row = 0; row < height; ++row)
    {
        unsigned color = 0;
        if (row < 480 / 3) {
            color = 0xfff01010U; // Red
        }
        else if (row < 480 * 2 / 3) {
            color = 0xff10f010U; // Green
        }
        else {
            color = 0xff1010f0U; // Blue
        }

        for (int col = 0; col < width; ++col) {
            pixelBuffer[row * width + col] = color;
        }
    }
    
    // Create the BITMAP instance
    s_hBitmap = CreateBitmap(width, height, 1U, 4U * 8U, pixelBuffer);

    free(pixelBuffer);
}

// Color to gray operation
static void ColorToGrayTransformBitmap(void)
{
    if (s_hBitmap == NULL) return;

    // Get the current bitmap size
    BITMAP bmp = { 0 };
    GetObjectA(s_hBitmap, sizeof(BITMAP), &bmp);

    const int orgWidth = (int)bmp.bmWidth;
    const int orgHeight = (int)bmp.bmHeight;
    const int orgComponents = (int)(bmp.bmBitsPixel / 8);
    const int orgWidthBytes = (int)bmp.bmWidthBytes;
    const int residuleWidthBytes = orgWidthBytes - orgWidth * orgComponents;
    const int orgImageBufferSize = orgWidthBytes * orgHeight;

    uint8_t* orgImageData = calloc(orgImageBufferSize, 1);
    if (orgImageData == NULL) exit(-1);
    // Read the raw data from the given HBITMAP object
    if (GetBitmapBits(s_hBitmap, orgImageBufferSize, orgImageData) == 0)
    {
        puts("Read original image data failed in `ColorToGrayTransformBitmap`!");
        free(orgImageData);
        return;
    }

    ClearBitmapResources();

    // The destination BITMAP uses BGRA8888 format,
    // So each pixel occupies 4 bytes
    uint8_t* dstImageData = calloc(orgWidth * orgHeight, 4);
    if (dstImageData == NULL) exit(-1);

    int orgIndex = 0, dstIndex = 0;
    for (int row = 0; row < orgHeight; ++row)
    {
        for (int col = 0; col < orgWidth; ++col)
        {
            const unsigned b = orgImageData[orgIndex + 0];
            const unsigned g = orgImageData[orgIndex + 1];
            const unsigned r = orgImageData[orgIndex + 2];
            
            // Apply: I = 0.3B + 0.59G + 0.11R
            const unsigned y = (unsigned)(0.3f * b + 0.59f * g + 0.11f * r);

            dstImageData[dstIndex++] = y;       // b
            dstImageData[dstIndex++] = y;       // g
            dstImageData[dstIndex++] = y;       // r
            dstImageData[dstIndex++] = 255U;    // a

            orgIndex += orgComponents;
        }

        orgIndex += residuleWidthBytes;
    }

    // Create the new BITMAP instance
    s_hBitmap = CreateBitmap(orgWidth, orgHeight, 1U, 4U * 8U, dstImageData);

    free(orgImageData);
    free(dstImageData);
}

// Reverse Pixel operation
static void ReversePixelBitmap(void)
{
    if (s_hBitmap == NULL) return;

    // Get the current bitmap size
    BITMAP bmp = { 0 };
    GetObjectA(s_hBitmap, sizeof(BITMAP), &bmp);

    const int orgWidth = (int)bmp.bmWidth;
    const int orgHeight = (int)bmp.bmHeight;
    const int orgComponents = (int)(bmp.bmBitsPixel / 8);
    const int orgWidthBytes = (int)bmp.bmWidthBytes;
    const int residuleWidthBytes = orgWidthBytes - orgWidth * orgComponents;
    const int orgImageBufferSize = orgWidthBytes * orgHeight;

    uint8_t* orgImageData = calloc(orgImageBufferSize, 1);
    if (orgImageData == NULL) exit(-1);
    // Read the raw data from the given HBITMAP object
    if (GetBitmapBits(s_hBitmap, orgImageBufferSize, orgImageData) == 0)
    {
        puts("Read original image data failed in `ColorToGrayTransformBitmap`!");
        free(orgImageData);
        return;
    }

    ClearBitmapResources();

    // The destination BITMAP uses BGRA8888 format,
    // So each pixel occupies 4 bytes
    uint8_t* dstImageData = calloc(orgWidth * orgHeight, 4);
    if (dstImageData == NULL) exit(-1);

    int orgIndex = 0, dstIndex = 0;
    for (int row = 0; row < orgHeight; ++row)
    {
        for (int col = 0; col < orgWidth; ++col)
        {
            const unsigned b = orgImageData[orgIndex + 0];
            const unsigned g = orgImageData[orgIndex + 1];
            const unsigned r = orgImageData[orgIndex + 2];

            // Each channel applies: 255 - <channel value>
            dstImageData[dstIndex++] = 255U - b;
            dstImageData[dstIndex++] = 255U - g;
            dstImageData[dstIndex++] = 255U - r;
            dstImageData[dstIndex++] = 255U;    // a

            orgIndex += orgComponents;
        }

        orgIndex += residuleWidthBytes;
    }

    // Create the new BITMAP instance
    s_hBitmap = CreateBitmap(orgWidth, orgHeight, 1U, 4U * 8U, dstImageData);

    free(orgImageData);
    free(dstImageData);
}

// Image Binarization
static void ImageBinarize(void)
{
    if (s_hBitmap == NULL) return;

    // Get the current bitmap size
    BITMAP bmp = { 0 };
    GetObjectA(s_hBitmap, sizeof(BITMAP), &bmp);

    const int orgWidth = (int)bmp.bmWidth;
    const int orgHeight = (int)bmp.bmHeight;
    const int orgComponents = (int)(bmp.bmBitsPixel / 8);
    const int orgWidthBytes = (int)bmp.bmWidthBytes;
    const int residuleWidthBytes = orgWidthBytes - orgWidth * orgComponents;
    const int orgImageBufferSize = orgWidthBytes * orgHeight;

    uint8_t* orgImageData = calloc(orgImageBufferSize, 1);
    if (orgImageData == NULL) exit(-1);
    // Read the raw data from the given HBITMAP object
    if (GetBitmapBits(s_hBitmap, orgImageBufferSize, orgImageData) == 0)
    {
        puts("Read original image data failed in `ColorToGrayTransformBitmap`!");
        free(orgImageData);
        return;
    }

    ClearBitmapResources();

    // The destination BITMAP uses BGRA8888 format,
    // So each pixel occupies 4 bytes
    uint8_t* dstImageData = calloc(orgWidth * orgHeight, 4);
    if (dstImageData == NULL) exit(-1);

    int orgIndex = 0, dstIndex = 0;
    for (int row = 0; row < orgHeight; ++row)
    {
        for (int col = 0; col < orgWidth; ++col)
        {
            const unsigned b = orgImageData[orgIndex + 0];
            const unsigned g = orgImageData[orgIndex + 1];
            const unsigned r = orgImageData[orgIndex + 2];

            // First, convert to gray image applying: I = 0.3B + 0.59G + 0.11R
            unsigned y = (unsigned)(0.3f * b + 0.59f * g + 0.11f * r);

            // Then, binarize the pixel according to the specified threshold
            const unsigned threshold = 128U;
            y = y > threshold ? 255U : 0U;

            dstImageData[dstIndex++] = y;       // b
            dstImageData[dstIndex++] = y;       // g
            dstImageData[dstIndex++] = y;       // r
            dstImageData[dstIndex++] = 255U;    // a

            orgIndex += orgComponents;
        }

        orgIndex += residuleWidthBytes;
    }

    // Create the new BITMAP instance
    s_hBitmap = CreateBitmap(orgWidth, orgHeight, 1U, 4U * 8U, dstImageData);

    free(orgImageData);
    free(dstImageData);
}

static void(* const s_comboBoxOperations[])(void) = {
    &ClearPreviousBitmap,
    &GenerateCustomBitmap,
    &ColorToGrayTransformBitmap,
    &ReversePixelBitmap,
    &ImageBinarize
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
            if (notifCode == BN_CLICKED) {
                BrowseFileOpenDialog(hInstance, hWnd);
            }
        }
        else if ((HWND)lParam == s_loadImageButton)
        {
            const DWORD notifCode = HIWORD(wParam);
            if (notifCode == BN_CLICKED) {
                LoadImageFile(hWnd);
            }
        }
        else if ((HWND)lParam == s_generateImageButton)
        {
            const LRESULT itemIndex = SendMessageA(s_optionComboBox, CB_GETCURSEL, 0, 0);
            if((size_t)itemIndex < sizeof(s_comboBoxOperations) / sizeof(s_comboBoxOperations[0])) {
                s_comboBoxOperations[itemIndex]();
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
            GetObjectA(s_hBitmap, sizeof(BITMAP), &bmp);
            const int x = (WINDOW_WIDTH - bmp.bmWidth) / 2;
            const int y = (WINDOW_HEIGHT - bmp.bmHeight) / 2;
            BitBlt(hdc, x, y, bmp.bmWidth, bmp.bmHeight, s_hMemDC, 0, 0, SRCCOPY);

            SelectObject(s_hMemDC, hOldBitmap);
            
            const RECT rect = { 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT };
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

    return DefWindowProcA(hWnd, uMsg, wParam, lParam);
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
        300,
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
        400,                        // x position 
        10,                         // y position 
        90,                     // Button width
        30,                     // Button height
        hWnd,                   // Parent window
        NULL,                   // No menu.
        hInstance,
        NULL);                  // Pointer not needed.

    s_generateImageButton = CreateWindowExA(
        0,
        WC_BUTTON,          // Predefined class;
        "Generate Image",   // Button text 
        WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,  // Styles 
        510,                        // x position
        10,                         // y position
        120,                    // Button width
        30,                     // Button height
        hWnd,               // Parent window
        NULL,                   // No menu.
        hInstance,
        NULL);                  // Pointer not needed.

    s_optionComboBox = CreateWindowExA(
        0,
        WC_COMBOBOX,
        "",
        WS_CHILD | WS_OVERLAPPED | WS_VISIBLE | CBS_DROPDOWNLIST | CBS_HASSTRINGS,
        640,
        12,
        130,
        200,
        hWnd,
        NULL,
        hInstance,
        NULL);

    // Add string to comboBox.
    SendMessageA(s_optionComboBox, CB_ADDSTRING, 0, (LPARAM)"Clear");   // Clear previous bitmap using the background (white) color
    SendMessageA(s_optionComboBox, CB_ADDSTRING, 0, (LPARAM)"Custom");  // Generate a custom bitmap (with red, green and blue strips)
    SendMessageA(s_optionComboBox, CB_ADDSTRING, 0, (LPARAM)"Color to Gray");   // Color to gray operation
    SendMessageA(s_optionComboBox, CB_ADDSTRING, 0, (LPARAM)"Reverse Pixel");   // Reverse Pixel operation
    SendMessageA(s_optionComboBox, CB_ADDSTRING, 0, (LPARAM)"Binarization");    // Image Binarization

    // Send the CB_SETCURSEL message to display an initial item in the selection field
    const WPARAM selectedItemIndex = 0;
    SendMessageA(s_optionComboBox, CB_SETCURSEL, selectedItemIndex, 0);

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

