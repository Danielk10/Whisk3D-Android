# CMake configuration for Whisk3D Android
cmake_minimum_required(VERSION 4.1.2)

project("myapplication")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(WHISK3D_DIR "${CMAKE_CURRENT_SOURCE_DIR}/whisk3d")

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${WHISK3D_DIR}
    ${WHISK3D_DIR}/base
    ${WHISK3D_DIR}/math
    ${WHISK3D_DIR}/gfx
    ${WHISK3D_DIR}/objects
    ${WHISK3D_DIR}/io
    ${WHISK3D_DIR}/animation
    ${WHISK3D_DIR}/physics
    ${WHISK3D_DIR}/audio
    ${WHISK3D_DIR}/thirdparty
)

# Whisk3D Core Sources
set(WHISK3D_SOURCES
    ${WHISK3D_DIR}/math/Matrix4.cpp
    ${WHISK3D_DIR}/math/Vector3.cpp
    ${WHISK3D_DIR}/math/Quaternion.cpp
    ${WHISK3D_DIR}/base/w3dlog.cpp
    ${WHISK3D_DIR}/base/W3dConfig.cpp
    ${WHISK3D_DIR}/base/W3dInteractionState.cpp
    ${WHISK3D_DIR}/base/W3dVibrar.cpp
    ${WHISK3D_DIR}/objects/Objects.cpp
    ${WHISK3D_DIR}/objects/Mesh.cpp
    ${WHISK3D_DIR}/objects/Light.cpp
    ${WHISK3D_DIR}/objects/CameraBase.cpp
    ${WHISK3D_DIR}/objects/Armature.cpp
    ${WHISK3D_DIR}/objects/Materials.cpp
    ${WHISK3D_DIR}/objects/Textures.cpp
    ${WHISK3D_DIR}/objects/MallaDatos.cpp
    ${WHISK3D_DIR}/objects/RenderColors.cpp
    ${WHISK3D_DIR}/objects/TexturaCache.cpp
    ${WHISK3D_DIR}/objects/VisSet.cpp
    ${WHISK3D_DIR}/animation/Animation.cpp
    ${WHISK3D_DIR}/animation/SkeletalAnimation.cpp
    ${WHISK3D_DIR}/animation/Armature2DAnimation.cpp
    ${WHISK3D_DIR}/animation/Flipbook.cpp
    ${WHISK3D_DIR}/animation/VertexAnimation.cpp
    ${WHISK3D_DIR}/gfx/w3dTexture.cpp
    ${WHISK3D_DIR}/gfx/w3dPantalla.cpp
    ${WHISK3D_DIR}/gfx/w3dSafeArea.cpp
    ${WHISK3D_DIR}/gles2/w3dGraphicsGLES2.cpp
    ${WHISK3D_DIR}/io/w3dFilesystem.cpp
    ${WHISK3D_DIR}/io/W3dMalla.cpp
    ${WHISK3D_DIR}/io/W3dPack.cpp
    ${WHISK3D_DIR}/io/w3dCompress.cpp
    ${WHISK3D_DIR}/io/W3dTexto.cpp
    ${WHISK3D_DIR}/objects/MeshRuntime.cpp
    ${WHISK3D_DIR}/io/W3dRecursos.cpp
    ${WHISK3D_DIR}/physics/W3dFisica.cpp
    ${WHISK3D_DIR}/physics/W3dRigido.cpp
)

# Shared game library
add_library(myapplication SHARED
    main.cpp
    AndroidOut.cpp
    Renderer.cpp
    Shader.cpp
    TextureAsset.cpp
    Utility.cpp
    ${WHISK3D_SOURCES}
)

target_compile_definitions(myapplication PRIVATE
    __ANDROID__
    W3D_WEBGL
    W3D_STB_IMPL
)

# Searches for game-activity package
find_package(game-activity REQUIRED CONFIG)

# Keep the JNI entry point for GameActivity
set(CMAKE_SHARED_LINKER_FLAGS
        "${CMAKE_SHARED_LINKER_FLAGS} -u Java_com_google_androidgamesdk_GameActivity_initializeNativeCode")

# Link libraries
target_link_libraries(myapplication
    game-activity::game-activity_static
    EGL
    GLESv2
    GLESv3
    jnigraphics
    android
    log
)
