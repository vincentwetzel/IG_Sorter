#include "utils/WebpDecoder.h"
#include "utils/LogManager.h"
#include <QFile>

#ifdef Q_OS_WIN
#include <windows.h>
#include <wincodec.h>
#include <comdef.h>

QImage decodeWebpViaWic(const QString& filePath) {
    // Open the file
    HANDLE hFile = CreateFileW(
        reinterpret_cast<const wchar_t*>(filePath.utf16()),
        GENERIC_READ,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );

    if (hFile == INVALID_HANDLE_VALUE) {
        return QImage();
    }

    LARGE_INTEGER fileSize;
    if (!GetFileSizeEx(hFile, &fileSize)) {
        CloseHandle(hFile);
        return QImage();
    }

    // Read entire file into memory
    QByteArray data(fileSize.QuadPart, Qt::Uninitialized);
    DWORD bytesRead = 0;
    if (!ReadFile(hFile, data.data(), static_cast<DWORD>(fileSize.QuadPart), &bytesRead, nullptr)) {
        CloseHandle(hFile);
        return QImage();
    }
    CloseHandle(hFile);

    // Initialize COM
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    bool coInit = SUCCEEDED(hr);

    IWICImagingFactory* pFactory = nullptr;
    hr = CoCreateInstance(
        CLSID_WICImagingFactory,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&pFactory)
    );

    if (FAILED(hr) || !pFactory) {
        if (coInit) CoUninitialize();
        return QImage();
    }

    // Create stream from memory
    IWICStream* pStream = nullptr;
    hr = pFactory->CreateStream(&pStream);
    if (FAILED(hr) || !pStream) {
        pFactory->Release();
        if (coInit) CoUninitialize();
        return QImage();
    }

    hr = pStream->InitializeFromMemory(
        reinterpret_cast<BYTE*>(data.data()),
        static_cast<DWORD>(data.size())
    );

    if (FAILED(hr)) {
        pStream->Release();
        pFactory->Release();
        if (coInit) CoUninitialize();
        return QImage();
    }

    // Create decoder
    IWICBitmapDecoder* pDecoder = nullptr;
    hr = pFactory->CreateDecoderFromStream(
        pStream,
        nullptr,
        WICDecodeMetadataCacheOnDemand,
        &pDecoder
    );

    if (FAILED(hr) || !pDecoder) {
        pStream->Release();
        pFactory->Release();
        if (coInit) CoUninitialize();
        return QImage();
    }

    // Get first frame
    IWICBitmapFrameDecode* pFrame = nullptr;
    hr = pDecoder->GetFrame(0, &pFrame);

    // Convert to 32bppPBGRA (which maps to QImage::Format_ARGB32)
    IWICFormatConverter* pConverter = nullptr;
    if (SUCCEEDED(hr)) {
        hr = pFactory->CreateFormatConverter(&pConverter);
    }

    if (SUCCEEDED(hr)) {
        hr = pConverter->Initialize(
            pFrame,
            GUID_WICPixelFormat32bppPBGRA,
            WICBitmapDitherTypeNone,
            nullptr,
            0.0,
            WICBitmapPaletteTypeMedianCut
        );
    }

    if (FAILED(hr) || !pConverter) {
        if (pConverter) pConverter->Release();
        if (pFrame) pFrame->Release();
        pDecoder->Release();
        pStream->Release();
        pFactory->Release();
        if (coInit) CoUninitialize();
        return QImage();
    }

    // Get dimensions
    UINT width = 0, height = 0;
    pConverter->GetSize(&width, &height);

    if (width == 0 || height == 0 || width > 30000 || height > 30000) {
        pConverter->Release();
        if (pFrame) pFrame->Release();
        pDecoder->Release();
        pStream->Release();
        pFactory->Release();
        if (coInit) CoUninitialize();
        return QImage();
    }

    // Copy pixels to QImage
    QImage image(static_cast<int>(width), static_cast<int>(height), QImage::Format_ARGB32);
    UINT stride = static_cast<UINT>(image.bytesPerLine());
    UINT bufferSize = static_cast<UINT>(image.sizeInBytes());
    hr = pConverter->CopyPixels(
        nullptr,
        stride,
        bufferSize,
        image.bits()
    );

    // Cleanup
    pConverter->Release();
    if (pFrame) pFrame->Release();
    pDecoder->Release();
    pStream->Release();
    pFactory->Release();
    if (coInit) CoUninitialize();

    if (FAILED(hr)) {
        return QImage();
    }

    return image;
}
#else
QImage decodeWebpViaWic(const QString& filePath) {
    Q_UNUSED(filePath);
    return QImage();
}
#endif
