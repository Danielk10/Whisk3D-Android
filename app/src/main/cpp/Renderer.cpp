#include "Renderer.h"

#include <game-activity/native_app_glue/android_native_app_glue.h>
#include <GLES3/gl3.h>
#include <memory>
#include <vector>
#include <android/imagedecoder.h>

#include "AndroidOut.h"
#include "Shader.h"
#include "Utility.h"
#include "TextureAsset.h"

// Whisk3D Core includes
#include "gfx/w3dGraphics.h"
#include "io/w3dFilesystem.h"
#include "physics/W3dFisica.h"

#define PRINT_GL_STRING(s) {aout << #s": "<< glGetString(s) << std::endl;}

#define PRINT_GL_STRING_AS_LIST(s) { \
std::istringstream extensionStream((const char *) glGetString(s));\
std::vector<std::string> extensionList(\
        std::istream_iterator<std::string>{extensionStream},\
        std::istream_iterator<std::string>());\
aout << #s":\n";\
for (auto& extension: extensionList) {\
    aout << extension << "\n";\
}\
aout << std::endl;\
}

Renderer::~Renderer() {
    if (display_ != EGL_NO_DISPLAY) {
        eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (context_ != EGL_NO_CONTEXT) {
            eglDestroyContext(display_, context_);
            context_ = EGL_NO_CONTEXT;
        }
        if (surface_ != EGL_NO_SURFACE) {
            eglDestroySurface(display_, surface_);
            surface_ = EGL_NO_SURFACE;
        }
        eglTerminate(display_);
        display_ = EGL_NO_DISPLAY;
    }
}

void Renderer::initWhisk3D() {
    aout << "Inicializando Whisk3D Core en Android..." << std::endl;

    // Configurar el sistema de archivos de Whisk3D para leer desde los assets del APK
    w3dFileSystem::SetAssetManager(app_->activity->assetManager);
    if (app_->activity->internalDataPath) {
        w3dFileSystem::SetUserDataDir(app_->activity->internalDataPath);
    }

    // Inicializar backend GLES2/3 de Whisk3D
    w3dEngine::GLES2Init(nullptr);

    // Estados de renderizado por defecto
    w3dEngine::Enable(w3dEngine::DepthTest);
    w3dEngine::DepthFunc(w3dEngine::DepthLEqual);
    w3dEngine::Enable(w3dEngine::CullFace);

    aout << "Whisk3D Core inicializado con exito!" << std::endl;
}

void Renderer::renderWhisk3D() {
    angle_ += 1.0f;
    if (angle_ >= 360.0f) angle_ -= 360.0f;

    // Paso de simulacion fisica de Whisk3D
    W3dFisicaPaso(1.0f / 60.0f);

    // Configurar viewport en Whisk3D
    w3dEngine::Viewport(0, 0, width_, height_);

    // Matriz de Proyeccion
    float aspect = (height_ > 0) ? static_cast<float>(width_) / static_cast<float>(height_) : 1.0f;
    w3dEngine::MatrixMode(w3dEngine::Projection);
    w3dEngine::LoadIdentity();
    w3dEngine::Perspective(60.0f, aspect, 0.1f, 100.0f);

    // Matriz de ModelView
    w3dEngine::MatrixMode(w3dEngine::ModelView);
    w3dEngine::LoadIdentity();
    w3dEngine::Translatef(0.0f, 0.0f, -4.0f);
    w3dEngine::Rotatef(angle_, 1.0f, 1.0f, 0.5f);

    // Geometria de cubo 3D retro con color por vertice
    static const float cubeVertices[] = {
        // Front
        -1.0f, -1.0f,  1.0f,   1.0f, -1.0f,  1.0f,   1.0f,  1.0f,  1.0f,  -1.0f,  1.0f,  1.0f,
        // Back
        -1.0f, -1.0f, -1.0f,  -1.0f,  1.0f, -1.0f,   1.0f,  1.0f, -1.0f,   1.0f, -1.0f, -1.0f,
        // Top
        -1.0f,  1.0f, -1.0f,  -1.0f,  1.0f,  1.0f,   1.0f,  1.0f,  1.0f,   1.0f,  1.0f, -1.0f,
        // Bottom
        -1.0f, -1.0f, -1.0f,   1.0f, -1.0f, -1.0f,   1.0f, -1.0f,  1.0f,  -1.0f, -1.0f,  1.0f,
        // Right
         1.0f, -1.0f, -1.0f,   1.0f,  1.0f, -1.0f,   1.0f,  1.0f,  1.0f,   1.0f, -1.0f,  1.0f,
        // Left
        -1.0f, -1.0f, -1.0f,  -1.0f, -1.0f,  1.0f,  -1.0f,  1.0f,  1.0f,  -1.0f,  1.0f, -1.0f
    };

    static const unsigned char cubeColors[] = {
        // Front (Cyan)
        0, 200, 255, 255,   0, 200, 255, 255,   100, 255, 255, 255,   100, 255, 255, 255,
        // Back (Rojo/Magenta)
        255, 50, 100, 255,  255, 50, 100, 255,  255, 100, 150, 255,   255, 100, 150, 255,
        // Top (Amarillo)
        255, 200, 0, 255,   255, 200, 0, 255,   255, 240, 50, 255,    255, 240, 50, 255,
        // Bottom (Verde)
        50, 220, 50, 255,   50, 220, 50, 255,   80, 255, 80, 255,     80, 255, 80, 255,
        // Right (Purpura)
        180, 50, 255, 255,  180, 50, 255, 255,  200, 80, 255, 255,    200, 80, 255, 255,
        // Left (Naranja)
        255, 120, 0, 255,   255, 120, 0, 255,   255, 160, 50, 255,    255, 160, 50, 255
    };

    static const MeshIndex cubeIndices[] = {
        0, 1, 2,   0, 2, 3,       // Front
        4, 5, 6,   4, 6, 7,       // Back
        8, 9, 10,  8, 10, 11,     // Top
        12, 13, 14, 12, 14, 15,   // Bottom
        16, 17, 18, 16, 18, 19,   // Right
        20, 21, 22, 20, 22, 23    // Left
    };

    w3dEngine::Enable(w3dEngine::DepthTest);
    w3dEngine::Enable(w3dEngine::ColorMaterial);
    w3dEngine::EnableArray(w3dEngine::VertexArray);
    w3dEngine::EnableArray(w3dEngine::ColorArray);

    w3dEngine::VertexPointer3f(0, cubeVertices);
    w3dEngine::ColorPointer4ub(cubeColors);

    w3dEngine::DrawTriangles(36, cubeIndices);

    w3dEngine::DisableArray(w3dEngine::ColorArray);
    w3dEngine::DisableArray(w3dEngine::VertexArray);
}

void Renderer::render() {
    updateRenderArea();

    // Limpiar pantalla usando el sistema de renderizado de Whisk3D
    w3dEngine::ClearColor(0.10f, 0.12f, 0.16f, 1.0f);
    w3dEngine::Clear(w3dEngine::ColorBuffer | w3dEngine::DepthBuffer);

    // Dibujar escena 3D con Whisk3D
    renderWhisk3D();

    auto swapResult = eglSwapBuffers(display_, surface_);
    assert(swapResult == EGL_TRUE);
}

void Renderer::initRenderer() {
    constexpr EGLint attribs[] = {
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
            EGL_BLUE_SIZE, 8,
            EGL_GREEN_SIZE, 8,
            EGL_RED_SIZE, 8,
            EGL_DEPTH_SIZE, 24,
            EGL_NONE
    };

    auto display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(display, nullptr, nullptr);

    EGLint numConfigs;
    eglChooseConfig(display, attribs, nullptr, 0, &numConfigs);

    std::unique_ptr<EGLConfig[]> supportedConfigs(new EGLConfig[numConfigs]);
    eglChooseConfig(display, attribs, supportedConfigs.get(), numConfigs, &numConfigs);

    auto config = *std::find_if(
            supportedConfigs.get(),
            supportedConfigs.get() + numConfigs,
            [&display](const EGLConfig &config) {
                EGLint red, green, blue, depth;
                if (eglGetConfigAttrib(display, config, EGL_RED_SIZE, &red)
                    && eglGetConfigAttrib(display, config, EGL_GREEN_SIZE, &green)
                    && eglGetConfigAttrib(display, config, EGL_BLUE_SIZE, &blue)
                    && eglGetConfigAttrib(display, config, EGL_DEPTH_SIZE, &depth)) {

                    aout << "Config con " << red << ", " << green << ", " << blue << ", "
                         << depth << std::endl;
                    return red == 8 && green == 8 && blue == 8 && depth == 24;
                }
                return false;
            });

    aout << "Configs encontrados: " << numConfigs << std::endl;

    EGLint format;
    eglGetConfigAttrib(display, config, EGL_NATIVE_VISUAL_ID, &format);
    EGLSurface surface = eglCreateWindowSurface(display, config, app_->window, nullptr);

    EGLint contextAttribs[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
    EGLContext context = eglCreateContext(display, config, nullptr, contextAttribs);

    auto madeCurrent = eglMakeCurrent(display, surface, surface, context);
    assert(madeCurrent);

    display_ = display;
    surface_ = surface;
    context_ = context;

    width_ = -1;
    height_ = -1;

    PRINT_GL_STRING(GL_VENDOR);
    PRINT_GL_STRING(GL_RENDERER);
    PRINT_GL_STRING(GL_VERSION);

    // Inicializar Whisk3D Core
    initWhisk3D();
}

void Renderer::updateRenderArea() {
    EGLint width;
    eglQuerySurface(display_, surface_, EGL_WIDTH, &width);

    EGLint height;
    eglQuerySurface(display_, surface_, EGL_HEIGHT, &height);

    if (width != width_ || height != height_) {
        width_ = width;
        height_ = height;
        glViewport(0, 0, width, height);
        shaderNeedsNewProjectionMatrix_ = true;
    }
}

void Renderer::handleInput() {
    // Manejo de eventos de entrada
    auto *inputBuffer = android_app_swap_input_buffers(app_);
    if (!inputBuffer) {
        return;
    }

    for (auto i = 0; i < inputBuffer->motionEventsCount; i++) {
        auto &motionEvent = inputBuffer->motionEvents[i];
        auto action = motionEvent.action;
        auto pointerIndex = (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK)
                >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
        auto &pointer = motionEvent.pointers[pointerIndex];
        auto x = GameActivityPointerAxes_getX(&pointer);
        auto y = GameActivityPointerAxes_getY(&pointer);
        (void)x; (void)y;
    }
    android_app_clear_motion_events(inputBuffer);
    android_app_clear_key_events(inputBuffer);
}
