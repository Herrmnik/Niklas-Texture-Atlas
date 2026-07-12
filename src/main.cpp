#include <glad/glad.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "TextureAtlas.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <unordered_map>
#include <cmath>

#include <nlohmann/json.hpp>
using json = nlohmann::json;

// Reads an entire text file into a string.
// Used for loading shader source code from disk.
static std::string readFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return {};
    std::stringstream ss;
    ss << file.rdbuf();
    return ss.str();
}
// Compiles a single GLSL shader stage and prints any compile errors.
static GLuint compileShader(GLenum type, const std::string& source) {
    GLuint shader = glCreateShader(type);
    const char* src = source.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLint len = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
        std::string log(len, '\0');
        glGetShaderInfoLog(shader, len, nullptr, log.data());
        std::cerr << log << std::endl;
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}
// Creates a shader program by loading, compiling, and linking a vertex and fragment shader.
static GLuint createProgram(const std::string& vsPath, const std::string& fsPath) {
    std::string vsSrc = readFile(vsPath);
    std::string fsSrc = readFile(fsPath);

    if (vsSrc.empty() || fsSrc.empty()) return 0;

    GLuint vs = compileShader(GL_VERTEX_SHADER, vsSrc);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fsSrc);
    if (!vs || !fs) return 0;

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);

    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        GLint len = 0;
        glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &len);
        std::string log(len, '\0');
        glGetProgramInfoLog(prog, len, nullptr, log.data());
        std::cerr << log << std::endl;
        glDeleteProgram(prog);
        prog = 0;
    }

    glDeleteShader(vs);
    glDeleteShader(fs);
    return prog;
}
// Creates a screen-space quad with position and UV attributes.
// The same quad is reused for every sprite.
static GLuint createQuadVAO(GLuint& outVBO) {
    float vertices[] = {
        -0.5f, -0.5f,  0.0f, 0.0f,
         0.5f, -0.5f,  1.0f, 0.0f,
         0.5f,  0.5f,  1.0f, 1.0f,

         0.5f,  0.5f,  1.0f, 1.0f,
        -0.5f,  0.5f,  0.0f, 1.0f,
        -0.5f, -0.5f,  0.0f, 0.0f
    };

    GLuint vao = 0;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &outVBO);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, outVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

    glBindVertexArray(0);
    return vao;
}
// Updates the quad vertex buffer so the quad is centered at (x, y)
// and uses the UV rectangle [u0, v0] to [u1, v1].
static void setQuad(GLuint vbo, float x, float y, float w, float h,
                    float u0, float v0, float u1, float v1) {
    float vertices[] = {
        x - w, y - h,  u0, v1,
        x + w, y - h,  u1, v1,
        x + w, y + h,  u1, v0,

        x + w, y + h,  u1, v0,
        x - w, y + h,  u0, v0,
        x - w, y - h,  u0, v1
    };

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);
}
// Describes one drawable sprite in the scene.
// x/y define its position in normalized screen space,
// w/h define its size, and regionName selects the atlas region.
struct Sprite {
    float x;
    float y;
    float w;
    float h;
    std::string regionName;
};
// Loads the texture atlas configuration from JSON.
// The JSON file contains:
// - "images": source image paths
// - "regions": crop rectangles and region names
static bool loadImagesJson(const std::string& path,
                           std::vector<std::string>& outImages,
                           std::vector<AtlasRegion>& outRegions) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Failed to open JSON file: " << path << "\n";
        return false;
    }

    json j;
    try {
        file >> j;
    } catch (const std::exception& e) {
        std::cerr << "JSON parse error: " << e.what() << "\n";
        return false;
    }

    if (!j.contains("images") || !j["images"].is_array()) {
        std::cerr << "JSON missing 'images' array\n";
        return false;
    }

    if (!j.contains("regions") || !j["regions"].is_array()) {
        std::cerr << "JSON missing 'regions' array\n";
        return false;
    }

    outImages.clear();
    outRegions.clear();

    for (const auto& item : j["images"]) {
        if (item.is_string()) {
            outImages.push_back(item.get<std::string>());
        }
    }

    for (const auto& item : j["regions"]) {
        if (!item.is_object()) continue;

        AtlasRegion r;
        r.name = item.value("name", "");
        r.sourceName = item.value("source", "");
        r.srcX = item.value("x", 0);
        r.srcY = item.value("y", 0);
        r.srcW = item.value("w", 0);
        r.srcH = item.value("h", 0);

        if (r.sourceName.empty() || r.srcW <= 0 || r.srcH <= 0) {
            std::cerr << "Invalid region entry in JSON\n";
            return false;
        }

        outRegions.push_back(r);
    }

    return true;
}
// Builds a lookup table from region name to atlas region pointer.
// This makes sprite rendering fast, since each sprite only needs a name.
static std::unordered_map<std::string, const AtlasRegion*> buildRegionMap(const std::vector<AtlasRegion>& regions) {
    std::unordered_map<std::string, const AtlasRegion*> map;
    for (const auto& r : regions) {
        if (!r.name.empty()) {
            map[r.name] = &r;
        }
    }
    return map;
}
// Creates a test scene with many sprites arranged in a grid.
static void buildGridSprites(std::vector<Sprite>& sprites) {
    sprites.clear();

    const std::vector<std::string> names = {
        "grass_large",
        "grass_small",
        "jungle_large",
        "jungle_small",
        "multileaf_bright",
        "multileaf_dark"
    };

    const int cols = 10;
    const int rows = 10;

    const float startX = -0.90f;
    const float startY =  0.80f;
    const float stepX = 0.18f;
    const float stepY = 0.16f;

    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < cols; ++x) {
            Sprite s;
            s.x = startX + x * stepX;
            s.y = startY - y * stepY;
            s.w = 0.07f;
            s.h = 0.07f;
            s.regionName = names[(x + y) % names.size()];
            sprites.push_back(s);
        }
    }
}

static float frand(float a, float b) {
    return a + (b - a) * (float(rand()) / float(RAND_MAX));
}
// Creates the final forest-style scene using grass, jungle, and fern regions.
static void buildForestScene(std::vector<Sprite>& sprites) {
    sprites.clear();

    // Ground strip.
    for (int i = 0; i < 24; ++i) {
        Sprite s;
        s.x = -1.05f + i * 0.095f;
        s.y = -0.82f;
        s.w = 0.055f;
        s.h = 0.055f;
        s.regionName = (i % 3 == 0) ? "grass_large" : "grass_small";
        sprites.push_back(s);
    }

    // Second ground row for depth.
    for (int i = 0; i < 20; ++i) {
        Sprite s;
        s.x = -1.00f + i * 0.105f;
        s.y = -0.70f;
        s.w = 0.045f;
        s.h = 0.045f;
        s.regionName = (i % 4 == 0) ? "jungle_small" : "grass_small";
        sprites.push_back(s);
    }

    // Background plants.
    for (int i = 0; i < 6; ++i) {
        Sprite s;
        s.x = -0.95f + i * 0.38f;
        s.y = -0.10f + frand(-0.03f, 0.03f);
        s.w = 0.11f + frand(-0.02f, 0.02f);
        s.h = 0.20f + frand(-0.03f, 0.03f);
        s.regionName = (i % 2 == 0) ? "jungle_large" : "jungle_small";
        sprites.push_back(s);
    }

    // Left fern cluster.
    {
        Sprite s;
        s.x = -0.62f;
        s.y = -0.26f;
        s.w = 0.17f;
        s.h = 0.30f;
        s.regionName = "multileaf_dark";
        sprites.push_back(s);
    }
    {
        Sprite s;
        s.x = -0.50f;
        s.y = -0.18f;
        s.w = 0.20f;
        s.h = 0.34f;
        s.regionName = "multileaf_bright";
        sprites.push_back(s);
    }

    // Center fern cluster.
    {
        Sprite s;
        s.x = -0.08f;
        s.y = -0.24f;
        s.w = 0.22f;
        s.h = 0.36f;
        s.regionName = "multileaf_bright";
        sprites.push_back(s);
    }
    {
        Sprite s;
        s.x = 0.10f;
        s.y = -0.20f;
        s.w = 0.18f;
        s.h = 0.31f;
        s.regionName = "multileaf_dark";
        sprites.push_back(s);
    }

    // Right fern cluster.
    {
        Sprite s;
        s.x = 0.52f;
        s.y = -0.23f;
        s.w = 0.16f;
        s.h = 0.28f;
        s.regionName = "multileaf_dark";
        sprites.push_back(s);
    }
    {
        Sprite s;
        s.x = 0.66f;
        s.y = -0.16f;
        s.w = 0.21f;
        s.h = 0.35f;
        s.regionName = "multileaf_bright";
        sprites.push_back(s);
    }

    // Small foreground accents.
    for (int i = 0; i < 4; ++i) {
        Sprite s;
        s.x = -0.85f + i * 0.55f;
        s.y = -0.44f + frand(-0.03f, 0.03f);
        s.w = 0.09f + frand(-0.01f, 0.02f);
        s.h = 0.15f + frand(-0.02f, 0.02f);
        s.regionName = (i % 2 == 0) ? "multileaf_dark" : "jungle_small";
        sprites.push_back(s);
    }
}
// Reads keyboard input to pan the camera and zoom in/out.
static void processCameraInput(GLFWwindow* window, float& camX, float& camY, float& zoom, float dt) {
    const float panSpeed = 1.5f;
    const float zoomSpeed = 1.0f;

    if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)  camX += panSpeed * dt;
    if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) camX -= panSpeed * dt;
    if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)    camY -= panSpeed * dt;
    if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)  camY += panSpeed * dt;

    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) zoom += zoomSpeed * dt;
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) zoom -= zoomSpeed * dt;

    if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) {
        camX = 0.0f;
        camY = 0.0f;
        zoom = 1.0f;
    }

    if (zoom < 0.2f) zoom = 0.2f;
    if (zoom > 4.0f) zoom = 4.0f;
}
// Computes the total area used by packed atlas regions.
// Used only for debug metrics.
static size_t computeUsedArea(const std::vector<AtlasRegion>& regions) {
    size_t used = 0;
    for (const auto& r : regions) {
        used += static_cast<size_t>(r.srcW) * static_cast<size_t>(r.srcH);
    }
    return used;
}
// Prints atlas statistics such as size, utilization, and draw calls.
static void printAtlasMetrics(const TextureAtlas& atlas,
                              const std::vector<std::string>& sourceImages,
                              const std::vector<AtlasRegion>& packedRegions,
                              int drawCallsPerFrame) {
    const size_t usedArea = computeUsedArea(packedRegions);
    const int atlasW = atlas.getWidth();
    const int atlasH = atlas.getHeight();
    const double totalArea = static_cast<double>(atlasW) * static_cast<double>(atlasH);
    const double utilization = (totalArea > 0.0) ? (100.0 * static_cast<double>(usedArea) / totalArea) : 0.0;

    std::cout << "=== Atlas Metrics ===\n";
    std::cout << "Source textures: " << sourceImages.size() << "\n";
    std::cout << "Atlas textures: 1\n";
    std::cout << "Packed regions: " << packedRegions.size() << "\n";
    std::cout << "Draw calls per frame: " << drawCallsPerFrame << "\n";
    std::cout << "Atlas size: " << atlasW << " x " << atlasH << "\n";
    std::cout << "Used area: " << usedArea << "\n";
    std::cout << "Utilization: " << utilization << "%\n";
}
// Sorts regions by area so larger regions can be packed first.
// This is optional and can improve atlas packing behavior.
static void sortRegionsByAreaDescending(std::vector<AtlasRegion>& regions) {
    std::sort(regions.begin(), regions.end(),
              [](const AtlasRegion& a, const AtlasRegion& b) {
                  return (a.srcW * a.srcH) > (b.srcW * b.srcH);
              });
}

int main() {
    // Initialize GLFW, which is responsible for window creation and input handling.
    if (!glfwInit()) return 1;
    // Request an OpenGL 3.3 core profile context.
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    // Create the application window.
    GLFWwindow* window = glfwCreateWindow(1280, 720, "Niklas' Texture Atlas", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return 1;
    }
    // Make the window's OpenGL context current so rendering commands affect it.
    glfwMakeContextCurrent(window);
    // Load OpenGL function pointers using GLAD.
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }
    // Load OpenGL function pointers using GLAD.
    glViewport(0, 0, 1280, 720);
    glClearColor(0.1f, 0.1f, 0.12f, 1.0f);
    // Enable alpha blending so transparent PNG pixels blend correctly with the background.
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    // Load and link the shader program used for sprite rendering.
    GLuint program = createProgram("shaders/shader.vert.txt", "shaders/shader.frag.txt");
    if (!program) {
        std::cerr << "Failed to create shader program\n";
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }
    // Create reusable quad geometry.
    // Each sprite is drawn using the same quad, then resized and retargeted with UVs.
    GLuint quadVBO = 0;
    GLuint quadVAO = createQuadVAO(quadVBO);
    // Load the atlas configuration from JSON.
    // The JSON file tells the program which source images exist and which rectangles to pack
    std::vector<std::string> sourceImages;
    std::vector<AtlasRegion> regions;

    if (!loadImagesJson("assets/images.json", sourceImages, regions)) {
        std::cerr << "Failed to load JSON data\n";
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }
    // Optionally sort a copy of the regions for packing experiments.
    std::vector<AtlasRegion> regionsForBuild = regions;
    sortRegionsByAreaDescending(regionsForBuild); // optional experiment
    //// Build the actual atlas texture from the region definitions.
    TextureAtlas atlas;
    if (!atlas.build(regions, sourceImages, 2048, 2048, 1)) {
        std::cerr << "Failed to build atlas\n";
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }
    // Create a fast lookup table from region name to UV coordinates.
    auto regionMap = buildRegionMap(atlas.getRegions());
    // Tell the shader that the atlas texture is bound to texture unit 0.
    glUseProgram(program);
    glUniform1i(glGetUniformLocation(program, "uTexture"), 0);
     // Build the list of sprites that will be drawn.
    std::vector<Sprite> sprites;
    //buildGridSprites(sprites); Makes a long line of grids showing all the pictures
    buildForestScene(sprites); //Builds a preset Forest scene for showcase

    // Camera controls for panning and zooming.
    float camX = 0.0f;
    float camY = 0.0f;
    float zoom = 1.0f;
    // Timing values for smooth movement.
    double lastTime = glfwGetTime();
    double timer = lastTime;
    bool printedMetrics = false;
    // Number of draw calls in the current frame.
    int drawCalls = 0;
    // Main render loop.
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        // Compute frame delta time.
        double now = glfwGetTime();
        float dt = static_cast<float>(now - lastTime);
        lastTime = now;
        drawCalls = 0;
        // Handle keyboard input for camera movement.
        processCameraInput(window, camX, camY, zoom, dt);
        // Clear the frame buffer.
        glClear(GL_COLOR_BUFFER_BIT);
        // Bind the shader, atlas texture, and quad geometry before drawing.
        glUseProgram(program);
        atlas.bind(0);
        glBindVertexArray(quadVAO);
        // Draw each sprite using its atlas region.
        for (const auto& s : sprites) {
            auto it = regionMap.find(s.regionName);
            if (it == regionMap.end() || it->second == nullptr) continue;

            const AtlasRegion& r = *it->second;
            // Apply camera transform in normalized screen space.
            float x = (s.x + camX) * zoom;
            float y = (s.y + camY) * zoom;
            float w = s.w * zoom;
            float h = s.h * zoom;
            // Update quad vertices with the region's atlas UV coordinates.
            setQuad(quadVBO, x, y, w, h, r.u0, r.v0, r.u1, r.v1);
            // Draw the quad.
            glDrawArrays(GL_TRIANGLES, 0, 6);
            ++drawCalls;
        }
        // Print atlas statistics once after the first frame.
        if (!printedMetrics){
            printAtlasMetrics(atlas, sourceImages, atlas.getRegions(), drawCalls);
            printedMetrics = true;
        }
        // Present the frame.
        glfwSwapBuffers(window);
    }
    // Clean up OpenGL objects and GLFW resources.
    atlas.destroy();
    glDeleteBuffers(1, &quadVBO);
    glDeleteVertexArrays(1, &quadVAO);
    glDeleteProgram(program);
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}