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
    void initWorldEnvironment();
    void resetMission();

    // 3D Scene Rendering
    void renderWhisk3D();
    void renderSkyAndOcean();
    void renderSun();
    void renderClouds();
    void renderIslandsAndTrees();
    void renderAircraft();
    void renderEnemyJets();
    void renderTarget();
    void renderProjectiles();
    void renderExplosions();

    void spawnExplosion(float x, float y, float z, float maxRadius = 3.6f, float r = 1.0f, float g = 0.5f, float b = 0.15f);
    void spawnEnemySquadron();

    // 2D HUD & Menu UI Rendering (Optimized for Portrait Mode)
    void renderGameUI();
    void renderMainMenuUI();
    void renderPauseUI();
    void renderGameOverUI();
    void renderDigits(float x, float y, float charW, float charH, const std::string& text);
    void renderHUDQuad(float x, float y, float w, float h, GLuint texId, float alpha = 1.0f);
    void renderHUDBar(float x, float y, float w, float h, float fillPct, float r, float g, float b, float a);
    void renderHUDRect(float x, float y, float w, float h, float r, float g, float b, float a);
    void renderHUDLine(float x0, float y0, float x1, float y1, float r, float g, float b, float a, float width = 2.0f);
    bool projectWorldToScreen(float wx, float wy, float wz, float &outSx, float &outSy) const;

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
    float enemySpawnTimer_;

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

    // Target state (Warship / Boss DDG)
    float targetX_;
    float targetY_;
    float targetZ_;
    float targetHealth_;
    float targetMaxHealth_;
    float targetHitFlashTime_;
    bool targetLocked_;
    bool prevTargetLocked_;
    float warshipTurretAngle_;

    // Multi-touch Controls State (Rock solid Pointer-ID tracking)
    int stickPointerId_;
    int firePointerId_;
    int missilePointerId_;
    int bombPointerId_;
    bool touchDown_;
    float touchX_;
    float touchY_;
    bool stickActive_;
    float stickOriginX_;
    float stickOriginY_;
    float stickKnobX_;
    float stickKnobY_;
    float stickDeflectX_;
    float stickDeflectY_;
    bool firePressed_;
    bool missilePressed_;
    bool bombPressed_;
    float cannonCooldown_;
    float muzzleFlashTime_;
    float missileFlightTime_;
    float missileCooldown_;
    int missileTargetMode_; // 0: Warship, 1: Enemy Jet
    int missileTargetIdx_;
    int bombCount_;
    float bombCooldown_;

    struct Bullet {
        float x, y, z;
        float vx, vy, vz;
        float life;
    };
    std::vector<Bullet> bullets_;

    struct PlayerMissile {
        float x, y, z;
        float vx, vy, vz;
        float pitch, yaw;
        int targetMode;
        int targetIdx;
        float life;
        bool active;
    };
    std::vector<PlayerMissile> activeMissiles_;

    struct Bomb {
        float x, y, z;
        float vx, vy, vz;
        float pitch;
        float life;
        bool active;
    };
    std::vector<Bomb> bombs_;

    struct SmokeParticle {
        float x, y, z;
        float vx, vy, vz;
        float size;
        float life;
        float maxLife;
        float r, g, b, a;
    };
    std::vector<SmokeParticle> missileSmoke_;

    struct EnemyBullet {
        float x, y, z;
        float vx, vy, vz;
        float life;
    };
    std::vector<EnemyBullet> enemyBullets_;

    struct EnemyJet {
        float x, y, z;
        float vx, vy, vz;
        float roll, pitch, yaw;
        float health;
        float maxHealth;
        float fireCooldown;
        float hitFlashTime;
        bool active;
        int type;
        float flightTimer;
        float baseX;
        float targetApproachY;
        bool breakingAway;
    };
    std::vector<EnemyJet> enemyJets_;

    struct CloudInstance {
        float x, y, z;
        float scaleX, scaleY;
        float speed;
        float alpha;
    };
    std::vector<CloudInstance> clouds_;

    struct IslandEntity {
        float x, z;
        float scale;
        float angle;
        int type;
    };
    std::vector<IslandEntity> islands_;

    struct Tree3D {
        float x, y, z;
        float scale;
    };
    std::vector<Tree3D> trees_;

    struct ExplosionFX {
        float x, y, z;
        float radius;
        float maxRadius;
        float life;
        float maxLife;
        float r, g, b;
    };
    std::vector<ExplosionFX> explosions_;

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
    std::shared_ptr<TextureAsset> texEnemyJet_;
    std::shared_ptr<TextureAsset> texSea_;
    std::shared_ptr<TextureAsset> texTerrain_;
    std::shared_ptr<TextureAsset> texTarget_;
    std::shared_ptr<TextureAsset> texSun_;
    std::shared_ptr<TextureAsset> texCloud_;
    std::shared_ptr<TextureAsset> texFxExplosion_;
    std::shared_ptr<TextureAsset> texHudCrosshair_;
    std::shared_ptr<TextureAsset> texHudRadar_;
    std::shared_ptr<TextureAsset> texBtnFire_;
    std::shared_ptr<TextureAsset> texBtnMissile_;
    std::shared_ptr<TextureAsset> texBtnBomb_;
    std::shared_ptr<TextureAsset> texBtnStick_;
    std::shared_ptr<TextureAsset> texBtnStickBase_;
    std::shared_ptr<TextureAsset> texBtnStickKnob_;
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
