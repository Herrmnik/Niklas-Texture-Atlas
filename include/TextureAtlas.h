#pragma once
#include <string>
#include <vector>
#include <glad/glad.h>
// Describes a single region packed into the atlas.
// sourceName identifies the source image file.
// srcX/srcY/srcW/srcH define the crop rectangle inside the source image.
// atlasX/atlasY/atlasW/atlasH store the final packed position inside the atlas.
// u0/v0/u1/v1 store normalized UV coordinates used by the shader.
struct AtlasRegion {
    std::string name;
    std::string sourceName;
    int srcX = 0;
    int srcY = 0;
    int srcW = 0;
    int srcH = 0;

    int atlasX = 0;
    int atlasY = 0;
    int atlasW = 0;
    int atlasH = 0;

    float u0 = 0.0f;
    float v0 = 0.0f;
    float u1 = 0.0f;
    float v1 = 0.0f;
};
// TextureAtlas manages a single OpenGL texture containing many packed image regions.
class TextureAtlas {
public:
    // Builds the atlas from source images and crop rectangles.
    bool build(const std::vector<AtlasRegion>& inputRegions,
               const std::vector<std::string>& imagePaths,
               int atlasWidth = 1024,
               int atlasHeight = 1024,
               int padding = 1);
    // Deletes the OpenGL texture and clears stored region data.
    void destroy();
    // Binds the atlas texture to the requested texture unit.
    void bind(GLuint unit = 0) const;

    const std::vector<AtlasRegion>& getRegions() const { return regions; }
    GLuint getTextureId() const { return textureId; }
    int getWidth() const { return width; }
    int getHeight() const { return height; }

private:
    GLuint textureId = 0;
    int width = 0;
    int height = 0;
    int padding = 1;
    std::vector<AtlasRegion> regions;
};