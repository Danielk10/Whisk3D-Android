#include "Renderer.h"

#include <game-activity/native_app_glue/android_native_app_glue.h>
#include <GLES3/gl3.h>
#include <memory>
#include <vector>
#include <cmath>
#include <algorithm>
#include <cassert>

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
        flakCooldown_(2.5f),
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
        targetY_(-1.8f),
        targetZ_(-80.0f),
        targetHealth_(100.0f),
        targetMaxHealth_(100.0f),
        targetHitFlashTime_(0.0f),
        targetLocked_(false),
        prevTargetLocked_(false),
        touchDown_(false),
        touchX_(0.0f),
        touchY_(0.0f),
        stickActive_(false),
        stickPointerId_(-1),
        stickOriginX_(120.0f),
        stickOriginY_(400.0f),
        stickDeflectX_(0.0f),
        stickDeflectY_(0.0f),
        firePressed_(false),
        missilePressed_(false),
        cannonCooldown_(0.0f),
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

void Renderer::onWindowInit() {
    if (display_ != EGL_NO_DISPLAY && context_ != EGL_NO_CONTEXT && app_->window != nullptr) {
        surface_ = eglCreateWindowSurface(display_, config_, app_->window, nullptr);
        if (surface_ != EGL_NO_SURFACE) {
            eglMakeCurrent(display_, surface_, surface_, context_);
            updateRenderArea();
            aout << "Whisk3D: EGL surface restored on window init." << std::endl;
        }
    }
}

void Renderer::onWindowTerm() {
    if (display_ != EGL_NO_DISPLAY && surface_ != EGL_NO_SURFACE) {
        eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        eglDestroySurface(display_, surface_);
        surface_ = EGL_NO_SURFACE;
        aout << "Whisk3D: EGL surface released on window term." << std::endl;
    }
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
    enemiesDestroyed_ = 0;
    targetX_ = 0.0f;
    targetY_ = -1.8f;
    targetZ_ = -90.0f;
    targetHealth_ = targetMaxHealth_;
    bullets_.clear();
    enemyBullets_.clear();
    firePressed_ = false;
    missilePressed_ = false;
    stickActive_ = false;
    missileFlightTime_ = 0.0f;
    targetHitFlashTime_ = 0.0f;
    muzzleFlashTime_ = 0.0f;
    flakCooldown_ = 2.0f;
}

void Renderer::initWhisk3D() {
    aout << "Whisk3D: Inicializando motor y backend grafico GLES2/3..." << std::endl;

    w3dFileSystem::SetAssetManager(app_->activity->assetManager);
    if (app_->activity->internalDataPath) {
        w3dFileSystem::SetUserDataDir(app_->activity->internalDataPath);
    }

    w3dEngine::GLES2Init(nullptr);

    if (w3dEngine::W3dAudioInit(44100)) {
        aout << "Whisk3D: Motor de audio OpenSL ES inicializado (44.1 kHz stereo)!" << std::endl;
        sndEngine_    = w3dEngine::W3dSoundLoad("sounds/engine.wav");
        sndCannon_    = w3dEngine::W3dSoundLoad("sounds/cannon.wav");
        sndMissile_   = w3dEngine::W3dSoundLoad("sounds/missile.wav");
        sndExplosion_ = w3dEngine::W3dSoundLoad("sounds/explosion.wav");
        sndLock_      = w3dEngine::W3dSoundLoad("sounds/lock.wav");

        if (sndEngine_) {
            engineVoiceId_ = w3dEngine::W3dSoundPlay(sndEngine_, 0.25f, true);
        }
    } else {
        aout << "Whisk3D: No se pudo abrir backend de audio" << std::endl;
    }

    loadGameTextures();

    aout << "Whisk3D: Escena y recursos listos!" << std::endl;
}

void Renderer::loadGameTextures() {
    aout << "Whisk3D: Cargando texturas de Sky Strike y UI..." << std::endl;
    auto assetMgr = app_->activity->assetManager;
    if (!assetMgr) return;

    // 3D Scene Textures
    texAirplane_     = TextureAsset::loadAsset(assetMgr, "textures/airplane.png");
    texSea_          = TextureAsset::loadAsset(assetMgr, "textures/sea.png");
    texTerrain_      = TextureAsset::loadAsset(assetMgr, "textures/terrain.png");
    texTarget_       = TextureAsset::loadAsset(assetMgr, "textures/target.png");

    // In-game HUD Textures
    texHudCrosshair_ = TextureAsset::loadAsset(assetMgr, "textures/hud_crosshair.png");
    texHudRadar_     = TextureAsset::loadAsset(assetMgr, "textures/hud_radar.png");
    texBtnFire_      = TextureAsset::loadAsset(assetMgr, "textures/btn_fire.png");
    texBtnMissile_   = TextureAsset::loadAsset(assetMgr, "textures/btn_missile.png");
    texBtnStick_     = TextureAsset::loadAsset(assetMgr, "textures/btn_stick.png");
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

void Renderer::handleInput() {
    auto *inputBuffer = android_app_swap_input_buffers(app_);
    if (!inputBuffer) return;

    float sndBtnX = width_ - 70.0f, sndBtnY = 20.0f;
    float sndBtnSize = 55.0f;

    for (auto i = 0; i < inputBuffer->motionEventsCount; i++) {
        auto &motionEvent = inputBuffer->motionEvents[i];
        auto actionMasked = motionEvent.action & AMOTION_EVENT_ACTION_MASK;
        auto pointerIndex = (motionEvent.action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK)
                >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;

        // -------------------------------------------------------------
        // 1. MANEJO DE ENTRADA EN MENÚ PRINCIPAL
        // -------------------------------------------------------------
        if (gameState_ == STATE_MAIN_MENU) {
            if (actionMasked == AMOTION_EVENT_ACTION_DOWN || actionMasked == AMOTION_EVENT_ACTION_POINTER_DOWN) {
                if (pointerIndex < motionEvent.pointerCount) {
                    auto &pointer = motionEvent.pointers[pointerIndex];
                    float px = GameActivityPointerAxes_getX(&pointer);
                    float py = GameActivityPointerAxes_getY(&pointer);

                    if (showHelpModal_) {
                        // Tocar en el botón cerrar o en cualquier parte cierra el diálogo de ayuda
                        showHelpModal_ = false;
                        if (sndLock_) w3dEngine::W3dSoundPlay(sndLock_, 0.6f, false);
                    } else {
                        // Botón de sonido
                        if (px >= sndBtnX && px <= sndBtnX + sndBtnSize && py >= sndBtnY && py <= sndBtnY + sndBtnSize) {
                            soundEnabled_ = !soundEnabled_;
                            w3dEngine::W3dAudioMasterVolume(soundEnabled_ ? 1.0f : 0.0f);
                            if (soundEnabled_ && sndLock_) w3dEngine::W3dSoundPlay(sndLock_, 0.55f, false);
                        }
                        // Botón JUGAR / DESPEGAR
                        float btnW = 340.0f, btnH = 80.0f;
                        float bx = (width_ - btnW) * 0.5f;
                        float by = height_ * 0.52f - 20.0f;
                        if (px >= bx && px <= bx + btnW && py >= by && py <= by + btnH) {
                            resetMission();
                            gameState_ = STATE_PLAYING;
                            if (sndLock_) w3dEngine::W3dSoundPlay(sndLock_, 0.85f, false);
                        }
                        // Botón AYUDA (?)
                        float hSize = 58.0f;
                        float hx = (width_ + 340.0f) * 0.5f + 15.0f;
                        float hy = height_ * 0.52f - 10.0f;
                        if (px >= hx && px <= hx + hSize && py >= hy && py <= hy + hSize) {
                            showHelpModal_ = true;
                            if (sndLock_) w3dEngine::W3dSoundPlay(sndLock_, 0.65f, false);
                        }
                    }
                }
            }
            continue;
        }

        // -------------------------------------------------------------
        // 2. MANEJO DE ENTRADA EN MENÚ DE PAUSA
        // -------------------------------------------------------------
        if (gameState_ == STATE_PAUSED) {
            if (actionMasked == AMOTION_EVENT_ACTION_DOWN || actionMasked == AMOTION_EVENT_ACTION_POINTER_DOWN) {
                if (pointerIndex < motionEvent.pointerCount) {
                    auto &pointer = motionEvent.pointers[pointerIndex];
                    float px = GameActivityPointerAxes_getX(&pointer);
                    float py = GameActivityPointerAxes_getY(&pointer);

                    // Botón de sonido
                    if (px >= sndBtnX && px <= sndBtnX + sndBtnSize && py >= sndBtnY && py <= sndBtnY + sndBtnSize) {
                        soundEnabled_ = !soundEnabled_;
                        w3dEngine::W3dAudioMasterVolume(soundEnabled_ ? 1.0f : 0.0f);
                        if (soundEnabled_ && sndLock_) w3dEngine::W3dSoundPlay(sndLock_, 0.55f, false);
                    }

                    float dlgH = 320.0f;
                    float dy = (height_ - dlgH) * 0.5f;
                    float btnW = 270.0f, btnH = 55.0f;
                    float bx = (width_ - btnW) * 0.5f;

                    // Continuar
                    float by1 = dy + 105.0f;
                    if (px >= bx && px <= bx + btnW && py >= by1 && py <= by1 + btnH) {
                        gameState_ = STATE_PLAYING;
                        if (sndLock_) w3dEngine::W3dSoundPlay(sndLock_, 0.7f, false);
                    }
                    // Reiniciar
                    float by2 = dy + 172.0f;
                    if (px >= bx && px <= bx + btnW && py >= by2 && py <= by2 + btnH) {
                        resetMission();
                        gameState_ = STATE_PLAYING;
                        if (sndLock_) w3dEngine::W3dSoundPlay(sndLock_, 0.8f, false);
                    }
                    // Salir al menú
                    float by3 = dy + 238.0f;
                    if (px >= bx && px <= bx + btnW && py >= by3 && py <= by3 + btnH) {
                        gameState_ = STATE_MAIN_MENU;
                        if (sndLock_) w3dEngine::W3dSoundPlay(sndLock_, 0.7f, false);
                    }
                }
            }
            continue;
        }

        // -------------------------------------------------------------
        // 3. MANEJO DE ENTRADA EN FIN DE PARTIDA / GAME OVER
        // -------------------------------------------------------------
        if (gameState_ == STATE_GAME_OVER) {
            if (actionMasked == AMOTION_EVENT_ACTION_DOWN || actionMasked == AMOTION_EVENT_ACTION_POINTER_DOWN) {
                if (pointerIndex < motionEvent.pointerCount) {
                    auto &pointer = motionEvent.pointers[pointerIndex];
                    float px = GameActivityPointerAxes_getX(&pointer);
                    float py = GameActivityPointerAxes_getY(&pointer);

                    float dlgH = 340.0f;
                    float dy = (height_ - dlgH) * 0.5f;
                    float btnW = 270.0f, btnH = 55.0f;
                    float bx = (width_ - btnW) * 0.5f;

                    // Reintentar
                    float by1 = dy + 185.0f;
                    if (px >= bx && px <= bx + btnW && py >= by1 && py <= by1 + btnH) {
                        resetMission();
                        gameState_ = STATE_PLAYING;
                        if (sndLock_) w3dEngine::W3dSoundPlay(sndLock_, 0.8f, false);
                    }
                    // Menú principal
                    float by2 = dy + 252.0f;
                    if (px >= bx && px <= bx + btnW && py >= by2 && py <= by2 + btnH) {
                        gameState_ = STATE_MAIN_MENU;
                        if (sndLock_) w3dEngine::W3dSoundPlay(sndLock_, 0.7f, false);
                    }
                }
            }
            continue;
        }

        // -------------------------------------------------------------
        // 4. MANEJO DE ENTRADA DURANTE EL JUEGO (STATE_PLAYING)
        // -------------------------------------------------------------
        float fireX0 = width_ - 145.0f, fireY0 = height_ - 145.0f;
        float fireX1 = width_ - 15.0f,  fireY1 = height_ - 15.0f;

        float mslX0 = width_ - 145.0f, mslY0 = height_ - 265.0f;
        float mslX1 = width_ - 15.0f,  mslY1 = height_ - 150.0f;

        float pauseBtnX = width_ - 130.0f, pauseBtnY = 20.0f;
        float pauseBtnSize = 52.0f;

        if (actionMasked == AMOTION_EVENT_ACTION_DOWN || actionMasked == AMOTION_EVENT_ACTION_POINTER_DOWN) {
            touchDown_ = true;
            if (pointerIndex < motionEvent.pointerCount) {
                auto &pointer = motionEvent.pointers[pointerIndex];
                float px = GameActivityPointerAxes_getX(&pointer);
                float py = GameActivityPointerAxes_getY(&pointer);
                int pId = pointer.id;

                if (px < width_ * 0.5f) {
                    // Joystick flotante anclado en la posición de toque
                    stickActive_ = true;
                    stickPointerId_ = pId;
                    stickOriginX_ = px;
                    stickOriginY_ = py;
                    stickDeflectX_ = 0.0f;
                    stickDeflectY_ = 0.0f;
                } else {
                    // Botón de Pausa
                    if (px >= pauseBtnX && px <= pauseBtnX + pauseBtnSize && py >= pauseBtnY && py <= pauseBtnY + pauseBtnSize) {
                        gameState_ = STATE_PAUSED;
                        firePressed_ = false;
                        stickActive_ = false;
                        if (sndLock_) w3dEngine::W3dSoundPlay(sndLock_, 0.65f, false);
                    }
                    // Botón de Sonido Mute/Unmute
                    else if (px >= sndBtnX && px <= sndBtnX + sndBtnSize && py >= sndBtnY && py <= sndBtnY + sndBtnSize) {
                        soundEnabled_ = !soundEnabled_;
                        w3dEngine::W3dAudioMasterVolume(soundEnabled_ ? 1.0f : 0.0f);
                        if (soundEnabled_ && sndLock_) {
                            w3dEngine::W3dSoundPlay(sndLock_, 0.55f, false);
                        }
                    } else if (px >= fireX0 && px <= fireX1 && py >= fireY0 && py <= fireY1) {
                        firePressed_ = true;
                        cannonCooldown_ = 0.0f;
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

                if (pId == stickPointerId_) {
                    float dx = px - stickOriginX_;
                    float dy = py - stickOriginY_;
                    float maxR = 85.0f;
                    float dist = std::sqrt(dx * dx + dy * dy);
                    if (dist > maxR) {
                        dx = (dx / dist) * maxR;
                        dy = (dy / dist) * maxR;
                    }
                    stickDeflectX_ = dx / maxR;
                    stickDeflectY_ = dy / maxR;
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
    // 1. Cielo atmosférico (Gradiente de horizonte lejano)
    static const float skyVerts[] = {
        -260.0f,  75.0f, -240.0f,
         260.0f,  75.0f, -240.0f,
         260.0f,  -6.0f, -240.0f,

        -260.0f,  75.0f, -240.0f,
         260.0f,  -6.0f, -240.0f,
        -260.0f,  -6.0f, -240.0f
    };
    static const unsigned char skyColors[] = {
        25, 75, 175, 255,
        25, 75, 175, 255,
        170, 215, 250, 255,

        25, 75, 175, 255,
        170, 215, 250, 255,
        170, 215, 250, 255
    };

    w3dEngine::Disable(w3dEngine::Texture2D);
    w3dEngine::Enable(w3dEngine::ColorMaterial);
    w3dEngine::EnableArray(w3dEngine::VertexArray);
    w3dEngine::EnableArray(w3dEngine::ColorArray);
    w3dEngine::VertexPointer3f(0, skyVerts);
    w3dEngine::ColorPointer4ub(skyColors);
    w3dEngine::DrawTrianglesArray(6);
    w3dEngine::DisableArray(w3dEngine::ColorArray);

    // 2. Océano 3D con malla subdividida 8x10
    if (texSea_) {
        float waveShift = timeSec_ * 0.15f;
        const int gridX = 8;
        const int gridZ = 10;
        const float minX = -180.0f, maxX = 180.0f;
        const float minZ = -240.0f, maxZ = 30.0f;
        const float stepX = (maxX - minX) / gridX;
        const float stepZ = (maxZ - minZ) / gridZ;

        std::vector<float> oceanVerts;
        std::vector<float> oceanUVs;
        oceanVerts.reserve(gridX * gridZ * 18);
        oceanUVs.reserve(gridX * gridZ * 12);

        for (int j = 0; j < gridZ; ++j) {
            float z0 = minZ + j * stepZ;
            float z1 = z0 + stepZ;
            float v0 = (float)j / gridZ * 18.0f + waveShift;
            float v1 = (float)(j + 1) / gridZ * 18.0f + waveShift;

            for (int i = 0; i < gridX; ++i) {
                float x0 = minX + i * stepX;
                float x1 = x0 + stepX;
                float u0 = (float)i / gridX * 14.0f;
                float u1 = (float)(i + 1) / gridX * 14.0f;

                oceanVerts.insert(oceanVerts.end(), {
                    x0, -2.5f, z0,
                    x1, -2.5f, z0,
                    x1, -2.5f, z1
                });
                oceanUVs.insert(oceanUVs.end(), {
                    u0, v0,
                    u1, v0,
                    u1, v1
                });

                oceanVerts.insert(oceanVerts.end(), {
                    x0, -2.5f, z0,
                    x1, -2.5f, z1,
                    x0, -2.5f, z1
                });
                oceanUVs.insert(oceanUVs.end(), {
                    u0, v0,
                    u1, v1,
                    u0, v1
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
    }
}

void Renderer::renderIslands() {
    if (!texTerrain_) return;

    w3dEngine::Enable(w3dEngine::Texture2D);
    w3dEngine::BindTexture(texTerrain_->getTextureID());
    w3dEngine::Color4f(1.0f, 1.0f, 1.0f, 1.0f);
    w3dEngine::EnableArray(w3dEngine::VertexArray);
    w3dEngine::EnableArray(w3dEngine::TexCoordArray);

    // Isla Principal Volcánica
    w3dEngine::PushMatrix();
    w3dEngine::Translatef(-38.0f, -2.5f, -95.0f);

    static const float islandVerts[] = {
        0.0f, 10.5f, 0.0f,   18.0f, 0.0f, 0.0f,    13.0f, 0.0f, 14.0f,
        0.0f, 10.5f, 0.0f,   13.0f, 0.0f, 14.0f,    0.0f, 0.0f, 20.0f,
        0.0f, 10.5f, 0.0f,    0.0f, 0.0f, 20.0f,  -15.0f, 0.0f, 15.0f,
        0.0f, 10.5f, 0.0f,  -15.0f, 0.0f, 15.0f,  -21.0f, 0.0f, 0.0f,
        0.0f, 10.5f, 0.0f,  -21.0f, 0.0f, 0.0f,   -14.0f, 0.0f, -17.0f,
        0.0f, 10.5f, 0.0f,  -14.0f, 0.0f, -17.0f,   0.0f, 0.0f, -22.0f,
        0.0f, 10.5f, 0.0f,    0.0f, 0.0f, -22.0f,  15.0f, 0.0f, -14.0f,
        0.0f, 10.5f, 0.0f,   15.0f, 0.0f, -14.0f,  18.0f, 0.0f, 0.0f
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

    w3dEngine::VertexPointer3f(0, islandVerts);
    w3dEngine::TexCoordPointer2f(0, islandUVs);
    w3dEngine::DrawTrianglesArray(24);
    w3dEngine::PopMatrix();

    // Atolón Derecho
    w3dEngine::PushMatrix();
    w3dEngine::Translatef(44.0f, -2.5f, -135.0f);
    static const float atollVerts[] = {
        0.0f, 6.0f, 0.0f,   14.0f, 0.0f, 0.0f,    10.0f, 0.0f, 11.0f,
        0.0f, 6.0f, 0.0f,   10.0f, 0.0f, 11.0f,    0.0f, 0.0f, 15.0f,
        0.0f, 6.0f, 0.0f,    0.0f, 0.0f, 15.0f,  -11.0f, 0.0f, 11.0f,
        0.0f, 6.0f, 0.0f,  -11.0f, 0.0f, 11.0f,  -15.0f, 0.0f, 0.0f,
        0.0f, 6.0f, 0.0f,  -15.0f, 0.0f, 0.0f,   -10.0f, 0.0f, -12.0f,
        0.0f, 6.0f, 0.0f,  -10.0f, 0.0f, -12.0f,   0.0f, 0.0f, -16.0f,
        0.0f, 6.0f, 0.0f,    0.0f, 0.0f, -16.0f,  11.0f, 0.0f, -10.0f,
        0.0f, 6.0f, 0.0f,   11.0f, 0.0f, -10.0f,  14.0f, 0.0f, 0.0f
    };
    w3dEngine::VertexPointer3f(0, atollVerts);
    w3dEngine::TexCoordPointer2f(0, islandUVs);
    w3dEngine::DrawTrianglesArray(24);
    w3dEngine::PopMatrix();

    w3dEngine::DisableArray(w3dEngine::TexCoordArray);
}

void Renderer::renderTarget() {
    if (!texTarget_) return;

    w3dEngine::PushMatrix();
    w3dEngine::Translatef(targetX_, targetY_, targetZ_);

    w3dEngine::Enable(w3dEngine::Texture2D);
    w3dEngine::BindTexture(texTarget_->getTextureID());
    if (targetHitFlashTime_ > 0.0f) {
        w3dEngine::Color4f(1.0f, 0.35f, 0.35f, 1.0f);
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
        -2.8f,  1.2f,  -4.0f,   -2.8f, -1.2f,  -4.0f,   -2.8f, -1.2f,   8.0f,
        -2.8f,  1.2f,  -4.0f,   -2.8f, -1.2f,   8.0f,   -2.8f,  1.2f,   8.0f,
         2.8f,  1.2f,  -4.0f,    2.8f,  1.2f,   8.0f,    2.8f, -1.2f,   8.0f,
         2.8f,  1.2f,  -4.0f,    2.8f, -1.2f,   8.0f,    2.8f, -1.2f,  -4.0f,
        -2.8f,  1.2f,   8.0f,    2.8f,  1.2f,   8.0f,    2.8f, -1.2f,   8.0f,
        -2.8f,  1.2f,   8.0f,    2.8f, -1.2f,   8.0f,   -2.8f, -1.2f,   8.0f,

        // --- 3. Superestructura / Castillo de Mando (Bridge) ---
        -1.4f,  3.5f,  -1.2f,    1.4f,  3.5f,  -1.2f,    1.4f,  3.5f,   2.5f,
        -1.4f,  3.5f,  -1.2f,    1.4f,  3.5f,   2.5f,   -1.4f,  3.5f,   2.5f,
        -1.4f,  3.5f,  -1.2f,    1.4f,  1.2f,  -1.2f,    1.4f,  3.5f,  -1.2f,
        -1.4f,  3.5f,  -1.2f,   -1.4f,  1.2f,  -1.2f,    1.4f,  1.2f,  -1.2f,
        -1.4f,  3.5f,  -1.2f,   -1.4f,  3.5f,   2.5f,   -1.4f,  1.2f,   2.5f,
        -1.4f,  3.5f,  -1.2f,   -1.4f,  1.2f,   2.5f,   -1.4f,  1.2f,  -1.2f,
         1.4f,  3.5f,  -1.2f,    1.4f,  1.2f,   2.5f,    1.4f,  3.5f,   2.5f,
         1.4f,  3.5f,  -1.2f,    1.4f,  1.2f,  -1.2f,    1.4f,  1.2f,   2.5f,
        -1.4f,  3.5f,   2.5f,    1.4f,  3.5f,   2.5f,    1.4f,  1.2f,   2.5f,
        -1.4f,  3.5f,   2.5f,    1.4f,  1.2f,   2.5f,   -1.4f,  1.2f,   2.5f
    };

    static const float shipUVs[] = {
        // Cubierta proa (Silos de misiles VLS)
        0.25f, 0.95f,   0.10f, 0.60f,   0.45f, 0.60f,
        // Cubierta popa (Helipuerto con insignia 'H')
        0.55f, 0.60f,   0.55f, 0.95f,   0.95f, 0.95f,
        0.55f, 0.60f,   0.95f, 0.95f,   0.95f, 0.60f,

        // Casco amuras con línea de flotación roja
        0.10f, 0.45f,   0.45f, 0.45f,   0.45f, 0.05f,
        0.10f, 0.45f,   0.45f, 0.05f,   0.10f, 0.05f,
        0.45f, 0.45f,   0.85f, 0.45f,   0.85f, 0.05f,
        0.45f, 0.45f,   0.85f, 0.05f,   0.45f, 0.05f,

        // Costados de casco (Placas de acero blindado)
        0.10f, 0.45f,   0.10f, 0.05f,   0.85f, 0.05f,
        0.10f, 0.45f,   0.85f, 0.05f,   0.85f, 0.45f,
        0.10f, 0.45f,   0.85f, 0.45f,   0.85f, 0.05f,
        0.10f, 0.45f,   0.85f, 0.05f,   0.10f, 0.45f,

        // Popa
        0.25f, 0.45f,   0.75f, 0.45f,   0.75f, 0.05f,
        0.25f, 0.45f,   0.75f, 0.05f,   0.25f, 0.05f,

        // Puente techo
        0.25f, 0.65f,   0.75f, 0.65f,   0.75f, 0.85f,
        0.25f, 0.65f,   0.75f, 0.85f,   0.25f, 0.85f,

        // Puente frontal (Ventanales)
        0.20f, 0.45f,   0.80f, 0.25f,   0.80f, 0.45f,
        0.20f, 0.45f,   0.20f, 0.25f,   0.80f, 0.25f,

        // Puente babor
        0.20f, 0.45f,   0.60f, 0.45f,   0.60f, 0.25f,
        0.20f, 0.45f,   0.60f, 0.25f,   0.20f, 0.25f,

        // Puente estribor
        0.20f, 0.45f,   0.60f, 0.25f,   0.60f, 0.45f,
        0.20f, 0.45f,   0.20f, 0.25f,   0.60f, 0.25f,

        // Puente trasero
        0.30f, 0.45f,   0.70f, 0.45f,   0.70f, 0.25f,
        0.30f, 0.45f,   0.70f, 0.25f,   0.30f, 0.25f
    };

    w3dEngine::VertexPointer3f(0, shipVerts);
    w3dEngine::TexCoordPointer2f(0, shipUVs);
    w3dEngine::DrawTrianglesArray(69);
    w3dEngine::DisableArray(w3dEngine::TexCoordArray);
    w3dEngine::Enable(w3dEngine::CullFace);

    w3dEngine::PopMatrix();
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

    // Modelo 3D del Caza apuntando hacia adelante (-Z)
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

        // --- 4. Alas Delta en Flecha ---
        -0.42f, 0.05f, -0.7f,  -3.2f,  0.02f,  1.0f,   -0.52f, 0.05f,  1.4f,
        -0.42f,-0.02f, -0.7f,  -0.52f,-0.02f,  1.4f,   -3.2f, -0.02f,  1.0f,
         0.42f, 0.05f, -0.7f,   0.52f, 0.05f,  1.4f,    3.2f,  0.02f,  1.0f,
         0.42f,-0.02f, -0.7f,   3.2f, -0.02f,  1.0f,    0.52f,-0.02f,  1.4f,

        // --- 5. Estabilizadores Verticales Dobles ---
        -0.40f, 0.16f,  0.6f,  -0.65f, 1.15f,  1.6f,   -0.40f, 0.16f,  1.5f,
        -0.40f, 0.16f,  0.6f,  -0.40f, 0.16f,  1.5f,   -0.65f, 1.15f,  1.6f,
         0.40f, 0.16f,  0.6f,   0.40f, 0.16f,  1.5f,    0.65f, 1.15f,  1.6f,
         0.40f, 0.16f,  0.6f,   0.65f, 1.15f,  1.6f,    0.40f, 0.16f,  1.5f
    };

    static const float jetUVs[] = {
        // Morro superior
        0.75f, 0.95f,  0.60f, 0.65f,  0.75f, 0.65f,
        0.75f, 0.95f,  0.75f, 0.65f,  0.90f, 0.65f,
        // Morro inferior
        0.25f, 0.45f,  0.10f, 0.15f,  0.25f, 0.15f,
        0.25f, 0.45f,  0.40f, 0.15f,  0.25f, 0.15f,

        // Cabina
        0.25f, 0.60f,  0.08f, 0.75f,  0.25f, 0.92f,
        0.25f, 0.60f,  0.25f, 0.92f,  0.42f, 0.75f,
        0.25f, 0.92f,  0.08f, 0.75f,  0.25f, 0.70f,
        0.25f, 0.92f,  0.25f, 0.70f,  0.42f, 0.75f,

        // Fuselaje
        0.60f, 0.90f,  0.60f, 0.55f,  0.90f, 0.55f,
        0.60f, 0.90f,  0.90f, 0.55f,  0.90f, 0.90f,
        0.10f, 0.45f,  0.40f, 0.10f,  0.10f, 0.10f,
        0.10f, 0.45f,  0.40f, 0.45f,  0.40f, 0.10f,

        // Alas Superiores e Inferiores
        0.60f, 0.60f,  0.95f, 0.95f,  0.95f, 0.60f,
        0.10f, 0.10f,  0.45f, 0.45f,  0.45f, 0.10f,
        0.60f, 0.60f,  0.95f, 0.60f,  0.95f, 0.95f,
        0.10f, 0.10f,  0.45f, 0.10f,  0.45f, 0.45f,

        // Colas
        0.65f, 0.60f,  0.88f, 0.95f,  0.88f, 0.60f,
        0.65f, 0.60f,  0.88f, 0.60f,  0.88f, 0.95f,
        0.65f, 0.60f,  0.88f, 0.60f,  0.88f, 0.95f,
        0.65f, 0.60f,  0.88f, 0.88f,  0.88f, 0.60f
    };

    w3dEngine::VertexPointer3f(0, jetVerts);
    w3dEngine::TexCoordPointer2f(0, jetUVs);
    w3dEngine::DrawTrianglesArray(60);

    // Llamas de postcombustión hacia +Z
    float flamePulse = 0.85f + 0.35f * std::sin(timeSec_ * 32.0f);
    float flameVerts[] = {
        -0.34f, -0.02f, 1.6f,   -0.16f, -0.02f, 1.6f,   -0.25f, -0.02f, 1.6f + (0.95f * flamePulse),
        -0.25f,  0.07f, 1.6f,   -0.25f, -0.11f, 1.6f,   -0.25f, -0.02f, 1.6f + (0.95f * flamePulse),
         0.16f, -0.02f, 1.6f,    0.34f, -0.02f, 1.6f,    0.25f, -0.02f, 1.6f + (0.95f * flamePulse),
         0.25f,  0.07f, 1.6f,    0.25f, -0.11f, 1.6f,    0.25f, -0.02f, 1.6f + (0.95f * flamePulse)
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

void Renderer::renderProjectiles() {
    // 1. Trazadoras del Cañón
    if (!bullets_.empty()) {
        std::vector<float> bulletVerts;
        std::vector<unsigned char> bulletColors;
        bulletVerts.reserve(bullets_.size() * 6);
        bulletColors.reserve(bullets_.size() * 8);

        for (const auto& b : bullets_) {
            bulletVerts.push_back(b.x);
            bulletVerts.push_back(b.y);
            bulletVerts.push_back(b.z);

            bulletVerts.push_back(b.x);
            bulletVerts.push_back(b.y);
            bulletVerts.push_back(b.z + 3.8f);

            bulletColors.insert(bulletColors.end(), {
                255, 245, 120, 255,
                255, 110, 30,  180
            });
        }

        w3dEngine::Disable(w3dEngine::Texture2D);
        w3dEngine::Enable(w3dEngine::ColorMaterial);
        w3dEngine::LineWidth(3.8f);
        w3dEngine::EnableArray(w3dEngine::VertexArray);
        w3dEngine::EnableArray(w3dEngine::ColorArray);
        w3dEngine::VertexPointer3f(0, bulletVerts.data());
        w3dEngine::ColorPointer4ub(bulletColors.data());
        w3dEngine::DrawLines(static_cast<int>(bullets_.size() * 2));
        w3dEngine::DisableArray(w3dEngine::ColorArray);
    }

    // 2. Destello de boca
    if (muzzleFlashTime_ > 0.0f) {
        float mfVerts[] = {
            planeX_ - 0.9f, planeY_ - 0.2f, -7.2f,
            planeX_ - 0.9f, planeY_ - 0.2f, -8.6f,
            planeX_ + 0.9f, planeY_ - 0.2f, -7.2f,
            planeX_ + 0.9f, planeY_ - 0.2f, -8.6f
        };
        static const unsigned char mfColors[] = {
            255, 255, 200, 255,   255, 150, 40, 220,
            255, 255, 200, 255,   255, 150, 40, 220
        };
        w3dEngine::Disable(w3dEngine::Texture2D);
        w3dEngine::Enable(w3dEngine::ColorMaterial);
        w3dEngine::LineWidth(4.5f);
        w3dEngine::EnableArray(w3dEngine::VertexArray);
        w3dEngine::EnableArray(w3dEngine::ColorArray);
        w3dEngine::VertexPointer3f(0, mfVerts);
        w3dEngine::ColorPointer4ub(mfColors);
        w3dEngine::DrawLines(4);
        w3dEngine::DisableArray(w3dEngine::ColorArray);
    }

    // 3. Misil guiado hacia el objetivo
    if (missileFlightTime_ > 0.0f) {
        float progress = CLAMP(1.0f - (missileFlightTime_ / 1.6f), 0.0f, 1.0f);
        float mX = planeX_ * (1.0f - progress) + targetX_ * progress;
        float mY = (planeY_ - 0.4f) * (1.0f - progress) + targetY_ * progress;
        float mZ = -7.5f * (1.0f - progress) + targetZ_ * progress;

        w3dEngine::PushMatrix();
        w3dEngine::Translatef(mX, mY, mZ);
        static const float mslVerts[] = {
             0.0f,  0.0f, -0.8f,   -0.15f, 0.0f,  0.6f,    0.15f, 0.0f,  0.6f,
             0.0f,  0.15f,  0.6f,  -0.15f, 0.0f,  0.6f,    0.15f, 0.0f,  0.6f
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

    // 4. Proyectiles antiaéreos (Flak) enemigos
    if (!enemyBullets_.empty()) {
        std::vector<float> ebVerts;
        std::vector<unsigned char> ebColors;
        for (const auto& b : enemyBullets_) {
            ebVerts.push_back(b.x);
            ebVerts.push_back(b.y);
            ebVerts.push_back(b.z);

            ebVerts.push_back(b.x);
            ebVerts.push_back(b.y);
            ebVerts.push_back(b.z - 3.2f);

            ebColors.insert(ebColors.end(), {
                255, 60,  60, 255,
                255, 140, 30, 200
            });
        }
        w3dEngine::Disable(w3dEngine::Texture2D);
        w3dEngine::Enable(w3dEngine::ColorMaterial);
        w3dEngine::LineWidth(4.2f);
        w3dEngine::EnableArray(w3dEngine::VertexArray);
        w3dEngine::EnableArray(w3dEngine::ColorArray);
        w3dEngine::VertexPointer3f(0, ebVerts.data());
        w3dEngine::ColorPointer4ub(ebColors.data());
        w3dEngine::DrawLines(static_cast<int>(enemyBullets_.size() * 2));
        w3dEngine::DisableArray(w3dEngine::ColorArray);
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
        0.0f, 1.0f,
        1.0f, 1.0f,
        1.0f, 0.0f,

        0.0f, 1.0f,
        1.0f, 0.0f,
        0.0f, 0.0f
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
    float bgVerts[] = {
        x,     y,         x + w, y,         x + w, y + h,
        x,     y,         x + w, y + h,     x,     y + h
    };
    w3dEngine::Disable(w3dEngine::Texture2D);
    w3dEngine::Color4f(0.05f, 0.12f, 0.20f, 0.75f);
    w3dEngine::EnableArray(w3dEngine::VertexArray);
    w3dEngine::VertexPointer2f(0, bgVerts);
    w3dEngine::DrawTrianglesArray(6);

    float fillW = w * CLAMP(fillPct, 0.0f, 1.0f);
    float fgVerts[] = {
        x,         y,         x + fillW, y,         x + fillW, y + h,
        x,         y,         x + fillW, y + h,     x,         y + h
    };
    w3dEngine::Color4f(r, g, b, a);
    w3dEngine::VertexPointer2f(0, fgVerts);
    w3dEngine::DrawTrianglesArray(6);
}

void Renderer::renderHUDRect(float x, float y, float w, float h, float r, float g, float b, float a) {
    float rVerts[] = {
        x,     y,         x + w, y,         x + w, y + h,
        x,     y,         x + w, y + h,     x,     y + h
    };
    w3dEngine::Disable(w3dEngine::Texture2D);
    w3dEngine::Color4f(r, g, b, a);
    w3dEngine::EnableArray(w3dEngine::VertexArray);
    w3dEngine::VertexPointer2f(0, rVerts);
    w3dEngine::DrawTrianglesArray(6);
}

void Renderer::renderHUDLine(float x0, float y0, float x1, float y1, float r, float g, float b, float a, float width) {
    float lVerts[] = { x0, y0, x1, y1 };
    w3dEngine::Disable(w3dEngine::Texture2D);
    w3dEngine::LineWidth(width);
    w3dEngine::Color4f(r, g, b, a);
    w3dEngine::EnableArray(w3dEngine::VertexArray);
    w3dEngine::VertexPointer2f(0, lVerts);
    w3dEngine::DrawLines(2);
}

void Renderer::renderDigits(float x, float y, float charW, float charH, const std::string& text) {
    if (!texHudDigits_) return;
    static const std::string charset = "0123456789:/-XKTSP";
    float totalChars = static_cast<float>(charset.size());

    w3dEngine::Enable(w3dEngine::Texture2D);
    w3dEngine::BindTexture(texHudDigits_->getTextureID());
    w3dEngine::Color4f(1.0f, 1.0f, 1.0f, 1.0f);
    w3dEngine::EnableArray(w3dEngine::VertexArray);
    w3dEngine::EnableArray(w3dEngine::TexCoordArray);

    float curX = x;
    for (char c : text) {
        if (c == ' ') {
            curX += charW * 0.6f;
            continue;
        }
        auto pos = charset.find(c);
        if (pos == std::string::npos) {
            curX += charW;
            continue;
        }
        float u0 = static_cast<float>(pos) / totalChars;
        float u1 = static_cast<float>(pos + 1) / totalChars;

        float qVerts[] = {
            curX,         y,
            curX + charW, y,
            curX + charW, y + charH,

            curX,         y,
            curX + charW, y + charH,
            curX,         y + charH
        };
        float qUVs[] = {
            u0, 1.0f,
            u1, 1.0f,
            u1, 0.0f,

            u0, 1.0f,
            u1, 0.0f,
            u0, 0.0f
        };

        w3dEngine::VertexPointer2f(0, qVerts);
        w3dEngine::TexCoordPointer2f(0, qUVs);
        w3dEngine::DrawTrianglesArray(6);

        curX += charW * 0.85f;
    }
    w3dEngine::DisableArray(w3dEngine::TexCoordArray);
}

void Renderer::renderMainMenuUI() {
    w3dEngine::Disable(w3dEngine::DepthTest);
    w3dEngine::Enable(w3dEngine::Blend);

    w3dEngine::MatrixMode(w3dEngine::Projection);
    w3dEngine::LoadIdentity();
    w3dEngine::Ortho(0.0f, (float)width_, (float)height_, 0.0f, -1.0f, 1.0f);

    w3dEngine::MatrixMode(w3dEngine::ModelView);
    w3dEngine::LoadIdentity();

    // Sombra cinemática oscura
    renderHUDRect(0.0f, 0.0f, (float)width_, (float)height_, 0.02f, 0.06f, 0.12f, 0.40f);

    // Banner del Título
    if (texMenuTitle_) {
        float titleW = 440.0f;
        float titleH = 138.0f;
        float tx = (width_ - titleW) * 0.5f;
        float ty = 35.0f;
        renderHUDQuad(tx, ty, titleW, titleH, texMenuTitle_->getTextureID(), 1.0f);
    }

    // Botón JUGAR / DESPEGAR
    if (texBtnPlay_) {
        float pulse = 0.90f + 0.10f * std::sin(timeSec_ * 5.0f);
        float btnW = 340.0f * (0.98f + 0.02f * pulse);
        float btnH = 78.0f * (0.98f + 0.02f * pulse);
        float bx = (width_ - btnW) * 0.5f;
        float by = height_ * 0.52f - 20.0f;
        renderHUDQuad(bx, by, btnW, btnH, texBtnPlay_->getTextureID(), 1.0f);
    }

    // Botón AYUDA (?)
    if (texBtnHelp_) {
        float hSize = 58.0f;
        float hx = (width_ + 340.0f) * 0.5f + 15.0f;
        float hy = height_ * 0.52f - 10.0f;
        renderHUDQuad(hx, hy, hSize, hSize, texBtnHelp_->getTextureID(), 0.95f);
    }

    // Botón Sonido
    if (texBtnSound_) {
        float sndX = width_ - 70.0f;
        float sndY = 20.0f;
        float sndSize = 52.0f;
        renderHUDQuad(sndX, sndY, sndSize, sndSize, texBtnSound_->getTextureID(), soundEnabled_ ? 0.95f : 0.45f);
    }

    // Puntuación récord
    if (highScore_ > 0) {
        float recordW = 280.0f;
        float rx = (width_ - recordW) * 0.5f;
        float ry = height_ - 55.0f;
        renderHUDRect(rx, ry, recordW, 36.0f, 0.05f, 0.15f, 0.25f, 0.75f);
        renderHUDLine(rx, ry, rx + recordW, ry, 0.0f, 0.85f, 0.95f, 0.9f, 2.0f);
        std::string recStr = "RECORD X " + std::to_string(highScore_);
        renderDigits(rx + 25.0f, ry + 6.0f, 18.0f, 24.0f, recStr);
    }

    // Diálogo Modal de Ayuda
    if (showHelpModal_ && texDialogHelp_) {
        renderHUDRect(0.0f, 0.0f, (float)width_, (float)height_, 0.0f, 0.0f, 0.0f, 0.75f);
        float dlgW = 580.0f;
        float dlgH = 400.0f;
        float dx = (width_ - dlgW) * 0.5f;
        float dy = (height_ - dlgH) * 0.5f;
        renderHUDQuad(dx, dy, dlgW, dlgH, texDialogHelp_->getTextureID(), 1.0f);
    }
}

void Renderer::renderPauseUI() {
    w3dEngine::Disable(w3dEngine::DepthTest);
    w3dEngine::Enable(w3dEngine::Blend);

    w3dEngine::MatrixMode(w3dEngine::Projection);
    w3dEngine::LoadIdentity();
    w3dEngine::Ortho(0.0f, (float)width_, (float)height_, 0.0f, -1.0f, 1.0f);

    w3dEngine::MatrixMode(w3dEngine::ModelView);
    w3dEngine::LoadIdentity();

    // Fondo oscuro translúcido
    renderHUDRect(0.0f, 0.0f, (float)width_, (float)height_, 0.02f, 0.05f, 0.10f, 0.70f);

    float dlgW = 380.0f;
    float dlgH = 320.0f;
    float dx = (width_ - dlgW) * 0.5f;
    float dy = (height_ - dlgH) * 0.5f;

    if (texDialogPause_) {
        renderHUDQuad(dx, dy, dlgW, dlgH, texDialogPause_->getTextureID(), 1.0f);
    }

    float btnW = 270.0f, btnH = 55.0f;
    float bx = (width_ - btnW) * 0.5f;

    // Continuar
    if (texBtnResume_) {
        float by1 = dy + 105.0f;
        renderHUDQuad(bx, by1, btnW, btnH, texBtnResume_->getTextureID(), 0.95f);
    }

    // Reiniciar
    if (texBtnRestart_) {
        float by2 = dy + 172.0f;
        renderHUDQuad(bx, by2, btnW, btnH, texBtnRestart_->getTextureID(), 0.95f);
    }

    // Menú Principal
    if (texBtnQuit_) {
        float by3 = dy + 238.0f;
        renderHUDQuad(bx, by3, btnW, btnH, texBtnQuit_->getTextureID(), 0.95f);
    }

    // Botón de sonido
    if (texBtnSound_) {
        float sndX = width_ - 70.0f;
        float sndY = 20.0f;
        float sndSize = 52.0f;
        renderHUDQuad(sndX, sndY, sndSize, sndSize, texBtnSound_->getTextureID(), soundEnabled_ ? 0.95f : 0.45f);
    }
}

void Renderer::renderGameOverUI() {
    w3dEngine::Disable(w3dEngine::DepthTest);
    w3dEngine::Enable(w3dEngine::Blend);

    w3dEngine::MatrixMode(w3dEngine::Projection);
    w3dEngine::LoadIdentity();
    w3dEngine::Ortho(0.0f, (float)width_, (float)height_, 0.0f, -1.0f, 1.0f);

    w3dEngine::MatrixMode(w3dEngine::ModelView);
    w3dEngine::LoadIdentity();

    // Fondo de alerta táctica rojizo
    renderHUDRect(0.0f, 0.0f, (float)width_, (float)height_, 0.14f, 0.02f, 0.03f, 0.75f);

    float dlgW = 440.0f;
    float dlgH = 340.0f;
    float dx = (width_ - dlgW) * 0.5f;
    float dy = (height_ - dlgH) * 0.5f;

    if (texDialogGameOver_) {
        renderHUDQuad(dx, dy, dlgW, dlgH, texDialogGameOver_->getTextureID(), 1.0f);
    }

    // Estadísticas
    float statY = dy + 105.0f;
    std::string killStr = "X " + std::to_string(enemiesDestroyed_);
    renderDigits(dx + 70.0f, statY, 18.0f, 26.0f, killStr);

    int totalScore = enemiesDestroyed_ * 1500;
    std::string scoreStr = "P " + std::to_string(totalScore);
    renderDigits(dx + 230.0f, statY, 18.0f, 26.0f, scoreStr);

    float btnW = 270.0f, btnH = 55.0f;
    float bx = (width_ - btnW) * 0.5f;

    // Reintentar
    if (texBtnRestart_) {
        float by1 = dy + 185.0f;
        renderHUDQuad(bx, by1, btnW, btnH, texBtnRestart_->getTextureID(), 0.95f);
    }

    // Menú Principal
    if (texBtnQuit_) {
        float by2 = dy + 252.0f;
        renderHUDQuad(bx, by2, btnW, btnH, texBtnQuit_->getTextureID(), 0.95f);
    }
}

void Renderer::renderGameUI() {
    w3dEngine::Disable(w3dEngine::DepthTest);
    w3dEngine::Enable(w3dEngine::Blend);

    w3dEngine::MatrixMode(w3dEngine::Projection);
    w3dEngine::LoadIdentity();
    w3dEngine::Ortho(0.0f, (float)width_, (float)height_, 0.0f, -1.0f, 1.0f);

    w3dEngine::MatrixMode(w3dEngine::ModelView);
    w3dEngine::LoadIdentity();

    // 1. Retícula Táctica Central
    if (texHudCrosshair_) {
        float reticleSize = 140.0f;
        float rx = (width_ - reticleSize) * 0.5f;
        float ry = (height_ - reticleSize) * 0.5f - 20.0f;
        renderHUDQuad(rx, ry, reticleSize, reticleSize, texHudCrosshair_->getTextureID(), targetLocked_ ? 1.0f : 0.85f);

        if (targetLocked_) {
            float bSize = 18.0f;
            float pad = 10.0f;
            float bx0 = rx - pad;
            float by0 = ry - pad;
            float bx1 = rx + reticleSize + pad;
            float by1 = ry + reticleSize + pad;
            float r = 1.0f, g = 0.25f, b = 0.15f, a = 0.95f;

            renderHUDLine(bx0, by0, bx0 + bSize, by0, r, g, b, a, 2.5f);
            renderHUDLine(bx0, by0, bx0, by0 + bSize, r, g, b, a, 2.5f);
            renderHUDLine(bx1, by0, bx1 - bSize, by0, r, g, b, a, 2.5f);
            renderHUDLine(bx1, by0, bx1, by0 + bSize, r, g, b, a, 2.5f);
            renderHUDLine(bx0, by1, bx0 + bSize, by1, r, g, b, a, 2.5f);
            renderHUDLine(bx0, by1, bx0, by1 - bSize, r, g, b, a, 2.5f);
            renderHUDLine(bx1, by1, bx1 - bSize, by1, r, g, b, a, 2.5f);
            renderHUDLine(bx1, by1, bx1, by1 - bSize, r, g, b, a, 2.5f);
        }
    }

    // 2. Radar Táctico
    if (texHudRadar_) {
        float radarSize = 120.0f;
        float rx = 25.0f;
        float ry = 55.0f;
        renderHUDQuad(rx, ry, radarSize, radarSize, texHudRadar_->getTextureID(), 0.88f);

        float rcX = rx + radarSize * 0.5f;
        float rcY = ry + radarSize * 0.5f;
        float radarRadius = radarSize * 0.44f;

        float sweepAngle = timeSec_ * 3.5f;
        float swX = rcX + std::cos(sweepAngle) * radarRadius;
        float swY = rcY + std::sin(sweepAngle) * radarRadius;
        renderHUDLine(rcX, rcY, swX, swY, 0.1f, 1.0f, 0.5f, 0.55f, 1.8f);

        // Blip del jugador
        renderHUDRect(rcX - 3.5f, rcY - 3.5f, 7.0f, 7.0f, 0.0f, 1.0f, 0.45f, 1.0f);

        // Blip del buque
        float relX = (targetX_ - planeX_) / 60.0f;
        float relZ = (targetZ_ - (-6.5f)) / 140.0f;
        float dist = std::sqrt(relX * relX + relZ * relZ);
        if (dist > 1.0f) {
            relX /= dist;
            relZ /= dist;
        }
        float blipX = rcX + relX * radarRadius;
        float blipY = rcY + relZ * radarRadius;

        float blipPulse = 0.65f + 0.35f * std::sin(timeSec_ * 10.0f);
        float blipSize = 7.0f * (0.85f + 0.25f * blipPulse);
        renderHUDRect(blipX - blipSize * 0.5f, blipY - blipSize * 0.5f, blipSize, blipSize, 1.0f, 0.2f, 0.15f, blipPulse);

        if (targetLocked_) {
            renderHUDLine(blipX - 6.0f, blipY - 6.0f, blipX + 6.0f, blipY - 6.0f, 1.0f, 0.9f, 0.1f, 0.9f, 1.5f);
            renderHUDLine(blipX + 6.0f, blipY - 6.0f, blipX + 6.0f, blipY + 6.0f, 1.0f, 0.9f, 0.1f, 0.9f, 1.5f);
            renderHUDLine(blipX + 6.0f, blipY + 6.0f, blipX - 6.0f, blipY + 6.0f, 1.0f, 0.9f, 0.1f, 0.9f, 1.5f);
            renderHUDLine(blipX - 6.0f, blipY + 6.0f, blipX - 6.0f, blipY - 6.0f, 1.0f, 0.9f, 0.1f, 0.9f, 1.5f);
        }
    }

    // 3. Stick Virtual Flotante
    if (texBtnStick_) {
        float baseCenterX = stickActive_ ? stickOriginX_ : 120.0f;
        float baseCenterY = stickActive_ ? stickOriginY_ : ((height_ > 0) ? (height_ - 130.0f) : 400.0f);

        renderHUDQuad(baseCenterX - 55.0f, baseCenterY - 55.0f, 110.0f, 110.0f,
                      texBtnStick_->getTextureID(), stickActive_ ? 0.92f : 0.50f);

        float knobX = baseCenterX - 28.0f + (stickDeflectX_ * 32.0f);
        float knobY = baseCenterY - 28.0f + (stickDeflectY_ * 32.0f);
        renderHUDQuad(knobX, knobY, 56.0f, 56.0f,
                      texBtnStick_->getTextureID(), stickActive_ ? 1.0f : 0.70f);
    }

    // 4. Botones Tácticos de Armamento
    if (texBtnFire_) {
        float fireW = 115.0f;
        float fx = width_ - 145.0f;
        float fy = height_ - 145.0f;
        renderHUDQuad(fx, fy, fireW, fireW, texBtnFire_->getTextureID(), firePressed_ ? 1.0f : 0.85f);
    }

    if (texBtnMissile_) {
        float mslW = 105.0f;
        float mx = width_ - 140.0f;
        float my = height_ - 265.0f;
        float alpha = (missileFlightTime_ > 0.0f || missilePressed_) ? 1.0f : 0.85f;
        renderHUDQuad(mx, my, mslW, mslW, texBtnMissile_->getTextureID(), alpha);

        float pipGap = 5.0f;
        float pipW = (mslW - (pipGap * 3.0f)) / 4.0f;
        float pipH = 7.0f;
        float pipY = my + mslW + 4.0f;

        for (int i = 0; i < 4; ++i) {
            float px = mx + i * (pipW + pipGap);
            if (i < missileCount_) {
                renderHUDRect(px, pipY, pipW, pipH, 0.1f, 0.95f, 0.35f, 0.95f);
            } else {
                renderHUDRect(px, pipY, pipW, pipH, 0.25f, 0.3f, 0.35f, 0.5f);
            }
        }
    }

    // 5. Barras de Telemetría HUD
    float speedPct = CLAMP((speedKnots_ - 380.0f) / 200.0f, 0.0f, 1.0f);
    renderHUDBar(155.0f, 65.0f, 110.0f, 12.0f, speedPct, 0.0f, 0.9f, 0.8f, 0.9f);

    float altPct = CLAMP((altitudeFeet_ - 1500.0f) / 1800.0f, 0.0f, 1.0f);
    renderHUDBar(155.0f, 85.0f, 110.0f, 12.0f, altPct, 0.0f, 0.85f, 1.0f, 0.9f);

    // Barra de Integridad del Avión
    float hullBarW = 200.0f;
    renderHUDBar((width_ - hullBarW) * 0.5f, height_ - 30.0f, hullBarW, 10.0f, healthPct_, 0.2f, 0.95f, 0.3f, 0.85f);

    // Vida del Buque Enemigo
    float bannerW = 240.0f;
    float bannerX = (width_ - bannerW) * 0.5f;
    float bannerY = 35.0f;
    if (targetLocked_) {
        float tgtPct = CLAMP(targetHealth_ / targetMaxHealth_, 0.0f, 1.0f);
        renderHUDBar(bannerX, bannerY, bannerW, 14.0f, tgtPct, 0.95f, 0.15f, 0.15f, 0.92f);
    } else {
        float scanPct = 0.5f + 0.45f * std::sin(timeSec_ * 4.0f);
        renderHUDBar(bannerX, bannerY, bannerW, 14.0f, scanPct, 0.95f, 0.75f, 0.15f, 0.75f);
    }

    // 6. Botones de Pausa y Sonido (Esquina Superior Derecha)
    if (texBtnPause_) {
        float pauseX = width_ - 130.0f;
        float pauseY = 20.0f;
        float pauseSize = 52.0f;
        renderHUDQuad(pauseX, pauseY, pauseSize, pauseSize, texBtnPause_->getTextureID(), 0.90f);
    }

    if (texBtnSound_) {
        float sndX = width_ - 70.0f;
        float sndY = 20.0f;
        float sndSize = 52.0f;
        renderHUDQuad(sndX, sndY, sndSize, sndSize, texBtnSound_->getTextureID(), soundEnabled_ ? 0.95f : 0.40f);
        if (soundEnabled_) {
            renderHUDBar(sndX + 4.0f, sndY + sndSize + 2.0f, sndSize - 8.0f, 4.0f, 1.0f, 0.1f, 0.95f, 0.4f, 0.9f);
        } else {
            renderHUDBar(sndX + 4.0f, sndY + sndSize + 2.0f, sndSize - 8.0f, 4.0f, 1.0f, 0.95f, 0.2f, 0.2f, 0.9f);
        }
    }

    // 7. Contadores Digitales en HUD
    std::string killStr = "X " + std::to_string(enemiesDestroyed_);
    renderDigits(155.0f, 38.0f, 14.0f, 20.0f, killStr);

    int totalScore = enemiesDestroyed_ * 1500;
    std::string scoreStr = "P " + std::to_string(totalScore);
    renderDigits(width_ - 260.0f, 26.0f, 14.0f, 20.0f, scoreStr);
}

void Renderer::renderWhisk3D() {
    float dt = 0.01667f;

    // -------------------------------------------------------------
    // ACTUALIZACIÓN DE ESTADO DEL JUEGO
    // -------------------------------------------------------------
    if (gameState_ == STATE_MAIN_MENU) {
        timeSec_ += dt * 0.7f;
        // Piloto automático cinemático suave sobre el océano
        targetRoll_  = std::sin(timeSec_ * 0.8f) * 14.0f;
        targetPitch_ = std::cos(timeSec_ * 0.6f) * 6.0f;
        targetYaw_   = targetRoll_ * 0.3f;
        planeX_ = std::sin(timeSec_ * 0.4f) * 2.2f;
        planeY_ = 0.4f + std::cos(timeSec_ * 0.5f) * 0.6f;

        planeRoll_  += (targetRoll_ - planeRoll_) * 0.10f;
        planePitch_ += (targetPitch_ - planePitch_) * 0.10f;
        planeYaw_   += (targetYaw_ - planeYaw_) * 0.10f;
    }
    else if (gameState_ == STATE_PLAYING) {
        timeSec_ += dt;

        if (stickActive_) {
            targetRoll_  = stickDeflectX_ * 42.0f;
            targetPitch_ = -stickDeflectY_ * 26.0f;
            targetYaw_   = stickDeflectX_ * 16.0f;

            planeX_ += stickDeflectX_ * 0.14f;
            planeY_ += (-stickDeflectY_) * 0.11f;
        } else {
            targetRoll_  = std::sin(timeSec_ * 1.5f) * 1.2f;
            targetPitch_ = std::cos(timeSec_ * 1.0f) * 0.6f;
            targetYaw_   = 0.0f;
        }

        planeRoll_  += (targetRoll_ - planeRoll_) * 0.18f;
        planePitch_ += (targetPitch_ - planePitch_) * 0.18f;
        planeYaw_   += (targetYaw_ - planeYaw_) * 0.18f;

        planeX_ = CLAMP(planeX_, -5.5f, 5.5f);
        planeY_ = CLAMP(planeY_, -1.8f, 3.2f);

        altitudeFeet_ = 2400.0f + (planeY_ * 300.0f);
        speedKnots_   = 480.0f - (planePitch_ * 2.2f);

        if (missileCount_ < 4 && std::fmod(timeSec_, 8.0f) < 0.02f) {
            missileCount_++;
        }

        // Avance del buque enemigo
        targetZ_ += 0.26f;
        if (targetZ_ > 8.0f) {
            targetZ_ = -140.0f;
            targetX_ = std::sin(timeSec_ * 0.6f) * 20.0f;
            targetHealth_ = targetMaxHealth_;
        }

        // Enganche táctico
        float relTargetX = targetX_ - planeX_;
        float relTargetY = (targetY_ + 1.2f) - (planeY_ - 0.5f);
        float relTargetZ = targetZ_ - (-6.5f);

        bool nowLocked = (relTargetZ < -10.0f && relTargetZ > -110.0f &&
                          std::fabs(relTargetX) < 7.5f && std::fabs(relTargetY) < 5.5f);
        if (nowLocked && !prevTargetLocked_ && sndLock_) {
            w3dEngine::W3dSoundPlay(sndLock_, 0.65f, false);
        }
        prevTargetLocked_ = nowLocked;
        targetLocked_ = nowLocked;

        // Disparo continuo de cañón
        if (firePressed_) {
            cannonCooldown_ -= dt;
            if (cannonCooldown_ <= 0.0f) {
                cannonCooldown_ = 0.085f;
                muzzleFlashTime_ = 0.08f;
                if (sndCannon_) {
                    w3dEngine::W3dSoundPlayPitch(sndCannon_, 0.55f, false, 0.94f + (rand() % 12) * 0.01f);
                }
                Bullet b1;
                b1.x = planeX_ - 0.9f;
                b1.y = planeY_ - 0.3f;
                b1.z = -7.2f;
                b1.vx = (planeRoll_ * 0.06f);
                b1.vy = (planePitch_ * 0.06f);
                b1.vz = -180.0f;
                b1.life = 0.85f;
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

        // Balas del jugador
        for (auto it = bullets_.begin(); it != bullets_.end(); ) {
            it->x += it->vx * dt;
            it->y += it->vy * dt;
            it->z += it->vz * dt;
            it->life -= dt;

            bool hit = (it->z <= targetZ_ + 9.0f && it->z >= targetZ_ - 11.0f &&
                        it->x >= targetX_ - 3.5f && it->x <= targetX_ + 3.5f &&
                        it->y >= targetY_ - 1.2f && it->y <= targetY_ + 4.5f);

            if (hit) {
                targetHealth_ -= 4.0f;
                targetHitFlashTime_ = 0.12f;
                it = bullets_.erase(it);
                if (targetHealth_ <= 0.0f) {
                    enemiesDestroyed_++;
                    targetHealth_ = targetMaxHealth_;
                    targetZ_ = -140.0f;
                    targetX_ = (rand() % 36 - 18) * 1.0f;
                    if (sndExplosion_) {
                        w3dEngine::W3dSoundPlay(sndExplosion_, 0.95f, false);
                    }
                }
            } else if (it->life <= 0.0f || it->z < -260.0f) {
                it = bullets_.erase(it);
            } else {
                ++it;
            }
        }

        // Misil guiado
        if (missileFlightTime_ > 0.0f) {
            missileFlightTime_ -= dt;
            if (missileFlightTime_ <= 0.0f) {
                missileFlightTime_ = 0.0f;
                enemiesDestroyed_++;
                targetHealth_ = targetMaxHealth_;
                targetZ_ = -140.0f;
                targetX_ = (rand() % 36 - 18) * 1.0f;
                targetHitFlashTime_ = 0.45f;
                muzzleFlashTime_ = 0.3f;
                if (sndExplosion_) {
                    w3dEngine::W3dSoundPlay(sndExplosion_, 0.95f, false);
                }
            }
        }

        // Fuego antiaéreo de respuesta del buque enemigo (Flak)
        flakCooldown_ -= dt;
        if (flakCooldown_ <= 0.0f) {
            flakCooldown_ = 2.2f + (rand() % 10) * 0.1f;
            if (targetZ_ < -25.0f && targetZ_ > -130.0f) {
                EnemyBullet eb;
                eb.x = targetX_ + (rand() % 4 - 2) * 0.6f;
                eb.y = targetY_ + 2.8f;
                eb.z = targetZ_ + 4.0f;
                eb.vx = (planeX_ - eb.x) * 0.32f;
                eb.vy = (planeY_ - eb.y) * 0.32f;
                eb.vz = 55.0f; // Vuela hacia el jugador
                eb.life = 3.2f;
                enemyBullets_.push_back(eb);
            }
        }

        // Actualizar proyectiles antiaéreos enemigos
        for (auto it = enemyBullets_.begin(); it != enemyBullets_.end(); ) {
            it->x += it->vx * dt;
            it->y += it->vy * dt;
            it->z += it->vz * dt;
            it->life -= dt;

            float dx = it->x - planeX_;
            float dy = it->y - (planeY_ - 0.5f);
            float dz = it->z - (-6.5f);
            float dist = std::sqrt(dx*dx + dy*dy + dz*dz);

            if (dist < 1.6f) {
                healthPct_ -= 0.12f;
                it = enemyBullets_.erase(it);
                if (healthPct_ <= 0.0f) {
                    healthPct_ = 0.0f;
                    gameState_ = STATE_GAME_OVER;
                    if (enemiesDestroyed_ > highScore_) {
                        highScore_ = enemiesDestroyed_;
                    }
                    if (sndExplosion_) {
                        w3dEngine::W3dSoundPlay(sndExplosion_, 1.0f, false);
                    }
                    break;
                }
            } else if (it->life <= 0.0f || it->z > 10.0f) {
                it = enemyBullets_.erase(it);
            } else {
                ++it;
            }
        }

        W3dFisicaPaso(1.0f / 60.0f);
    }
    // En STATE_PAUSED no se avanzan timers ni física

    // -------------------------------------------------------------
    // RENDERIZADO DE LA ESCENA 3D
    // -------------------------------------------------------------
    w3dEngine::Viewport(0, 0, width_, height_);

    float aspect = (height_ > 0) ? static_cast<float>(width_) / static_cast<float>(height_) : 1.0f;
    w3dEngine::MatrixMode(w3dEngine::Projection);
    w3dEngine::LoadIdentity();
    w3dEngine::Perspective(55.0f, aspect, 0.1f, 500.0f);

    float camLagX = planeX_ * 0.35f;
    float camLagY = planeY_ * 0.22f;
    float camBank = planeRoll_ * 0.12f;

    w3dEngine::MatrixMode(w3dEngine::ModelView);
    w3dEngine::LoadIdentity();
    w3dEngine::Rotatef(camBank, 0.0f, 0.0f, 1.0f);
    w3dEngine::Translatef(-camLagX, -0.9f - camLagY, -2.0f);

    w3dEngine::Enable(w3dEngine::DepthTest);
    w3dEngine::DepthFunc(w3dEngine::DepthLEqual);
    w3dEngine::Enable(w3dEngine::CullFace);

    renderSkyAndOcean();
    renderIslands();
    renderTarget();
    renderAircraft();
    renderProjectiles();

    // -------------------------------------------------------------
    // RENDERIZADO DE LA INTERFAZ DE USUARIO (2D UI)
    // -------------------------------------------------------------
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

    w3dEngine::ClearColor(0.06f, 0.12f, 0.22f, 1.0f);
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
