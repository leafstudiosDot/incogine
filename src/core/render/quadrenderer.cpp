#include "quadrenderer.h"
#include <SDL3/SDL.h>
#include <cstdio>

namespace {
    const char* kVertexShaderDesktop =
        "#version 330 core\n"
        "layout(location = 0) in vec2 aPos;\n"
        "layout(location = 1) in vec2 aUV;\n"
        "uniform mat4 uProj;\n"
        "out vec2 vUV;\n"
        "void main() {\n"
        "    vUV = aUV;\n"
        "    gl_Position = uProj * vec4(aPos, 0.0, 1.0);\n"
        "}\n";

    const char* kFragmentShaderDesktop =
        "#version 330 core\n"
        "in vec2 vUV;\n"
        "uniform sampler2D uTex;\n"
        "uniform vec4 uColor;\n"
        "out vec4 FragColor;\n"
        "void main() {\n"
        "    FragColor = uColor * texture(uTex, vUV);\n"
        "}\n";

    const char* kVertexShaderES =
        "#version 300 es\n"
        "layout(location = 0) in vec2 aPos;\n"
        "layout(location = 1) in vec2 aUV;\n"
        "uniform mat4 uProj;\n"
        "out vec2 vUV;\n"
        "void main() {\n"
        "    vUV = aUV;\n"
        "    gl_Position = uProj * vec4(aPos, 0.0, 1.0);\n"
        "}\n";

    const char* kFragmentShaderES =
        "#version 300 es\n"
        "precision mediump float;\n"
        "in vec2 vUV;\n"
        "uniform sampler2D uTex;\n"
        "uniform vec4 uColor;\n"
        "out vec4 FragColor;\n"
        "void main() {\n"
        "    FragColor = uColor * texture(uTex, vUV);\n"
        "}\n";

    GLuint compileShader(GLenum type, const char* source) {
        GLuint shader = glCreateShader(type);
        if (!shader) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "QuadRenderer: glCreateShader failed");
            return 0;
        }

        glShaderSource(shader, 1, &source, nullptr);
        glCompileShader(shader);

        GLint status = GL_FALSE;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
        if (!status) {
            char log[1024];
            glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "QuadRenderer: shader compile error: %s", log);
            glDeleteShader(shader);
            return 0;
        }

        return shader;
    }
}

bool QuadRenderer::sInitialized = false;
bool QuadRenderer::sIsES = false;
GLuint QuadRenderer::sProgram = 0;
GLuint QuadRenderer::sVAO = 0;
GLuint QuadRenderer::sVBO = 0;
GLuint QuadRenderer::sWhiteTexture = 0;
GLint QuadRenderer::sUniformProj = -1;
GLint QuadRenderer::sUniformColor = -1;
GLint QuadRenderer::sUniformTexture = -1;
float QuadRenderer::sProj[16] = {0};

bool QuadRenderer::Init(SDL_Window* window, SDL_GLContext context) {
    if (sInitialized) {
        return true;
    }

    if (!icgGLInit(window, context)) {
        return false;
    }

    sIsES = icgGLIsES();

    if (!CreateShaders()) {
        return false;
    }

    if (!CreateWhiteTexture()) {
        return false;
    }

    glGenVertexArrays(1, &sVAO);
    glGenBuffers(1, &sVBO);

    glBindVertexArray(sVAO);
    glBindBuffer(GL_ARRAY_BUFFER, sVBO);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (const void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (const void*)(2 * sizeof(float)));
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    glDisable(GL_DEPTH_TEST);

    sInitialized = true;
    return true;
}

void QuadRenderer::Shutdown() {
    if (sVAO) {
        glDeleteVertexArrays(1, &sVAO);
        sVAO = 0;
    }
    if (sVBO) {
        glDeleteBuffers(1, &sVBO);
        sVBO = 0;
    }
    if (sWhiteTexture) {
        glDeleteTextures(1, &sWhiteTexture);
        sWhiteTexture = 0;
    }
    if (sProgram) {
        glDeleteProgram(sProgram);
        sProgram = 0;
    }
    sInitialized = false;
    icgGLShutdown();
}

void QuadRenderer::SetViewport(int width, int height) {
    if (!sInitialized || width <= 0 || height <= 0) {
        return;
    }

    glViewport(0, 0, width, height);

    // Orthographic projection: origin top-left, y pointing down, z in [-1, 1].
    sProj[0] = 2.0f / (float)width;
    sProj[1] = 0.0f;
    sProj[2] = 0.0f;
    sProj[3] = 0.0f;
    sProj[4] = 0.0f;
    sProj[5] = -2.0f / (float)height;
    sProj[6] = 0.0f;
    sProj[7] = 0.0f;
    sProj[8] = 0.0f;
    sProj[9] = 0.0f;
    sProj[10] = -1.0f;
    sProj[11] = 0.0f;
    sProj[12] = -1.0f;
    sProj[13] = 1.0f;
    sProj[14] = 0.0f;
    sProj[15] = 1.0f;
}

void QuadRenderer::DrawQuad(float x, float y, float w, float h,
                            float r, float g, float b, float a,
                            GLuint texture,
                            float uvx0, float uvy0, float uvx1, float uvy1) {
    if (!sInitialized || w <= 0.0f || h <= 0.0f) {
        return;
    }

    float x2 = x + w;
    float y2 = y + h;

    // 6 vertices (2 triangles), interleaved position + uv.
    float vertices[6][4] = {
        { x,  y,  uvx0, uvy0 },
        { x2, y,  uvx1, uvy0 },
        { x2, y2, uvx1, uvy1 },
        { x,  y,  uvx0, uvy0 },
        { x2, y2, uvx1, uvy1 },
        { x,  y2, uvx0, uvy1 },
    };

    glUseProgram(sProgram);

    glUniformMatrix4fv(sUniformProj, 1, GL_FALSE, sProj);
    glUniform4f(sUniformColor, r, g, b, a);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture != 0 ? texture : sWhiteTexture);
    glUniform1i(sUniformTexture, 0);

    glBindVertexArray(sVAO);
    glBindBuffer(GL_ARRAY_BUFFER, sVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);
}

bool QuadRenderer::CreateShaders() {
    const char* vs = sIsES ? kVertexShaderES : kVertexShaderDesktop;
    const char* fs = sIsES ? kFragmentShaderES : kFragmentShaderDesktop;

    GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vs);
    if (!vertexShader) {
        return false;
    }

    GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fs);
    if (!fragmentShader) {
        glDeleteShader(vertexShader);
        return false;
    }

    sProgram = glCreateProgram();
    if (!sProgram) {
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        return false;
    }

    glAttachShader(sProgram, vertexShader);
    glAttachShader(sProgram, fragmentShader);
    glLinkProgram(sProgram);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    GLint status = GL_FALSE;
    glGetProgramiv(sProgram, GL_LINK_STATUS, &status);
    if (!status) {
        char log[1024];
        glGetProgramInfoLog(sProgram, sizeof(log), nullptr, log);
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "QuadRenderer: program link error: %s", log);
        glDeleteProgram(sProgram);
        sProgram = 0;
        return false;
    }

    sUniformProj = glGetUniformLocation(sProgram, "uProj");
    sUniformColor = glGetUniformLocation(sProgram, "uColor");
    sUniformTexture = glGetUniformLocation(sProgram, "uTex");

    return true;
}

bool QuadRenderer::CreateWhiteTexture() {
    const unsigned char pixel[4] = { 255, 255, 255, 255 };

    glGenTextures(1, &sWhiteTexture);
    glBindTexture(GL_TEXTURE_2D, sWhiteTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    return glGetError() == GL_NO_ERROR;
}
