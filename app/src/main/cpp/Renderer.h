#ifndef ANDROIDGLINVESTIGATIONS_RENDERER_H
#define ANDROIDGLINVESTIGATIONS_RENDERER_H

#include <EGL/egl.h>
#include <memory>
#include <vector>

#include "Model.h"
#include "Shader.h"

struct android_app;

class Renderer {
public:
    inline Renderer(android_app *pApp) :
            app_(pApp),
            display_(EGL_NO_DISPLAY),
            surface_(EGL_NO_SURFACE),
            context_(EGL_NO_CONTEXT),
            width_(0),
            height_(0),
            shaderNeedsNewProjectionMatrix_(true),
            angle_(0.0f) {
        initRenderer();
    }

    virtual ~Renderer();

    void handleInput();
    void render();

private:
    void initRenderer();
    void updateRenderArea();
    void createModels();

    // Whisk3D Core integration
    void initWhisk3D();
    void renderWhisk3D();

    android_app *app_;
    EGLDisplay display_;
    EGLSurface surface_;
    EGLContext context_;
    EGLint width_;
    EGLint height_;

    bool shaderNeedsNewProjectionMatrix_;
    float angle_;

    std::unique_ptr<Shader> shader_;
    std::vector<Model> models_;
};

#endif //ANDROIDGLINVESTIGATIONS_RENDERER_H
