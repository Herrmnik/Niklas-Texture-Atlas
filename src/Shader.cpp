#include "Shader.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <glad/glad.h>

// Constructor.
// Loads shader source from disk, compiles the vertex and fragment shaders,
// and links them into a single OpenGL program.
Shader::Shader(const std::string& vertPath, const std::string& fragPath)
    : id(0)
{
    std::string vertCode, fragCode;

    // Load the vertex shader source file into a string.
    {
        std::ifstream vFile(vertPath);
        if (!vFile.is_open()) {
            std::cerr << "Failed to open vertex shader file: " << vertPath << "\n";
            return;
        }

        std::stringstream vStream;
        vStream << vFile.rdbuf();
        vertCode = vStream.str();
        vFile.close();
    }

    // Load the fragment shader source file into a string.
    {
        std::ifstream fFile(fragPath);
        if (!fFile.is_open()) {
            std::cerr << "Failed to open fragment shader file: " << fragPath << "\n";
            return;
        }

        std::stringstream fStream;
        fStream << fFile.rdbuf();
        fragCode = fStream.str();
        fFile.close();
    }

    // Compile each shader stage separately.
    unsigned int vShader = compileShader(GL_VERTEX_SHADER, vertCode);
    unsigned int fShader = compileShader(GL_FRAGMENT_SHADER, fragCode);

    // If either shader failed to compile, clean up and stop.
    if (vShader == 0 || fShader == 0) {
        if (vShader) glDeleteShader(vShader);
        if (fShader) glDeleteShader(fShader);
        return;
    }

    // Link the compiled shaders into a single program object.
    id = createProgram(vShader, fShader);
}

// Destructor.
// Deletes the OpenGL shader program when the Shader object is destroyed.
Shader::~Shader() {
    if (id) {
        glDeleteProgram(id);
    }
}

// Makes this shader program active for subsequent rendering calls.
void Shader::use() const {
    if (id) {
        glUseProgram(id);
    }
}

// Sets an integer uniform in the shader program.
// Commonly used for texture samplers such as uTexture = 0.
void Shader::setInt(const std::string& name, int value) const {
    if (!id) return;

    int loc = glGetUniformLocation(id, name.c_str());
    if (loc != -1) {
        glUniform1i(loc, value);
    }
}

// Sets a 4x4 matrix uniform in the shader program.
// Useful for transforms such as model, view, or projection matrices.
void Shader::setMat4(const std::string& name, const float* m) const {
    if (!id) return;

    int loc = glGetUniformLocation(id, name.c_str());
    if (loc != -1) {
        glUniformMatrix4fv(loc, 1, GL_FALSE, m);
    }
}

// Compiles a single GLSL shader stage.
// 'type' is usually GL_VERTEX_SHADER or GL_FRAGMENT_SHADER.
// Returns the compiled shader object, or 0 if compilation fails.
unsigned int Shader::compileShader(unsigned int type, const std::string& source) {
    unsigned int shader = glCreateShader(type);

    const char* src = source.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    int success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);

        std::cerr << "Shader compilation error ("
                  << (type == GL_VERTEX_SHADER ? "vertex" : "fragment")
                  << "):\n" << infoLog << "\n";

        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

// Links the compiled vertex and fragment shaders into one OpenGL program.
// Returns the program object, or 0 if linking fails.
unsigned int Shader::createProgram(unsigned int vertex, unsigned int fragment) {
    unsigned int program = glCreateProgram();

    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glLinkProgram(program);

    int success = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(program, 512, nullptr, infoLog);

        std::cerr << "Shader program linking error:\n" << infoLog << "\n";
        glDeleteProgram(program);
        return 0;
    }

    // Once linked, the individual shader objects are no longer needed.
    glDeleteShader(vertex);
    glDeleteShader(fragment);

    return program;
}