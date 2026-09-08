#include "Renderer.h"

#include <game-activity/native_app_glue/android_native_app_glue.h>
#include <GLES3/gl3.h>
#include <memory>
#include <vector>
#include <cmath>
#include <algorithm>
#include <cassert>
#include <string>

#include "AndroidOut.h"
#include "Shader.h"
#include "Utility.h"
#include "TextureAsset.h"

// Whisk3D Core includes
#include "gfx/w3dGraphics.h"
#include "io/w3dFilesystem.h"
#include "physics/W3dFisica.h"
#include "audio/W3dAudio.h"

#define CLAMP(v, min_v, max_v) ((v) < (min_v) ? (min_v) : ((v) > (max_v) ? (max_v) : (v)))

Renderer::Renderer(android_app *pApp) :
        app_(pApp),
        display_(EGL_NO_DISPLAY),
        surface_(EGL_NO_SURFACE),
        context_(EGL_NO_CONTEXT),
        config_(nullptr),
        width_(0),
        height_(0),
        shaderNeedsNewProjectionMatrix_(true),
        timeSec_(0.0f),
        gameState_(STATE_MAIN_MENU),
        showHelpModal_(false),
        highScore_(0),
        flakCooldown_(2.2f),
        enemySpawnTimer_(1.5f),
        planePitch_(0.0f),
        planeRoll_(0.0f),
        planeYaw_(0.0f),
        targetPitch_(0.0f),
        targetRoll_(0.0f),
        targetYaw_(0.0f),
        planeX_(0.0f),
        planeY_(0.0f),
        planeZ_(0.0f),
        speedKnots_(480.0f),
        altitudeFeet_(2400.0f),
        healthPct_(1.0f),
        missileCount_(4),
        enemiesDestroyed_(0),
        targetX_(0.0f),
        targetY_(-2.5f),
        targetZ_(-120.0f),
        targetHealth_(100.0f),
        targetMaxHealth_(100.0f),
        targetHitFlashTime_(0.0f),
        targetLocked_(false),
        prevTargetLocked_(false),
        warshipTurretAngle_(0.0f),
        stickPointerId_(-1),
        firePointerId_(-1),
        missilePointerId_(-1),
        bombPointerId_(-1),
        touchDown_(false),
        touchX_(0.0f),
        touchY_(0.0f),
        stickActive_(false),
        stickOriginX_(120.0f),
        stickOriginY_(400.0f),
        stickKnobX_(120.0f),
        stickKnobY_(400.0f),
        stickDeflectX_(0.0f),
        stickDeflectY_(0.0f),
        firePressed_(false),
        missilePressed_(false),
        bombPressed_(false),
        cannonCooldown_(0.0f),
        muzzleFlashTime_(0.0f),
        missileFlightTime_(0.0f),
        missileCooldown_(0.0f),
        missileTargetMode_(0),
        missileTargetIdx_(-1),
        bombCount_(4),
        bombCooldown_(0.0f),
        soundEnabled_(true),
        engineVoiceId_(0),
        sndEngine_(nullptr),
        sndCannon_(nullptr),
        sndMissile_(nullptr),
        sndExplosion_(nullptr),
        sndLock_(nullptr) {
    initWorldEnvironment();
    initRenderer();
}

Renderer::~Renderer() {
    if (engineVoiceId_ > 0) {
        w3dEngine::W3dSoundStop(engineVoiceId_);
        engineVoiceId_ = 0;
    }
    if (sndEngine_)    { w3dEngine::W3dSoundFree(sndEngine_); sndEngine_ = nullptr; }
    if (sndCannon_)    { w3dEngine::W3dSoundFree(sndCannon_); sndCannon_ = nullptr; }
    if (sndMissile_)   { w3dEngine::W3dSoundFree(sndMissile_); sndMissile_ = nullptr; }
    if (sndExplosion_) { w3dEngine::W3dSoundFree(sndExplosion_); sndExplosion_ = nullptr; }
    if (sndLock_)      { w3dEngine::W3dSoundFree(sndLock_); sndLock_ = nullptr; }
    w3dEngine::W3dAudioShutdown();

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

void Renderer::onWindowInit() {
    if (display_ != EGL_NO_DISPLAY && context_ != EGL_NO_CONTEXT && app_->window != nullptr) {
        surface_ = eglCreateWindowSurface(display_, config_, app_->window, nullptr);
        if (surface_ != EGL_NO_SURFACE) {
            eglMakeCurrent(display_, surface_, surface_, context_);
            updateRenderArea();
            aout << "Whisk3D: Superficie EGL restaurada con éxito en modo retrato." << std::endl;
        }
    }
}

void Renderer::onWindowTerm() {
    if (display_ != EGL_NO_DISPLAY && surface_ != EGL_NO_SURFACE) {
        eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        eglDestroySurface(display_, surface_);
        surface_ = EGL_NO_SURFACE;
        aout << "Whisk3D: Superficie EGL liberada en pausa de ventana." << std::endl;
    }
}

void Renderer::onPause() {
    w3dEngine::W3dAudioPause();
}

void Renderer::onResume() {
    if (soundEnabled_) {
        w3dEngine::W3dAudioResume();
    }
}

void Renderer::initWorldEnvironment() {
    clouds_.clear();
    islands_.clear();
    trees_.clear();

    // 14 nubes estilizadas flotando a distintas alturas
    for (int i = 0; i < 14; ++i) {
        CloudInstance c;
        c.x = ((rand() % 140) - 70) * 1.0f;
        c.y = 8.0f + (rand() % 24) * 1.0f;
        c.z = -250.0f + (i * 20.0f) + (rand() % 12);
        c.scaleX = 14.0f + (rand() % 10);
        c.scaleY = 7.0f + (rand() % 6);
        c.speed = 12.0f + (rand() % 8);
        c.alpha = 0.75f + (rand() % 25) * 0.01f;
        clouds_.push_back(c);
    }

    // Archipiélago de islas tropicales inspirado en el logo
    islands_.push_back({ -32.0f, -90.0f, 1.30f, 0.0f, 0 });   // Isla montañosa principal
    islands_.push_back({  36.0f, -145.0f, 1.10f, 25.0f, 1 });  // Atolón derecho
    islands_.push_back({ -35.0f, -195.0f, 1.00f, -15.0f, 1 }); // Atolón izquierdo
    islands_.push_back({  24.0f, -245.0f, 1.35f, 10.0f, 0 });  // Isla volcánica lejana
    islands_.push_back({ -10.0f, -38.0f, 0.75f, 45.0f, 2 });   // Islote cercano
    islands_.push_back({  30.0f, -65.0f, 0.85f, -30.0f, 2 });  // Islote intermedio

    // Vegetación 3D (Árboles tropicales / palmeras) sobre las laderas verdes
    trees_.push_back({ -38.0f, 0.0f, -88.0f, 1.0f });
    trees_.push_back({ -34.0f, 1.8f, -82.0f, 1.1f });
    trees_.push_back({ -30.0f, 3.2f, -94.0f, 1.0f });
    trees_.push_back({ -26.0f, 1.8f, -86.0f, 1.2f });
    trees_.push_back({ -40.0f, -0.4f, -94.0f, 0.9f });
    trees_.push_back({ -32.0f, 4.6f, -98.0f, 1.0f });
    trees_.push_back({ -22.0f, 0.4f, -84.0f, 1.0f });

    trees_.push_back({ 30.0f, 0.0f, -140.0f, 1.0f });
    trees_.push_back({ 36.0f, 1.8f, -148.0f, 1.1f });
    trees_.push_back({ 42.0f, 0.8f, -142.0f, 0.95f });
    trees_.push_back({ 34.0f, 2.5f, -152.0f, 1.0f });

    trees_.push_back({ -38.0f, 0.4f, -192.0f, 1.0f });
    trees_.push_back({ -32.0f, 1.6f, -198.0f, 1.1f });
    trees_.push_back({ -28.0f, 0.0f, -194.0f, 0.9f });

    trees_.push_back({ 18.0f, 1.0f, -240.0f, 1.2f });
    trees_.push_back({ 24.0f, 3.8f, -248.0f, 1.1f });
    trees_.push_back({ 30.0f, 2.2f, -252.0f, 1.0f });
    trees_.push_back({ 22.0f, 5.0f, -244.0f, 1.0f });
}

void Renderer::resetMission() {
    planeX_ = 0.0f;
    planeY_ = 0.0f;
    planePitch_ = 0.0f;
    planeRoll_ = 0.0f;
    planeYaw_ = 0.0f;
    targetPitch_ = 0.0f;
    targetRoll_ = 0.0f;
    targetYaw_ = 0.0f;
    healthPct_ = 1.0f;
    missileCount_ = 4;
    bombCount_ = 4;
    bombCooldown_ = 0.0f;
    missileCooldown_ = 0.0f;
    enemiesDestroyed_ = 0;

    targetX_ = 0.0f;
    targetY_ = -2.5f;
    targetZ_ = -120.0f;
    targetHealth_ = targetMaxHealth_;
    targetHitFlashTime_ = 0.0f;
    targetLocked_ = false;
    prevTargetLocked_ = false;

    bullets_.clear();
    activeMissiles_.clear();
    bombs_.clear();
    missileSmoke_.clear();
    enemyBullets_.clear();
    enemyJets_.clear();
    explosions_.clear();
    explosionDebris_.clear();

    stickPointerId_ = -1;
    firePointerId_ = -1;
    missilePointerId_ = -1;
    bombPointerId_ = -1;
    stickActive_ = false;
    firePressed_ = false;
    missilePressed_ = false;
    bombPressed_ = false;
    stickDeflectX_ = 0.0f;
    stickDeflectY_ = 0.0f;
    missileFlightTime_ = 0.0f;
    targetHitFlashTime_ = 0.0f;
    muzzleFlashTime_ = 0.0f;
    flakCooldown_ = 2.0f;
    enemySpawnTimer_ = 1.2f;
}

void Renderer::initWhisk3D() {
    aout << "Whisk3D: Inicializando motor retro y backend grafico GLES2/3..." << std::endl;

    w3dFileSystem::SetAssetManager(app_->activity->assetManager);
    if (app_->activity->internalDataPath) {
        w3dFileSystem::SetUserDataDir(app_->activity->internalDataPath);
    }

    w3dEngine::GLES2Init(nullptr);

    if (w3dEngine::W3dAudioInit(44100)) {
        aout << "Whisk3D: Audio OpenSL ES inicializado!" << std::endl;
        sndEngine_    = w3dEngine::W3dSoundLoad("sounds/engine.wav");
        sndCannon_    = w3dEngine::W3dSoundLoad("sounds/cannon.wav");
        sndMissile_   = w3dEngine::W3dSoundLoad("sounds/missile.wav");
        sndExplosion_ = w3dEngine::W3dSoundLoad("sounds/explosion.wav");
        sndLock_      = w3dEngine::W3dSoundLoad("sounds/lock.wav");

        if (sndEngine_) {
            engineVoiceId_ = w3dEngine::W3dSoundPlay(sndEngine_, 0.16f, true);
        }
    }

    loadGameTextures();
    aout << "Whisk3D: Escena, texturas y modelos listos!" << std::endl;
}

void Renderer::loadGameTextures() {
    aout << "Whisk3D: Cargando texturas fieles al logo..." << std::endl;
    auto assetMgr = app_->activity->assetManager;
    if (!assetMgr) return;

    // 3D Scene Textures
    texAirplane_     = TextureAsset::loadAsset(assetMgr, "textures/airplane.png");
    texEnemyJet_     = TextureAsset::loadAsset(assetMgr, "textures/enemy_jet.png");
    texSea_          = TextureAsset::loadAsset(assetMgr, "textures/sea.png");
    texTerrain_      = TextureAsset::loadAsset(assetMgr, "textures/terrain.png");
    texTarget_       = TextureAsset::loadAsset(assetMgr, "textures/target.png");
    texSun_          = TextureAsset::loadAsset(assetMgr, "textures/sun.png");
    texCloud_        = TextureAsset::loadAsset(assetMgr, "textures/cloud.png");
    texFxExplosion_  = TextureAsset::loadAsset(assetMgr, "textures/fx_explosion.png");

    // In-game HUD Textures
    texHudCrosshair_ = TextureAsset::loadAsset(assetMgr, "textures/hud_crosshair.png");
    texHudRadar_     = TextureAsset::loadAsset(assetMgr, "textures/hud_radar.png");
    texBtnFire_      = TextureAsset::loadAsset(assetMgr, "textures/btn_fire.png");
    texBtnMissile_   = TextureAsset::loadAsset(assetMgr, "textures/btn_missile.png");
    texBtnBomb_      = TextureAsset::loadAsset(assetMgr, "textures/btn_bomb.png");
    texBtnStick_     = TextureAsset::loadAsset(assetMgr, "textures/btn_stick.png");
    texBtnStickBase_ = TextureAsset::loadAsset(assetMgr, "textures/btn_stick_base.png");
    texBtnStickKnob_ = TextureAsset::loadAsset(assetMgr, "textures/btn_stick_knob.png");
    texBtnSound_     = TextureAsset::loadAsset(assetMgr, "textures/btn_sound.png");

    // Menu UI Textures
    texMenuTitle_      = TextureAsset::loadAsset(assetMgr, "textures/menu_title.png");
    texBtnPlay_        = TextureAsset::loadAsset(assetMgr, "textures/btn_play.png");
    texBtnResume_      = TextureAsset::loadAsset(assetMgr, "textures/btn_resume.png");
    texBtnRestart_     = TextureAsset::loadAsset(assetMgr, "textures/btn_restart.png");
    texBtnQuit_        = TextureAsset::loadAsset(assetMgr, "textures/btn_quit.png");
    texBtnPause_       = TextureAsset::loadAsset(assetMgr, "textures/btn_pause.png");
    texBtnHelp_        = TextureAsset::loadAsset(assetMgr, "textures/btn_help.png");
    texDialogHelp_     = TextureAsset::loadAsset(assetMgr, "textures/dialog_help.png");
    texDialogPause_    = TextureAsset::loadAsset(assetMgr, "textures/dialog_pause.png");
    texDialogGameOver_ = TextureAsset::loadAsset(assetMgr, "textures/dialog_gameover.png");
    texHudDigits_      = TextureAsset::loadAsset(assetMgr, "textures/hud_digits.png");
}

void Renderer::spawnExplosion(float x, float y, float z, float maxRadius, float r, float g, float b) {
    ExplosionFX exp;
    exp.x = x;
    exp.y = y;
    exp.z = z;
    exp.radius = 0.4f;
    exp.maxRadius = maxRadius;
    exp.life = 0.65f;
    exp.maxLife = 0.65f;
    exp.r = r;
    exp.g = g;
    exp.b = b;
    exp.angle = (rand() % 628) * 0.01f;
    explosions_.push_back(exp);

    // Para impactos medios/grandes, generar explosiones secundarias escalonadas y chispas
    if (maxRadius >= 2.0f) {
        int extraCount = (maxRadius >= 4.5f) ? 3 : 1;
        for (int k = 0; k < extraCount; ++k) {
            ExplosionFX sub;
            sub.x = x + (rand() % 20 - 10) * 0.08f * maxRadius;
            sub.y = y + (rand() % 20 - 10) * 0.08f * maxRadius;
            sub.z = z + (rand() % 20 - 10) * 0.08f * maxRadius;
            sub.radius = 0.3f;
            sub.maxRadius = maxRadius * (0.55f + (rand() % 30) * 0.01f);
            sub.life = 0.55f + (rand() % 20) * 0.01f;
            sub.maxLife = sub.life;
            sub.r = r;
            sub.g = g * 0.9f;
            sub.b = b * 0.8f;
            sub.angle = (rand() % 628) * 0.01f;
            explosions_.push_back(sub);
        }

        // Chispas y restos incandescentes volando con física y gravedad
        int debrisCount = (maxRadius >= 5.0f) ? 14 : 7;
        for (int k = 0; k < debrisCount; ++k) {
            ExplosionDebris deb;
            deb.x = x;
            deb.y = y;
            deb.z = z;
            float spd = 12.0f + (rand() % 35) * 0.5f;
            float phi = (rand() % 628) * 0.01f;
            float costheta = (rand() % 200 - 100) * 0.01f;
            float sintheta = std::sqrt(std::max(0.0f, 1.0f - costheta * costheta));
            deb.vx = spd * sintheta * std::cos(phi);
            deb.vy = std::fabs(spd * sintheta * std::sin(phi)) + 6.0f; // Impulso ascendente
            deb.vz = spd * costheta;
            deb.size = 0.15f + (rand() % 15) * 0.01f;
            deb.life = 0.45f + (rand() % 35) * 0.01f;
            deb.maxLife = deb.life;
            deb.r = 1.0f;
            deb.g = 0.65f + (rand() % 35) * 0.01f;
            deb.b = 0.15f;
            explosionDebris_.push_back(deb);
        }
    }
}

void Renderer::spawnEnemySquadron() {
    int count = 2 + (rand() % 2); // 2 o 3 cazas enemigos en escuadrilla
    float baseSquadX = (rand() % 32 - 16) * 0.15f; // Entre -2.4m y +2.4m
    int squadPattern = rand() % 3; // 0: Interceptores en V, 1: Evasivos Zigzag, 2: Ataque en Gran Altitud

    for (int i = 0; i < count; ++i) {
        EnemyJet ej;
        ej.type = squadPattern;
        ej.flightTimer = 0.0f;
        ej.breakingAway = false;
        ej.hitFlashTime = 0.0f;
        ej.active = true;
        ej.pitch = 0.0f;
        ej.yaw = 0.0f;

        if (squadPattern == 2) {
            // Caza Pesado / Bombardero de Gran Altitud (aprovecha la nueva altura del cielo)
            ej.baseX = (i == 0 ? 0.0f : (i == 1 ? -2.6f : 2.6f));
            ej.x = ej.baseX;
            ej.y = 3.6f + (i * 0.45f);
            ej.targetApproachY = ej.y;
            ej.z = -135.0f - (i * 12.0f);
            ej.vx = 0.0f;
            ej.vy = 0.0f;
            ej.vz = 22.0f; // Permite 7-8 segundos en pantalla
            ej.health = 32.0f;
            ej.maxHealth = 32.0f;
            ej.fireCooldown = 1.4f + (rand() % 10) * 0.1f;
            ej.roll = (i == 1 ? -12.0f : (i == 2 ? 12.0f : 0.0f));
        } else if (squadPattern == 1) {
            // Cazas Evasivos con Zigzag dinámico
            ej.baseX = baseSquadX + (i == 0 ? 0.0f : (i == 1 ? -2.5f : 2.5f));
            ej.x = ej.baseX;
            ej.y = 1.0f + (i * 0.5f);
            ej.targetApproachY = ej.y;
            ej.z = -128.0f - (i * 10.0f);
            ej.vx = 0.0f;
            ej.vy = 0.0f;
            ej.vz = 25.0f;
            ej.health = 22.0f;
            ej.maxHealth = 22.0f;
            ej.fireCooldown = 1.2f + (rand() % 10) * 0.1f;
            ej.roll = 0.0f;
        } else {
            // Escuadrilla de Cazas Interceptores en V
            ej.baseX = baseSquadX + (i == 0 ? 0.0f : (i == 1 ? -2.5f : 2.5f));
            ej.x = ej.baseX;
            ej.y = 0.8f + (i == 0 ? 0.9f : 0.2f);
            ej.targetApproachY = (i == 0 ? 1.6f : 0.9f);
            ej.z = -124.0f - (i == 0 ? 0.0f : 12.0f);
            ej.vx = 0.0f;
            ej.vy = 0.0f;
            ej.vz = 26.0f;
            ej.health = 24.0f;
            ej.maxHealth = 24.0f;
            ej.fireCooldown = 0.9f + (rand() % 10) * 0.1f;
            ej.roll = (i == 1 ? -15.0f : (i == 2 ? 15.0f : 0.0f));
        }

        enemyJets_.push_back(ej);
    }
}

// ----------------------------------------------------------------------------
// SISTEMA DE CONTROLES MULTI-TACTILES ROBUSTO (SIN FALLOS INTERMITENTES)
// ----------------------------------------------------------------------------
void Renderer::handleInput() {
    auto *inputBuffer = android_app_swap_input_buffers(app_);
    if (!inputBuffer) return;

    if (width_ <= 0 || height_ <= 0) {
        updateRenderArea();
    }

    // Coordenadas fijas de interfaz adaptadas al modo retrato
    float sndBtnX = width_ - 135.0f, sndBtnY = 22.0f;
    float sndBtnSize = 52.0f;
    float pauseBtnX = width_ - 70.0f, pauseBtnY = 22.0f;
    float pauseBtnSize = 52.0f;

    // Botones de combate en esquina inferior derecha (generosos para el pulgar)
    float fireBtnX = width_ - 90.0f, fireBtnY = height_ - 110.0f;
    float fireHitR = 85.0f;
    float mslBtnX = width_ - 90.0f, mslBtnY = height_ - 235.0f;
    float mslHitR = 75.0f;
    float bombBtnX = width_ - 195.0f, bombBtnY = height_ - 165.0f;
    float bombHitR = 75.0f;

    for (auto i = 0; i < inputBuffer->motionEventsCount; i++) {
        auto &motionEvent = inputBuffer->motionEvents[i];
        auto actionMasked = motionEvent.action & AMOTION_EVENT_ACTION_MASK;
        auto pointerIndex = (motionEvent.action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK)
                >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;

        // 1. MANEJO EN MENÚ PRINCIPAL
        if (gameState_ == STATE_MAIN_MENU) {
            if (actionMasked == AMOTION_EVENT_ACTION_DOWN || actionMasked == AMOTION_EVENT_ACTION_POINTER_DOWN) {
                if (pointerIndex < motionEvent.pointerCount) {
                    auto &pointer = motionEvent.pointers[pointerIndex];
                    float px = GameActivityPointerAxes_getX(&pointer);
                    float py = GameActivityPointerAxes_getY(&pointer);

                    if (showHelpModal_) {
                        showHelpModal_ = false;
                        if (sndLock_) w3dEngine::W3dSoundPlay(sndLock_, 0.6f, false);
                        continue;
                    }

                    // Botón de sonido (Esquina superior derecha)
                    float sndX = (float)width_ - 64.0f;
                    float sndY = 20.0f;
                    float sndSize = 48.0f;
                    if (px >= sndX - 12.0f && px <= sndX + sndSize + 12.0f && py >= sndY - 12.0f && py <= sndY + sndSize + 12.0f) {
                        soundEnabled_ = !soundEnabled_;
                        w3dEngine::W3dAudioMasterVolume(soundEnabled_ ? 1.0f : 0.0f);
                        if (soundEnabled_ && sndLock_) w3dEngine::W3dSoundPlay(sndLock_, 0.55f, false);
                        continue;
                    }

                    // Botón AYUDA / MANUAL (?)
                    float btnW = std::min((float)width_ * 0.82f, 320.0f);
                    float btnH = 68.0f;
                    float bx = ((float)width_ - btnW) * 0.5f;
                    float by = (float)height_ * 0.48f;

                    float hBtnW = std::min((float)width_ * 0.72f, 260.0f);
                    float hBtnH = 48.0f;
                    float hx = ((float)width_ - hBtnW) * 0.5f;
                    float hy = by + btnH + 48.0f;

                    if (px >= hx - 12.0f && px <= hx + hBtnW + 12.0f && py >= hy - 12.0f && py <= hy + hBtnH + 12.0f) {
                        showHelpModal_ = true;
                        if (sndLock_) w3dEngine::W3dSoundPlay(sndLock_, 0.65f, false);
                        continue;
                    }

                    // Botón JUGAR / DESPEGAR (Solo inicia pulsando este botón)
                    if (px >= bx - 12.0f && px <= bx + btnW + 12.0f && py >= by - 12.0f && py <= by + btnH + 12.0f) {
                        resetMission();
                        gameState_ = STATE_PLAYING;
                        if (sndLock_) w3dEngine::W3dSoundPlay(sndLock_, 0.85f, false);
                        continue;
                    }
                }
            }
            continue;
        }

        // 2. MANEJO EN MENÚ DE PAUSA
        if (gameState_ == STATE_PAUSED) {
            if (actionMasked == AMOTION_EVENT_ACTION_DOWN || actionMasked == AMOTION_EVENT_ACTION_POINTER_DOWN) {
                if (pointerIndex < motionEvent.pointerCount) {
                    auto &pointer = motionEvent.pointers[pointerIndex];
                    float px = GameActivityPointerAxes_getX(&pointer);
                    float py = GameActivityPointerAxes_getY(&pointer);

                    // Botón de sonido en pausa
                    float sX = width_ - 70.0f, sY = 22.0f;
                    if (px >= sX - 20.0f && px <= sX + 70.0f && py >= sY - 15.0f && py <= sY + 70.0f) {
                        soundEnabled_ = !soundEnabled_;
                        w3dEngine::W3dAudioMasterVolume(soundEnabled_ ? 1.0f : 0.0f);
                        if (soundEnabled_ && sndLock_) w3dEngine::W3dSoundPlay(sndLock_, 0.55f, false);
                        continue;
                    }

                    float dlgW = std::min(width_ * 0.88f, 380.0f);
                    float dlgH = 340.0f;
                    float dy = (height_ - dlgH) * 0.5f;
                    float btnW = dlgW * 0.78f, btnH = 50.0f;
                    float bx = (width_ - btnW) * 0.5f;

                    // Reanudar
                    float by1 = dy + 115.0f;
                    if (px >= bx - 15.0f && px <= bx + btnW + 15.0f && py >= by1 - 10.0f && py <= by1 + btnH + 10.0f) {
                        gameState_ = STATE_PLAYING;
                        if (sndLock_) w3dEngine::W3dSoundPlay(sndLock_, 0.75f, false);
                        continue;
                    }
                    // Reiniciar
                    float by2 = dy + 180.0f;
                    if (px >= bx - 15.0f && px <= bx + btnW + 15.0f && py >= by2 - 10.0f && py <= by2 + btnH + 10.0f) {
                        resetMission();
                        gameState_ = STATE_PLAYING;
                        if (sndLock_) w3dEngine::W3dSoundPlay(sndLock_, 0.8f, false);
                        continue;
                    }
                    // Menú principal
                    float by3 = dy + 245.0f;
                    if (px >= bx - 15.0f && px <= bx + btnW + 15.0f && py >= by3 - 10.0f && py <= by3 + btnH + 10.0f) {
                        gameState_ = STATE_MAIN_MENU;
                        if (sndLock_) w3dEngine::W3dSoundPlay(sndLock_, 0.7f, false);
                        continue;
                    }
                }
            }
            continue;
        }

        // 3. MANEJO EN GAME OVER
        if (gameState_ == STATE_GAME_OVER) {
            if (actionMasked == AMOTION_EVENT_ACTION_DOWN || actionMasked == AMOTION_EVENT_ACTION_POINTER_DOWN) {
                if (pointerIndex < motionEvent.pointerCount) {
                    auto &pointer = motionEvent.pointers[pointerIndex];
                    float px = GameActivityPointerAxes_getX(&pointer);
                    float py = GameActivityPointerAxes_getY(&pointer);

                    float dlgW = std::min(width_ * 0.90f, 420.0f);
                    float dlgH = 360.0f;
                    float dy = (height_ - dlgH) * 0.5f;
                    float btnW = dlgW * 0.78f, btnH = 54.0f;
                    float bx = (width_ - btnW) * 0.5f;

                    // Reintentar
                    float by1 = dy + 195.0f;
                    if (px >= bx - 15.0f && px <= bx + btnW + 15.0f && py >= by1 - 10.0f && py <= by1 + btnH + 10.0f) {
                        resetMission();
                        gameState_ = STATE_PLAYING;
                        if (sndLock_) w3dEngine::W3dSoundPlay(sndLock_, 0.8f, false);
                        continue;
                    }
                    // Menú principal
                    float by2 = dy + 265.0f;
                    if (px >= bx - 15.0f && px <= bx + btnW + 15.0f && py >= by2 - 10.0f && py <= by2 + btnH + 10.0f) {
                        gameState_ = STATE_MAIN_MENU;
                        if (sndLock_) w3dEngine::W3dSoundPlay(sndLock_, 0.7f, false);
                        continue;
                    }
                }
            }
            continue;
        }

        // 4. MANEJO DURANTE EL VUELO (STATE_PLAYING)
        if (actionMasked == AMOTION_EVENT_ACTION_DOWN || actionMasked == AMOTION_EVENT_ACTION_POINTER_DOWN) {
            touchDown_ = true;
            if (pointerIndex < motionEvent.pointerCount) {
                auto &pointer = motionEvent.pointers[pointerIndex];
                float px = GameActivityPointerAxes_getX(&pointer);
                float py = GameActivityPointerAxes_getY(&pointer);
                int pId = pointer.id;

                // Barra superior de seguridad (Pausa y Sonido)
                if (py < 90.0f) {
                    if (px >= width_ - 85.0f) {
                        gameState_ = STATE_PAUSED;
                        firePressed_ = false;
                        missilePressed_ = false;
                        bombPressed_ = false;
                        stickActive_ = false;
                        stickPointerId_ = -1;
                        firePointerId_ = -1;
                        missilePointerId_ = -1;
                        bombPointerId_ = -1;
                        if (sndLock_) w3dEngine::W3dSoundPlay(sndLock_, 0.65f, false);
                        continue;
                    }
                    if (px >= width_ - 160.0f && px < width_ - 85.0f) {
                        soundEnabled_ = !soundEnabled_;
                        w3dEngine::W3dAudioMasterVolume(soundEnabled_ ? 1.0f : 0.0f);
                        if (soundEnabled_ && sndLock_) w3dEngine::W3dSoundPlay(sndLock_, 0.55f, false);
                        continue;
                    }
                }

                // Botón de Disparo de Cañón
                float dfx = px - fireBtnX;
                float dfy = py - fireBtnY;
                if (dfx * dfx + dfy * dfy <= fireHitR * fireHitR) {
                    firePointerId_ = pId;
                    firePressed_ = true;
                    cannonCooldown_ = 0.0f;
                    continue;
                }

                // Botón de Misil Guiado
                float dmx = px - mslBtnX;
                float dmy = py - mslBtnY;
                if (dmx * dmx + dmy * dmy <= mslHitR * mslHitR) {
                    missilePointerId_ = pId;
                    missilePressed_ = true;
                    if (missileCount_ > 0 && missileCooldown_ <= 0.0f) {
                        missileCount_--;
                        missileCooldown_ = 0.45f;
                        PlayerMissile pm;
                        float wingOffset = (missileCount_ % 2 == 0) ? -1.15f : 1.15f;
                        pm.x = planeX_ + wingOffset;
                        pm.y = planeY_ - 0.45f;
                        pm.z = -6.8f;
                        pm.vx = planeRoll_ * 0.06f;
                        pm.vy = 2.0f;
                        pm.vz = -45.0f;
                        pm.pitch = 0.0f;
                        pm.yaw = 0.0f;
                        pm.targetMode = missileTargetMode_;
                        pm.targetIdx = missileTargetIdx_;
                        pm.life = 3.2f;
                        pm.active = true;
                        activeMissiles_.push_back(pm);
                        if (sndMissile_) w3dEngine::W3dSoundPlay(sndMissile_, 0.9f, false);
                    }
                    continue;
                }

                // Botón de Bombas
                float dbx = px - bombBtnX;
                float dby = py - bombBtnY;
                if (dbx * dbx + dby * dby <= bombHitR * bombHitR) {
                    bombPointerId_ = pId;
                    bombPressed_ = true;
                    if (bombCount_ > 0 && bombCooldown_ <= 0.0f) {
                        bombCount_--;
                        bombCooldown_ = 0.55f;
                        Bomb b;
                        b.x = planeX_;
                        b.y = planeY_ - 0.7f;
                        b.z = -6.8f;
                        b.vx = planeRoll_ * 0.08f;
                        b.vy = -6.0f;
                        b.vz = -32.0f;
                        b.pitch = 15.0f;
                        b.life = 4.0f;
                        b.active = true;
                        bombs_.push_back(b);
                        if (sndMissile_) w3dEngine::W3dSoundPlayPitch(sndMissile_, 0.8f, false, 0.65f);
                    }
                    continue;
                }

                // Joystick Virtual de Navegación (en la mitad inferior izquierda de la pantalla)
                if (stickPointerId_ == -1 && px < (float)width_ * 0.52f && py > (float)height_ * 0.38f) {
                    stickPointerId_ = pId;
                    stickActive_ = true;
                    stickOriginX_ = px;
                    stickOriginY_ = py;
                    stickKnobX_ = px;
                    stickKnobY_ = py;
                    stickDeflectX_ = 0.0f;
                    stickDeflectY_ = 0.0f;
                }
            }
        } else if (actionMasked == AMOTION_EVENT_ACTION_MOVE) {
            for (uint32_t p = 0; p < motionEvent.pointerCount; ++p) {
                auto &pointer = motionEvent.pointers[p];
                float px = GameActivityPointerAxes_getX(&pointer);
                float py = GameActivityPointerAxes_getY(&pointer);
                int pId = pointer.id;

                if (pId == stickPointerId_) {
                    float dx = px - stickOriginX_;
                    float dy = py - stickOriginY_;
                    float maxR = 90.0f;
                    float dist = std::sqrt(dx * dx + dy * dy);
                    if (dist > maxR) {
                        dx = (dx / dist) * maxR;
                        dy = (dy / dist) * maxR;
                    }
                    stickKnobX_ = stickOriginX_ + dx;
                    stickKnobY_ = stickOriginY_ + dy;

                    float deadzone = 5.0f;
                    float norm = (dist > deadzone) ? (dist - deadzone) / (maxR - deadzone) : 0.0f;
                    stickDeflectX_ = (dist > 0.001f) ? (dx / dist) * norm : 0.0f;
                    stickDeflectY_ = (dist > 0.001f) ? (dy / dist) * norm : 0.0f;
                } else if (pId == firePointerId_) {
                    firePressed_ = true;
                } else if (pId == missilePointerId_) {
                    missilePressed_ = true;
                } else if (pId == bombPointerId_) {
                    bombPressed_ = true;
                }
            }
        } else if (actionMasked == AMOTION_EVENT_ACTION_POINTER_UP) {
            if (pointerIndex < motionEvent.pointerCount) {
                auto &pointer = motionEvent.pointers[pointerIndex];
                int pId = pointer.id;

                if (pId == stickPointerId_) {
                    stickActive_ = false;
                    stickPointerId_ = -1;
                    stickDeflectX_ = 0.0f;
                    stickDeflectY_ = 0.0f;
                }
                if (pId == firePointerId_) {
                    firePressed_ = false;
                    firePointerId_ = -1;
                }
                if (pId == missilePointerId_) {
                    missilePressed_ = false;
                    missilePointerId_ = -1;
                }
                if (pId == bombPointerId_) {
                    bombPressed_ = false;
                    bombPointerId_ = -1;
                }
            }
        } else if (actionMasked == AMOTION_EVENT_ACTION_UP || actionMasked == AMOTION_EVENT_ACTION_CANCEL) {
            stickActive_ = false;
            stickPointerId_ = -1;
            stickDeflectX_ = 0.0f;
            stickDeflectY_ = 0.0f;
            firePressed_ = false;
            firePointerId_ = -1;
            missilePressed_ = false;
            missilePointerId_ = -1;
            bombPressed_ = false;
            bombPointerId_ = -1;
            touchDown_ = false;
        }
    }
    android_app_clear_motion_events(inputBuffer);
    android_app_clear_key_events(inputBuffer);
}

// ----------------------------------------------------------------------------
// RENDERIZADO DE ELEMENTOS 3D (FIEL AL LOGO)
// ----------------------------------------------------------------------------

void Renderer::renderSkyAndOcean() {
    // 1. Cielo atmosférico tropical con gradiente radiante completo
    static const float skyVerts[] = {
        -360.0f,  220.0f, -250.0f,
         360.0f,  220.0f, -250.0f,
         360.0f,  -25.0f, -250.0f,

        -360.0f,  220.0f, -250.0f,
         360.0f,  -25.0f, -250.0f,
        -360.0f,  -25.0f, -250.0f
    };
    static const unsigned char skyColors[] = {
        18,  70, 160, 255,   // Azul zénit profundo
        18,  70, 160, 255,
        140, 210, 245, 255,  // Cyan horizonte luminoso
        18,  70, 160, 255,
        140, 210, 245, 255,
        140, 210, 245, 255
    };

    w3dEngine::Disable(w3dEngine::Texture2D);
    w3dEngine::Disable(w3dEngine::CullFace);
    w3dEngine::Enable(w3dEngine::ColorMaterial);
    w3dEngine::EnableArray(w3dEngine::VertexArray);
    w3dEngine::EnableArray(w3dEngine::ColorArray);
    w3dEngine::VertexPointer3f(0, skyVerts);
    w3dEngine::ColorPointer4ub(skyColors);
    w3dEngine::DrawTrianglesArray(6);
    w3dEngine::DisableArray(w3dEngine::ColorArray);
    w3dEngine::Disable(w3dEngine::ColorMaterial);

    // 2. Océano tropical subdividido con oleaje dinámico
    if (texSea_) {
        w3dEngine::Disable(w3dEngine::CullFace);
        float waveShift = std::fmod(timeSec_ * 0.69f, 22.0f);
        const int gridX = 14;
        const int gridZ = 16;
        const float minX = -320.0f, maxX = 320.0f;
        const float minZ = -450.0f, maxZ = 60.0f;
        const float stepX = (maxX - minX) / gridX;
        const float stepZ = (maxZ - minZ) / gridZ;

        std::vector<float> oceanVerts;
        std::vector<float> oceanUVs;
        oceanVerts.reserve(gridX * gridZ * 18);
        oceanUVs.reserve(gridX * gridZ * 12);

        for (int j = 0; j < gridZ; ++j) {
            float z0 = minZ + j * stepZ;
            float z1 = z0 + stepZ;
            float v0 = (float)j / gridZ * 22.0f - waveShift;
            float v1 = (float)(j + 1) / gridZ * 22.0f - waveShift;

            for (int i = 0; i < gridX; ++i) {
                float x0 = minX + i * stepX;
                float x1 = x0 + stepX;
                float u0 = (float)i / gridX * 20.0f;
                float u1 = (float)(i + 1) / gridX * 20.0f;

                // Triángulos CCW vistos desde arriba (Y > -2.5f)
                oceanVerts.insert(oceanVerts.end(), {
                    x0, -2.5f, z0,
                    x0, -2.5f, z1,
                    x1, -2.5f, z1,

                    x0, -2.5f, z0,
                    x1, -2.5f, z1,
                    x1, -2.5f, z0
                });
                oceanUVs.insert(oceanUVs.end(), {
                    u0, v0,
                    u0, v1,
                    u1, v1,

                    u0, v0,
                    u1, v1,
                    u1, v0
                });
            }
        }

        w3dEngine::Enable(w3dEngine::Texture2D);
        w3dEngine::BindTexture(texSea_->getTextureID());
        w3dEngine::Color4f(1.0f, 1.0f, 1.0f, 1.0f);
        w3dEngine::EnableArray(w3dEngine::VertexArray);
        w3dEngine::EnableArray(w3dEngine::TexCoordArray);
        w3dEngine::VertexPointer3f(0, oceanVerts.data());
        w3dEngine::TexCoordPointer2f(0, oceanUVs.data());
        w3dEngine::DrawTrianglesArray(static_cast<int>(oceanVerts.size() / 3));
        w3dEngine::DisableArray(w3dEngine::TexCoordArray);
        w3dEngine::Enable(w3dEngine::CullFace);
    }
}

void Renderer::renderSun() {
    // Sol radiante en el cuadrante superior derecho del cielo
    w3dEngine::PushMatrix();
    w3dEngine::Translatef(36.0f, 52.0f, -145.0f);

    w3dEngine::Enable(w3dEngine::Blend);
    w3dEngine::SetMezcla(w3dEngine::MezclaAddAlpha);
    w3dEngine::Disable(w3dEngine::CullFace);
    w3dEngine::DepthMask(false);

    float sunSize = 44.0f;
    float sunVerts[] = {
        -sunSize, -sunSize, 0.0f,
         sunSize, -sunSize, 0.0f,
         sunSize,  sunSize, 0.0f,

        -sunSize, -sunSize, 0.0f,
         sunSize,  sunSize, 0.0f,
        -sunSize,  sunSize, 0.0f
    };
    static const float sunUVs[] = {
        0.0f, 0.0f,   1.0f, 0.0f,   1.0f, 1.0f,
        0.0f, 0.0f,   1.0f, 1.0f,   0.0f, 1.0f
    };

    if (texSun_) {
        w3dEngine::Enable(w3dEngine::Texture2D);
        w3dEngine::BindTexture(texSun_->getTextureID());
        w3dEngine::Color4f(1.0f, 1.0f, 1.0f, 1.0f);
        w3dEngine::EnableArray(w3dEngine::VertexArray);
        w3dEngine::EnableArray(w3dEngine::TexCoordArray);
        w3dEngine::VertexPointer3f(0, sunVerts);
        w3dEngine::TexCoordPointer2f(0, sunUVs);
        w3dEngine::DrawTrianglesArray(6);
        w3dEngine::DisableArray(w3dEngine::TexCoordArray);
    }

    w3dEngine::DepthMask(true);
    w3dEngine::SetMezcla(w3dEngine::MezclaAlpha);
    w3dEngine::Enable(w3dEngine::CullFace);
    w3dEngine::PopMatrix();
}

void Renderer::renderClouds() {
    if (!texCloud_) return;

    w3dEngine::Enable(w3dEngine::Texture2D);
    w3dEngine::BindTexture(texCloud_->getTextureID());
    w3dEngine::Enable(w3dEngine::Blend);
    w3dEngine::SetMezcla(w3dEngine::MezclaAlpha);
    w3dEngine::Disable(w3dEngine::CullFace);
    w3dEngine::DepthMask(false);
    w3dEngine::EnableArray(w3dEngine::VertexArray);
    w3dEngine::EnableArray(w3dEngine::TexCoordArray);

    static const float cloudUVs[] = {
        0.0f, 0.0f,   1.0f, 0.0f,   1.0f, 1.0f,
        0.0f, 0.0f,   1.0f, 1.0f,   0.0f, 1.0f
    };

    for (const auto &c : clouds_) {
        w3dEngine::PushMatrix();
        w3dEngine::Translatef(c.x, c.y, c.z);

        float hx = c.scaleX * 0.5f;
        float hy = c.scaleY * 0.5f;
        float cVerts[] = {
            -hx, -hy, 0.0f,
             hx, -hy, 0.0f,
             hx,  hy, 0.0f,

            -hx, -hy, 0.0f,
             hx,  hy, 0.0f,
            -hx,  hy, 0.0f
        };

        w3dEngine::Color4f(1.0f, 1.0f, 1.0f, c.alpha);
        w3dEngine::VertexPointer3f(0, cVerts);
        w3dEngine::TexCoordPointer2f(0, cloudUVs);
        w3dEngine::DrawTrianglesArray(6);
        w3dEngine::PopMatrix();
    }

    w3dEngine::DepthMask(true);
    w3dEngine::DisableArray(w3dEngine::TexCoordArray);
    w3dEngine::Enable(w3dEngine::CullFace);
}

void Renderer::renderIslandsAndTrees() {
    if (!texTerrain_) return;

    w3dEngine::Enable(w3dEngine::Texture2D);
    w3dEngine::BindTexture(texTerrain_->getTextureID());
    w3dEngine::Color4f(1.0f, 1.0f, 1.0f, 1.0f);
    w3dEngine::EnableArray(w3dEngine::VertexArray);
    w3dEngine::EnableArray(w3dEngine::TexCoordArray);

    // Malla geométrica de Isla Volcánica Principal
    static const float islandVerts[] = {
        0.0f, 11.0f, 0.0f,   18.0f, 0.0f, 0.0f,    13.0f, 0.0f, 14.0f,
        0.0f, 11.0f, 0.0f,   13.0f, 0.0f, 14.0f,    0.0f, 0.0f, 20.0f,
        0.0f, 11.0f, 0.0f,    0.0f, 0.0f, 20.0f,  -15.0f, 0.0f, 15.0f,
        0.0f, 11.0f, 0.0f,  -15.0f, 0.0f, 15.0f,  -21.0f, 0.0f, 0.0f,
        0.0f, 11.0f, 0.0f,  -21.0f, 0.0f, 0.0f,   -14.0f, 0.0f, -17.0f,
        0.0f, 11.0f, 0.0f,  -14.0f, 0.0f, -17.0f,   0.0f, 0.0f, -22.0f,
        0.0f, 11.0f, 0.0f,    0.0f, 0.0f, -22.0f,  15.0f, 0.0f, -14.0f,
        0.0f, 11.0f, 0.0f,   15.0f, 0.0f, -14.0f,  18.0f, 0.0f, 0.0f
    };
    static const float islandUVs[] = {
        0.5f, 0.05f,   0.95f, 0.95f,   0.85f, 0.95f,
        0.5f, 0.05f,   0.85f, 0.95f,   0.65f, 0.95f,
        0.5f, 0.05f,   0.65f, 0.95f,   0.45f, 0.95f,
        0.5f, 0.05f,   0.45f, 0.95f,   0.25f, 0.95f,
        0.5f, 0.05f,   0.25f, 0.95f,   0.15f, 0.95f,
        0.5f, 0.05f,   0.15f, 0.95f,   0.35f, 0.95f,
        0.5f, 0.05f,   0.35f, 0.95f,   0.75f, 0.95f,
        0.5f, 0.05f,   0.75f, 0.95f,   0.95f, 0.95f
    };

    // Malla geométrica de Atolón
    static const float atollVerts[] = {
        0.0f, 6.5f, 0.0f,   14.0f, 0.0f, 0.0f,    10.0f, 0.0f, 11.0f,
        0.0f, 6.5f, 0.0f,   10.0f, 0.0f, 11.0f,    0.0f, 0.0f, 15.0f,
        0.0f, 6.5f, 0.0f,    0.0f, 0.0f, 15.0f,  -11.0f, 0.0f, 11.0f,
        0.0f, 6.5f, 0.0f,  -11.0f, 0.0f, 11.0f,  -15.0f, 0.0f, 0.0f,
        0.0f, 6.5f, 0.0f,  -15.0f, 0.0f, 0.0f,   -10.0f, 0.0f, -12.0f,
        0.0f, 6.5f, 0.0f,  -10.0f, 0.0f, -12.0f,   0.0f, 0.0f, -16.0f,
        0.0f, 6.5f, 0.0f,    0.0f, 0.0f, -16.0f,  11.0f, 0.0f, -10.0f,
        0.0f, 6.5f, 0.0f,   11.0f, 0.0f, -10.0f,  14.0f, 0.0f, 0.0f
    };

    for (const auto &isl : islands_) {
        w3dEngine::PushMatrix();
        w3dEngine::Translatef(isl.x, -2.5f, isl.z);
        w3dEngine::Rotatef(isl.angle, 0.0f, 1.0f, 0.0f);
        w3dEngine::Scalef(isl.scale, isl.scale, isl.scale);

        if (isl.type == 0) {
            w3dEngine::VertexPointer3f(0, islandVerts);
            w3dEngine::TexCoordPointer2f(0, islandUVs);
            w3dEngine::DrawTrianglesArray(24);
        } else {
            w3dEngine::VertexPointer3f(0, atollVerts);
            w3dEngine::TexCoordPointer2f(0, islandUVs);
            w3dEngine::DrawTrianglesArray(24);
        }
        w3dEngine::PopMatrix();
    }

    w3dEngine::DisableArray(w3dEngine::TexCoordArray);

    // Renderizado de Árboles 3D (Tronco y copa de follaje facetado)
    w3dEngine::Disable(w3dEngine::Texture2D);
    w3dEngine::Enable(w3dEngine::ColorMaterial);
    w3dEngine::EnableArray(w3dEngine::VertexArray);

    for (const auto &tr : trees_) {
        w3dEngine::PushMatrix();
        w3dEngine::Translatef(tr.x, tr.y, tr.z);
        w3dEngine::Scalef(tr.scale, tr.scale, tr.scale);

        // Tronco (Marrón leñoso)
        static const float trunkVerts[] = {
            -0.2f, 0.0f,  0.0f,    0.2f, 0.0f,  0.0f,    0.0f, 1.8f,  0.0f,
             0.0f, 0.0f, -0.2f,    0.0f, 0.0f,  0.2f,    0.0f, 1.8f,  0.0f
        };
        w3dEngine::Color4f(0.42f, 0.26f, 0.15f, 1.0f);
        w3dEngine::VertexPointer3f(0, trunkVerts);
        w3dEngine::DrawTrianglesArray(6);

        // Copa de Hojas / Palmera (Verde esmeralda tropical)
        static const float foliageVerts[] = {
            // Nivel 1
             0.0f, 3.2f,  0.0f,   -1.4f, 1.6f, -1.4f,    1.4f, 1.6f, -1.4f,
             0.0f, 3.2f,  0.0f,    1.4f, 1.6f, -1.4f,    1.4f, 1.6f,  1.4f,
             0.0f, 3.2f,  0.0f,    1.4f, 1.6f,  1.4f,   -1.4f, 1.6f,  1.4f,
             0.0f, 3.2f,  0.0f,   -1.4f, 1.6f,  1.4f,   -1.4f, 1.6f, -1.4f,
            // Nivel 2 superior
             0.0f, 4.2f,  0.0f,   -1.0f, 2.7f, -1.0f,    1.0f, 2.7f, -1.0f,
             0.0f, 4.2f,  0.0f,    1.0f, 2.7f, -1.0f,    1.0f, 2.7f,  1.0f,
             0.0f, 4.2f,  0.0f,    1.0f, 2.7f,  1.0f,   -1.0f, 2.7f,  1.0f,
             0.0f, 4.2f,  0.0f,   -1.0f, 2.7f,  1.0f,   -1.0f, 2.7f, -1.0f
        };
        w3dEngine::Color4f(0.12f, 0.65f, 0.22f, 1.0f);
        w3dEngine::VertexPointer3f(0, foliageVerts);
        w3dEngine::DrawTrianglesArray(24);

        w3dEngine::PopMatrix();
    }
}

void Renderer::renderAircraft() {
    if (!texAirplane_) return;

    w3dEngine::PushMatrix();
    w3dEngine::Translatef(planeX_, planeY_ - 0.5f, -6.5f);

    w3dEngine::Rotatef(-planeRoll_,  0.0f, 0.0f, 1.0f);
    w3dEngine::Rotatef(planePitch_,  1.0f, 0.0f, 0.0f);
    w3dEngine::Rotatef(-planeYaw_,   0.0f, 1.0f, 0.0f);

    w3dEngine::Enable(w3dEngine::Texture2D);
    w3dEngine::BindTexture(texAirplane_->getTextureID());
    w3dEngine::Disable(w3dEngine::CullFace);
    w3dEngine::EnableArray(w3dEngine::VertexArray);
    w3dEngine::EnableArray(w3dEngine::TexCoordArray);

    // Modelo 3D del Caza WHISK apuntando hacia adelante (-Z)
    static const float jetVerts[] = {
        // --- 1. Morro / Nariz aerodinámica ---
         0.0f,  0.0f, -2.8f,   -0.38f,  0.0f, -1.1f,    0.0f,  0.18f, -1.1f,
         0.0f,  0.0f, -2.8f,    0.0f,  0.18f, -1.1f,    0.38f,  0.0f, -1.1f,
         0.0f,  0.0f, -2.8f,    0.0f, -0.16f, -1.1f,   -0.38f,  0.0f, -1.1f,
         0.0f,  0.0f, -2.8f,    0.38f,  0.0f, -1.1f,    0.0f, -0.16f, -1.1f,

        // --- 2. Cúpula de la Cabina (Cockpit Bubble) ---
         0.0f,  0.18f, -1.1f,  -0.22f,  0.18f, -0.2f,    0.0f,  0.46f, -0.2f,
         0.0f,  0.18f, -1.1f,   0.0f,  0.46f, -0.2f,    0.22f,  0.18f, -0.2f,
         0.0f,  0.46f, -0.2f,  -0.22f,  0.18f, -0.2f,    0.0f,  0.22f,  0.6f,
         0.0f,  0.46f, -0.2f,   0.0f,  0.22f,  0.6f,    0.22f,  0.18f, -0.2f,

        // --- 3. Fuselaje Central Dorsal y Vientre ---
        -0.42f, 0.16f, -1.1f,  -0.52f, 0.16f,  1.6f,    0.52f, 0.16f,  1.6f,
        -0.42f, 0.16f, -1.1f,   0.52f, 0.16f,  1.6f,    0.42f, 0.16f, -1.1f,
        -0.42f,-0.16f, -1.1f,   0.52f,-0.16f,  1.6f,   -0.52f,-0.16f,  1.6f,
        -0.42f,-0.16f, -1.1f,   0.42f,-0.16f, -1.1f,    0.52f,-0.16f,  1.6f,

        // --- 4. Alas Delta en Flecha (Con franjas naranja/rojas) ---
        -0.42f, 0.05f, -0.7f,  -3.2f,  0.02f,  1.0f,   -0.52f, 0.05f,  1.4f,
        -0.42f,-0.02f, -0.7f,  -0.52f,-0.02f,  1.4f,   -3.2f, -0.02f,  1.0f,
         0.42f, 0.05f, -0.7f,   0.52f, 0.05f,  1.4f,    3.2f,  0.02f,  1.0f,
         0.42f,-0.02f, -0.7f,   3.2f, -0.02f,  1.0f,    0.52f,-0.02f,  1.4f,

        // --- 5. Estabilizadores Verticales Dobles con logo "W" ---
        -0.40f, 0.16f,  0.6f,  -0.65f, 1.15f,  1.6f,   -0.40f, 0.16f,  1.5f,
        -0.40f, 0.16f,  0.6f,  -0.40f, 0.16f,  1.5f,   -0.65f, 1.15f,  1.6f,
         0.40f, 0.16f,  0.6f,   0.40f, 0.16f,  1.5f,    0.65f, 1.15f,  1.6f,
         0.40f, 0.16f,  0.6f,   0.65f, 1.15f,  1.6f,    0.40f, 0.16f,  1.5f
    };

    static const float jetUVs[] = {
        // Morro superior (piel blanca aeronáutica elegante)
        0.60f, 0.96f,  0.54f, 0.80f,  0.60f, 0.80f,
        0.60f, 0.96f,  0.60f, 0.80f,  0.66f, 0.80f,
        // Morro inferior (panel ventral oscuro)
        0.25f, 0.45f,  0.25f, 0.10f,  0.10f, 0.10f,
        0.25f, 0.45f,  0.40f, 0.10f,  0.25f, 0.10f,

        // Cabina (Cúpula de cristal azul con reflejos blancos)
        0.25f, 0.95f,  0.06f, 0.75f,  0.25f, 0.75f,
        0.25f, 0.95f,  0.25f, 0.75f,  0.44f, 0.75f,
        0.25f, 0.75f,  0.06f, 0.75f,  0.25f, 0.55f,
        0.25f, 0.75f,  0.25f, 0.55f,  0.44f, 0.75f,

        // Fuselaje dorsal (Escarapela de Estrella de Combate) y vientre
        0.52f, 0.73f,  0.52f, 0.61f,  0.69f, 0.61f,
        0.52f, 0.73f,  0.69f, 0.61f,  0.69f, 0.73f,
        0.10f, 0.45f,  0.40f, 0.10f,  0.10f, 0.10f,
        0.10f, 0.45f,  0.40f, 0.45f,  0.40f, 0.10f,

        // Alas Delta (Franjas aerodinámicas de velocidad naranja/rojas)
        0.58f, 0.95f,  0.95f, 0.55f,  0.75f, 0.55f,
        0.10f, 0.45f,  0.40f, 0.45f,  0.25f, 0.10f,
        0.58f, 0.95f,  0.75f, 0.55f,  0.95f, 0.55f,
        0.10f, 0.45f,  0.25f, 0.10f,  0.40f, 0.45f,

        // Estabilizadores Verticales Dobles (Insignia "W WHISK" visible y erguida)
        0.77f, 0.77f,  0.96f, 0.96f,  0.96f, 0.77f,
        0.77f, 0.77f,  0.96f, 0.77f,  0.96f, 0.96f,
        0.77f, 0.77f,  0.96f, 0.77f,  0.96f, 0.96f,
        0.77f, 0.77f,  0.96f, 0.96f,  0.96f, 0.77f
    };

    w3dEngine::Color4f(1.0f, 1.0f, 1.0f, 1.0f);
    w3dEngine::VertexPointer3f(0, jetVerts);
    w3dEngine::TexCoordPointer2f(0, jetUVs);
    w3dEngine::DrawTrianglesArray(60);

    // Llamas gemelas de postcombustión hacia +Z (Idénticas a los reactores del logo)
    float flamePulse = 0.85f + 0.35f * std::sin(timeSec_ * 32.0f);
    float flameVerts[] = {
        -0.34f, -0.02f, 1.6f,   -0.16f, -0.02f, 1.6f,   -0.25f, -0.02f, 1.6f + (1.05f * flamePulse),
        -0.25f,  0.07f, 1.6f,   -0.25f, -0.11f, 1.6f,   -0.25f, -0.02f, 1.6f + (1.05f * flamePulse),
         0.16f, -0.02f, 1.6f,    0.34f, -0.02f, 1.6f,    0.25f, -0.02f, 1.6f + (1.05f * flamePulse),
         0.25f,  0.07f, 1.6f,    0.25f, -0.11f, 1.6f,    0.25f, -0.02f, 1.6f + (1.05f * flamePulse)
    };
    static const float flameUVs[] = {
        0.65f, 0.15f,  0.85f, 0.15f,  0.75f, 0.35f,
        0.65f, 0.15f,  0.85f, 0.15f,  0.75f, 0.35f,
        0.65f, 0.15f,  0.85f, 0.15f,  0.75f, 0.35f,
        0.65f, 0.15f,  0.85f, 0.15f,  0.75f, 0.35f
    };
    w3dEngine::VertexPointer3f(0, flameVerts);
    w3dEngine::TexCoordPointer2f(0, flameUVs);
    w3dEngine::DrawTrianglesArray(12);

    w3dEngine::DisableArray(w3dEngine::TexCoordArray);
    w3dEngine::Enable(w3dEngine::CullFace);
    w3dEngine::PopMatrix();
}

void Renderer::renderEnemyJets() {
    if (enemyJets_.empty()) return;

    w3dEngine::Enable(w3dEngine::Texture2D);
    w3dEngine::BindTexture(texEnemyJet_ ? texEnemyJet_->getTextureID() : (texAirplane_ ? texAirplane_->getTextureID() : 0));
    w3dEngine::Disable(w3dEngine::CullFace);
    w3dEngine::EnableArray(w3dEngine::VertexArray);
    w3dEngine::EnableArray(w3dEngine::TexCoordArray);

    static const float ejVerts[] = {
        // Morro hacia +Z (vuela hacia el jugador)
         0.0f,  0.0f,  2.6f,   -0.35f,  0.0f,  0.9f,    0.0f,  0.18f,  0.9f,
         0.0f,  0.0f,  2.6f,    0.0f,  0.18f,  0.9f,    0.35f,  0.0f,  0.9f,
         0.0f,  0.0f,  2.6f,    0.0f, -0.16f,  0.9f,   -0.35f,  0.0f,  0.9f,
         0.0f,  0.0f,  2.6f,    0.35f,  0.0f,  0.9f,    0.0f, -0.16f,  0.9f,

        // Cabina
         0.0f,  0.18f,  0.9f,  -0.20f,  0.18f,  0.0f,    0.0f,  0.42f,  0.0f,
         0.0f,  0.18f,  0.9f,   0.0f,  0.42f,  0.0f,    0.20f,  0.18f,  0.0f,
         0.0f,  0.42f,  0.0f,  -0.20f,  0.18f,  0.0f,    0.0f,  0.20f, -0.6f,
         0.0f,  0.42f,  0.0f,   0.0f,  0.20f, -0.6f,    0.20f,  0.18f,  0.0f,

        // Fuselaje
        -0.40f, 0.16f,  0.9f,  -0.48f, 0.16f, -1.5f,    0.48f, 0.16f, -1.5f,
        -0.40f, 0.16f,  0.9f,   0.48f, 0.16f, -1.5f,    0.40f, 0.16f,  0.9f,
        -0.40f,-0.16f,  0.9f,   0.48f,-0.16f, -1.5f,   -0.48f,-0.16f, -1.5f,
        -0.40f,-0.16f,  0.9f,   0.40f,-0.16f,  0.9f,    0.48f,-0.16f, -1.5f,

        // Alas Delta
        -0.40f, 0.05f,  0.5f,  -2.9f,  0.02f, -1.0f,   -0.48f, 0.05f, -1.3f,
        -0.40f,-0.02f,  0.5f,  -0.48f,-0.02f, -1.3f,   -2.9f, -0.02f, -1.0f,
         0.40f, 0.05f,  0.5f,   0.48f, 0.05f, -1.3f,    2.9f,  0.02f, -1.0f,
         0.40f,-0.02f,  0.5f,   2.9f, -0.02f, -1.0f,    0.48f,-0.02f, -1.3f,

        // Estabilizadores dobles
        -0.38f, 0.16f, -0.5f,  -0.60f, 1.05f, -1.5f,   -0.38f, 0.16f, -1.4f,
        -0.38f, 0.16f, -0.5f,  -0.38f, 0.16f, -1.4f,   -0.60f, 1.05f, -1.5f,
         0.38f, 0.16f, -0.5f,   0.38f, 0.16f, -1.4f,    0.60f, 1.05f, -1.5f,
         0.38f, 0.16f, -0.5f,   0.60f, 1.05f, -1.5f,    0.38f, 0.16f, -1.4f
    };

    static const float ejUVs[] = {
        // Morro superior
        0.60f, 0.96f,  0.54f, 0.80f,  0.60f, 0.80f,
        0.60f, 0.96f,  0.60f, 0.80f,  0.66f, 0.80f,
        // Morro inferior
        0.25f, 0.45f,  0.25f, 0.10f,  0.10f, 0.10f,
        0.25f, 0.45f,  0.40f, 0.10f,  0.25f, 0.10f,

        // Cabina (Cúpula roja táctica)
        0.25f, 0.95f,  0.06f, 0.75f,  0.25f, 0.75f,
        0.25f, 0.95f,  0.25f, 0.75f,  0.44f, 0.75f,
        0.25f, 0.75f,  0.06f, 0.75f,  0.25f, 0.55f,
        0.25f, 0.75f,  0.25f, 0.55f,  0.44f, 0.75f,

        // Fuselaje e insignia de intercepción
        0.55f, 0.88f,  0.55f, 0.60f,  0.88f, 0.60f,
        0.55f, 0.88f,  0.88f, 0.60f,  0.88f, 0.88f,
        0.10f, 0.45f,  0.40f, 0.10f,  0.10f, 0.10f,
        0.10f, 0.45f,  0.40f, 0.45f,  0.40f, 0.10f,

        // Alas Delta
        0.58f, 0.95f,  0.95f, 0.55f,  0.75f, 0.55f,
        0.10f, 0.45f,  0.40f, 0.45f,  0.25f, 0.10f,
        0.58f, 0.95f,  0.75f, 0.55f,  0.95f, 0.55f,
        0.10f, 0.45f,  0.25f, 0.10f,  0.40f, 0.45f,

        // Estabilizadores dobles
        0.75f, 0.65f,  0.95f, 0.95f,  0.95f, 0.65f,
        0.75f, 0.65f,  0.95f, 0.65f,  0.95f, 0.95f,
        0.75f, 0.65f,  0.95f, 0.65f,  0.95f, 0.95f,
        0.75f, 0.65f,  0.95f, 0.95f,  0.95f, 0.65f
    };

    for (const auto &ej : enemyJets_) {
        if (!ej.active) continue;

        w3dEngine::PushMatrix();
        w3dEngine::Translatef(ej.x, ej.y, ej.z);
        w3dEngine::Rotatef(ej.roll, 0.0f, 0.0f, 1.0f);
        w3dEngine::Rotatef(ej.pitch, 1.0f, 0.0f, 0.0f);
        w3dEngine::Rotatef(ej.yaw, 0.0f, 1.0f, 0.0f);
        w3dEngine::Scalef(1.25f, 1.25f, 1.25f);

        if (ej.hitFlashTime > 0.0f) {
            w3dEngine::Color4f(1.0f, 0.35f, 0.35f, 1.0f);
        } else {
            w3dEngine::Color4f(1.0f, 1.0f, 1.0f, 1.0f);
        }

        w3dEngine::VertexPointer3f(0, ejVerts);
        w3dEngine::TexCoordPointer2f(0, ejUVs);
        w3dEngine::DrawTrianglesArray(60);

        // Toberas y postcombustión enemiga
        static const float ejFlameVerts[] = {
            -0.20f, 0.0f, -1.5f,   -0.20f, 0.0f, -2.4f,   -0.08f, 0.0f, -1.5f,
             0.08f, 0.0f, -1.5f,    0.20f, 0.0f, -2.4f,    0.20f, 0.0f, -1.5f
        };
        static const unsigned char ejFlameColors[] = {
            255, 120, 20, 255,   255, 40, 10, 0,   255, 200, 50, 255,
            255, 200, 50, 255,   255, 40, 10, 0,   255, 120, 20, 255
        };
        w3dEngine::Disable(w3dEngine::Texture2D);
        w3dEngine::Enable(w3dEngine::ColorMaterial);
        w3dEngine::EnableArray(w3dEngine::ColorArray);
        w3dEngine::VertexPointer3f(0, ejFlameVerts);
        w3dEngine::ColorPointer4ub(ejFlameColors);
        w3dEngine::DrawTrianglesArray(6);
        w3dEngine::DisableArray(w3dEngine::ColorArray);
        w3dEngine::Disable(w3dEngine::ColorMaterial);
        w3dEngine::Enable(w3dEngine::Texture2D);

        w3dEngine::PopMatrix();
    }

    w3dEngine::DisableArray(w3dEngine::TexCoordArray);
    w3dEngine::Enable(w3dEngine::CullFace);
}

void Renderer::renderTarget() {
    if (!texTarget_) return;

    w3dEngine::PushMatrix();
    w3dEngine::Translatef(targetX_, targetY_, targetZ_);

    w3dEngine::Enable(w3dEngine::Texture2D);
    w3dEngine::BindTexture(texTarget_->getTextureID());
    if (targetHitFlashTime_ > 0.0f) {
        w3dEngine::Color4f(1.0f, 0.40f, 0.40f, 1.0f);
    } else {
        w3dEngine::Color4f(1.0f, 1.0f, 1.0f, 1.0f);
    }
    w3dEngine::Disable(w3dEngine::CullFace);
    w3dEngine::EnableArray(w3dEngine::VertexArray);
    w3dEngine::EnableArray(w3dEngine::TexCoordArray);

    // Buque de combate naval enemigo (DDG-88) con orientación proa a -Z
    static const float shipVerts[] = {
        // --- 1. Cubierta Superior (Deck) ---
         0.0f,  1.2f, -10.0f,   -2.8f,  1.2f,  -4.0f,    2.8f,  1.2f,  -4.0f,
        -2.8f,  1.2f,  -4.0f,   -2.8f,  1.2f,   8.0f,    2.8f,  1.2f,   8.0f,
        -2.8f,  1.2f,  -4.0f,    2.8f,  1.2f,   8.0f,    2.8f,  1.2f,  -4.0f,

        // --- 2. Costados del Casco y Línea de Flotación ---
        -2.8f,  1.2f,  -4.0f,    0.0f,  1.2f, -10.0f,    0.0f, -1.2f, -10.0f,
        -2.8f,  1.2f,  -4.0f,    0.0f, -1.2f, -10.0f,   -2.8f, -1.2f,  -4.0f,
         0.0f,  1.2f, -10.0f,    2.8f,  1.2f,  -4.0f,    2.8f, -1.2f,  -4.0f,
         0.0f,  1.2f, -10.0f,    2.8f, -1.2f,  -4.0f,    0.0f, -1.2f, -10.0f,
        -2.8f,  1.2f,  -4.0f,   -2.8f, -1.2f,  -4.0f,   -2.8f,  1.2f,   8.0f,
        -2.8f, -1.2f,  -4.0f,   -2.8f, -1.2f,   8.0f,   -2.8f,  1.2f,   8.0f,
         2.8f,  1.2f,  -4.0f,    2.8f,  1.2f,   8.0f,    2.8f, -1.2f,  -4.0f,
         2.8f,  1.2f,   8.0f,    2.8f, -1.2f,   8.0f,    2.8f, -1.2f,  -4.0f,
        -2.8f,  1.2f,   8.0f,   -2.8f, -1.2f,   8.0f,    2.8f,  1.2f,   8.0f,
         2.8f,  1.2f,   8.0f,   -2.8f, -1.2f,   8.0f,    2.8f, -1.2f,   8.0f,

        // --- 3. Superestructura del Puente de Mando ---
        -1.8f,  3.0f,  -2.5f,    1.8f,  3.0f,  -2.5f,    1.8f,  3.0f,   3.0f,
        -1.8f,  3.0f,  -2.5f,    1.8f,  3.0f,   3.0f,   -1.8f,  3.0f,   3.0f,
        -1.8f,  1.2f,  -2.5f,    1.8f,  1.2f,  -2.5f,    1.8f,  3.0f,  -2.5f,
        -1.8f,  1.2f,  -2.5f,    1.8f,  3.0f,  -2.5f,   -1.8f,  3.0f,  -2.5f,
        -1.8f,  1.2f,   3.0f,   -1.8f,  3.0f,  -2.5f,   -1.8f,  1.2f,  -2.5f,
        -1.8f,  1.2f,   3.0f,   -1.8f,  3.0f,   3.0f,   -1.8f,  3.0f,  -2.5f,
         1.8f,  1.2f,  -2.5f,    1.8f,  3.0f,  -2.5f,    1.8f,  1.2f,   3.0f,
         1.8f,  3.0f,  -2.5f,    1.8f,  3.0f,   3.0f,    1.8f,  1.2f,   3.0f,
        -1.8f,  1.2f,   3.0f,    1.8f,  1.2f,   3.0f,    1.8f,  3.0f,   3.0f,
        -1.8f,  1.2f,   3.0f,    1.8f,  3.0f,   3.0f,   -1.8f,  3.0f,   3.0f
    };

    static const float shipUVs[] = {
        // 1. Cubierta de proa (VLS y lanzador)
        0.25f, 0.95f,   0.05f, 0.55f,   0.45f, 0.55f,
        // Cubierta de popa (Helipuerto con insignia "H")
        0.55f, 0.95f,   0.55f, 0.55f,   0.95f, 0.55f,
        0.55f, 0.95f,   0.95f, 0.55f,   0.95f, 0.95f,

        // 2. Proa de babor
        0.45f, 0.35f,   0.05f, 0.35f,   0.05f, 0.05f,
        0.45f, 0.35f,   0.05f, 0.05f,   0.45f, 0.05f,
        // Proa de estribor
        0.55f, 0.35f,   0.95f, 0.35f,   0.95f, 0.05f,
        0.55f, 0.35f,   0.95f, 0.05f,   0.55f, 0.05f,
        // Costado babor con código "DDG-88" y flotación
        0.05f, 0.35f,   0.05f, 0.05f,   0.48f, 0.35f,
        0.05f, 0.05f,   0.48f, 0.05f,   0.48f, 0.35f,
        // Costado estribor con código "AEGIS" y flotación
        0.52f, 0.35f,   0.95f, 0.35f,   0.52f, 0.05f,
        0.95f, 0.35f,   0.95f, 0.05f,   0.52f, 0.05f,
        // Espejo de popa
        0.10f, 0.35f,   0.10f, 0.05f,   0.40f, 0.35f,
        0.40f, 0.35f,   0.10f, 0.05f,   0.40f, 0.05f,

        // 3. Superestructura y Puente de Mando (Techo y Ventanas Radar)
        0.15f, 0.85f,   0.45f, 0.85f,   0.45f, 0.65f,
        0.15f, 0.85f,   0.45f, 0.65f,   0.15f, 0.65f,
        // Frontal del puente (ventanales cian/verde)
        0.10f, 0.36f,   0.90f, 0.36f,   0.90f, 0.48f,
        0.10f, 0.36f,   0.90f, 0.48f,   0.10f, 0.48f,
        // Lateral babor del puente
        0.90f, 0.36f,   0.10f, 0.48f,   0.10f, 0.36f,
        0.90f, 0.36f,   0.90f, 0.48f,   0.10f, 0.48f,
        // Lateral estribor del puente
        0.10f, 0.36f,   0.10f, 0.48f,   0.90f, 0.36f,
        0.10f, 0.48f,   0.90f, 0.48f,   0.90f, 0.36f,
        // Popa del puente
        0.10f, 0.36f,   0.90f, 0.36f,   0.90f, 0.48f,
        0.10f, 0.36f,   0.90f, 0.48f,   0.10f, 0.48f
    };

    w3dEngine::VertexPointer3f(0, shipVerts);
    w3dEngine::TexCoordPointer2f(0, shipUVs);
    w3dEngine::DrawTrianglesArray(69);

    // Torretas de artillería antiaérea rotatorias
    w3dEngine::PushMatrix();
    w3dEngine::Translatef(0.0f, 1.4f, -6.5f);
    w3dEngine::Rotatef(warshipTurretAngle_, 0.0f, 1.0f, 0.0f);
    static const float turretVerts[] = {
        -0.8f, 0.0f, -0.8f,    0.8f, 0.0f, -0.8f,    0.8f, 0.6f, -0.8f,
        -0.8f, 0.0f, -0.8f,    0.8f, 0.6f, -0.8f,   -0.8f, 0.6f, -0.8f,
         0.0f, 0.3f, -0.8f,    0.0f, 0.6f, -2.4f,    0.0f, 0.1f, -2.4f
    };
    w3dEngine::Color4f(0.85f, 0.85f, 0.88f, 1.0f);
    w3dEngine::VertexPointer3f(0, turretVerts);
    w3dEngine::DrawTrianglesArray(9);
    w3dEngine::PopMatrix();

    w3dEngine::DisableArray(w3dEngine::TexCoordArray);
    w3dEngine::Enable(w3dEngine::CullFace);
    w3dEngine::PopMatrix();
}

void Renderer::renderProjectiles() {
    // 1. Trazadoras Láser del Cañón (Dardos de Plasma Supónicos Incandescentes)
    if (!bullets_.empty()) {
        std::vector<float> bulletVerts;
        std::vector<unsigned char> bulletColors;
        bulletVerts.reserve(bullets_.size() * 54);
        bulletColors.reserve(bullets_.size() * 72);

        for (const auto& b : bullets_) {
            float hw = 0.035f; // Estilizado, afilado y elegante
            float len = 2.4f;

            // Cinta horizontal cruzada con punta cónica y cola afilada
            bulletVerts.insert(bulletVerts.end(), {
                b.x - hw * 0.3f, b.y, b.z,
                b.x + hw * 0.3f, b.y, b.z,
                b.x + hw,        b.y, b.z + len * 0.35f,

                b.x - hw * 0.3f, b.y, b.z,
                b.x + hw,        b.y, b.z + len * 0.35f,
                b.x - hw,        b.y, b.z + len * 0.35f,

                b.x - hw,        b.y, b.z + len * 0.35f,
                b.x + hw,        b.y, b.z + len * 0.35f,
                b.x,             b.y, b.z + len
            });

            // Cinta vertical cruzada
            bulletVerts.insert(bulletVerts.end(), {
                b.x, b.y - hw * 0.3f, b.z,
                b.x, b.y + hw * 0.3f, b.z,
                b.x, b.y + hw,        b.z + len * 0.35f,

                b.x, b.y - hw * 0.3f, b.z,
                b.x, b.y + hw,        b.z + len * 0.35f,
                b.x, b.y - hw,        b.z + len * 0.35f,

                b.x, b.y - hw,        b.z + len * 0.35f,
                b.x, b.y + hw,        b.z + len * 0.35f,
                b.x, b.y,             b.z + len
            });

            for (int k = 0; k < 2; ++k) {
                // Punta: blanco incandescente (255, 255, 240, 255)
                // Centro: oro radiante (255, 210, 50, 240)
                // Cola: naranja suave que desvanece (255, 80, 10, 0)
                bulletColors.insert(bulletColors.end(), {
                    255, 255, 240, 255,
                    255, 255, 240, 255,
                    255, 210,  50, 240,

                    255, 255, 240, 255,
                    255, 210,  50, 240,
                    255, 210,  50, 240,

                    255, 210,  50, 240,
                    255, 210,  50, 240,
                    255,  80,  10,   0
                });
            }
        }

        w3dEngine::Disable(w3dEngine::Texture2D);
        w3dEngine::Disable(w3dEngine::CullFace);
        w3dEngine::Enable(w3dEngine::Blend);
        w3dEngine::SetMezcla(w3dEngine::MezclaAddAlpha);
        w3dEngine::DepthMask(false);
        w3dEngine::Enable(w3dEngine::ColorMaterial);
        w3dEngine::EnableArray(w3dEngine::VertexArray);
        w3dEngine::EnableArray(w3dEngine::ColorArray);
        w3dEngine::VertexPointer3f(0, bulletVerts.data());
        w3dEngine::ColorPointer4ub(bulletColors.data());
        w3dEngine::DrawTrianglesArray(static_cast<int>(bulletVerts.size() / 3));
        w3dEngine::DisableArray(w3dEngine::ColorArray);
        w3dEngine::DepthMask(true);
        w3dEngine::SetMezcla(w3dEngine::MezclaAlpha);
    }

    // 2. Destello de boca
    if (muzzleFlashTime_ > 0.0f) {
        float mfVerts[] = {
            planeX_ - 0.9f, planeY_ - 0.2f, -7.2f,
            planeX_ - 0.9f, planeY_ - 0.2f, -8.8f,
            planeX_ + 0.9f, planeY_ - 0.2f, -7.2f,
            planeX_ + 0.9f, planeY_ - 0.2f, -8.8f
        };
        static const unsigned char mfColors[] = {
            255, 255, 220, 255,   255, 140, 30, 220,
            255, 255, 220, 255,   255, 140, 30, 220
        };
        w3dEngine::Disable(w3dEngine::Texture2D);
        w3dEngine::Enable(w3dEngine::ColorMaterial);
        w3dEngine::LineWidth(5.5f);
        w3dEngine::EnableArray(w3dEngine::VertexArray);
        w3dEngine::EnableArray(w3dEngine::ColorArray);
        w3dEngine::VertexPointer3f(0, mfVerts);
        w3dEngine::ColorPointer4ub(mfColors);
        w3dEngine::DrawLines(4);
        w3dEngine::DisableArray(w3dEngine::ColorArray);
    }

    // 3. Estelas de humo de misiles y bombas (Billboard Soft Smoke Puffs)
    if (!missileSmoke_.empty()) {
        w3dEngine::Enable(w3dEngine::Blend);
        w3dEngine::SetMezcla(w3dEngine::MezclaAlpha);
        w3dEngine::Disable(w3dEngine::CullFace);
        w3dEngine::DepthMask(false);
        w3dEngine::EnableArray(w3dEngine::VertexArray);

        if (texFxExplosion_) {
            w3dEngine::Enable(w3dEngine::Texture2D);
            w3dEngine::BindTexture(texFxExplosion_->getTextureID());
            w3dEngine::EnableArray(w3dEngine::TexCoordArray);
        } else {
            w3dEngine::Disable(w3dEngine::Texture2D);
            w3dEngine::Enable(w3dEngine::ColorMaterial);
        }

        std::vector<float> smkVerts;
        std::vector<float> smkUVs;
        std::vector<unsigned char> smkColors;
        smkVerts.reserve(missileSmoke_.size() * 18);
        if (texFxExplosion_) smkUVs.reserve(missileSmoke_.size() * 12);
        smkColors.reserve(missileSmoke_.size() * 24);

        for (const auto& sp : missileSmoke_) {
            float c = std::cos(sp.angle) * sp.size;
            float s = std::sin(sp.angle) * sp.size;

            smkVerts.insert(smkVerts.end(), {
                sp.x - c + s, sp.y - s - c, sp.z,
                sp.x + c + s, sp.y + s - c, sp.z,
                sp.x + c - s, sp.y + s + c, sp.z,

                sp.x - c + s, sp.y - s - c, sp.z,
                sp.x + c - s, sp.y + s + c, sp.z,
                sp.x - c - s, sp.y - s + c, sp.z
            });

            if (texFxExplosion_) {
                smkUVs.insert(smkUVs.end(), {
                    0.505f, 0.005f,   0.995f, 0.005f,   0.995f, 0.495f,
                    0.505f, 0.005f,   0.995f, 0.495f,   0.505f, 0.495f
                });
            }

            unsigned char a = static_cast<unsigned char>(CLAMP(sp.a, 0.0f, 1.0f) * 255.0f);
            unsigned char r = static_cast<unsigned char>(CLAMP(sp.r, 0.0f, 1.0f) * 255.0f);
            unsigned char g = static_cast<unsigned char>(CLAMP(sp.g, 0.0f, 1.0f) * 255.0f);
            unsigned char b = static_cast<unsigned char>(CLAMP(sp.b, 0.0f, 1.0f) * 255.0f);
            for (int k = 0; k < 6; ++k) {
                smkColors.insert(smkColors.end(), { r, g, b, a });
            }
        }

        w3dEngine::Enable(w3dEngine::ColorMaterial);
        w3dEngine::EnableArray(w3dEngine::ColorArray);
        w3dEngine::VertexPointer3f(0, smkVerts.data());
        if (texFxExplosion_) w3dEngine::TexCoordPointer2f(0, smkUVs.data());
        w3dEngine::ColorPointer4ub(smkColors.data());
        w3dEngine::DrawTrianglesArray(static_cast<int>(smkVerts.size() / 3));

        w3dEngine::DisableArray(w3dEngine::ColorArray);
        if (texFxExplosion_) w3dEngine::DisableArray(w3dEngine::TexCoordArray);
        w3dEngine::DepthMask(true);
        w3dEngine::Enable(w3dEngine::CullFace);
    }

    // 4. Misiles Guiados Activos en 3D
    if (!activeMissiles_.empty()) {
        static const float mslBodyVerts[] = {
            // Ojiva cónica frontal
             0.0f,  0.0f, -1.2f,   -0.12f,  0.12f, -0.4f,    0.12f,  0.12f, -0.4f,
             0.0f,  0.0f, -1.2f,    0.12f,  0.12f, -0.4f,    0.12f, -0.12f, -0.4f,
             0.0f,  0.0f, -1.2f,    0.12f, -0.12f, -0.4f,   -0.12f, -0.12f, -0.4f,
             0.0f,  0.0f, -1.2f,   -0.12f, -0.12f, -0.4f,   -0.12f,  0.12f, -0.4f,
            // Fuselaje cilíndrico
            -0.12f,  0.12f, -0.4f,  -0.12f,  0.12f,  0.8f,    0.12f,  0.12f,  0.8f,
            -0.12f,  0.12f, -0.4f,   0.12f,  0.12f,  0.8f,    0.12f,  0.12f, -0.4f,
            -0.12f, -0.12f, -0.4f,   0.12f, -0.12f,  0.8f,   -0.12f, -0.12f,  0.8f,
            -0.12f, -0.12f, -0.4f,   0.12f, -0.12f, -0.4f,    0.12f, -0.12f,  0.8f,
            // Aletas de estabilización en cruz
            -0.38f,  0.0f,  0.5f,    0.38f,  0.0f,  0.5f,    0.0f,   0.0f,   0.85f,
             0.0f, -0.38f,  0.5f,    0.0f,   0.38f,  0.5f,    0.0f,   0.0f,   0.85f,
            // Llama de postcombustión de cohete
             0.0f,  0.0f,  1.4f,   -0.10f,  0.0f,  0.8f,    0.10f,  0.0f,   0.8f,
             0.0f,  0.0f,  1.4f,    0.0f,  -0.10f, 0.8f,    0.0f,   0.10f,  0.8f
        };
        static const unsigned char mslBodyColors[] = {
            // Ojiva amarilla/roja
            255, 210, 40, 255,   255, 60, 40, 255,   255, 60, 40, 255,
            255, 210, 40, 255,   255, 60, 40, 255,   255, 60, 40, 255,
            255, 210, 40, 255,   255, 60, 40, 255,   255, 60, 40, 255,
            255, 210, 40, 255,   255, 60, 40, 255,   255, 60, 40, 255,
            // Cuerpo blanco militar
            245, 245, 245, 255,  230, 230, 230, 255, 230, 230, 230, 255,
            245, 245, 245, 255,  230, 230, 230, 255, 245, 245, 245, 255,
            200, 200, 200, 255,  190, 190, 190, 255, 190, 190, 190, 255,
            200, 200, 200, 255,  200, 200, 200, 255, 190, 190, 190, 255,
            // Aletas rojas
            220, 30, 30, 255,    220, 30, 30, 255,   255, 180, 50, 255,
            220, 30, 30, 255,    220, 30, 30, 255,   255, 180, 50, 255,
            // Llama de escape ardiente
            255, 140, 20, 255,   255, 240, 100, 255, 255, 240, 100, 255,
            255, 140, 20, 255,   255, 240, 100, 255, 255, 240, 100, 255
        };

        w3dEngine::Disable(w3dEngine::Texture2D);
        w3dEngine::Disable(w3dEngine::CullFace);
        w3dEngine::Enable(w3dEngine::ColorMaterial);
        w3dEngine::EnableArray(w3dEngine::VertexArray);
        w3dEngine::EnableArray(w3dEngine::ColorArray);

        for (const auto& m : activeMissiles_) {
            w3dEngine::PushMatrix();
            w3dEngine::Translatef(m.x, m.y, m.z);
            w3dEngine::Rotatef(-m.yaw, 0.0f, 1.0f, 0.0f);
            w3dEngine::Rotatef(m.pitch, 1.0f, 0.0f, 0.0f);

            w3dEngine::VertexPointer3f(0, mslBodyVerts);
            w3dEngine::ColorPointer4ub(mslBodyColors);
            w3dEngine::DrawTrianglesArray(36);
            w3dEngine::PopMatrix();
        }
        w3dEngine::DisableArray(w3dEngine::ColorArray);
    }

    // 5. Bombas Aéreas en Caída Libre (Modelo 3D con Alabes de Cola)
    if (!bombs_.empty()) {
        static const float bombVerts[] = {
            // Ojiva aerodinámica redondeada
             0.0f,  0.0f, -1.1f,   -0.24f,  0.0f, -0.4f,    0.0f,  0.24f, -0.4f,
             0.0f,  0.0f, -1.1f,    0.0f,  0.24f, -0.4f,    0.24f,  0.0f, -0.4f,
             0.0f,  0.0f, -1.1f,    0.0f, -0.24f, -0.4f,   -0.24f,  0.0f, -0.4f,
             0.0f,  0.0f, -1.1f,    0.24f,  0.0f, -0.4f,    0.0f, -0.24f, -0.4f,
            // Cuerpo pesado de la bomba
            -0.24f,  0.0f, -0.4f,  -0.24f,  0.0f,  0.6f,    0.0f,  0.24f,  0.6f,
            -0.24f,  0.0f, -0.4f,   0.0f,  0.24f,  0.6f,    0.0f,  0.24f, -0.4f,
             0.0f,  0.24f, -0.4f,   0.0f,  0.24f,  0.6f,    0.24f,  0.0f,  0.6f,
             0.0f,  0.24f, -0.4f,   0.24f,  0.0f,  0.6f,    0.24f,  0.0f, -0.4f,
            -0.24f,  0.0f, -0.4f,   0.0f, -0.24f,  0.6f,   -0.24f,  0.0f,  0.6f,
            -0.24f,  0.0f, -0.4f,   0.0f, -0.24f, -0.4f,    0.0f, -0.24f,  0.6f,
             0.0f, -0.24f, -0.4f,   0.24f,  0.0f,  0.6f,    0.0f, -0.24f,  0.6f,
             0.0f, -0.24f, -0.4f,   0.24f,  0.0f, -0.4f,    0.24f,  0.0f,  0.6f,
            // Aletas de estabilización de cola
            -0.42f,  0.0f,  0.4f,    0.42f,  0.0f,  0.4f,    0.0f,   0.0f,  0.88f,
             0.0f, -0.42f,  0.4f,    0.0f,   0.42f,  0.4f,    0.0f,   0.0f,  0.88f
        };
        static const unsigned char bombColors[] = {
            // Ojiva oliva oscuro
            65, 85, 55, 255,   65, 85, 55, 255,   85, 110, 75, 255,
            65, 85, 55, 255,   85, 110, 75, 255,  65, 85, 55, 255,
            50, 70, 45, 255,   50, 70, 45, 255,   65, 85, 55, 255,
            50, 70, 45, 255,   65, 85, 55, 255,   50, 70, 45, 255,
            // Franja amarilla de advertencia en el cuerpo
            255, 200, 30, 255, 65, 85, 55, 255,   255, 200, 30, 255,
            255, 200, 30, 255, 255, 200, 30, 255, 65, 85, 55, 255,
            255, 200, 30, 255, 255, 200, 30, 255, 65, 85, 55, 255,
            255, 200, 30, 255, 65, 85, 55, 255,   65, 85, 55, 255,
            50, 70, 45, 255,   50, 70, 45, 255,   255, 200, 30, 255,
            50, 70, 45, 255,   255, 200, 30, 255, 255, 200, 30, 255,
            50, 70, 45, 255,   255, 200, 30, 255, 50, 70, 45, 255,
            50, 70, 45, 255,   50, 70, 45, 255,   255, 200, 30, 255,
            // Aletas de cola rojas / óxido
            180, 50, 30, 255,  180, 50, 30, 255,  220, 90, 40, 255,
            180, 50, 30, 255,  180, 50, 30, 255,  220, 90, 40, 255
        };

        w3dEngine::Disable(w3dEngine::Texture2D);
        w3dEngine::Disable(w3dEngine::CullFace);
        w3dEngine::Enable(w3dEngine::ColorMaterial);
        w3dEngine::EnableArray(w3dEngine::VertexArray);
        w3dEngine::EnableArray(w3dEngine::ColorArray);

        for (const auto& b : bombs_) {
            w3dEngine::PushMatrix();
            w3dEngine::Translatef(b.x, b.y, b.z);
            w3dEngine::Rotatef(b.pitch, 1.0f, 0.0f, 0.0f);

            w3dEngine::VertexPointer3f(0, bombVerts);
            w3dEngine::ColorPointer4ub(bombColors);
            w3dEngine::DrawTrianglesArray(42);
            w3dEngine::PopMatrix();
        }
        w3dEngine::DisableArray(w3dEngine::ColorArray);
    }

    // 6. Proyectiles antiaéreos (Flak) y balas de cazas enemigos (Dardos Carmesí Rápidos)
    if (!enemyBullets_.empty()) {
        std::vector<float> ebVerts;
        std::vector<unsigned char> ebColors;
        ebVerts.reserve(enemyBullets_.size() * 54);
        ebColors.reserve(enemyBullets_.size() * 72);

        for (const auto& b : enemyBullets_) {
            float hw = 0.040f;
            float len = 2.0f;
            // Punta hacia +Z (hacia el jugador), cola hacia -Z
            ebVerts.insert(ebVerts.end(), {
                b.x - hw * 0.3f, b.y, b.z,
                b.x + hw * 0.3f, b.y, b.z,
                b.x + hw,        b.y, b.z - len * 0.35f,

                b.x - hw * 0.3f, b.y, b.z,
                b.x + hw,        b.y, b.z - len * 0.35f,
                b.x - hw,        b.y, b.z - len * 0.35f,

                b.x - hw,        b.y, b.z - len * 0.35f,
                b.x + hw,        b.y, b.z - len * 0.35f,
                b.x,             b.y, b.z - len
            });
            ebVerts.insert(ebVerts.end(), {
                b.x, b.y - hw * 0.3f, b.z,
                b.x, b.y + hw * 0.3f, b.z,
                b.x, b.y + hw,        b.z - len * 0.35f,

                b.x, b.y - hw * 0.3f, b.z,
                b.x, b.y + hw,        b.z - len * 0.35f,
                b.x, b.y - hw,        b.z - len * 0.35f,

                b.x, b.y - hw,        b.z - len * 0.35f,
                b.x, b.y + hw,        b.z - len * 0.35f,
                b.x, b.y,             b.z - len
            });

            for (int k = 0; k < 2; ++k) {
                ebColors.insert(ebColors.end(), {
                    255, 230, 230, 255,
                    255, 230, 230, 255,
                    255,  40,  30, 230,

                    255, 230, 230, 255,
                    255,  40,  30, 230,
                    255,  40,  30, 230,

                    255,  40,  30, 230,
                    255,  40,  30, 230,
                    200,  10,  10,   0
                });
            }
        }

        w3dEngine::Disable(w3dEngine::Texture2D);
        w3dEngine::Disable(w3dEngine::CullFace);
        w3dEngine::Enable(w3dEngine::Blend);
        w3dEngine::SetMezcla(w3dEngine::MezclaAddAlpha);
        w3dEngine::DepthMask(false);
        w3dEngine::Enable(w3dEngine::ColorMaterial);
        w3dEngine::EnableArray(w3dEngine::VertexArray);
        w3dEngine::EnableArray(w3dEngine::ColorArray);
        w3dEngine::VertexPointer3f(0, ebVerts.data());
        w3dEngine::ColorPointer4ub(ebColors.data());
        w3dEngine::DrawTrianglesArray(static_cast<int>(ebVerts.size() / 3));
        w3dEngine::DisableArray(w3dEngine::ColorArray);
        w3dEngine::DepthMask(true);
        w3dEngine::SetMezcla(w3dEngine::MezclaAlpha);
    }
}

void Renderer::renderExplosions() {
    // 1. Explosiones animadas con sprite sheet 2x2
    if (!explosions_.empty()) {
        w3dEngine::Enable(w3dEngine::Blend);
        w3dEngine::Disable(w3dEngine::CullFace);
        w3dEngine::DepthMask(false);
        w3dEngine::EnableArray(w3dEngine::VertexArray);

        if (texFxExplosion_) {
            w3dEngine::Enable(w3dEngine::Texture2D);
            w3dEngine::BindTexture(texFxExplosion_->getTextureID());
            w3dEngine::EnableArray(w3dEngine::TexCoordArray);
        } else {
            w3dEngine::Disable(w3dEngine::Texture2D);
            w3dEngine::Enable(w3dEngine::ColorMaterial);
        }

        for (const auto& exp : explosions_) {
            w3dEngine::PushMatrix();
            w3dEngine::Translatef(exp.x, exp.y, exp.z);

            float prog = 1.0f - (exp.life / exp.maxLife);
            prog = CLAMP(prog, 0.0f, 1.0f);

            // Determinar cuadrante animado (OpenGL invertido por stb_image)
            float u0, u1, v0, v1;
            if (prog < 0.22f) {
                // Cuadrante 0: Destello estelar inicial
                u0 = 0.005f; u1 = 0.495f; v0 = 0.505f; v1 = 0.995f;
                w3dEngine::SetMezcla(w3dEngine::MezclaAddAlpha);
            } else if (prog < 0.52f) {
                // Cuadrante 1: Bola de fuego densa
                u0 = 0.505f; u1 = 0.995f; v0 = 0.505f; v1 = 0.995f;
                w3dEngine::SetMezcla(w3dEngine::MezclaAddAlpha);
            } else if (prog < 0.80f) {
                // Cuadrante 2: Expansión de humo ardiente
                u0 = 0.005f; u1 = 0.495f; v0 = 0.005f; v1 = 0.495f;
                w3dEngine::SetMezcla(w3dEngine::MezclaAlpha);
            } else {
                // Cuadrante 3: Nube de humo y cenizas disipándose
                u0 = 0.505f; u1 = 0.995f; v0 = 0.005f; v1 = 0.495f;
                w3dEngine::SetMezcla(w3dEngine::MezclaAlpha);
            }

            if (texFxExplosion_) {
                float curUVs[] = {
                    u0, v0,   u1, v0,   u1, v1,
                    u0, v0,   u1, v1,   u0, v1
                };
                w3dEngine::TexCoordPointer2f(0, curUVs);
            }

            float c = std::cos(exp.angle) * exp.radius;
            float s = std::sin(exp.angle) * exp.radius;
            float expVerts[] = {
                -c + s, -s - c, 0.0f,
                 c + s,  s - c, 0.0f,
                 c - s,  s + c, 0.0f,

                -c + s, -s - c, 0.0f,
                 c - s,  s + c, 0.0f,
                -c - s, -s + c, 0.0f
            };

            float alpha = std::sin(prog * 3.14159f);
            alpha = CLAMP(alpha * 1.25f, 0.0f, 1.0f);
            w3dEngine::Color4f(exp.r, exp.g, exp.b, alpha);
            w3dEngine::VertexPointer3f(0, expVerts);
            w3dEngine::DrawTrianglesArray(6);

            w3dEngine::PopMatrix();
        }

        if (texFxExplosion_) {
            w3dEngine::DisableArray(w3dEngine::TexCoordArray);
        }
        w3dEngine::DepthMask(true);
        w3dEngine::SetMezcla(w3dEngine::MezclaAlpha);
        w3dEngine::Enable(w3dEngine::CullFace);
    }

    // 2. Chispas y restos incandescentes volando con física (Debris)
    if (!explosionDebris_.empty()) {
        w3dEngine::Disable(w3dEngine::Texture2D);
        w3dEngine::Disable(w3dEngine::CullFace);
        w3dEngine::Enable(w3dEngine::Blend);
        w3dEngine::SetMezcla(w3dEngine::MezclaAddAlpha);
        w3dEngine::DepthMask(false);
        w3dEngine::Enable(w3dEngine::ColorMaterial);
        w3dEngine::EnableArray(w3dEngine::VertexArray);
        w3dEngine::EnableArray(w3dEngine::ColorArray);

        std::vector<float> dVerts;
        std::vector<unsigned char> dColors;
        dVerts.reserve(explosionDebris_.size() * 18);
        dColors.reserve(explosionDebris_.size() * 24);

        for (const auto& deb : explosionDebris_) {
            float s = deb.size;
            dVerts.insert(dVerts.end(), {
                deb.x - s, deb.y - s, deb.z,
                deb.x + s, deb.y - s, deb.z,
                deb.x + s, deb.y + s, deb.z,

                deb.x - s, deb.y - s, deb.z,
                deb.x + s, deb.y + s, deb.z,
                deb.x - s, deb.y + s, deb.z
            });
            float a = CLAMP(deb.life / deb.maxLife, 0.0f, 1.0f);
            unsigned char r = static_cast<unsigned char>(deb.r * 255.0f);
            unsigned char g = static_cast<unsigned char>(deb.g * 255.0f);
            unsigned char b = static_cast<unsigned char>(deb.b * 255.0f);
            unsigned char alpha = static_cast<unsigned char>(a * 255.0f);
            for (int k = 0; k < 6; ++k) {
                dColors.insert(dColors.end(), { r, g, b, alpha });
            }
        }

        w3dEngine::VertexPointer3f(0, dVerts.data());
        w3dEngine::ColorPointer4ub(dColors.data());
        w3dEngine::DrawTrianglesArray(static_cast<int>(dVerts.size() / 3));
        w3dEngine::DisableArray(w3dEngine::ColorArray);
        w3dEngine::DepthMask(true);
        w3dEngine::SetMezcla(w3dEngine::MezclaAlpha);
        w3dEngine::Enable(w3dEngine::CullFace);
    }
}

// ----------------------------------------------------------------------------
// INTERFAZ 2D & MENÚS (DISEÑADOS PARA MODO RETRATO)
// ----------------------------------------------------------------------------

void Renderer::renderHUDQuad(float x, float y, float w, float h, GLuint texId, float alpha) {
    if (!texId) return;

    // Vértices en orden Counter-Clockwise (CCW) para proyección 2D Ortho invertida
    float qVerts[] = {
        x,     y,
        x,     y + h,
        x + w, y + h,

        x,     y,
        x + w, y + h,
        x + w, y
    };
    static const float qUVs[] = {
        0.0f, 1.0f,
        0.0f, 0.0f,
        1.0f, 0.0f,

        0.0f, 1.0f,
        1.0f, 0.0f,
        1.0f, 1.0f
    };

    w3dEngine::Disable(w3dEngine::CullFace);
    w3dEngine::Disable(w3dEngine::DepthTest);
    w3dEngine::Enable(w3dEngine::Blend);
    w3dEngine::SetMezcla(w3dEngine::MezclaAlpha);
    w3dEngine::Enable(w3dEngine::Texture2D);
    w3dEngine::BindTexture(texId);
    w3dEngine::Color4f(1.0f, 1.0f, 1.0f, alpha);
    w3dEngine::EnableArray(w3dEngine::VertexArray);
    w3dEngine::EnableArray(w3dEngine::TexCoordArray);
    w3dEngine::VertexPointer2f(0, qVerts);
    w3dEngine::TexCoordPointer2f(0, qUVs);
    w3dEngine::DrawTrianglesArray(6);
    w3dEngine::DisableArray(w3dEngine::TexCoordArray);
}

void Renderer::renderHUDBar(float x, float y, float w, float h, float fillPct, float r, float g, float b, float a) {
    w3dEngine::Disable(w3dEngine::CullFace);
    w3dEngine::Disable(w3dEngine::DepthTest);
    w3dEngine::Enable(w3dEngine::Blend);
    w3dEngine::SetMezcla(w3dEngine::MezclaAlpha);
    w3dEngine::Disable(w3dEngine::Texture2D);

    float bgVerts[] = {
        x,     y,
        x,     y + h,
        x + w, y + h,

        x,     y,
        x + w, y + h,
        x + w, y
    };
    w3dEngine::Color4f(0.05f, 0.12f, 0.20f, 0.75f);
    w3dEngine::EnableArray(w3dEngine::VertexArray);
    w3dEngine::VertexPointer2f(0, bgVerts);
    w3dEngine::DrawTrianglesArray(6);

    float fillW = w * CLAMP(fillPct, 0.0f, 1.0f);
    if (fillW > 0.0f) {
        float fgVerts[] = {
            x,         y,
            x,         y + h,
            x + fillW, y + h,

            x,         y,
            x + fillW, y + h,
            x + fillW, y
        };
        w3dEngine::Color4f(r, g, b, a);
        w3dEngine::VertexPointer2f(0, fgVerts);
        w3dEngine::DrawTrianglesArray(6);
    }
}

void Renderer::renderHUDRect(float x, float y, float w, float h, float r, float g, float b, float a) {
    float rVerts[] = {
        x,     y,
        x,     y + h,
        x + w, y + h,

        x,     y,
        x + w, y + h,
        x + w, y
    };
    w3dEngine::Disable(w3dEngine::CullFace);
    w3dEngine::Disable(w3dEngine::DepthTest);
    w3dEngine::Enable(w3dEngine::Blend);
    w3dEngine::SetMezcla(w3dEngine::MezclaAlpha);
    w3dEngine::Disable(w3dEngine::Texture2D);
    w3dEngine::Color4f(r, g, b, a);
    w3dEngine::EnableArray(w3dEngine::VertexArray);
    w3dEngine::VertexPointer2f(0, rVerts);
    w3dEngine::DrawTrianglesArray(6);
}

void Renderer::renderHUDLine(float x0, float y0, float x1, float y1, float r, float g, float b, float a, float width) {
    float lVerts[] = { x0, y0, x1, y1 };
    w3dEngine::Disable(w3dEngine::CullFace);
    w3dEngine::Disable(w3dEngine::DepthTest);
    w3dEngine::Enable(w3dEngine::Blend);
    w3dEngine::SetMezcla(w3dEngine::MezclaAlpha);
    w3dEngine::Disable(w3dEngine::Texture2D);
    w3dEngine::LineWidth(width);
    w3dEngine::Color4f(r, g, b, a);
    w3dEngine::EnableArray(w3dEngine::VertexArray);
    w3dEngine::VertexPointer2f(0, lVerts);
    w3dEngine::DrawLines(2);
}

void Renderer::renderDigits(float x, float y, float charW, float charH, const std::string& text) {
    if (!texHudDigits_) return;
    static const std::string charset = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ:/-.!?[]%+# ";
    float totalChars = static_cast<float>(charset.size());

    w3dEngine::Disable(w3dEngine::CullFace);
    w3dEngine::Disable(w3dEngine::DepthTest);
    w3dEngine::Enable(w3dEngine::Blend);
    w3dEngine::SetMezcla(w3dEngine::MezclaAlpha);
    w3dEngine::Enable(w3dEngine::Texture2D);
    w3dEngine::BindTexture(texHudDigits_->getTextureID());
    w3dEngine::Color4f(1.0f, 1.0f, 1.0f, 1.0f);
    w3dEngine::EnableArray(w3dEngine::VertexArray);
    w3dEngine::EnableArray(w3dEngine::TexCoordArray);

    float curX = x;
    for (char rawC : text) {
        char c = (char)std::toupper((unsigned char)rawC);
        if (c == ' ') {
            curX += charW * 0.50f;
            continue;
        }
        auto pos = charset.find(c);
        if (pos == std::string::npos) {
            curX += charW * 0.50f;
            continue;
        }
        float u0 = static_cast<float>(pos) / totalChars;
        float u1 = static_cast<float>(pos + 1) / totalChars;

        float qVerts[] = {
            curX,         y,
            curX,         y + charH,
            curX + charW, y + charH,

            curX,         y,
            curX + charW, y + charH,
            curX + charW, y
        };
        float qUVs[] = {
            u0, 1.0f,
            u0, 0.0f,
            u1, 0.0f,

            u0, 1.0f,
            u1, 0.0f,
            u1, 1.0f
        };

        w3dEngine::VertexPointer2f(0, qVerts);
        w3dEngine::TexCoordPointer2f(0, qUVs);
        w3dEngine::DrawTrianglesArray(6);

        curX += charW * 0.78f;
    }
    w3dEngine::DisableArray(w3dEngine::TexCoordArray);
}

void Renderer::renderMainMenuUI() {
    w3dEngine::Disable(w3dEngine::CullFace);
    w3dEngine::Disable(w3dEngine::DepthTest);
    w3dEngine::Enable(w3dEngine::Blend);
    w3dEngine::SetMezcla(w3dEngine::MezclaAlpha);

    w3dEngine::MatrixMode(w3dEngine::Projection);
    w3dEngine::LoadIdentity();
    w3dEngine::Ortho(0.0f, (float)width_, (float)height_, 0.0f, -1.0f, 1.0f);

    w3dEngine::MatrixMode(w3dEngine::ModelView);
    w3dEngine::LoadIdentity();

    // 1. Sombra cinemática suave para destacar el menú sobre el cielo y mar 3D
    renderHUDRect(0.0f, 0.0f, (float)width_, (float)height_, 0.02f, 0.06f, 0.12f, 0.40f);

    // 2. Título Principal (WHISK 3D)
    float titleW = std::min((float)width_ * 0.90f, 420.0f);
    float titleH = titleW * (160.0f / 512.0f);
    float tx = ((float)width_ - titleW) * 0.5f;
    float ty = (float)height_ * 0.12f;

    // Placa de cabina con bisel de neón cian
    renderHUDRect(tx - 8.0f, ty - 8.0f, titleW + 16.0f, titleH + 16.0f, 0.03f, 0.10f, 0.20f, 0.80f);
    renderHUDLine(tx - 8.0f, ty - 8.0f, tx + titleW + 8.0f, ty - 8.0f, 0.0f, 0.85f, 1.0f, 0.90f, 2.0f);
    renderHUDLine(tx - 8.0f, ty + titleH + 8.0f, tx + titleW + 8.0f, ty + titleH + 8.0f, 0.0f, 0.85f, 1.0f, 0.90f, 2.0f);
    renderHUDLine(tx - 8.0f, ty - 8.0f, tx - 8.0f, ty + titleH + 8.0f, 0.0f, 0.85f, 1.0f, 0.90f, 2.0f);
    renderHUDLine(tx + titleW + 8.0f, ty - 8.0f, tx + titleW + 8.0f, ty + titleH + 8.0f, 0.0f, 0.85f, 1.0f, 0.90f, 2.0f);

    if (texMenuTitle_) {
        renderHUDQuad(tx, ty, titleW, titleH, texMenuTitle_->getTextureID(), 1.0f);
    } else {
        renderDigits(tx + 20.0f, ty + 15.0f, 24.0f, 32.0f, "WHISK 3D");
    }

    // 3. Botón JUGAR / DESPEGAR (Pulsante e interactivo)
    float pulse = 0.96f + 0.04f * std::sin(timeSec_ * 5.0f);
    float btnW = std::min((float)width_ * 0.82f, 320.0f) * pulse;
    float btnH = 68.0f * pulse;
    float bx = ((float)width_ - btnW) * 0.5f;
    float by = (float)height_ * 0.48f;

    // Resplandor táctil
    renderHUDRect(bx - 5.0f, by - 5.0f, btnW + 10.0f, btnH + 10.0f, 0.06f, 0.25f, 0.45f, 0.75f);
    renderHUDLine(bx - 5.0f, by - 5.0f, bx + btnW + 5.0f, by - 5.0f, 0.2f, 1.0f, 0.90f, 0.95f, 2.0f);
    renderHUDLine(bx - 5.0f, by + btnH + 5.0f, bx + btnW + 5.0f, by + btnH + 5.0f, 0.2f, 1.0f, 0.90f, 0.95f, 2.0f);
    renderHUDLine(bx - 5.0f, by - 5.0f, bx - 5.0f, by + btnH + 5.0f, 0.2f, 1.0f, 0.90f, 0.95f, 2.0f);
    renderHUDLine(bx + btnW + 5.0f, by - 5.0f, bx + btnW + 5.0f, by + btnH + 5.0f, 0.2f, 1.0f, 0.90f, 0.95f, 2.0f);

    if (texBtnPlay_) {
        renderHUDQuad(bx, by, btnW, btnH, texBtnPlay_->getTextureID(), 1.0f);
    } else {
        renderDigits(bx + 30.0f, by + 18.0f, 22.0f, 28.0f, "DESPEGAR");
    }

    // 4. Indicador Parpadeante: "PULSA DESPEGAR"
    float blink = 0.5f + 0.5f * std::sin(timeSec_ * 5.5f);
    if (blink > 0.40f) {
        std::string tapStr = "PULSA DESPEGAR";
        float tw = tapStr.size() * 13.0f * 0.78f;
        renderDigits(((float)width_ - tw) * 0.5f, by + btnH + 16.0f, 13.0f, 18.0f, tapStr);
    }

    // 5. Botón AYUDA / MANUAL (?)
    float hBtnW = std::min((float)width_ * 0.72f, 260.0f);
    float hBtnH = 48.0f;
    float hx = ((float)width_ - hBtnW) * 0.5f;
    float hy = by + btnH + 48.0f;
    renderHUDRect(hx, hy, hBtnW, hBtnH, 0.05f, 0.15f, 0.28f, 0.85f);
    renderHUDLine(hx, hy, hx + hBtnW, hy, 0.0f, 0.75f, 0.95f, 0.80f, 1.5f);
    renderHUDLine(hx, hy + hBtnH, hx + hBtnW, hy + hBtnH, 0.0f, 0.75f, 0.95f, 0.80f, 1.5f);
    renderHUDLine(hx, hy, hx, hy + hBtnH, 0.0f, 0.75f, 0.95f, 0.80f, 1.5f);
    renderHUDLine(hx + hBtnW, hy, hx + hBtnW, hy + hBtnH, 0.0f, 0.75f, 0.95f, 0.80f, 1.5f);

    if (texBtnHelp_) {
        renderHUDQuad(hx + 14.0f, hy + (hBtnH - 28.0f) * 0.5f, 28.0f, 28.0f, texBtnHelp_->getTextureID(), 0.95f);
    }
    renderDigits(hx + 52.0f, hy + 14.0f, 13.0f, 20.0f, "MANUAL / AYUDA");

    // 6. Botón Sonido (Esquina superior derecha)
    float sndX = (float)width_ - 64.0f;
    float sndY = 20.0f;
    float sndSize = 48.0f;
    renderHUDRect(sndX - 4.0f, sndY - 4.0f, sndSize + 8.0f, sndSize + 8.0f, 0.05f, 0.15f, 0.25f, 0.75f);
    renderHUDLine(sndX - 4.0f, sndY - 4.0f, sndX + sndSize + 4.0f, sndY - 4.0f, 0.0f, 0.8f, 1.0f, 0.8f, 1.5f);
    renderHUDLine(sndX - 4.0f, sndY + sndSize + 4.0f, sndX + sndSize + 4.0f, sndY + sndSize + 4.0f, 0.0f, 0.8f, 1.0f, 0.8f, 1.5f);
    renderHUDLine(sndX - 4.0f, sndY - 4.0f, sndX - 4.0f, sndY + sndSize + 4.0f, 0.0f, 0.8f, 1.0f, 0.8f, 1.5f);
    renderHUDLine(sndX + sndSize + 4.0f, sndY - 4.0f, sndX + sndSize + 4.0f, sndY + sndSize + 4.0f, 0.0f, 0.8f, 1.0f, 0.8f, 1.5f);
    if (texBtnSound_) {
        renderHUDQuad(sndX, sndY, sndSize, sndSize, texBtnSound_->getTextureID(), soundEnabled_ ? 0.98f : 0.35f);
    }

    // 7. Récord de puntuación (Pie de pantalla)
    float recordW = std::min((float)width_ * 0.85f, 300.0f);
    float rx = ((float)width_ - recordW) * 0.5f;
    float ry = (float)height_ - 62.0f;
    renderHUDRect(rx, ry, recordW, 36.0f, 0.04f, 0.12f, 0.22f, 0.80f);
    renderHUDLine(rx, ry, rx + recordW, ry, 0.0f, 0.85f, 0.95f, 0.9f, 1.8f);
    renderHUDLine(rx, ry + 36.0f, rx + recordW, ry + 36.0f, 0.0f, 0.85f, 0.95f, 0.9f, 1.8f);
    renderHUDLine(rx, ry, rx, ry + 36.0f, 0.0f, 0.85f, 0.95f, 0.9f, 1.8f);
    renderHUDLine(rx + recordW, ry, rx + recordW, ry + 36.0f, 0.0f, 0.85f, 0.95f, 0.9f, 1.8f);
    std::string recStr = "RECORD: " + std::to_string(highScore_) + " PTS";
    float recW = recStr.size() * 14.0f * 0.78f;
    renderDigits(((float)width_ - recW) * 0.5f, ry + 8.0f, 14.0f, 20.0f, recStr);

    // 8. Modal de Ayuda (Manual de vuelo y combate)
    if (showHelpModal_) {
        renderHUDRect(0.0f, 0.0f, (float)width_, (float)height_, 0.0f, 0.0f, 0.0f, 0.85f);
        float dlgW = std::min((float)width_ * 0.94f, 540.0f);
        float dlgH = dlgW * (440.0f / 640.0f);
        float dx = ((float)width_ - dlgW) * 0.5f;
        float dy = ((float)height_ - dlgH) * 0.5f;
        if (texDialogHelp_) {
            renderHUDQuad(dx, dy, dlgW, dlgH, texDialogHelp_->getTextureID(), 1.0f);
        } else {
            renderHUDRect(dx, dy, dlgW, dlgH, 0.05f, 0.15f, 0.30f, 0.95f);
        }
        std::string closeStr = "TOCA PARA CERRAR";
        float cw = closeStr.size() * 14.0f * 0.78f;
        renderDigits(((float)width_ - cw) * 0.5f, dy + dlgH + 16.0f, 14.0f, 20.0f, closeStr);
    }
}

void Renderer::renderPauseUI() {
    w3dEngine::Disable(w3dEngine::CullFace);
    w3dEngine::Disable(w3dEngine::DepthTest);
    w3dEngine::Enable(w3dEngine::Blend);
    w3dEngine::SetMezcla(w3dEngine::MezclaAlpha);

    w3dEngine::MatrixMode(w3dEngine::Projection);
    w3dEngine::LoadIdentity();
    w3dEngine::Ortho(0.0f, (float)width_, (float)height_, 0.0f, -1.0f, 1.0f);

    w3dEngine::MatrixMode(w3dEngine::ModelView);
    w3dEngine::LoadIdentity();

    // Fondo oscurecido
    renderHUDRect(0.0f, 0.0f, (float)width_, (float)height_, 0.02f, 0.05f, 0.10f, 0.70f);

    float dlgW = std::min((float)width_ * 0.88f, 380.0f);
    float dlgH = 340.0f;
    float dx = ((float)width_ - dlgW) * 0.5f;
    float dy = ((float)height_ - dlgH) * 0.5f;

    if (texDialogPause_) {
        renderHUDQuad(dx, dy, dlgW, dlgH, texDialogPause_->getTextureID(), 1.0f);
    } else {
        renderHUDRect(dx, dy, dlgW, dlgH, 0.05f, 0.15f, 0.28f, 0.92f);
    }

    float btnW = dlgW * 0.82f, btnH = 50.0f;
    float bx = ((float)width_ - btnW) * 0.5f;

    if (texBtnResume_)  renderHUDQuad(bx, dy + 105.0f, btnW, btnH, texBtnResume_->getTextureID(), 0.95f);
    if (texBtnRestart_) renderHUDQuad(bx, dy + 175.0f, btnW, btnH, texBtnRestart_->getTextureID(), 0.95f);
    if (texBtnQuit_)    renderHUDQuad(bx, dy + 245.0f, btnW, btnH, texBtnQuit_->getTextureID(), 0.95f);

    // Botón de sonido también disponible en pausa
    float sndX = (float)width_ - 64.0f, sndY = 20.0f, sndSize = 48.0f;
    renderHUDRect(sndX - 4.0f, sndY - 4.0f, sndSize + 8.0f, sndSize + 8.0f, 0.05f, 0.15f, 0.25f, 0.75f);
    if (texBtnSound_) {
        renderHUDQuad(sndX, sndY, sndSize, sndSize, texBtnSound_->getTextureID(), soundEnabled_ ? 0.98f : 0.35f);
    }
}

void Renderer::renderGameOverUI() {
    w3dEngine::Disable(w3dEngine::CullFace);
    w3dEngine::Disable(w3dEngine::DepthTest);
    w3dEngine::Enable(w3dEngine::Blend);
    w3dEngine::SetMezcla(w3dEngine::MezclaAlpha);

    w3dEngine::MatrixMode(w3dEngine::Projection);
    w3dEngine::LoadIdentity();
    w3dEngine::Ortho(0.0f, (float)width_, (float)height_, 0.0f, -1.0f, 1.0f);

    w3dEngine::MatrixMode(w3dEngine::ModelView);
    w3dEngine::LoadIdentity();

    renderHUDRect(0.0f, 0.0f, (float)width_, (float)height_, 0.12f, 0.02f, 0.02f, 0.75f);

    float dlgW = std::min((float)width_ * 0.90f, 400.0f);
    float dlgH = 340.0f;
    float dx = ((float)width_ - dlgW) * 0.5f;
    float dy = ((float)height_ - dlgH) * 0.5f;

    if (texDialogGameOver_) {
        renderHUDQuad(dx, dy, dlgW, dlgH, texDialogGameOver_->getTextureID(), 1.0f);
    } else {
        renderHUDRect(dx, dy, dlgW, dlgH, 0.25f, 0.05f, 0.05f, 0.95f);
    }

    std::string scoreStr = "PUNTOS: " + std::to_string(enemiesDestroyed_ * 1500);
    float sw = scoreStr.size() * 14.0f * 0.78f;
    renderDigits(((float)width_ - sw) * 0.5f, dy + 115.0f, 14.0f, 20.0f, scoreStr);

    std::string killStr = "ENEMIGOS: " + std::to_string(enemiesDestroyed_);
    float kw = killStr.size() * 14.0f * 0.78f;
    renderDigits(((float)width_ - kw) * 0.5f, dy + 145.0f, 14.0f, 20.0f, killStr);

    float btnW = dlgW * 0.80f, btnH = 50.0f;
    float bx = ((float)width_ - btnW) * 0.5f;

    if (texBtnRestart_) renderHUDQuad(bx, dy + 190.0f, btnW, btnH, texBtnRestart_->getTextureID(), 0.95f);
    if (texBtnQuit_)    renderHUDQuad(bx, dy + 255.0f, btnW, btnH, texBtnQuit_->getTextureID(), 0.95f);
}

bool Renderer::projectWorldToScreen(float wx, float wy, float wz, float &outSx, float &outSy) const {
    if (width_ <= 0 || height_ <= 0) return false;

    float aspect = static_cast<float>(width_) / static_cast<float>(height_);
    float fovY = (aspect < 1.0f) ? 66.0f : 55.0f;
    float camLagX = planeX_ * 0.38f;
    float camLagY = planeY_ * 0.40f;
    float camBank = planeRoll_ * 0.14f;

    // 1. Traslación según la posición de la cámara en 3D
    float x1 = wx - camLagX;
    float y1 = wy - (1.2f + camLagY);
    float z1 = wz - 2.5f;

    // 2. Rotación por inclinación de cámara (alabeo)
    float rad = camBank * 0.0174532925f;
    float cosA = std::cos(rad);
    float sinA = std::sin(rad);
    float x2 = x1 * cosA - y1 * sinA;
    float y2 = x1 * sinA + y1 * cosA;
    float z2 = z1;

    // Debe estar en frente del plano de la cámara
    if (z2 >= -0.5f) return false;

    // 3. Proyección en perspectiva
    float fovRad = fovY * 0.0174532925f;
    float tanHalfY = std::tan(fovRad * 0.5f);
    float tanHalfX = tanHalfY * aspect;

    float ndcX = x2 / ((-z2) * tanHalfX);
    float ndcY = y2 / ((-z2) * tanHalfY);

    outSx = (ndcX + 1.0f) * 0.5f * (float)width_;
    outSy = (1.0f - ndcY) * 0.5f * (float)height_;
    return true;
}

void Renderer::renderGameUI() {
    w3dEngine::Disable(w3dEngine::DepthTest);
    w3dEngine::Enable(w3dEngine::Blend);

    w3dEngine::MatrixMode(w3dEngine::Projection);
    w3dEngine::LoadIdentity();
    w3dEngine::Ortho(0.0f, (float)width_, (float)height_, 0.0f, -1.0f, 1.0f);

    w3dEngine::MatrixMode(w3dEngine::ModelView);
    w3dEngine::LoadIdentity();

    // 1. Mira de Cañón Dinámica (Pipper Boresight del Avión)
    float gunSx = (float)width_ * 0.5f;
    float gunSy = (float)height_ * 0.44f;
    projectWorldToScreen(planeX_, planeY_ - 0.25f, -38.0f, gunSx, gunSy);

    if (texHudCrosshair_) {
        float reticleSize = 92.0f;
        float rx = gunSx - (reticleSize * 0.5f);
        float ry = gunSy - (reticleSize * 0.5f);
        renderHUDQuad(rx, ry, reticleSize, reticleSize, texHudCrosshair_->getTextureID(), 0.85f);
    }

    // 1b. Mira Predictiva de Bombardeo (CCIP en la superficie del océano)
    float tFall = std::sqrt(std::max(0.1f, 2.0f * (planeY_ - (-2.5f)) / 26.0f)) + 0.15f;
    float bombImpactZ = -6.8f - (32.0f * tFall + 0.5f * 8.0f * tFall * tFall);
    float bombImpactX = planeX_ + (planeRoll_ * 0.08f * tFall);
    float bsx, bsy;
    if (projectWorldToScreen(bombImpactX, -2.4f, bombImpactZ, bsx, bsy)) {
        float bRad = 12.0f;
        renderHUDLine(bsx - bRad, bsy, bsx, bsy - bRad, 1.0f, 0.85f, 0.1f, 0.65f, 1.5f);
        renderHUDLine(bsx, bsy - bRad, bsx + bRad, bsy, 1.0f, 0.85f, 0.1f, 0.65f, 1.5f);
        renderHUDLine(bsx + bRad, bsy, bsx, bsy + bRad, 1.0f, 0.85f, 0.1f, 0.65f, 1.5f);
        renderHUDLine(bsx, bsy + bRad, bsx - bRad, bsy, 1.0f, 0.85f, 0.1f, 0.65f, 1.5f);
        renderDigits(bsx - 10.0f, bsy + bRad + 2.0f, 7.0f, 10.0f, "BMB");
    }

    // 1c. Caja Táctica de Rastreo y Bloqueo sobre el Objetivo Real en 3D
    float twx = targetX_, twy = targetY_ + 1.8f, twz = targetZ_;
    float targetDist = std::hypot(twx - planeX_, twy - planeY_, twz - (-6.5f));
    if (missileTargetMode_ == 1 && missileTargetIdx_ >= 0 && missileTargetIdx_ < (int)enemyJets_.size() && enemyJets_[missileTargetIdx_].active) {
        twx = enemyJets_[missileTargetIdx_].x;
        twy = enemyJets_[missileTargetIdx_].y;
        twz = enemyJets_[missileTargetIdx_].z;
        targetDist = std::hypot(twx - planeX_, twy - planeY_, twz - (-6.5f));
    }

    float tsx, tsy;
    if (projectWorldToScreen(twx, twy, twz, tsx, tsy)) {
        float boxSize = targetLocked_ ? 64.0f : 50.0f;
        float halfB = boxSize * 0.5f;
        float bx0 = tsx - halfB, by0 = tsy - halfB;
        float bx1 = tsx + halfB, by1 = tsy + halfB;
        float bCorner = 14.0f;

        float r = targetLocked_ ? 1.0f : 0.95f;
        float g = targetLocked_ ? 0.25f : 0.85f;
        float b = targetLocked_ ? 0.15f : 0.20f;
        float a = targetLocked_ ? (0.85f + 0.15f * std::sin(timeSec_ * 14.0f)) : 0.65f;

        // Corchetes tácticos limpios sobre el objetivo 3D
        renderHUDLine(bx0, by0, bx0 + bCorner, by0, r, g, b, a, 2.0f);
        renderHUDLine(bx0, by0, bx0, by0 + bCorner, r, g, b, a, 2.0f);
        renderHUDLine(bx1, by0, bx1 - bCorner, by0, r, g, b, a, 2.0f);
        renderHUDLine(bx1, by0, bx1, by0 + bCorner, r, g, b, a, 2.0f);
        renderHUDLine(bx0, by1, bx0 + bCorner, by1, r, g, b, a, 2.0f);
        renderHUDLine(bx0, by1, bx0, by1 - bCorner, r, g, b, a, 2.0f);
        renderHUDLine(bx1, by1, bx1 - bCorner, by1, r, g, b, a, 2.0f);
        renderHUDLine(bx1, by1, bx1, by1 - bCorner, r, g, b, a, 2.0f);

        // Rombo central de fijación
        float dSize = 7.0f;
        renderHUDLine(tsx, tsy - dSize, tsx + dSize, tsy, r, g, b, a, 1.8f);
        renderHUDLine(tsx + dSize, tsy, tsx, tsy + dSize, r, g, b, a, 1.8f);
        renderHUDLine(tsx, tsy + dSize, tsx - dSize, tsy, r, g, b, a, 1.8f);
        renderHUDLine(tsx - dSize, tsy, tsx, tsy - dSize, r, g, b, a, 1.8f);

        if (targetLocked_) {
            std::string lockStr = "[ENGANCHADO]";
            float lsw = lockStr.size() * 11.0f * 0.78f;
            renderDigits(tsx - lsw * 0.5f, by1 + 4.0f, 10.0f, 14.0f, lockStr);
        }

        int distMeters = static_cast<int>(targetDist * 10.0f);
        std::string distStr = std::to_string(distMeters) + "M";
        float dsw = distStr.size() * 9.0f * 0.78f;
        renderDigits(tsx - dsw * 0.5f, by0 - 15.0f, 9.0f, 13.0f, distStr);
    }

    // 2. Radar Táctico en Esquina Superior Izquierda y Panel de Telemetría
    float radarSize = 96.0f;
    float rx = 16.0f;
    float ry = 16.0f;
    if (texHudRadar_) {
        renderHUDQuad(rx, ry, radarSize, radarSize, texHudRadar_->getTextureID(), 0.92f);

        float rcX = rx + radarSize * 0.5f;
        float rcY = ry + radarSize * 0.5f;
        float radarRadius = radarSize * 0.44f;

        float sweepAngle = timeSec_ * 3.5f;
        float swX = rcX + std::cos(sweepAngle) * radarRadius;
        float swY = rcY + std::sin(sweepAngle) * radarRadius;
        renderHUDLine(rcX, rcY, swX, swY, 0.1f, 1.0f, 0.5f, 0.55f, 1.8f);

        // Blip del jugador (verde)
        renderHUDRect(rcX - 3.0f, rcY - 3.0f, 6.0f, 6.0f, 0.0f, 1.0f, 0.45f, 1.0f);

        // Blip del buque (rojo pulsante)
        float relX = (targetX_ - planeX_) / 50.0f;
        float relZ = (targetZ_ - (-6.5f)) / 130.0f;
        float dist = std::sqrt(relX * relX + relZ * relZ);
        if (dist > 1.0f) { relX /= dist; relZ /= dist; }
        float blipX = rcX + relX * radarRadius;
        float blipY = rcY + relZ * radarRadius;

        float blipPulse = 0.65f + 0.35f * std::sin(timeSec_ * 10.0f);
        float blipSize = 7.0f * (0.85f + 0.25f * blipPulse);
        renderHUDRect(blipX - blipSize * 0.5f, blipY - blipSize * 0.5f, blipSize, blipSize, 1.0f, 0.2f, 0.15f, blipPulse);

        // Blips de cazas enemigos (naranja)
        for (const auto& ej : enemyJets_) {
            if (!ej.active) continue;
            float erelX = (ej.x - planeX_) / 50.0f;
            float erelZ = (ej.z - (-6.5f)) / 130.0f;
            float edist = std::sqrt(erelX * erelX + erelZ * erelZ);
            if (edist > 1.0f) { erelX /= edist; erelZ /= edist; }
            float ebx = rcX + erelX * radarRadius;
            float eby = rcY + erelZ * radarRadius;
            renderHUDRect(ebx - 2.5f, eby - 2.5f, 5.0f, 5.0f, 1.0f, 0.7f, 0.1f, 0.9f);
        }
    }

    // Panel de Telemetría al costado del Radar
    float telX = rx + radarSize + 12.0f;
    renderDigits(telX, 18.0f, 11.0f, 15.0f, "BAJAS: " + std::to_string(enemiesDestroyed_));
    renderDigits(telX, 36.0f, 11.0f, 15.0f, "PTS: " + std::to_string(enemiesDestroyed_ * 1500));

    float speedPct = CLAMP((speedKnots_ - 380.0f) / 200.0f, 0.0f, 1.0f);
    renderDigits(telX, 54.0f, 9.0f, 13.0f, "VEL " + std::to_string((int)speedKnots_) + " KTS");
    renderHUDBar(telX, 69.0f, 85.0f, 6.0f, speedPct, 0.0f, 0.9f, 0.85f, 0.9f);

    float altPct = CLAMP((altitudeFeet_ - 1500.0f) / 1800.0f, 0.0f, 1.0f);
    renderDigits(telX, 78.0f, 9.0f, 13.0f, "ALT " + std::to_string((int)altitudeFeet_) + " FT");
    renderHUDBar(telX, 93.0f, 85.0f, 6.0f, altPct, 0.0f, 0.85f, 1.0f, 0.9f);

    // 3. Barra de Vida del Buque / Jefe (Centro Superior)
    if (targetLocked_ || (targetZ_ > -110.0f && targetZ_ < 0.0f)) {
        float bannerW = std::min((float)width_ * 0.48f, 200.0f);
        float bannerX = ((float)width_ - bannerW) * 0.5f;
        float bannerY = 16.0f;
        std::string bossTitle = "JEFE: DDG-88";
        float btw = bossTitle.size() * 10.0f * 0.78f;
        renderDigits(((float)width_ - btw) * 0.5f, bannerY, 10.0f, 14.0f, bossTitle);

        float tgtPct = CLAMP(targetHealth_ / targetMaxHealth_, 0.0f, 1.0f);
        renderHUDBar(bannerX, bannerY + 16.0f, bannerW, 8.0f, tgtPct, 0.95f, 0.15f, 0.15f, 0.92f);
    }

    // 4. Joystick Virtual de Vuelo (Esquina Inferior Izquierda)
    {
        float baseCenterX = stickActive_ ? stickOriginX_ : ((float)width_ * 0.22f);
        float baseCenterY = stickActive_ ? stickOriginY_ : ((float)height_ - 130.0f);
        float knobCenterX = stickActive_ ? stickKnobX_ : baseCenterX;
        float knobCenterY = stickActive_ ? stickKnobY_ : baseCenterY;

        GLuint baseTexId = texBtnStickBase_ ? texBtnStickBase_->getTextureID() : (texBtnStick_ ? texBtnStick_->getTextureID() : 0);
        GLuint knobTexId = texBtnStickKnob_ ? texBtnStickKnob_->getTextureID() : (texBtnStick_ ? texBtnStick_->getTextureID() : 0);

        if (baseTexId) {
            renderHUDQuad(baseCenterX - 55.0f, baseCenterY - 55.0f, 110.0f, 110.0f,
                          baseTexId, stickActive_ ? 0.90f : 0.40f);
        }

        if (stickActive_) {
            renderHUDLine(baseCenterX, baseCenterY, knobCenterX, knobCenterY, 0.0f, 0.85f, 1.0f, 0.60f, 2.0f);
        }

        if (knobTexId) {
            renderHUDQuad(knobCenterX - 28.0f, knobCenterY - 28.0f, 56.0f, 56.0f,
                          knobTexId, stickActive_ ? 0.98f : 0.65f);
        }
    }

    // 5. Botones de Combate en Esquina Inferior Derecha (Triángulo Ergonómico)
    float fireBtnX = (float)width_ - 90.0f, fireBtnY = (float)height_ - 110.0f;
    float mslBtnX  = (float)width_ - 90.0f, mslBtnY  = (float)height_ - 235.0f;
    float bombBtnX = (float)width_ - 195.0f, bombBtnY = (float)height_ - 165.0f;

    if (texBtnFire_) {
        float fireW = 96.0f;
        float fx = fireBtnX - (fireW * 0.5f);
        float fy = fireBtnY - (fireW * 0.5f);
        float alpha = firePressed_ ? 1.0f : 0.85f;
        renderHUDQuad(fx, fy, fireW, fireW, texBtnFire_->getTextureID(), alpha);
    }

    if (texBtnMissile_) {
        float mslW = 86.0f;
        float mx = mslBtnX - (mslW * 0.5f);
        float my = mslBtnY - (mslW * 0.5f);
        float alpha = (!activeMissiles_.empty() || missilePressed_) ? 1.0f : 0.85f;
        renderHUDQuad(mx, my, mslW, mslW, texBtnMissile_->getTextureID(), alpha);

        // Indicador de Pips de Munición de Misiles
        float pipGap = 3.0f;
        float pipW = (mslW - (pipGap * 3.0f)) / 4.0f;
        float pipH = 6.0f;
        float pipY = my + mslW + 3.0f;

        for (int i = 0; i < 4; ++i) {
            float px = mx + i * (pipW + pipGap);
            if (i < missileCount_) {
                renderHUDRect(px, pipY, pipW, pipH, 0.1f, 0.95f, 0.35f, 0.95f);
            } else {
                renderHUDRect(px, pipY, pipW, pipH, 0.25f, 0.3f, 0.35f, 0.5f);
            }
        }
    }

    if (texBtnBomb_) {
        float bombW = 86.0f;
        float bx = bombBtnX - (bombW * 0.5f);
        float by = bombBtnY - (bombW * 0.5f);
        float alpha = (!bombs_.empty() || bombPressed_) ? 1.0f : 0.85f;
        renderHUDQuad(bx, by, bombW, bombW, texBtnBomb_->getTextureID(), alpha);

        // Indicador de Pips de Munición de Bombas
        float pipGap = 3.0f;
        float pipW = (bombW - (pipGap * 3.0f)) / 4.0f;
        float pipH = 6.0f;
        float pipY = by + bombW + 3.0f;

        for (int i = 0; i < 4; ++i) {
            float px = bx + i * (pipW + pipGap);
            if (i < bombCount_) {
                renderHUDRect(px, pipY, pipW, pipH, 1.0f, 0.75f, 0.1f, 0.95f);
            } else {
                renderHUDRect(px, pipY, pipW, pipH, 0.35f, 0.3f, 0.2f, 0.5f);
            }
        }
    }

    // 6. Barra de Blindaje del Avión (Pie Central)
    float hullBarW = std::min((float)width_ * 0.40f, 160.0f);
    float hx = ((float)width_ - hullBarW) * 0.5f;
    float hy = (float)height_ - 30.0f;
    std::string armorStr = "BLINDAJE: " + std::to_string((int)(healthPct_ * 100.0f)) + "%";
    float aw = armorStr.size() * 9.0f * 0.78f;
    renderDigits(((float)width_ - aw) * 0.5f, hy - 14.0f, 9.0f, 13.0f, armorStr);
    renderHUDBar(hx, hy, hullBarW, 7.0f, healthPct_,
                 healthPct_ > 0.4f ? 0.2f : 0.95f,
                 healthPct_ > 0.4f ? 0.95f : 0.25f,
                 0.25f, 0.85f);

    // 7. Botones de Pausa y Sonido (Esquina Superior Derecha)
    float pauseX = (float)width_ - 56.0f;
    float pauseY = 16.0f;
    float pauseSize = 44.0f;
    renderHUDRect(pauseX - 3.0f, pauseY - 3.0f, pauseSize + 6.0f, pauseSize + 6.0f, 0.05f, 0.15f, 0.25f, 0.75f);
    renderHUDLine(pauseX - 3.0f, pauseY - 3.0f, pauseX + pauseSize + 3.0f, pauseY - 3.0f, 0.0f, 0.85f, 1.0f, 0.85f, 1.5f);
    renderHUDLine(pauseX - 3.0f, pauseY + pauseSize + 3.0f, pauseX + pauseSize + 3.0f, pauseY + pauseSize + 3.0f, 0.0f, 0.85f, 1.0f, 0.85f, 1.5f);
    renderHUDLine(pauseX - 3.0f, pauseY - 3.0f, pauseX - 3.0f, pauseY + pauseSize + 3.0f, 0.0f, 0.85f, 1.0f, 0.85f, 1.5f);
    renderHUDLine(pauseX + pauseSize + 3.0f, pauseY - 3.0f, pauseX + pauseSize + 3.0f, pauseY + pauseSize + 3.0f, 0.0f, 0.85f, 1.0f, 0.85f, 1.5f);
    if (texBtnPause_) {
        renderHUDQuad(pauseX, pauseY, pauseSize, pauseSize, texBtnPause_->getTextureID(), 0.95f);
    }

    float sndX = pauseX - 52.0f;
    float sndY = 16.0f;
    float sndSize = 44.0f;
    renderHUDRect(sndX - 3.0f, sndY - 3.0f, sndSize + 6.0f, sndSize + 6.0f, 0.05f, 0.15f, 0.25f, 0.75f);
    renderHUDLine(sndX - 3.0f, sndY - 3.0f, sndX + sndSize + 3.0f, sndY - 3.0f, 0.0f, 0.8f, 1.0f, 0.8f, 1.5f);
    renderHUDLine(sndX - 3.0f, sndY + sndSize + 3.0f, sndX + sndSize + 3.0f, sndY + sndSize + 3.0f, 0.0f, 0.8f, 1.0f, 0.8f, 1.5f);
    renderHUDLine(sndX - 3.0f, sndY - 3.0f, sndX - 3.0f, sndY + sndSize + 3.0f, 0.0f, 0.8f, 1.0f, 0.8f, 1.5f);
    renderHUDLine(sndX + sndSize + 3.0f, sndY - 3.0f, sndX + sndSize + 3.0f, sndY + sndSize + 3.0f, 0.0f, 0.8f, 1.0f, 0.8f, 1.5f);
    if (texBtnSound_) {
        renderHUDQuad(sndX, sndY, sndSize, sndSize, texBtnSound_->getTextureID(), soundEnabled_ ? 0.98f : 0.35f);
    }
}

void Renderer::renderWhisk3D() {
    float dt = 0.01667f;

    // ------------------------------------------------------------------------
    // ACTUALIZACIÓN DE ESTADO DEL JUEGO
    // ------------------------------------------------------------------------
    if (gameState_ == STATE_MAIN_MENU) {
        timeSec_ += dt * 0.7f;
        targetRoll_  = std::sin(timeSec_ * 0.8f) * 14.0f;
        targetPitch_ = std::cos(timeSec_ * 0.6f) * 6.0f;
        targetYaw_   = targetRoll_ * 0.3f;
        planeX_ = std::sin(timeSec_ * 0.4f) * 1.8f;
        planeY_ = 0.4f + std::cos(timeSec_ * 0.5f) * 0.5f;

        planeRoll_  += (targetRoll_ - planeRoll_) * 0.10f;
        planePitch_ += (targetPitch_ - planePitch_) * 0.10f;
        planeYaw_   += (targetYaw_ - planeYaw_) * 0.10f;
    }
    else if (gameState_ == STATE_PLAYING) {
        timeSec_ += dt;

        // Vuelo del Jugador con Joystick Virtual
        if (stickActive_) {
            targetRoll_  = stickDeflectX_ * 42.0f;
            targetPitch_ = -stickDeflectY_ * 28.0f;
            targetYaw_   = stickDeflectX_ * 16.0f;

            planeX_ += stickDeflectX_ * 0.17f;
            planeY_ += (-stickDeflectY_) * 0.16f;
        } else {
            targetRoll_  = std::sin(timeSec_ * 1.5f) * 1.2f;
            targetPitch_ = std::cos(timeSec_ * 1.0f) * 0.6f;
            targetYaw_   = 0.0f;
        }

        planeRoll_  += (targetRoll_ - planeRoll_) * 0.18f;
        planePitch_ += (targetPitch_ - planePitch_) * 0.18f;
        planeYaw_   += (targetYaw_ - planeYaw_) * 0.18f;

        // Ampliación del área de navegación vertical y horizontal
        planeX_ = CLAMP(planeX_, -4.2f, 4.2f);
        planeY_ = CLAMP(planeY_, -2.0f, 5.2f);

        altitudeFeet_ = 1800.0f + (planeY_ + 2.0f) * 750.0f;
        speedKnots_   = 480.0f - (planePitch_ * 2.2f);

        // Recarga de misiles y bombas periódica
        if (missileCount_ < 4 && std::fmod(timeSec_, 9.0f) < 0.02f) {
            missileCount_++;
        }
        if (bombCount_ < 4 && std::fmod(timeSec_, 14.0f) < 0.02f) {
            bombCount_++;
        }
        if (bombCooldown_ > 0.0f) bombCooldown_ -= dt;
        if (missileCooldown_ > 0.0f) missileCooldown_ -= dt;

        // Movimiento de nubes
        for (auto &c : clouds_) {
            c.z += c.speed * dt;
            if (c.z > 20.0f) {
                c.z = -250.0f;
                c.x = ((rand() % 140) - 70) * 1.0f;
            }
        }

        // Avance del escenario (Islas y árboles desplazándose hacia el jugador)
        for (auto &isl : islands_) {
            isl.z += 16.0f * dt;
            if (isl.z > 35.0f) isl.z -= 250.0f;
        }
        for (auto &tr : trees_) {
            tr.z += 16.0f * dt;
            if (tr.z > 35.0f) tr.z -= 250.0f;
        }

        // Spawn táctico de cazas enemigos (mantiene presencia enemiga activa constante)
        enemySpawnTimer_ -= dt;
        if (enemySpawnTimer_ <= 0.0f || enemyJets_.size() < 2) {
            enemySpawnTimer_ = 4.2f + (rand() % 16) * 0.1f;
            spawnEnemySquadron();
        }

        // Actualización de cazas enemigos con IA táctica mejorada
        for (auto it = enemyJets_.begin(); it != enemyJets_.end(); ) {
            it->z += it->vz * dt;
            it->flightTimer += dt;
            if (it->hitFlashTime > 0.0f) it->hitFlashTime -= dt;

            // Desenganche táctico al sobrepasar la zona de ataque cercana (z > -20.0f)
            if (it->z > -20.0f && !it->breakingAway) {
                it->breakingAway = true;
                it->vx = (it->x < 0.0f) ? -22.0f : 22.0f;
                it->vy = (it->y < planeY_) ? -6.0f : 8.0f;
            }

            if (it->breakingAway) {
                it->x += it->vx * dt;
                it->y += it->vy * dt;
                it->roll += ((it->vx < 0.0f ? 45.0f : -45.0f) - it->roll) * 0.15f;
            } else if (it->type == 1) {
                // Tipo 1: Caza Evasivo (Zigzag continuo por la pantalla con alabeo coordinado)
                float weaveX = it->baseX + std::sin(it->flightTimer * 2.2f + it->baseX) * 3.4f;
                float desiredVx = (weaveX - it->x) * 4.2f;
                it->vx += (desiredVx - it->vx) * 0.15f;
                it->x += it->vx * dt;
                it->roll = -it->vx * 2.4f;
            } else if (it->type == 0) {
                // Tipo 0: Caza Interceptor (Rastrea al jugador en X e Y para ametrallarlo)
                float leadX = planeX_ * 0.75f;
                float desiredVx = (leadX - it->x) * 1.8f;
                it->vx += (desiredVx - it->vx) * 0.14f;
                it->x += it->vx * dt;

                float desiredVy = (planeY_ - it->y) * 0.85f;
                it->vy += (desiredVy - it->vy) * 0.14f;
                it->y += it->vy * dt;
                it->roll = -it->vx * 2.2f;
            } else {
                // Tipo 2: Bombardero / Caza de Gran Altitud
                it->x += it->vx * dt;
                it->roll = std::sin(it->flightTimer * 1.5f) * 12.0f;
            }

            // Disparo del caza enemigo hacia el jugador (solo si está en frente)
            it->fireCooldown -= dt;
            if (it->fireCooldown <= 0.0f && it->z < -14.0f && it->z > -90.0f && !it->breakingAway) {
                it->fireCooldown = 1.8f + (rand() % 10) * 0.1f;
                EnemyBullet eb;
                eb.x = it->x + (rand() % 2 == 0 ? -0.7f : 0.7f);
                eb.y = it->y - 0.1f;
                eb.z = it->z + 1.8f;
                float distZ = (-6.5f) - eb.z;
                float travelTime = (distZ > 5.0f) ? (distZ / 155.0f) : 0.2f;
                eb.vx = (planeX_ + (rand() % 10 - 5) * 0.08f - eb.x) / travelTime;
                eb.vy = ((planeY_ - 0.2f) - eb.y) / travelTime;
                eb.vz = 155.0f;
                eb.life = travelTime + 0.35f;
                enemyBullets_.push_back(eb);
            }

            if (it->z > 20.0f || !it->active || std::fabs(it->x) > 45.0f) {
                it = enemyJets_.erase(it);
            } else {
                ++it;
            }
        }

        // Avance del buque enemigo
        targetZ_ += 0.26f;
        if (targetZ_ > -2.0f) {
            targetZ_ = -150.0f;
            targetX_ = std::sin(timeSec_ * 0.6f) * 16.0f;
            targetHealth_ = targetMaxHealth_;
        }

        // Ángulo de las torretas del buque apuntando al jugador
        warshipTurretAngle_ = std::atan2(planeX_ - targetX_, -(planeZ_ - targetZ_)) * 57.2957f;

        // Búsqueda del mejor objetivo en el cono frontal
        float bestScore = 9999.0f;
        int bestJetIdx = -1;
        bool foundJet = false;

        for (size_t idx = 0; idx < enemyJets_.size(); ++idx) {
            const auto& ej = enemyJets_[idx];
            if (!ej.active || ej.z > -12.0f || ej.z < -120.0f) continue;
            float edx = ej.x - planeX_;
            float edy = ej.y - planeY_;
            float edz = ej.z - (-6.5f);
            float angleDist = std::hypot(edx, edy);
            if (angleDist < 6.0f) {
                float score = angleDist * 2.5f + std::fabs(edz) * 0.08f;
                if (score < bestScore) {
                    bestScore = score;
                    bestJetIdx = static_cast<int>(idx);
                    foundJet = true;
                }
            }
        }

        bool nowLocked = false;
        if (foundJet) {
            missileTargetMode_ = 1;
            missileTargetIdx_ = bestJetIdx;
            nowLocked = true;
        } else {
            float relTargetX = targetX_ - planeX_;
            float relTargetY = (targetY_ + 1.2f) - (planeY_ - 0.5f);
            float relTargetZ = targetZ_ - (-6.5f);
            if (relTargetZ < -10.0f && relTargetZ > -120.0f && std::fabs(relTargetX) < 8.5f && std::fabs(relTargetY) < 6.0f) {
                missileTargetMode_ = 0;
                missileTargetIdx_ = -1;
                nowLocked = true;
            } else {
                missileTargetMode_ = -1;
                missileTargetIdx_ = -1;
                nowLocked = false;
            }
        }

        if (nowLocked && !prevTargetLocked_ && sndLock_) {
            w3dEngine::W3dSoundPlay(sndLock_, 0.65f, false);
        }
        prevTargetLocked_ = nowLocked;
        targetLocked_ = nowLocked;

        // Disparo continuo automático de cañón
        if (firePressed_) {
            cannonCooldown_ -= dt;
            if (cannonCooldown_ <= 0.0f) {
                cannonCooldown_ = 0.082f;
                muzzleFlashTime_ = 0.08f;
                if (sndCannon_) {
                    w3dEngine::W3dSoundPlayPitch(sndCannon_, 0.55f, false, 0.94f + (rand() % 12) * 0.01f);
                }
                Bullet b1;
                b1.x = planeX_ - 0.9f;
                b1.y = planeY_ - 0.28f;
                b1.z = -7.2f;
                b1.vx = (planeRoll_ * 0.05f);
                b1.vy = (planePitch_ * 0.05f);
                b1.vz = -390.0f;
                b1.life = 0.65f;
                bullets_.push_back(b1);

                Bullet b2 = b1;
                b2.x = planeX_ + 0.9f;
                bullets_.push_back(b2);
            }
        } else {
            cannonCooldown_ = 0.0f;
        }

        if (muzzleFlashTime_ > 0.0f) muzzleFlashTime_ -= dt;
        if (targetHitFlashTime_ > 0.0f) targetHitFlashTime_ -= dt;

        // Balas del jugador e impactos con detección continua (Swept-Z)
        for (auto it = bullets_.begin(); it != bullets_.end(); ) {
            float prevZ = it->z;
            it->x += it->vx * dt;
            it->y += it->vy * dt;
            it->z += it->vz * dt;
            it->life -= dt;

            bool hitSomething = false;

            // Impacto con Cazas Enemigos (Swept-Z)
            for (auto &ej : enemyJets_) {
                if (!ej.active) continue;
                if (it->z <= ej.z + 2.5f && prevZ >= ej.z - 2.5f) {
                    float dx = it->x - ej.x;
                    float dy = it->y - ej.y;
                    if (dx * dx + dy * dy < 7.5f) {
                        ej.health -= 12.0f;
                        ej.hitFlashTime = 0.16f;
                        hitSomething = true;
                        spawnExplosion(it->x, it->y, it->z, 1.4f, 1.0f, 0.85f, 0.3f);
                        if (ej.health <= 0.0f) {
                            ej.active = false;
                            enemiesDestroyed_++;
                            spawnExplosion(ej.x, ej.y, ej.z, 5.0f, 1.0f, 0.55f, 0.15f);
                            if (sndExplosion_) w3dEngine::W3dSoundPlay(sndExplosion_, 0.95f, false);
                        }
                        break;
                    }
                }
            }

            // Impacto con Buque (Swept-Z)
            if (!hitSomething) {
                if (it->z <= targetZ_ + 11.0f && prevZ >= targetZ_ - 11.0f) {
                    if (std::fabs(it->x - targetX_) < 4.8f && it->y >= targetY_ - 1.5f && it->y <= targetY_ + 5.5f) {
                        targetHealth_ -= 5.0f;
                        targetHitFlashTime_ = 0.15f;
                        hitSomething = true;
                        spawnExplosion(it->x, it->y, it->z, 1.8f, 1.0f, 0.7f, 0.2f);
                        if (targetHealth_ <= 0.0f) {
                            enemiesDestroyed_ += 3;
                            spawnExplosion(targetX_, targetY_ + 1.5f, targetZ_, 7.5f, 1.0f, 0.45f, 0.1f);
                            targetHealth_ = targetMaxHealth_;
                            targetZ_ = -150.0f;
                            targetX_ = (rand() % 28 - 14) * 1.0f;
                            if (sndExplosion_) w3dEngine::W3dSoundPlay(sndExplosion_, 1.0f, false);
                        }
                    }
                }
            }

            if (hitSomething || it->life <= 0.0f || it->z < -260.0f) {
                it = bullets_.erase(it);
            } else {
                ++it;
            }
        }

        // Misiles guiados activos en 3D
        for (auto it = activeMissiles_.begin(); it != activeMissiles_.end(); ) {
            it->life -= dt;
            it->z += it->vz * dt;
            it->x += it->vx * dt;
            it->y += it->vy * dt;

            float tx = targetX_, ty = targetY_ + 1.5f, tz = targetZ_;
            if (it->targetMode == 1 && it->targetIdx >= 0 && it->targetIdx < (int)enemyJets_.size() && enemyJets_[it->targetIdx].active) {
                tx = enemyJets_[it->targetIdx].x;
                ty = enemyJets_[it->targetIdx].y;
                tz = enemyJets_[it->targetIdx].z;
            }

            float ddx = tx - it->x;
            float ddy = ty - it->y;
            float ddz = tz - it->z;
            float dist = std::sqrt(ddx * ddx + ddy * ddy + ddz * ddz);

            if (dist > 1.0f) {
                float targetVx = (ddx / dist) * 75.0f;
                float targetVy = (ddy / dist) * 75.0f;
                float targetVz = (ddz / dist) * 75.0f;
                it->vx += (targetVx - it->vx) * 0.18f;
                it->vy += (targetVy - it->vy) * 0.18f;
                it->vz += (targetVz - it->vz) * 0.22f;
            }

            it->pitch = std::atan2(-it->vy, -it->vz) * 57.2957f;
            it->yaw = std::atan2(it->vx, -it->vz) * 57.2957f;

            // Estela de humo
            SmokeParticle sp;
            sp.x = it->x + (rand() % 5 - 2) * 0.05f;
            sp.y = it->y + (rand() % 5 - 2) * 0.05f;
            sp.z = it->z + 0.6f;
            sp.vx = (rand() % 5 - 2) * 0.3f;
            sp.vy = (rand() % 5 - 2) * 0.3f;
            sp.vz = 4.0f;
            sp.size = 0.5f;
            sp.life = 0.7f;
            sp.maxLife = 0.7f;
            sp.r = 0.95f; sp.g = 0.95f; sp.b = 0.95f; sp.a = 0.85f;
            sp.angle = (rand() % 628) * 0.01f;
            missileSmoke_.push_back(sp);

            bool detonated = false;
            if (dist < 4.5f || it->life <= 0.0f) {
                detonated = true;
                if (it->targetMode == 1 && it->targetIdx >= 0 && it->targetIdx < (int)enemyJets_.size() && enemyJets_[it->targetIdx].active) {
                    enemyJets_[it->targetIdx].health -= 35.0f;
                    if (enemyJets_[it->targetIdx].health <= 0.0f) {
                        enemyJets_[it->targetIdx].active = false;
                        enemiesDestroyed_ += 2;
                        spawnExplosion(tx, ty, tz, 6.0f, 1.0f, 0.5f, 0.15f);
                    } else {
                        spawnExplosion(tx, ty, tz, 4.0f, 1.0f, 0.6f, 0.2f);
                    }
                } else {
                    targetHealth_ -= 35.0f;
                    targetHitFlashTime_ = 0.45f;
                    if (targetHealth_ <= 0.0f) {
                        enemiesDestroyed_ += 3;
                        spawnExplosion(targetX_, targetY_ + 1.5f, targetZ_, 7.5f, 1.0f, 0.45f, 0.1f);
                        targetHealth_ = targetMaxHealth_;
                        targetZ_ = -150.0f;
                        targetX_ = (rand() % 28 - 14) * 1.0f;
                    } else {
                        spawnExplosion(tx, ty, tz, 6.5f, 1.0f, 0.5f, 0.15f);
                    }
                }
                if (sndExplosion_) w3dEngine::W3dSoundPlay(sndExplosion_, 1.0f, false);
            }

            if (detonated) {
                it = activeMissiles_.erase(it);
            } else {
                ++it;
            }
        }

        // Bombas activas
        for (auto it = bombs_.begin(); it != bombs_.end(); ) {
            it->life -= dt;
            it->vy -= 26.0f * dt;
            it->vz -= 8.0f * dt;
            it->x += it->vx * dt;
            it->y += it->vy * dt;
            it->z += it->vz * dt;
            it->pitch = std::atan2(-it->vy, -it->vz) * 57.2957f;

            SmokeParticle sp;
            sp.x = it->x;
            sp.y = it->y + 0.3f;
            sp.z = it->z + 0.4f;
            sp.vx = 0.0f; sp.vy = 1.0f; sp.vz = 2.0f;
            sp.size = 0.45f;
            sp.life = 0.45f;
            sp.maxLife = 0.45f;
            sp.r = 0.85f; sp.g = 0.85f; sp.b = 0.85f; sp.a = 0.6f;
            sp.angle = (rand() % 628) * 0.01f;
            missileSmoke_.push_back(sp);

            bool detonated = false;

            if (std::hypot(it->x - targetX_, it->z - targetZ_) < 8.5f && it->y <= targetY_ + 4.5f) {
                detonated = true;
                targetHealth_ -= 60.0f;
                targetHitFlashTime_ = 0.5f;
                spawnExplosion(it->x, it->y, it->z, 9.5f, 1.0f, 0.45f, 0.1f);
                if (targetHealth_ <= 0.0f) {
                    enemiesDestroyed_ += 3;
                    spawnExplosion(targetX_, targetY_ + 1.5f, targetZ_, 8.5f, 1.0f, 0.45f, 0.1f);
                    targetHealth_ = targetMaxHealth_;
                    targetZ_ = -150.0f;
                    targetX_ = (rand() % 28 - 14) * 1.0f;
                }
                if (sndExplosion_) w3dEngine::W3dSoundPlay(sndExplosion_, 1.0f, false);
            } else if (it->y <= -2.5f) {
                detonated = true;
                spawnExplosion(it->x, -2.0f, it->z, 8.5f, 0.9f, 0.6f, 0.2f);
                for (auto &ej : enemyJets_) {
                    if (!ej.active) continue;
                    float dx = ej.x - it->x;
                    float dz = ej.z - it->z;
                    if (dx*dx + dz*dz < 100.0f) {
                        ej.health -= 50.0f;
                        ej.hitFlashTime = 0.3f;
                        if (ej.health <= 0.0f) {
                            ej.active = false;
                            enemiesDestroyed_++;
                            spawnExplosion(ej.x, ej.y, ej.z, 5.0f, 1.0f, 0.6f, 0.2f);
                        }
                    }
                }
                if (sndExplosion_) w3dEngine::W3dSoundPlay(sndExplosion_, 1.0f, false);
            }

            if (detonated || it->life <= 0.0f || it->z < -200.0f) {
                it = bombs_.erase(it);
            } else {
                ++it;
            }
        }

        // Actualizar partículas de humo
        for (auto it = missileSmoke_.begin(); it != missileSmoke_.end(); ) {
            it->life -= dt;
            it->x += it->vx * dt;
            it->y += it->vy * dt;
            it->z += it->vz * dt;
            it->size += dt * 1.1f;
            it->a = (it->life / it->maxLife) * 0.75f;
            if (it->life <= 0.0f) {
                it = missileSmoke_.erase(it);
            } else {
                ++it;
            }
        }

        // Fuego antiaéreo del buque (Flak de alta velocidad)
        flakCooldown_ -= dt;
        if (flakCooldown_ <= 0.0f) {
            flakCooldown_ = 1.8f + (rand() % 10) * 0.1f;
            if (targetZ_ < -20.0f && targetZ_ > -140.0f) {
                EnemyBullet eb;
                eb.x = targetX_ + (rand() % 4 - 2) * 0.6f;
                eb.y = targetY_ + 2.8f;
                eb.z = targetZ_ + 4.0f;
                float distZ = (-6.5f) - eb.z;
                float travelTime = (distZ > 5.0f) ? (distZ / 145.0f) : 0.2f;
                eb.vx = (planeX_ - eb.x) / travelTime;
                eb.vy = ((planeY_ - 0.2f) - eb.y) / travelTime;
                eb.vz = 145.0f;
                eb.life = travelTime + 0.35f;
                enemyBullets_.push_back(eb);
            }
        }

        // Actualizar proyectiles antiaéreos enemigos con detección continua Swept-Z
        for (auto it = enemyBullets_.begin(); it != enemyBullets_.end(); ) {
            float prevZ = it->z;
            it->x += it->vx * dt;
            it->y += it->vy * dt;
            it->z += it->vz * dt;
            it->life -= dt;

            // Detección de impacto continua contra el avión del jugador
            bool hit = false;
            if (prevZ <= -5.8f && it->z >= -7.2f) {
                float dx = it->x - planeX_;
                float dy = it->y - (planeY_ - 0.4f);
                if (dx * dx + dy * dy < 2.4f) {
                    hit = true;
                }
            } else {
                float dx = it->x - planeX_;
                float dy = it->y - (planeY_ - 0.4f);
                float dz = it->z - (-6.5f);
                if (dx*dx + dy*dy + dz*dz < 2.2f) {
                    hit = true;
                }
            }

            if (hit) {
                healthPct_ -= 0.12f;
                spawnExplosion(it->x, it->y, it->z, 1.8f, 1.0f, 0.45f, 0.15f);
                it = enemyBullets_.erase(it);
                if (healthPct_ <= 0.0f) {
                    healthPct_ = 0.0f;
                    gameState_ = STATE_GAME_OVER;
                    if (enemiesDestroyed_ > highScore_) highScore_ = enemiesDestroyed_;
                    spawnExplosion(planeX_, planeY_, -6.5f, 5.5f);
                    if (sndExplosion_) w3dEngine::W3dSoundPlay(sndExplosion_, 1.0f, false);
                    break;
                }
            } else if (it->life <= 0.0f || it->z > 15.0f) {
                it = enemyBullets_.erase(it);
            } else {
                ++it;
            }
        }

        // Actualizar animaciones de explosiones
        for (auto it = explosions_.begin(); it != explosions_.end(); ) {
            it->life -= dt;
            it->radius += (it->maxRadius - it->radius) * 0.16f;
            if (it->life <= 0.0f) {
                it = explosions_.erase(it);
            } else {
                ++it;
            }
        }

        // Actualizar chispas de escombros de explosiones
        for (auto it = explosionDebris_.begin(); it != explosionDebris_.end(); ) {
            it->x += it->vx * dt;
            it->y += it->vy * dt;
            it->z += it->vz * dt;
            it->vy -= 22.0f * dt;
            it->vx *= 0.97f;
            it->vz *= 0.97f;
            it->life -= dt;
            if (it->life <= 0.0f || it->y < -3.8f) {
                it = explosionDebris_.erase(it);
            } else {
                ++it;
            }
        }

        W3dFisicaPaso(1.0f / 60.0f);
    }

    // ------------------------------------------------------------------------
    // RENDERIZADO 3D (ADAPTADO A MODO RETRATO CON SEGUIMIENTO VERTICAL AMPLIO)
    // ------------------------------------------------------------------------
    w3dEngine::Viewport(0, 0, width_, height_);

    float aspect = (height_ > 0) ? static_cast<float>(width_) / static_cast<float>(height_) : 1.0f;
    w3dEngine::MatrixMode(w3dEngine::Projection);
    w3dEngine::LoadIdentity();

    // FOV optimizado para vista vertical amplia en modo retrato
    float fovY = (aspect < 1.0f) ? 66.0f : 55.0f;
    w3dEngine::Perspective(fovY, aspect, 0.1f, 600.0f);

    float camLagX = planeX_ * 0.38f;
    float camLagY = planeY_ * 0.40f;
    float camBank = planeRoll_ * 0.14f;

    w3dEngine::MatrixMode(w3dEngine::ModelView);
    w3dEngine::LoadIdentity();
    w3dEngine::Rotatef(camBank, 0.0f, 0.0f, 1.0f);
    w3dEngine::Translatef(-camLagX, -1.2f - camLagY, -2.5f);

    w3dEngine::Enable(w3dEngine::DepthTest);
    w3dEngine::DepthFunc(w3dEngine::DepthLEqual);
    w3dEngine::Enable(w3dEngine::CullFace);

    renderSkyAndOcean();
    renderSun();
    renderClouds();
    renderIslandsAndTrees();
    renderTarget();
    renderEnemyJets();
    renderAircraft();
    renderProjectiles();
    renderExplosions();

    // ------------------------------------------------------------------------
    // RENDERIZADO DE INTERFAZ 2D
    // ------------------------------------------------------------------------
    w3dEngine::Disable(w3dEngine::CullFace);
    w3dEngine::Disable(w3dEngine::DepthTest);
    w3dEngine::Enable(w3dEngine::Blend);
    w3dEngine::SetMezcla(w3dEngine::MezclaAlpha);

    if (gameState_ == STATE_MAIN_MENU) {
        renderMainMenuUI();
    } else if (gameState_ == STATE_PLAYING) {
        renderGameUI();
    } else if (gameState_ == STATE_PAUSED) {
        renderPauseUI();
    } else if (gameState_ == STATE_GAME_OVER) {
        renderGameOverUI();
    }
}

void Renderer::render() {
    updateRenderArea();

    w3dEngine::ClearColor(0.07f, 0.27f, 0.62f, 1.0f);
    w3dEngine::Clear(w3dEngine::ColorBuffer | w3dEngine::DepthBuffer);

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
                    return red == 8 && green == 8 && blue == 8 && depth == 24;
                }
                return false;
            });

    config_ = config;

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
