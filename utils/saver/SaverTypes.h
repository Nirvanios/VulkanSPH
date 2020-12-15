//
// Created by root on 14.12.20.
//

#ifndef VULKANAPP_SAVERTYPES_H
#define VULKANAPP_SAVERTYPES_H

enum class PixelFormat {
    RGB,
    RGBA
};

inline int getComponentCount(PixelFormat pixelFormat) {
    switch (pixelFormat) {
        case PixelFormat::RGB:
            return 3;
        case PixelFormat::RGBA:
            return 4;
    }
    return 0;
}

enum class ImageFormat {
    PNG,
    JPEG,
    BMP
};

enum class FilenameFormat {
    None,
    WithDateTime
};

#endif //VULKANAPP_SAVERTYPES_H
