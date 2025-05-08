#pragma once
//#include "mp_sdk_gui2.h"
#include "Drawing.h"
#include "GinBlur.h"
#include "fast_gamma.h"

#define BLUR_USE_SDK3 1

#if BLUR_USE_SDK3
namespace drawing = GmpiDrawing;
using Rect = GmpiDrawing::Rect;
using Point = GmpiDrawing::Point;
using SizeU = GmpiDrawing::SizeU;
using Size = GmpiDrawing::Size;
#endif

struct cachedBlur
{
    drawing::Color tint = /* colorFromHex */ drawing::Color(0xd4c1ffu);

    drawing::Bitmap buffer2;

    void invalidate()
    {
        buffer2 = {};
    }

    void draw(
          drawing::Graphics& g
        , Rect bounds
        , std::function<void(drawing::Graphics&)> drawer
    )
    {
        if (buffer2)
        {
            g.DrawBitmap(buffer2, Point{}, bounds);
            return;
        }

#if BLUR_USE_SDK3
        const auto mysize = SizeU{
              static_cast<uint32_t>(bounds.getWidth())
            , static_cast<uint32_t>(bounds.getHeight())
        };
#else
		const auto mysize = SizeU{ getWidth(bounds), getHeight(bounds) };
#endif

        const auto mysize2 = Size{ static_cast<float>(mysize.width), static_cast<float>(mysize.height) };

        drawing::Bitmap buffer; // monochrome mask
        {
            auto dc = g.CreateCompatibleRenderTarget(mysize2 /*, (int32_t)BitmapRenderTargetFlags::Mask | (int32_t)BitmapRenderTargetFlags::CpuReadable */);

            drawing::Graphics gbitmap(dc.Get());

            dc.BeginDraw();

            drawer(gbitmap);

            dc.EndDraw();
            buffer = dc.GetBitmap();
        }

        // modify the buffer
        if (true)
        {
            auto data = buffer.lockPixels(GmpiDrawing_API::MP1_BITMAP_LOCK_READ | GmpiDrawing_API::MP1_BITMAP_LOCK_WRITE); // BitmapLockFlags::ReadWrite);

            if(data)
            {
                auto imageSize = buffer.GetSize();
                constexpr int pixelSize = 8; // 8 bytes per pixel for half-float
                auto stride = data.getBytesPerRow();
                auto format = data.getPixelFormat();
                const int totalPixels = (int)imageSize.height * stride / pixelSize;

                const int pixelSizeTest = stride / imageSize.width; // 8 for half-float RGB, 4 for 8-bit sRGB, 1 for alpha mask

#if 0
                // un-premultiply alpha
                {
                    auto pixel = (half*)data.getAddress();

                    for (int i = 0; i < totalPixels; ++i)
                    {
                        const float alpha = pixel[3];
                        if (alpha != 1.0f && alpha != 0.0f)
                        {
                            const float overAlphaNorm = 1.0f / alpha;
                            for (int j = 0; j < 3; ++j)
                            {
                                const float p = pixel[j];
                                if (p != 0.0f)
                                {
                                    pixel[j] = p * overAlphaNorm;
                                }
                            }
                        }
                        pixel += 4;
                    }
                }
#endif
                // modify pixels here
#if 0
                {
                    auto pixel = (half*)data.getAddress();
                    ginARGB(pixel, imageSize.width, imageSize.height, 5);
                }
#else
                {
                    // create a blurred mask of the image.
                    auto pixel = data.getAddress();
                    ginSingleChannel(pixel, imageSize.width, imageSize.height, 5);
                }
#endif


#if 0
                // re-premultiply alpha
                {
                    auto pixel = (half*)data.getAddress();

                    for (int i = 0; i < totalPixels; ++i)
                    {
                        const float alpha = pixel[3];
                        if (alpha == 0.0f)
                        {
                            pixel[0] = pixel[1] = pixel[2] = 0.0f;
                        }
                        else
                        {
                            for (int j = 0; j < 3; ++j)
                            {
                                const float p = pixel[j];
                                pixel[j] = p * alpha;
                            }
                        }
                        pixel += 4;
                    }
                }
#endif
                // create bitmap
                buffer2 = g.GetFactory().CreateImage(mysize /*, (int32_t)gmpi::drawing::BitmapRenderTargetFlags::EightBitPixels */);
                {
                    auto destdata = buffer2.lockPixels(GmpiDrawing_API::MP1_BITMAP_LOCK_WRITE); // BitmapLockFlags::Write);
                    auto imageSize = buffer2.GetSize();
                    constexpr int pixelSize = 4; // 8 bytes per pixel for half-float, 4 for 8-bit
                    auto stride = destdata.getBytesPerRow();
                    auto format = destdata.getPixelFormat();
                    const int totalPixels = (int)imageSize.height * stride / pixelSize;

                    const int pixelSizeTest = stride / imageSize.width; // 8 for half-float RGB, 4 for 8-bit sRGB, 1 for alpha mask

                    auto pixelsrc = data.getAddress();
                    //                        auto pixeldest = (half*)destdata.getAddress();
                    auto pixeldest = destdata.getAddress();

                    //                    uint8_t tint8[4] = { 0xff, 0xd4, 0xc1, 0xff }; // xff7777ff
                    //                    Color tint = colorFromArgb(tint8[0], tint8[1], tint8[2]);// { Colors::BlueViolet };
                    float tintf[4] = { tint.r, tint.g, tint.b, tint.a };

                    constexpr float inv255 = 1.0f / 255.0f;

                    for (int i = 0; i < totalPixels; ++i)
                    {
                        const auto alpha = *pixelsrc;
                        if (alpha == 0)
                        {
                            pixeldest[0] = pixeldest[1] = pixeldest[2] = pixeldest[3] = 0.0f;
                        }
                        else
                        {
                            //for (int j = 0; j < 3; ++j)
                            //{
                            //    const float p = 1.0f;
                            //    pixeldest[j] = p * alpha;
                            //}
                            // _RPTN(0, "%f\n", alpha / 255.0f);
#if 1 // correct premultiply. much brighter.
                            const float AlphaNorm = alpha * inv255;
                            for (int j = 0; j < 3; ++j)
                            {
                                // To linear
                                auto cf = tintf[j]; // gmpi::drawing::SRGBPixelToLinear(static_cast<unsigned char>(tint8[i]));

                                // pre-multiply in linear space.
                                cf *= AlphaNorm;

                                // back to SRGB
                                pixeldest[j] = se_sdk::FastGamma::linearToSrgb(cf);// drawing::linearPixelToSRGB(cf);
                            }
#else // naive premultply
                            pixeldest[0] = fast8bitScale(alpha, tint8[0]); // alpha* tint.r* inv255;
                            pixeldest[1] = fast8bitScale(alpha, tint8[1]); // alpha * tint.g * inv255;
                            pixeldest[2] = fast8bitScale(alpha, tint8[2]); // alpha * tint.b * inv255;
#endif
                            pixeldest[3] = alpha; //alpha * inv255;
                        }

                        pixelsrc++;
                        pixeldest += 4;
                    }
                }
                /*
                                    for (float y = 0; y < imageSize.height; y++)
                                    {
                                        for (float x = 0; x < imageSize.width; x++)
                                        {
                                            // pixel to Color.
                                            gmpi::drawing::Color color{ pixel[0], pixel[1], pixel[2], pixel[3] };

                                            if (pixel[0] > 0.0f)
                                            {
                                                color.r = x / imageSize.width;
                                                color.g = 1.0f - pixel[0];
                                                color.b = y / imageSize.width;
                                                color.a = 1.0f - pixel[2];
                                            }

                                            // Color to pixel
                                            pixel[0] = color.r;
                                            pixel[1] = color;
                                            pixel[2] = color;
                                            pixel[3] = color;

                                            pixel += 4;
                                        }
                                    }
                */
            }
        }
        g.DrawBitmap(buffer2, Point{}, bounds);
    }
};

