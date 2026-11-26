// Rasterizes the master SVG icon into the Windows .ico and package PNGs.
//
// Usage:
//   make-icons <icon.svg> --ico <out.ico> [--png <size> <out.png>]...
//
// Direct2D renders the SVG, so no third-party converter is needed. Each ICO
// size is rendered from the vector source rather than scaled down from a
// large bitmap, which keeps the small sizes sharp.

#include <windows.h>
#include <d2d1_3.h>
#include <shlwapi.h>
#include <wincodec.h>
#include <wrl/client.h>

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

using Microsoft::WRL::ComPtr;

namespace {

constexpr UINT kIcoSizes[] = {16, 20, 24, 32, 40, 48, 64, 256};

struct Renderer {
  ComPtr<ID2D1Factory1> d2d;
  ComPtr<IWICImagingFactory> wic;
  ComPtr<IStream> svg;
};

bool Check(HRESULT result, const char* what) {
  if (FAILED(result)) {
    std::fprintf(stderr, "%s failed: 0x%08lX\n", what,
                 static_cast<unsigned long>(result));
    return false;
  }
  return true;
}

// Renders the SVG at size x size and returns the PNG bytes.
bool RenderPng(Renderer& renderer, UINT size, std::vector<uint8_t>& png) {
  ComPtr<IWICBitmap> bitmap;
  if (!Check(renderer.wic->CreateBitmap(size, size,
                                        GUID_WICPixelFormat32bppPBGRA,
                                        WICBitmapCacheOnLoad, &bitmap),
             "CreateBitmap")) {
    return false;
  }
  const D2D1_RENDER_TARGET_PROPERTIES properties = D2D1::RenderTargetProperties(
      D2D1_RENDER_TARGET_TYPE_SOFTWARE,
      D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
  ComPtr<ID2D1RenderTarget> target;
  ComPtr<ID2D1DeviceContext5> context;
  ComPtr<ID2D1SvgDocument> document;
  const LARGE_INTEGER start{};
  if (!Check(renderer.d2d->CreateWicBitmapRenderTarget(bitmap.Get(), properties,
                                                       &target),
             "CreateWicBitmapRenderTarget") ||
      !Check(target.As(&context), "ID2D1DeviceContext5") ||
      !Check(renderer.svg->Seek(start, STREAM_SEEK_SET, nullptr), "Seek") ||
      !Check(context->CreateSvgDocument(
                 renderer.svg.Get(),
                 D2D1::SizeF(static_cast<float>(size), static_cast<float>(size)),
                 &document),
             "CreateSvgDocument")) {
    return false;
  }
  // The master's width and height attributes fix its size in pixels; fill
  // the viewport instead so the viewBox scales to each output size.
  ComPtr<ID2D1SvgElement> root;
  document->GetRoot(&root);
  if (!Check(root->SetAttributeValue(L"width", D2D1_SVG_ATTRIBUTE_STRING_TYPE_SVG,
                                     L"100%"),
             "set width") ||
      !Check(root->SetAttributeValue(L"height",
                                     D2D1_SVG_ATTRIBUTE_STRING_TYPE_SVG, L"100%"),
             "set height")) {
    return false;
  }
  context->BeginDraw();
  context->Clear(D2D1::ColorF(0, 0.0f));
  context->DrawSvgDocument(document.Get());
  if (!Check(context->EndDraw(), "EndDraw")) {
    return false;
  }

  ComPtr<IWICBitmapSource> straight;
  ComPtr<IStream> stream;
  ComPtr<IWICBitmapEncoder> encoder;
  ComPtr<IWICBitmapFrameEncode> frame;
  WICPixelFormatGUID format = GUID_WICPixelFormat32bppBGRA;
  if (!Check(WICConvertBitmapSource(GUID_WICPixelFormat32bppBGRA, bitmap.Get(),
                                    &straight),
             "WICConvertBitmapSource") ||
      !Check(CreateStreamOnHGlobal(nullptr, TRUE, &stream), "stream") ||
      !Check(renderer.wic->CreateEncoder(GUID_ContainerFormatPng, nullptr,
                                         &encoder),
             "CreateEncoder") ||
      !Check(encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache),
             "Initialize") ||
      !Check(encoder->CreateNewFrame(&frame, nullptr), "CreateNewFrame") ||
      !Check(frame->Initialize(nullptr), "frame Initialize") ||
      !Check(frame->SetSize(size, size), "SetSize") ||
      !Check(frame->SetPixelFormat(&format), "SetPixelFormat") ||
      !Check(frame->WriteSource(straight.Get(), nullptr), "WriteSource") ||
      !Check(frame->Commit(), "frame Commit") ||
      !Check(encoder->Commit(), "encoder Commit")) {
    return false;
  }
  STATSTG stat{};
  if (!Check(stream->Stat(&stat, STATFLAG_NONAME), "Stat") ||
      !Check(stream->Seek(start, STREAM_SEEK_SET, nullptr), "Seek")) {
    return false;
  }
  png.resize(static_cast<size_t>(stat.cbSize.QuadPart));
  ULONG read = 0;
  return Check(stream->Read(png.data(), static_cast<ULONG>(png.size()), &read),
               "Read") &&
         read == png.size();
}

bool WriteBytes(const std::wstring& path, const std::vector<uint8_t>& bytes) {
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  out.write(reinterpret_cast<const char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
  if (!out) {
    std::fwprintf(stderr, L"Could not write %ls\n", path.c_str());
    return false;
  }
  return true;
}

void Append16(std::vector<uint8_t>& out, uint16_t value) {
  out.push_back(static_cast<uint8_t>(value & 0xFF));
  out.push_back(static_cast<uint8_t>(value >> 8));
}

void Append32(std::vector<uint8_t>& out, uint32_t value) {
  for (int shift = 0; shift < 32; shift += 8) {
    out.push_back(static_cast<uint8_t>((value >> shift) & 0xFF));
  }
}

// Writes an .ico whose entries are PNG images, one per size.
bool WriteIco(Renderer& renderer, const std::wstring& path) {
  std::vector<std::vector<uint8_t>> images;
  for (UINT size : kIcoSizes) {
    std::vector<uint8_t> png;
    if (!RenderPng(renderer, size, png)) {
      return false;
    }
    images.push_back(std::move(png));
  }
  const auto count = static_cast<uint16_t>(images.size());
  std::vector<uint8_t> ico;
  Append16(ico, 0);  // Reserved.
  Append16(ico, 1);  // Icon resource type.
  Append16(ico, count);
  uint32_t offset = 6 + 16u * count;
  for (size_t index = 0; index < images.size(); ++index) {
    const UINT size = kIcoSizes[index];
    ico.push_back(size >= 256 ? 0 : static_cast<uint8_t>(size));
    ico.push_back(size >= 256 ? 0 : static_cast<uint8_t>(size));
    ico.push_back(0);   // No palette.
    ico.push_back(0);   // Reserved.
    Append16(ico, 1);   // Color planes.
    Append16(ico, 32);  // Bits per pixel.
    Append32(ico, static_cast<uint32_t>(images[index].size()));
    Append32(ico, offset);
    offset += static_cast<uint32_t>(images[index].size());
  }
  for (const std::vector<uint8_t>& image : images) {
    ico.insert(ico.end(), image.begin(), image.end());
  }
  return WriteBytes(path, ico);
}

int Usage() {
  std::fprintf(stderr,
               "usage: make-icons <icon.svg> --ico <out.ico> "
               "[--png <size> <out.png>]...\n");
  return 2;
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
  if (argc < 4) {
    return Usage();
  }
  if (!Check(CoInitializeEx(nullptr, COINIT_MULTITHREADED), "CoInitializeEx")) {
    return 1;
  }
  int exit_code = 1;
  {
    Renderer renderer;
    if (Check(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,
                                renderer.d2d.GetAddressOf()),
              "D2D1CreateFactory") &&
        Check(CoCreateInstance(CLSID_WICImagingFactory, nullptr,
                               CLSCTX_INPROC_SERVER,
                               IID_PPV_ARGS(&renderer.wic)),
              "WIC factory") &&
        Check(SHCreateStreamOnFileEx(argv[1], STGM_READ, FILE_ATTRIBUTE_NORMAL,
                                     FALSE, nullptr, &renderer.svg),
              "open SVG")) {
      exit_code = 0;
      for (int index = 2; index < argc && exit_code == 0; ++index) {
        const std::wstring option = argv[index];
        if (option == L"--ico" && index + 1 < argc) {
          exit_code = WriteIco(renderer, argv[++index]) ? 0 : 1;
        } else if (option == L"--png" && index + 2 < argc) {
          const UINT size = static_cast<UINT>(_wtoi(argv[++index]));
          std::vector<uint8_t> png;
          exit_code = size > 0 && RenderPng(renderer, size, png) &&
                              WriteBytes(argv[++index], png)
                          ? 0
                          : 1;
        } else {
          exit_code = Usage();
        }
      }
    }
  }
  CoUninitialize();
  return exit_code;
}
