#pragma once
#include <string>

class Shader {
public:
    unsigned int id{};
    // Small utility class that loads, compiles, and links GLSL shaders.
    Shader(const std::string& vertPath, const std::string& fragPath);
     // Deletes the linked shader program.
    ~Shader();
    // Makes this shader program active for rendering.
    void use() const;
    // Sets an integer uniform, typically used for sampler binding.
    void setInt(const std::string& name, int value) const;
    // Sets a 4x4 matrix uniform.
    void setMat4(const std::string& name, const float* m) const;

private:
    // Compiles one GLSL shader stage.
    unsigned int compileShader(unsigned int type, const std::string& source);
    // Links the compiled vertex and fragment shaders into a single program.
    unsigned int createProgram(unsigned int vertex, unsigned int fragment);
};