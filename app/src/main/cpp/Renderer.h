#ifndef ANDROIDGLINVESTIGATIONS_RENDERER_H
#define ANDROIDGLINVESTIGATIONS_RENDERER_H

#include <EGL/egl.h>
#include <memory>
#include <vector>
#include <string>

#include "Model.h"
#include "Shader.h"
#include "TextureAsset.h"

struct android_app;

namespace w3dEngine {
    class W3dSound;
}

class Renderer {
public:
    explicit Renderer(android_app *pApp);
    virtual ~Renderer();

    void handleInput();
    void render();

private:
    void initRenderer();
    void updateRenderArea();
    void initWhisk3D();
    void loadGameTextures();

    // 3D Scene Rendering
    void renderWhisk3D();
    void renderSkyAndOcean();
    void renderIslands();
    void renderAircraft();
    void renderTarget();
    void renderProjectiles();

    // 2D HUD UI Rendering
    void renderGameUI();
    void renderHUDQuad(float x, float y, float w, float h, GLuint texId, float alpha = 1.0f);
    void renderHUDBar(float x, float y, float w, float h, float fillPct, float r, float g, float b, float a);

    android_app *app_;
    EGLDisplay display_;
    EGLSurface surface_;
    EGLContext context_;
    EGLint width_;
    EGLint height_;

    bool shaderNeedsNewProjectionMatrix_;
    float timeSec_;

    // Player Flight Telemetry & Controls
    float planePitch_;
    float planeRoll_;
    float planeYaw_;
    float planeX_;
    float planeY_;
    float planeZ_;
    float speedKnots_;
    float altitudeFeet_;
    float healthPct_;
    int missileCount_;
    int enemiesDestroyed_;

    // Target state
    float targetX_;
    float targetY_;
    float targetZ_;
    bool targetLocked_;

    // Touch Controls State
    bool touchDown_;
    float touchX_;
    float touchY_;
    bool stickActive_;
    int stickPointerId_;
    float stickDeflectX_;
    float stickDeflectY_;
    bool firePressed_;
    bool missilePressed_;
    float muzzleFlashTime_;
    float missileFlightTime_;

    struct Bullet {
        float x, y, z;
        float vx, vy, vz;
        float life;
    };
    std::vector<Bullet> bullets_;

    // Audio State & Sounds
    bool soundEnabled_;
    int engineVoiceId_;
    w3dEngine::W3dSound* sndEngine_;
    w3dEngine::W3dSound* sndCannon_;
    w3dEngine::W3dSound* sndMissile_;
    w3dEngine::W3dSound* sndExplosion_;
    w3dEngine::W3dSound* sndLock_;

    // Textures
    std::shared_ptr<TextureAsset> texAirplane_;
    std::shared_ptr<TextureAsset> texSea_;
    std::shared_ptr<TextureAsset> texTerrain_;
    std::shared_ptr<TextureAsset> texTarget_;
    std::shared_ptr<TextureAsset> texHudCrosshair_;
    std::shared_ptr<TextureAsset> texHudRadar_;
    std::shared_ptr<TextureAsset> texBtnFire_;
    std::shared_ptr<TextureAsset> texBtnMissile_;
    std::shared_ptr<TextureAsset> texBtnStick_;
    std::shared_ptr<TextureAsset> texBtnSound_;
};

#endif // ANDROIDGLINVESTIGATIONS_RENDERER_H
