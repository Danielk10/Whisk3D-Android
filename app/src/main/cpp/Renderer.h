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

enum GameState {
    STATE_MAIN_MENU = 0,
    STATE_PLAYING,
    STATE_PAUSED,
    STATE_GAME_OVER
};

class Renderer {
public:
    explicit Renderer(android_app *pApp);
    virtual ~Renderer();

    void handleInput();
    void render();

    bool canRender() const { return surface_ != EGL_NO_SURFACE && context_ != EGL_NO_CONTEXT; }
    void onWindowInit();
    void onWindowTerm();

private:
    void initRenderer();
    void updateRenderArea();
    void initWhisk3D();
    void loadGameTextures();
    void resetMission();

    // 3D Scene Rendering
    void renderWhisk3D();
    void renderSkyAndOcean();
    void renderIslands();
    void renderAircraft();
    void renderTarget();
    void renderProjectiles();

    // 2D HUD & Menu UI Rendering
    void renderGameUI();
    void renderMainMenuUI();
    void renderPauseUI();
    void renderGameOverUI();
    void renderDigits(float x, float y, float charW, float charH, const std::string& text);
    void renderHUDQuad(float x, float y, float w, float h, GLuint texId, float alpha = 1.0f);
    void renderHUDBar(float x, float y, float w, float h, float fillPct, float r, float g, float b, float a);
    void renderHUDRect(float x, float y, float w, float h, float r, float g, float b, float a);
    void renderHUDLine(float x0, float y0, float x1, float y1, float r, float g, float b, float a, float width = 2.0f);

    android_app *app_;
    EGLDisplay display_;
    EGLSurface surface_;
    EGLContext context_;
    EGLConfig config_;
    EGLint width_;
    EGLint height_;

    bool shaderNeedsNewProjectionMatrix_;
    float timeSec_;

    // Game State & Flow
    GameState gameState_;
    bool showHelpModal_;
    int highScore_;
    float flakCooldown_;

    // Player Flight Telemetry & Controls
    float planePitch_;
    float planeRoll_;
    float planeYaw_;
    float targetPitch_;
    float targetRoll_;
    float targetYaw_;
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
    float targetHealth_;
    float targetMaxHealth_;
    float targetHitFlashTime_;
    bool targetLocked_;
    bool prevTargetLocked_;

    // Touch Controls State
    bool touchDown_;
    float touchX_;
    float touchY_;
    bool stickActive_;
    int stickPointerId_;
    float stickOriginX_;
    float stickOriginY_;
    float stickDeflectX_;
    float stickDeflectY_;
    bool firePressed_;
    bool missilePressed_;
    float cannonCooldown_;
    float muzzleFlashTime_;
    float missileFlightTime_;

    struct Bullet {
        float x, y, z;
        float vx, vy, vz;
        float life;
    };
    std::vector<Bullet> bullets_;

    struct EnemyBullet {
        float x, y, z;
        float vx, vy, vz;
        float life;
    };
    std::vector<EnemyBullet> enemyBullets_;

    // Audio State & Sounds
    bool soundEnabled_;
    int engineVoiceId_;
    w3dEngine::W3dSound* sndEngine_;
    w3dEngine::W3dSound* sndCannon_;
    w3dEngine::W3dSound* sndMissile_;
    w3dEngine::W3dSound* sndExplosion_;
    w3dEngine::W3dSound* sndLock_;

    // In-game 3D & HUD Textures
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

    // Menu UI Textures
    std::shared_ptr<TextureAsset> texMenuTitle_;
    std::shared_ptr<TextureAsset> texBtnPlay_;
    std::shared_ptr<TextureAsset> texBtnResume_;
    std::shared_ptr<TextureAsset> texBtnRestart_;
    std::shared_ptr<TextureAsset> texBtnQuit_;
    std::shared_ptr<TextureAsset> texBtnPause_;
    std::shared_ptr<TextureAsset> texBtnHelp_;
    std::shared_ptr<TextureAsset> texDialogHelp_;
    std::shared_ptr<TextureAsset> texDialogPause_;
    std::shared_ptr<TextureAsset> texDialogGameOver_;
    std::shared_ptr<TextureAsset> texHudDigits_;
};

#endif // ANDROIDGLINVESTIGATIONS_RENDERER_H
