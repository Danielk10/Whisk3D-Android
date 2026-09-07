#include "TextureAsset.h"
#include "AndroidOut.h"
#include "Utility.h"
#include <cassert>

// Use stb_image for universal Android compatibility (API 23-37+)
#include "thirdparty/stb/stb_image.h"

std::shared_ptr<TextureAsset>
TextureAsset::loadAsset(AAssetManager *assetManager, const std::string &assetPath) {
    if (!assetManager) return nullptr;

    auto pAsset = AAssetManager_open(
            assetManager,
            assetPath.c_str(),
            AASSET_MODE_BUFFER);
    if (!pAsset) {
        aout << "Error: No se pudo abrir asset: " << assetPath << std::endl;
        return nullptr;
    }

    const void *buffer = AAsset_getBuffer(pAsset);
    off_t length = AAsset_getLength(pAsset);

    int width = 0, height = 0, channels = 0;
    stbi_set_flip_vertically_on_load(true);
    stbi_uc *pixels = stbi_load_from_memory(
            static_cast<const stbi_uc*>(buffer),
            static_cast<int>(length),
            &width,
            &height,
            &channels,
            STBI_rgb_alpha);

    AAsset_close(pAsset);

    if (!pixels) {
        aout << "Error al decodificar imagen con stb_image: " << assetPath << std::endl;
        return nullptr;
    }

    // Get an opengl texture
    GLuint textureId = 0;
    glGenTextures(1, &textureId);
    glBindTexture(GL_TEXTURE_2D, textureId);

    // Texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Load the texture into VRAM
    glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_RGBA,
            width,
            height,
            0,
            GL_RGBA,
            GL_UNSIGNED_BYTE,
            pixels
    );
    glGenerateMipmap(GL_TEXTURE_2D);

    stbi_image_free(pixels);

    return std::shared_ptr<TextureAsset>(new TextureAsset(textureId));
}

TextureAsset::~TextureAsset() {
    if (textureID_ != 0) {
        glDeleteTextures(1, &textureID_);
        textureID_ = 0;
    }
}
