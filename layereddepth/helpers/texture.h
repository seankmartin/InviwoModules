#ifndef LAYER_TEXTURE_H
#define LAYER_TEXTURE_H

#include <modules/layereddepth/layereddepthmoduledefine.h>
#include <inviwo/core/common/inviwo.h>
#include <modules/opengl/texture/textureunit.h>
#include <modules/opengl/inviwoopengl.h>
#include <string>
#include <memory>

//REFERENCE: Largely inspried by Dr Anton Geraldans book - Anton's OpenGL 4 Tutorials
namespace scene {
  class IVW_MODULE_LAYEREDDEPTH_API Texture {
    GLuint texture_id_;
    inviwo::TextureUnit textureUnit_;
    std::unique_ptr<char[]> filename_;
    GLenum type_;
    GLenum slot_ = GL_TEXTURE0;

  public:
    void Bind();
    void Load(const char* filename);
    void LoadNoMip(const char* filename);
    //Loads the tex image in filename, storing the dimensions in x and y
    unsigned char* LoadTexImage(const char* filename, int* x, int* y, bool flip);
    void CreateCubeMap(const char* front,
        const char* back,
        const char* top,
        const char* bottom,
        const char* left,
        const char* right);
    bool LoadCubeMapSide(GLenum side_target, const char* filename);
    Texture();
    Texture(const char* filename);
    ~Texture();
    //Loads an empty texture of a certain size and of a certain type
    Texture(const GLuint& size_x, const GLuint& size_y, const GLenum& type);
    inline const char* GetFileName() const { return filename_.get(); }
    inline void SetSlot(const GLenum& slot) { slot_ = slot; }
    inline void SetType(const GLenum& type) { type_ = type; }
    inline GLuint& GetChangeableID() { return texture_id_; }
    inline const GLuint& GetID() const { return texture_id_; }
    inline const GLenum& GetSlot() const { return slot_; }
  };

  class IVW_MODULE_LAYEREDDEPTH_API DepthTexture : public Texture {
  public:
    inline DepthTexture() {}
    DepthTexture(const GLuint& size_x, const GLuint& size_y, const GLenum& type);
  };
} //namespace scene

#endif