// STB Image implementation block.
// This allows the program to load PNG and other common image formats.
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "TextureAtlas.h"
#include <algorithm>
#include <iostream>
#include <vector>
// Holds a source image loaded from disk.
struct LoadedImage {
    std::string name;
    int w = 0;
    int h = 0;
    unsigned char* pixels = nullptr;
};
// Loads an image and converts it to RGBA so all atlas regions share one format.
static bool loadImage(const std::string& path, LoadedImage& out) {
    int channels = 0;
    out.pixels = stbi_load(path.c_str(), &out.w, &out.h, &channels, 4);
    out.name = path;

    if (!out.pixels) {
        std::cerr << "Failed to load image: " << path << "\n";
        return false;
    }

    std::cout << "Loaded image: " << path
              << " (" << out.w << "x" << out.h
              << ", channels=" << channels << ")\n";
    return true;
}
// Releases image memory after the atlas texture has been built.
static void freeImage(LoadedImage& img) {
    if (img.pixels) {
        stbi_image_free(img.pixels);
        img.pixels = nullptr;
    }
}
// Finds the loaded source image that matches a region's sourceName.
static const LoadedImage* findImage(const std::vector<LoadedImage>& images, const std::string& name) {
    for (const auto& img : images) {
        if (img.name == name) return &img;
    }
    return nullptr;
}
// Builds the OpenGL atlas texture.
// Steps:
// 1. Load all source images.
// 2. Validate region rectangles.
// 3. Pack regions into rows inside the atlas.
// 4. Upload cropped pixel data into the final texture.
// 5. Compute normalized UV coordinates for each region.
bool TextureAtlas::build(const std::vector<AtlasRegion>& inputRegions,
                         const std::vector<std::string>& imagePaths,
                         int atlasWidth,
                         int atlasHeight,
                         int pad) {
    destroy();

    width = atlasWidth;
    height = atlasHeight;
    padding = pad;

    std::vector<LoadedImage> images;
    images.reserve(imagePaths.size());

    for (const auto& path : imagePaths) {
        LoadedImage img;
        if (!loadImage(path, img)) {
            for (auto& i : images) freeImage(i);
            return false;
        }
        images.push_back(img);
    }

    regions = inputRegions;

    std::sort(regions.begin(), regions.end(),
              [](const AtlasRegion& a, const AtlasRegion& b) {
                  return a.srcH > b.srcH;
              });

    int x = padding;
    int y = padding;
    int rowH = 0;

    for (auto& r : regions) {
        const LoadedImage* img = findImage(images, r.sourceName);
        if (!img) {
            std::cerr << "No loaded image found for region source: " << r.sourceName << "\n";
            for (auto& i : images) freeImage(i);
            return false;
        }

        if (r.srcX < 0 || r.srcY < 0 || r.srcW <= 0 || r.srcH <= 0 ||
            r.srcX + r.srcW > img->w || r.srcY + r.srcH > img->h) {
            std::cerr << "Invalid crop rect for " << r.sourceName << "\n";
            for (auto& i : images) freeImage(i);
            return false;
        }

        const int packedW = r.srcW + padding * 2;
        const int packedH = r.srcH + padding * 2;

        if (packedW > width || packedH > height) {
            std::cerr << "Region too big for atlas: " << r.sourceName << "\n";
            for (auto& i : images) freeImage(i);
            return false;
        }

        if (x + packedW > width) {
            x = padding;
            y += rowH;
            rowH = 0;
        }

        if (y + packedH > height) {
            std::cerr << "Atlas ran out of space while packing: " << r.sourceName << "\n";
            for (auto& i : images) freeImage(i);
            return false;
        }

        r.atlasX = x + padding;
        r.atlasY = y + padding;
        r.atlasW = r.srcW;
        r.atlasH = r.srcH;

        x += packedW;
        rowH = std::max(rowH, packedH);
    }

    glGenTextures(1, &textureId);
    glBindTexture(GL_TEXTURE_2D, textureId);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
                 width, height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    for (const auto& r : regions) {
        const LoadedImage* img = findImage(images, r.sourceName);
        if (!img) continue;

        std::vector<unsigned char> subImage(r.srcW * r.srcH * 4);

        for (int row = 0; row < r.srcH; ++row) {
            const unsigned char* srcRow =
                img->pixels + ((r.srcY + row) * img->w + r.srcX) * 4;
            unsigned char* dstRow = &subImage[row * r.srcW * 4];
            std::copy(srcRow, srcRow + (r.srcW * 4), dstRow);
        }

        glTexSubImage2D(GL_TEXTURE_2D, 0,
                        r.atlasX, r.atlasY,
                        r.srcW, r.srcH,
                        GL_RGBA, GL_UNSIGNED_BYTE,
                        subImage.data());
    }

    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

    for (auto& r : regions) {
        r.u0 = static_cast<float>(r.atlasX) / static_cast<float>(width);
        r.v0 = static_cast<float>(r.atlasY) / static_cast<float>(height);
        r.u1 = static_cast<float>(r.atlasX + r.atlasW) / static_cast<float>(width);
        r.v1 = static_cast<float>(r.atlasY + r.atlasH) / static_cast<float>(height);
    }

    glBindTexture(GL_TEXTURE_2D, 0);

    for (auto& i : images) freeImage(i);

    std::cout << "Built atlas " << width << "x" << height
              << " with " << regions.size() << " regions.\n";

    return true;
}
// Deletes the atlas texture from GPU memory.
void TextureAtlas::destroy() {
    if (textureId) {
        glDeleteTextures(1, &textureId);
        textureId = 0;
    }
    regions.clear();
}
// Binds the atlas texture to a texture unit for rendering.
void TextureAtlas::bind(GLuint unit) const {
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, textureId);
}