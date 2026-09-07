#include "Renderer.h"

#include <game-activity/native_app_glue/android_native_app_glue.h>
#include <GLES3/gl3.h>
#include <memory>
#include <vector>
#include <cmath>
#include <algorithm>

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
        width_(0),
        height_(0),
        shaderNeedsNewProjectionMatrix_(true),
        timeSec_(0.0f),
        planePitch_(0.0f),
        planeRoll_(0.0f),
        planeYaw_(0.0f),
        planeX_(0.0f),
        planeY_(0.0f),
        planeZ_(0.0f),
        speedKnots_(480.0f),
        altitudeFeet_(2400.0f),
        healthPct_(1.0f),
        missileCount_(4),
        enemiesDestroyed_(1),
        targetX_(0.0f),
        targetY_(-1.8f),
        targetZ_(-80.0f),
        targetLocked_(true),
        touchDown_(false),
        touchX_(0.0f),
        touchY_(0.0f),
        stickActive_(false),
        stickPointerId_(-1),
        stickDeflectX_(0.0f),
        stickDeflectY_(0.0f),
        firePressed_(false),
        missilePressed_(false),
        muzzleFlashTime_(0.0f),
        missileFlightTime_(0.0f),
        soundEnabled_(true),
        engineVoiceId_(0),
        sndEngine_(nullptr),
        sndCannon_(nullptr),
        sndMissile_(nullptr),
        sndExplosion_(nullptr),
        sndLock_(nullptr) {
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

void Renderer::initWhisk3D() {
    aout << "Whisk3D: Inicializando motor y backend grafico GLES2/3..." << std::endl;

    // Configurar sistema de archivos de Whisk3D para leer desde los assets del APK
    w3dFileSystem::SetAssetManager(app_->activity->assetManager);
    if (app_->activity->internalDataPath) {
        w3dFileSystem::SetUserDataDir(app_->activity->internalDataPath);
    }

    // Inicializar backend GLES2 de Whisk3D
    w3dEngine::GLES2Init(nullptr);

    // Inicializar motor de audio nativo de Whisk3D
    if (w3dEngine::W3dAudioInit(44100)) {
        aout << "Whisk3D: Motor de audio OpenSL ES inicializado (44.1 kHz stereo)!" << std::endl;
        sndEngine_    = w3dEngine::W3dSoundLoad("sounds/engine.wav");
        sndCannon_    = w3dEngine::W3dSoundLoad("sounds/cannon.wav");
        sndMissile_   = w3dEngine::W3dSoundLoad("sounds/missile.wav");
        sndExplosion_ = w3dEngine::W3dSoundLoad("sounds/explosion.wav");
        sndLock_      = w3dEngine::W3dSoundLoad("sounds/lock.wav");

        // Arrancar loop ambiental del motor a reacción
        if (sndEngine_) {
            engineVoiceId_ = w3dEngine::W3dSoundPlay(sndEngine_, 0.42f, true);
        }
    } else {
        aout << "Whisk3D: No se pudo abrir backend de audio" << std::endl;
    }

    // Cargar texturas del juego
    loadGameTextures();

    aout << "Whisk3D: Escena y recursos listos!" << std::endl;
}

void Renderer::loadGameTextures() {
    aout << "Whisk3D: Cargando texturas de Sky Strike..." << std::endl;
    auto assetMgr = app_->activity->assetManager;
    if (!assetMgr) return;

    texAirplane_     = TextureAsset::loadAsset(assetMgr, "textures/airplane.png");
    texSea_          = TextureAsset::loadAsset(assetMgr, "textures/sea.png");
    texTerrain_      = TextureAsset::loadAsset(assetMgr, "textures/terrain.png");
    texTarget_       = TextureAsset::loadAsset(assetMgr, "textures/target.png");
    texHudCrosshair_ = TextureAsset::loadAsset(assetMgr, "textures/hud_crosshair.png");
    texHudRadar_     = TextureAsset::loadAsset(assetMgr, "textures/hud_radar.png");
    texBtnFire_      = TextureAsset::loadAsset(assetMgr, "textures/btn_fire.png");
    texBtnMissile_   = TextureAsset::loadAsset(assetMgr, "textures/btn_missile.png");
    texBtnStick_     = TextureAsset::loadAsset(assetMgr, "textures/btn_stick.png");
    texBtnSound_     = TextureAsset::loadAsset(assetMgr, "textures/btn_sound.png");
}

void Renderer::handleInput() {
    auto *inputBuffer = android_app_swap_input_buffers(app_);
    if (!inputBuffer) return;

    float stickCenterX = 110.0f;
    float stickCenterY = (height_ > 0) ? (height_ - 120.0f) : 400.0f;
    float stickRadius = 85.0f;

    float fireX0 = width_ - 150.0f, fireY0 = height_ - 150.0f;
    float fireX1 = width_ - 10.0f,  fireY1 = height_ - 10.0f;

    float mslX0 = width_ - 150.0f, mslY0 = height_ - 280.0f;
    float mslX1 = width_ - 10.0f,  mslY1 = height_ - 150.0f;

    float sndBtnX = width_ - 70.0f, sndBtnY = 20.0f;
    float sndBtnSize = 55.0f;

    for (auto i = 0; i < inputBuffer->motionEventsCount; i++) {
        auto &motionEvent = inputBuffer->motionEvents[i];
        auto actionMasked = motionEvent.action & AMOTION_EVENT_ACTION_MASK;
        auto pointerIndex = (motionEvent.action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK)
                >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;

        if (actionMasked == AMOTION_EVENT_ACTION_DOWN || actionMasked == AMOTION_EVENT_ACTION_POINTER_DOWN) {
            touchDown_ = true;
            if (pointerIndex < motionEvent.pointerCount) {
                auto &pointer = motionEvent.pointers[pointerIndex];
                float px = GameActivityPointerAxes_getX(&pointer);
                float py = GameActivityPointerAxes_getY(&pointer);
                int pId = pointer.id;

                if (px < width_ * 0.5f) {
                    stickActive_ = true;
                    stickPointerId_ = pId;
                    float dx = px - stickCenterX;
                    float dy = py - stickCenterY;
                    stickDeflectX_ = CLAMP(dx / stickRadius, -1.0f, 1.0f);
                    stickDeflectY_ = CLAMP(dy / stickRadius, -1.0f, 1.0f);
                    planeRoll_  = stickDeflectX_ * 42.0f;
                    planePitch_ = -stickDeflectY_ * 28.0f;
                } else {
                    // Botón de Sonido Mute/Unmute
                    if (px >= sndBtnX && px <= sndBtnX + sndBtnSize && py >= sndBtnY && py <= sndBtnY + sndBtnSize) {
                        soundEnabled_ = !soundEnabled_;
                        w3dEngine::W3dAudioMasterVolume(soundEnabled_ ? 1.0f : 0.0f);
                        if (soundEnabled_ && sndLock_) {
                            w3dEngine::W3dSoundPlay(sndLock_, 0.55f, false);
                        }
                    } else if (px >= fireX0 && px <= fireX1 && py >= fireY0 && py <= fireY1) {
                        firePressed_ = true;
                        muzzleFlashTime_ = 0.2f;
                        if (sndCannon_) {
                            w3dEngine::W3dSoundPlayPitch(sndCannon_, 0.55f, false, 0.96f + (rand() % 8) * 0.01f);
                        }
                    } else if (px >= mslX0 && px <= mslX1 && py >= mslY0 && py <= mslY1) {
                        missilePressed_ = true;
                        if (missileCount_ > 0 && missileFlightTime_ <= 0.0f) {
                            missileCount_--;
                            missileFlightTime_ = 1.6f;
                            if (sndMissile_) {
                                w3dEngine::W3dSoundPlay(sndMissile_, 0.85f, false);
                            }
                        }
                    }
                }
            }
        } else if (actionMasked == AMOTION_EVENT_ACTION_MOVE) {
            bool anyFire = false;
            for (uint32_t p = 0; p < motionEvent.pointerCount; ++p) {
                auto &pointer = motionEvent.pointers[p];
                float px = GameActivityPointerAxes_getX(&pointer);
                float py = GameActivityPointerAxes_getY(&pointer);
                int pId = pointer.id;

                if (pId == stickPointerId_ || (px < width_ * 0.5f && !stickActive_)) {
                    stickActive_ = true;
                    stickPointerId_ = pId;
                    float dx = px - stickCenterX;
                    float dy = py - stickCenterY;
                    stickDeflectX_ = CLAMP(dx / stickRadius, -1.0f, 1.0f);
                    stickDeflectY_ = CLAMP(dy / stickRadius, -1.0f, 1.0f);
                    planeRoll_  = stickDeflectX_ * 42.0f;
                    planePitch_ = -stickDeflectY_ * 28.0f;
                } else if (px >= width_ * 0.5f) {
                    if (px >= fireX0 && px <= fireX1 && py >= fireY0 && py <= fireY1) {
                        anyFire = true;
                    }
                }
            }
            firePressed_ = anyFire;
        } else if (actionMasked == AMOTION_EVENT_ACTION_UP || actionMasked == AMOTION_EVENT_ACTION_CANCEL) {
            stickActive_ = false;
            stickPointerId_ = -1;
            stickDeflectX_ = 0.0f;
            stickDeflectY_ = 0.0f;
            touchDown_ = false;
            firePressed_ = false;
            missilePressed_ = false;
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
                float px = GameActivityPointerAxes_getX(&pointer);
                float py = GameActivityPointerAxes_getY(&pointer);
                if (px >= fireX0 && px <= fireX1 && py >= fireY0 && py <= fireY1) {
                    firePressed_ = false;
                }
                if (px >= mslX0 && px <= mslX1 && py >= mslY0 && py <= mslY1) {
                    missilePressed_ = false;
                }
            }
        }
    }
    android_app_clear_motion_events(inputBuffer);
    android_app_clear_key_events(inputBuffer);
}

void Renderer::renderSkyAndOcean() {
    // 1. Cielo atmosférico (Quad de horizonte lejano)
    static const float skyVerts[] = {
        -250.0f,  60.0f, -220.0f,
         250.0f,  60.0f, -220.0f,
         250.0f,  -5.0f, -220.0f,

        -250.0f,  60.0f, -220.0f,
         250.0f,  -5.0f, -220.0f,
        -250.0f,  -5.0f, -220.0f
    };
    static const unsigned char skyColors[] = {
        50, 140, 240, 255,
        50, 140, 240, 255,
        180, 220, 255, 255,

        50, 140, 240, 255,
        180, 220, 255, 255,
        180, 220, 255, 255
    };

    w3dEngine::Disable(w3dEngine::Texture2D);
    w3dEngine::Enable(w3dEngine::ColorMaterial);
    w3dEngine::EnableArray(w3dEngine::VertexArray);
    w3dEngine::EnableArray(w3dEngine::ColorArray);
    w3dEngine::VertexPointer3f(0, skyVerts);
    w3dEngine::ColorPointer4ub(skyColors);
    w3dEngine::DrawTrianglesArray(6);
    w3dEngine::DisableArray(w3dEngine::ColorArray);

    // 2. Océano / Mar 3D con textura animada
    if (texSea_) {
        float waveShift = timeSec_ * 0.12f;
        static const float oceanVerts[] = {
            -180.0f, -2.5f, -220.0f,
             180.0f, -2.5f, -220.0f,
             180.0f, -2.5f,   30.0f,

            -180.0f, -2.5f, -220.0f,
             180.0f, -2.5f,   30.0f,
            -180.0f, -2.5f,   30.0f
        };

        const float oceanUVs[] = {
            0.0f,  20.0f + waveShift,
            15.0f, 20.0f + waveShift,
            15.0f, 0.0f  + waveShift,

            0.0f,  20.0f + waveShift,
            15.0f, 0.0f  + waveShift,
            0.0f,  0.0f  + waveShift
        };

        w3dEngine::Enable(w3dEngine::Texture2D);
        w3dEngine::BindTexture(texSea_->getTextureID());
        w3dEngine::Color4f(1.0f, 1.0f, 1.0f, 1.0f);
        w3dEngine::EnableArray(w3dEngine::VertexArray);
        w3dEngine::EnableArray(w3dEngine::TexCoordArray);
        w3dEngine::VertexPointer3f(0, oceanVerts);
        w3dEngine::TexCoordPointer2f(0, oceanUVs);
        w3dEngine::DrawTrianglesArray(6);
        w3dEngine::DisableArray(w3dEngine::TexCoordArray);
    }
}

void Renderer::renderIslands() {
    if (!texTerrain_) return;

    w3dEngine::Enable(w3dEngine::Texture2D);
    w3dEngine::BindTexture(texTerrain_->getTextureID());
    w3dEngine::EnableArray(w3dEngine::VertexArray);
    w3dEngine::EnableArray(w3dEngine::TexCoordArray);

    // Isla Principal (Costa izquierda)
    w3dEngine::PushMatrix();
    w3dEngine::Translatef(-38.0f, -2.5f, -90.0f);

    static const float islandVerts[] = {
        // Cima / Piramide montañosa
        0.0f,  9.0f,  0.0f,   -18.0f, 0.0f,  15.0f,    18.0f, 0.0f,  15.0f,
        0.0f,  9.0f,  0.0f,    18.0f, 0.0f,  15.0f,    22.0f, 0.0f, -16.0f,
        0.0f,  9.0f,  0.0f,    22.0f, 0.0f, -16.0f,   -20.0f, 0.0f, -18.0f,
        0.0f,  9.0f,  0.0f,   -20.0f, 0.0f, -18.0f,   -18.0f, 0.0f,  15.0f
    };
    static const float islandUVs[] = {
        0.5f, 0.1f,   0.0f, 1.0f,   1.0f, 1.0f,
        0.5f, 0.1f,   0.0f, 1.0f,   1.0f, 1.0f,
        0.5f, 0.1f,   0.0f, 1.0f,   1.0f, 1.0f,
        0.5f, 0.1f,   0.0f, 1.0f,   1.0f, 1.0f
    };

    w3dEngine::VertexPointer3f(0, islandVerts);
    w3dEngine::TexCoordPointer2f(0, islandUVs);
    w3dEngine::DrawTrianglesArray(12);
    w3dEngine::PopMatrix();

    // Atolón Derecho
    w3dEngine::PushMatrix();
    w3dEngine::Translatef(42.0f, -2.5f, -130.0f);
    static const float atollVerts[] = {
        0.0f,  5.5f,  0.0f,   -14.0f, 0.0f,  12.0f,    14.0f, 0.0f,  12.0f,
        0.0f,  5.5f,  0.0f,    14.0f, 0.0f,  12.0f,    16.0f, 0.0f, -12.0f,
        0.0f,  5.5f,  0.0f,    16.0f, 0.0f, -12.0f,   -15.0f, 0.0f, -14.0f,
        0.0f,  5.5f,  0.0f,   -15.0f, 0.0f, -14.0f,   -14.0f, 0.0f,  12.0f
    };
    w3dEngine::VertexPointer3f(0, atollVerts);
    w3dEngine::TexCoordPointer2f(0, islandUVs);
    w3dEngine::DrawTrianglesArray(12);
    w3dEngine::PopMatrix();

    w3dEngine::DisableArray(w3dEngine::TexCoordArray);
}

void Renderer::renderTarget() {
    if (!texTarget_) return;

    w3dEngine::PushMatrix();
    w3dEngine::Translatef(targetX_, targetY_, targetZ_);

    w3dEngine::Enable(w3dEngine::Texture2D);
    w3dEngine::BindTexture(texTarget_->getTextureID());
    w3dEngine::EnableArray(w3dEngine::VertexArray);
    w3dEngine::EnableArray(w3dEngine::TexCoordArray);

    // Barco de guerra / Buque enemigo
    static const float shipVerts[] = {
        // Proa (Bow)
         0.0f,  1.2f, -10.0f,   -3.0f,  1.2f,  -4.0f,    3.0f,  1.2f,  -4.0f,
        // Casco medio
        -3.0f,  1.2f,  -4.0f,   -3.0f,  1.2f,   8.0f,    3.0f,  1.2f,   8.0f,
        -3.0f,  1.2f,  -4.0f,    3.0f,  1.2f,   8.0f,    3.0f,  1.2f,  -4.0f,
        // Torre de mando
        -1.5f,  3.5f,  -1.0f,    1.5f,  3.5f,  -1.0f,    1.5f,  3.5f,   3.0f,
        -1.5f,  3.5f,  -1.0f,    1.5f,  3.5f,   3.0f,   -1.5f,  3.5f,   3.0f
    };
    static const float shipUVs[] = {
        0.5f, 0.0f,   0.0f, 0.4f,   1.0f, 0.4f,
        0.0f, 0.4f,   0.0f, 1.0f,   1.0f, 1.0f,
        0.0f, 0.4f,   1.0f, 1.0f,   1.0f, 0.4f,
        0.2f, 0.3f,   0.8f, 0.3f,   0.8f, 0.7f,
        0.2f, 0.3f,   0.8f, 0.7f,   0.2f, 0.7f
    };

    w3dEngine::VertexPointer3f(0, shipVerts);
    w3dEngine::TexCoordPointer2f(0, shipUVs);
    w3dEngine::DrawTrianglesArray(15);
    w3dEngine::DisableArray(w3dEngine::TexCoordArray);

    w3dEngine::PopMatrix();
}

void Renderer::renderAircraft() {
    if (!texAirplane_) return;

    w3dEngine::PushMatrix();
    // Posición del avión frente a la cámara de persecución
    w3dEngine::Translatef(planeX_, planeY_ - 0.5f, -6.5f);

    // Rotaciones de vuelo: Balanceo (Roll), Cabeceo (Pitch), Guiñada (Yaw)
    w3dEngine::Rotatef(planeRoll_,  0.0f, 0.0f, 1.0f);
    w3dEngine::Rotatef(planePitch_, 1.0f, 0.0f, 0.0f);
    w3dEngine::Rotatef(planeYaw_,   0.0f, 1.0f, 0.0f);

    w3dEngine::Enable(w3dEngine::Texture2D);
    w3dEngine::BindTexture(texAirplane_->getTextureID());
    w3dEngine::EnableArray(w3dEngine::VertexArray);
    w3dEngine::EnableArray(w3dEngine::TexCoordArray);

    // Geometría del Caza 3D (Fuselaje, Alas delta, Estabilizadores dobles, Cabina)
    static const float jetVerts[] = {
        // --- 1. Fuselaje Frontal & Nariz ---
         0.0f,  0.0f,  3.2f,   -0.5f,  0.2f,  0.8f,    0.5f,  0.2f,  0.8f, // Dorso nariz
         0.0f,  0.0f,  3.2f,    0.5f, -0.2f,  0.8f,   -0.5f, -0.2f,  0.8f, // Vientre nariz
         0.0f,  0.0f,  3.2f,   -0.5f, -0.2f,  0.8f,   -0.5f,  0.2f,  0.8f, // Costado izq
         0.0f,  0.0f,  3.2f,    0.5f,  0.2f,  0.8f,    0.5f, -0.2f,  0.8f, // Costado der

        // --- 2. Cabina de Cristal (Cockpit Canopy) ---
         0.0f,  0.6f,  0.2f,   -0.35f, 0.2f,  1.0f,    0.35f, 0.2f,  1.0f, // Parabrisas frontal
         0.0f,  0.6f,  0.2f,    0.35f, 0.2f,  1.0f,    0.35f, 0.2f, -0.6f, // Lado derecho
         0.0f,  0.6f,  0.2f,    0.35f, 0.2f, -0.6f,   -0.35f, 0.2f, -0.6f, // Posterior
         0.0f,  0.6f,  0.2f,   -0.35f, 0.2f, -0.6f,   -0.35f, 0.2f,  1.0f, // Lado izquierdo

        // --- 3. Fuselaje Central & Motores ---
        -0.6f,  0.1f,  0.8f,   -0.6f,  0.1f, -2.2f,    0.6f,  0.1f, -2.2f,
        -0.6f,  0.1f,  0.8f,    0.6f,  0.1f, -2.2f,    0.6f,  0.1f,  0.8f,

        // --- 4. Alas Delta (Superior) ---
         0.0f,  0.05f, 0.8f,   -3.4f,  0.05f, -1.2f,  -0.6f,  0.05f, -2.0f, // Ala izquierda
         0.0f,  0.05f, 0.8f,    0.6f,  0.05f, -2.0f,   3.4f,  0.05f, -1.2f, // Ala derecha

        // --- 5. Estabilizadores Verticales Dobles (Twin Tails) ---
        -0.45f, 0.1f, -1.5f,   -0.65f, 1.2f, -2.4f,   -0.45f, 0.1f, -2.4f,  // Cola izq
         0.45f, 0.1f, -1.5f,    0.45f, 0.1f, -2.4f,    0.65f, 1.2f, -2.4f   // Cola der
    };

    static const float jetUVs[] = {
        // Nariz
        0.2f, 0.2f,  0.1f, 0.5f,  0.3f, 0.5f,
        0.2f, 0.2f,  0.3f, 0.5f,  0.1f, 0.5f,
        0.2f, 0.2f,  0.1f, 0.5f,  0.1f, 0.4f,
        0.2f, 0.2f,  0.3f, 0.4f,  0.3f, 0.5f,

        // Cabina (Mapeada a la esquina superior izquierda brillante de airplane.png)
        0.22f, 0.1f,  0.05f, 0.4f,  0.4f, 0.4f,
        0.22f, 0.1f,  0.4f,  0.4f,  0.4f, 0.2f,
        0.22f, 0.1f,  0.4f,  0.2f,  0.05f, 0.2f,
        0.22f, 0.1f,  0.05f, 0.2f,  0.05f, 0.4f,

        // Fuselaje central
        0.1f, 0.5f,  0.1f, 0.9f,  0.4f, 0.9f,
        0.1f, 0.5f,  0.4f, 0.9f,  0.4f, 0.5f,

        // Alas (Mapeadas a las insignias y escarapelas del cuadrante superior derecho)
        0.6f, 0.1f,  0.55f, 0.5f, 0.95f, 0.5f,
        0.6f, 0.1f,  0.95f, 0.5f, 0.55f, 0.5f,

        // Colas dobles
        0.6f, 0.6f,  0.55f, 0.95f, 0.95f, 0.95f,
        0.6f, 0.6f,  0.95f, 0.95f, 0.55f, 0.95f
    };

    w3dEngine::VertexPointer3f(0, jetVerts);
    w3dEngine::TexCoordPointer2f(0, jetUVs);
    w3dEngine::DrawTrianglesArray(42);

    // --- Llamas de Postcombustión (Afterburners) ---
    float flamePulse = 0.8f + 0.3f * std::sin(timeSec_ * 30.0f);
    float flameVerts[] = {
        // Motor izquierdo
        -0.28f, -0.05f, -2.2f,   -0.12f, -0.05f, -2.2f,   -0.20f, -0.05f, -2.2f - (0.9f * flamePulse),
        // Motor derecho
         0.12f, -0.05f, -2.2f,    0.28f, -0.05f, -2.2f,    0.20f, -0.05f, -2.2f - (0.9f * flamePulse)
    };
    static const float flameUVs[] = {
        0.7f, 0.7f,  0.9f, 0.7f,  0.8f, 0.95f,
        0.7f, 0.7f,  0.9f, 0.7f,  0.8f, 0.95f
    };
    w3dEngine::VertexPointer3f(0, flameVerts);
    w3dEngine::TexCoordPointer2f(0, flameUVs);
    w3dEngine::DrawTrianglesArray(6);

    w3dEngine::DisableArray(w3dEngine::TexCoordArray);
    w3dEngine::PopMatrix();
}

void Renderer::renderProjectiles() {
    // 1. Trazadoras de ametralladora / Cañón
    if (firePressed_ || muzzleFlashTime_ > 0.0f) {
        muzzleFlashTime_ -= 0.016f;
        float bZ = -10.0f - std::fmod(timeSec_ * 120.0f, 80.0f);
        float tracerVerts[] = {
            planeX_ - 1.2f, planeY_ - 0.5f, bZ,
            planeX_ - 1.2f, planeY_ - 0.5f, bZ - 6.0f,

            planeX_ + 1.2f, planeY_ - 0.5f, bZ,
            planeX_ + 1.2f, planeY_ - 0.5f, bZ - 6.0f
        };
        static const unsigned char tracerColors[] = {
            255, 230, 80, 255,   255, 80, 20, 200,
            255, 230, 80, 255,   255, 80, 20, 200
        };
        w3dEngine::Disable(w3dEngine::Texture2D);
        w3dEngine::Enable(w3dEngine::ColorMaterial);
        w3dEngine::LineWidth(3.5f);
        w3dEngine::EnableArray(w3dEngine::VertexArray);
        w3dEngine::EnableArray(w3dEngine::ColorArray);
        w3dEngine::VertexPointer3f(0, tracerVerts);
        w3dEngine::ColorPointer4ub(tracerColors);
        w3dEngine::DrawLines(4);
        w3dEngine::DisableArray(w3dEngine::ColorArray);
    }

    // 2. Misil guiado hacia el objetivo
    if (missileFlightTime_ > 0.0f) {
        missileFlightTime_ -= 0.016f;
        float progress = 1.0f - (missileFlightTime_ / 1.8f);
        float mX = planeX_ * (1.0f - progress) + targetX_ * progress;
        float mY = (planeY_ - 0.5f) * (1.0f - progress) + targetY_ * progress;
        float mZ = -8.0f * (1.0f - progress) + targetZ_ * progress;

        w3dEngine::PushMatrix();
        w3dEngine::Translatef(mX, mY, mZ);
        static const float mslVerts[] = {
             0.0f,  0.0f,  0.8f,   -0.15f, 0.0f, -0.6f,    0.15f, 0.0f, -0.6f,
             0.0f,  0.15f, -0.6f,  -0.15f, 0.0f, -0.6f,    0.15f, 0.0f, -0.6f
        };
        static const unsigned char mslColors[] = {
            255, 255, 255, 255,   200, 40, 40, 255,   200, 40, 40, 255,
            255, 200, 50, 255,    200, 40, 40, 255,   200, 40, 40, 255
        };
        w3dEngine::Disable(w3dEngine::Texture2D);
        w3dEngine::Enable(w3dEngine::ColorMaterial);
        w3dEngine::EnableArray(w3dEngine::VertexArray);
        w3dEngine::EnableArray(w3dEngine::ColorArray);
        w3dEngine::VertexPointer3f(0, mslVerts);
        w3dEngine::ColorPointer4ub(mslColors);
        w3dEngine::DrawTrianglesArray(6);
        w3dEngine::DisableArray(w3dEngine::ColorArray);
        w3dEngine::PopMatrix();
    }
}

void Renderer::renderHUDQuad(float x, float y, float w, float h, GLuint texId, float alpha) {
    if (!texId) return;

    float qVerts[] = {
        x,     y,
        x + w, y,
        x + w, y + h,

        x,     y,
        x + w, y + h,
        x,     y + h
    };
    static const float qUVs[] = {
        0.0f, 0.0f,
        1.0f, 0.0f,
        1.0f, 1.0f,

        0.0f, 0.0f,
        1.0f, 1.0f,
        0.0f, 1.0f
    };

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
    // Fondo de la barra
    float bgVerts[] = {
        x,     y,         x + w, y,         x + w, y + h,
        x,     y,         x + w, y + h,     x,     y + h
    };
    w3dEngine::Disable(w3dEngine::Texture2D);
    w3dEngine::Color4f(0.05f, 0.12f, 0.20f, 0.75f);
    w3dEngine::EnableArray(w3dEngine::VertexArray);
    w3dEngine::VertexPointer2f(0, bgVerts);
    w3dEngine::DrawTrianglesArray(6);

    // Relleno de la barra
    float fillW = w * CLAMP(fillPct, 0.0f, 1.0f);
    float fgVerts[] = {
        x,         y,         x + fillW, y,         x + fillW, y + h,
        x,         y,         x + fillW, y + h,     x,         y + h
    };
    w3dEngine::Color4f(r, g, b, a);
    w3dEngine::VertexPointer2f(0, fgVerts);
    w3dEngine::DrawTrianglesArray(6);
}

void Renderer::renderGameUI() {
    // Modo Ortográfico 2D sobre la pantalla
    w3dEngine::Disable(w3dEngine::DepthTest);
    w3dEngine::Enable(w3dEngine::Blend);

    w3dEngine::MatrixMode(w3dEngine::Projection);
    w3dEngine::LoadIdentity();
    w3dEngine::Ortho(0.0f, (float)width_, (float)height_, 0.0f, -1.0f, 1.0f);

    w3dEngine::MatrixMode(w3dEngine::ModelView);
    w3dEngine::LoadIdentity();

    // 1. Retícula y Mirilla Táctica Central
    if (texHudCrosshair_) {
        float reticleSize = 140.0f;
        float rx = (width_ - reticleSize) * 0.5f;
        float ry = (height_ - reticleSize) * 0.5f - 20.0f;
        renderHUDQuad(rx, ry, reticleSize, reticleSize, texHudCrosshair_->getTextureID(), 0.9f);
    }

    // 2. Radar Táctico / Minimapa (Esquina superior izquierda)
    if (texHudRadar_) {
        float radarSize = 120.0f;
        renderHUDQuad(25.0f, 55.0f, radarSize, radarSize, texHudRadar_->getTextureID(), 0.88f);
    }

    // 3. Stick Virtual de Vuelo (Esquina inferior izquierda) con pomo desplazable
    if (texBtnStick_) {
        float stickCenterX = 110.0f;
        float stickCenterY = (height_ > 0) ? (height_ - 120.0f) : 400.0f;
        // Base del stick
        renderHUDQuad(stickCenterX - 55.0f, stickCenterY - 55.0f, 110.0f, 110.0f,
                      texBtnStick_->getTextureID(), stickActive_ ? 0.95f : 0.65f);
        // Pomo / Indicador de pulgar reactivo
        float knobX = stickCenterX - 28.0f + (stickDeflectX_ * 32.0f);
        float knobY = stickCenterY - 28.0f + (stickDeflectY_ * 32.0f);
        renderHUDQuad(knobX, knobY, 56.0f, 56.0f,
                      texBtnStick_->getTextureID(), stickActive_ ? 1.0f : 0.85f);
    }

    // 4. Botones Tácticos de Armamento (Esquina inferior derecha)
    // Botón de Cañón / Fuego
    if (texBtnFire_) {
        float fireW = 115.0f;
        float fx = width_ - 145.0f;
        float fy = height_ - 145.0f;
        float alpha = firePressed_ ? 1.0f : 0.85f;
        renderHUDQuad(fx, fy, fireW, fireW, texBtnFire_->getTextureID(), alpha);
    }

    // Botón de Misiles
    if (texBtnMissile_) {
        float mslW = 105.0f;
        float mx = width_ - 140.0f;
        float my = height_ - 265.0f;
        float alpha = (missileFlightTime_ > 0.0f || missilePressed_) ? 1.0f : 0.85f;
        renderHUDQuad(mx, my, mslW, mslW, texBtnMissile_->getTextureID(), alpha);
    }

    // 5. Barras Digitales de Estado y Telemetría HUD
    // Velocidad (Speed SPD: 480 KTS) - Superior Izquierda al lado del radar
    float speedPct = CLAMP((speedKnots_ - 380.0f) / 200.0f, 0.0f, 1.0f);
    renderHUDBar(155.0f, 65.0f, 110.0f, 12.0f, speedPct, 0.0f, 0.9f, 0.8f, 0.9f);

    // Altitud (Altitude ALT: 2,400 FT) - Superior Izquierda bajo velocidad
    float altPct = CLAMP((altitudeFeet_ - 1500.0f) / 1800.0f, 0.0f, 1.0f);
    renderHUDBar(155.0f, 85.0f, 110.0f, 12.0f, altPct, 0.0f, 0.85f, 1.0f, 0.9f);

    // Integridad del Casco (HULL 100%) - Centro Inferior
    float hullBarW = 200.0f;
    renderHUDBar((width_ - hullBarW) * 0.5f, height_ - 30.0f, hullBarW, 10.0f, healthPct_, 0.2f, 0.95f, 0.3f, 0.85f);

    // Objetivo Bloqueado - Banner Central Superior
    float bannerW = 240.0f;
    renderHUDBar((width_ - bannerW) * 0.5f, 35.0f, bannerW, 14.0f, 1.0f, 0.95f, 0.15f, 0.15f, 0.85f);

    // 6. Botón Táctico de Audio / Sonido (Esquina superior derecha)
    if (texBtnSound_) {
        float sndX = width_ - 70.0f;
        float sndY = 20.0f;
        float sndSize = 52.0f;
        float alpha = soundEnabled_ ? 0.95f : 0.40f;
        renderHUDQuad(sndX, sndY, sndSize, sndSize, texBtnSound_->getTextureID(), alpha);
        // Barra indicadora de estado
        if (soundEnabled_) {
            renderHUDBar(sndX + 4.0f, sndY + sndSize + 2.0f, sndSize - 8.0f, 4.0f, 1.0f, 0.1f, 0.95f, 0.4f, 0.9f);
        } else {
            renderHUDBar(sndX + 4.0f, sndY + sndSize + 2.0f, sndSize - 8.0f, 4.0f, 1.0f, 0.95f, 0.2f, 0.2f, 0.9f);
        }
    }
}

void Renderer::renderWhisk3D() {
    timeSec_ += 0.01667f;

    // Dinámica de vuelo y telemetría
    if (stickActive_) {
        planeX_ += (-planeRoll_ * 0.0035f);
        planeY_ += (planePitch_ * 0.0030f);
        planeYaw_ = -planeRoll_ * 0.26f;
    } else {
        // Estabilizador aerodinámico suave cuando se suelta el stick
        planeRoll_  *= 0.92f;
        planePitch_ *= 0.92f;
        planeYaw_   *= 0.92f;
        planeRoll_  += std::sin(timeSec_ * 1.5f) * 0.5f;
        planePitch_ += std::cos(timeSec_ * 1.0f) * 0.25f;
    }

    // Límites de envolvente de vuelo
    planeX_ = CLAMP(planeX_, -5.5f, 5.5f);
    planeY_ = CLAMP(planeY_, -1.8f, 3.2f);

    // Actualizar lecturas de telemetría digital
    altitudeFeet_ = 2400.0f + (planeY_ * 300.0f);
    speedKnots_   = 480.0f - (planePitch_ * 2.2f);

    // Recarga de munición de misiles cada 8 segundos
    if (missileCount_ < 4 && std::fmod(timeSec_, 8.0f) < 0.02f) {
        missileCount_++;
    }

    // Desplazamiento del buque de combate enemigo
    targetZ_ += 0.26f;
    if (targetZ_ > 8.0f) {
        // Buque rebasado, respawn más adelante
        targetZ_ = -130.0f;
        targetX_ = std::sin(timeSec_ * 0.6f) * 22.0f;
    }

    // Seguimiento y destrucción por misil
    if (missileFlightTime_ > 0.0f) {
        missileFlightTime_ -= 0.01667f;
        if (missileFlightTime_ <= 0.0f) {
            enemiesDestroyed_++;
            targetZ_ = -140.0f;
            targetX_ = std::cos(timeSec_ * 0.8f) * 20.0f;
            muzzleFlashTime_ = 0.3f;
            if (sndExplosion_) {
                w3dEngine::W3dSoundPlay(sndExplosion_, 0.90f, false);
            }
        }
    }

    // Paso físico de Whisk3D
    W3dFisicaPaso(1.0f / 60.0f);

    // 1. Configurar Viewport
    w3dEngine::Viewport(0, 0, width_, height_);

    // 2. Matriz de Proyección Perspectiva 3D
    float aspect = (height_ > 0) ? static_cast<float>(width_) / static_cast<float>(height_) : 1.0f;
    w3dEngine::MatrixMode(w3dEngine::Projection);
    w3dEngine::LoadIdentity();
    w3dEngine::Perspective(55.0f, aspect, 0.1f, 500.0f);

    // 3. Matriz ModelView de Cámara de persecución en 3ra persona con seguimiento cinemático
    float camLagX = planeX_ * 0.38f;
    float camLagY = planeY_ * 0.25f;
    float camBank = planeRoll_ * 0.14f;

    w3dEngine::MatrixMode(w3dEngine::ModelView);
    w3dEngine::LoadIdentity();
    w3dEngine::Rotatef(-camBank, 0.0f, 0.0f, 1.0f);
    w3dEngine::Translatef(-camLagX, -0.9f - camLagY, -2.0f);

    // 4. Estados 3D
    w3dEngine::Enable(w3dEngine::DepthTest);
    w3dEngine::DepthFunc(w3dEngine::DepthLEqual);
    w3dEngine::Enable(w3dEngine::CullFace);

    // 5. Dibujar elementos de la escena 3D
    renderSkyAndOcean();
    renderIslands();
    renderTarget();
    renderAircraft();
    renderProjectiles();

    // 6. Dibujar Interfaz de Usuario (HUD UI) 2D
    renderGameUI();
}

void Renderer::render() {
    updateRenderArea();

    // Limpiar pantalla con color de atmósfera aeroespacial
    w3dEngine::ClearColor(0.06f, 0.12f, 0.22f, 1.0f);
    w3dEngine::Clear(w3dEngine::ColorBuffer | w3dEngine::DepthBuffer);

    // Renderizar escena 3D y UI del juego con Whisk3D
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
