#define STB_IMAGE_IMPLEMENTATION
#include <modules/layereddepth/helpers/stb_image.h>
#include <modules/layereddepth/helpers/texture.h>
#include <math.h>
#include <iostream>
#include <stddef.h>
#include <string.h>
#include <cstring>

//REFERENCE: Largely inspried by Dr Anton Geraldans book - Anton's OpenGL 4 Tutorials
namespace scene {
  void Texture::Bind()
  {
    textureUnit_.activate();
    glBindTexture(type_, texture_id_);
  }

  //REFERENCE mipmap generation is from https://www.khronos.org/opengl/wiki/Common_Mistakes#Automatic_mipmap_generation
  void Texture::Load(const char* filename)
  {
    int x, y;
    filename_ = std::make_unique<char[]>(std::strlen(filename) + 1);
    std::strncpy(filename_.get(), filename, std::strlen(filename) + 1);
    // if (copy_error) fprintf(stderr, "error copying %s with errno %d", filename, copy_error);
    type_ = GL_TEXTURE_2D;
    unsigned char *image_data = LoadTexImage(filename, &x, &y, true);
    //Copy image data into the openGL texture
    glGenTextures(1, &texture_id_);
    glActiveTexture(slot_);
    glBindTexture(GL_TEXTURE_2D, texture_id_);
    std::cout << "Texture " << filename << " has " << log2(x) + 1 << " layers" << std::endl;
    glTexStorage2D(GL_TEXTURE_2D, (int)log2(x) + 1, GL_RGBA8, x, y);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, x, y, GL_RGBA, GL_UNSIGNED_BYTE, image_data);
    glGenerateMipmap(GL_TEXTURE_2D);
    //glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, x, y, 0, GL_RGBA, GL_UNSIGNED_BYTE, image_data);
    
    //Optional LOD bias
    //glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_LOD_BIAS, 3.0f);

    //Good for anti-aliasing and safe wrapping
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    stbi_image_free(image_data);
  }

  void Texture::LoadNoMip(const char* filename)
  {
    int x, y;
    filename_ = std::make_unique<char[]>(std::strlen(filename) + 1);
    std::strncpy(filename_.get(), filename, std::strlen(filename) + 1);
    //if (copy_error) fprintf(stderr, "error copying %s with errno %d", filename, copy_error);
    type_ = GL_TEXTURE_2D;
    unsigned char *image_data = LoadTexImage(filename, &x, &y, true);
    std::cout << "Image data " << int(image_data[x / 2 + y * (x / 2)] + 1) << std::endl; 
    //Copy image data into the openGL texture
    glGenTextures(1, &texture_id_);
    glActiveTexture(slot_);
    glBindTexture(GL_TEXTURE_2D, texture_id_);
    //TODO, think of what will happen if we are reading a RGB image
    std::cout << "Texture " << filename << " has " << log2(x) + 1 << " layers" << std::endl;
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, x, y, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, image_data);
    //(GL_TEXTURE_2D, 1, GL_RGBA8, x, y);
    //glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, x, y, GL_RGBA, GL_UNSIGNED_BYTE, image_data);
    //glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, x, y, 0, GL_RGBA, GL_UNSIGNED_BYTE, image_data);
    //Good for anti-aliasing and safe wrapping
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    stbi_image_free(image_data);
  }

  unsigned char* Texture::LoadTexImage(const char * filename, int* x, int* y, bool flip)
  {
      int n; //x is width y is height
      int force_channels = 4;
      unsigned char* image_data = stbi_load(filename, x, y, &n, force_channels);
      if (!image_data) {
          fprintf(stderr, "Error: could not load texture image in %s\n", filename);
          exit(-1);
      }
      //Check if the image is has dimensions which are a power of two
      if (((*x & (*x - 1)) != 0) || (*y & (*y - 1)) != 0) {
          fprintf(stderr, "WARNING: texture %s is not power of 2 dimensions\n", filename);
      }
      //Flip the image - openGL expects 0 on the Y-axis to be at the bottom of the texture
      //Do this by swapping the top and bottom halves
      int width_in_bytes = *x * 4;
      unsigned char* top = NULL;
      unsigned char* bottom = NULL;
      unsigned char temp = 0;
      int half_height = *y / 2;

      if (!flip) return image_data;

      for (int row = 0; row < half_height; ++row) {
          top = image_data + row * width_in_bytes;
          bottom = image_data + (*y - row - 1) * width_in_bytes;
          for (int col = 0; col < width_in_bytes; ++col) {
              temp = *top;
              *top = *bottom;
              *bottom = temp;
              top++;
              bottom++;
          }
      }
      return image_data;
  }

  void Texture::CreateCubeMap(const char * front, const char * back, const char * top, const char * bottom, const char * left, const char * right)
  {
      type_ = GL_TEXTURE_CUBE_MAP;
      glGenTextures(1, &texture_id_);
      glActiveTexture(GL_TEXTURE0);
      glBindTexture(GL_TEXTURE_CUBE_MAP, texture_id_);
      //Load each cube map side
      assert(LoadCubeMapSide(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, front));
      assert(LoadCubeMapSide(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, back));
      assert(LoadCubeMapSide(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, top));
      assert(LoadCubeMapSide(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, bottom));
      assert(LoadCubeMapSide(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, left));
      assert(LoadCubeMapSide(GL_TEXTURE_CUBE_MAP_POSITIVE_X, right));
      // format cube map texture
      glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  }

  bool Texture::LoadCubeMapSide(GLenum side_target, const char * filename)
  {
      int x, y;
      unsigned char* image_data = LoadTexImage(filename, &x, &y, false);
      glTexImage2D(side_target, 0, GL_RGBA, x, y, 0, GL_RGBA, GL_UNSIGNED_BYTE, image_data);
      stbi_image_free(image_data);
      return true;
  }

  Texture::Texture()
  {
  }

  Texture::Texture(const char* filename)
  {
    Load(filename);
  }

  Texture::Texture(const GLuint& size_x, const GLuint& size_y, const GLenum& type) {
    glGenTextures(1, &texture_id_);
    glBindTexture(GL_TEXTURE_2D, texture_id_);
    type_ = type;
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, size_x, size_y, 0, GL_RGBA, GL_FLOAT, 0);
    //Poor filtering is used here
    //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  }

  Texture::~Texture()
  {
    glDeleteTextures(1, &texture_id_);
  }
  DepthTexture::DepthTexture(const GLuint & size_x, const GLuint & size_y, const GLenum & type)
  {
    glGenTextures(1, &GetChangeableID());
    glBindTexture(GL_TEXTURE_2D, GetID());
    SetType(type);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32, size_x, size_y, 0, GL_DEPTH_COMPONENT, GL_FLOAT, 0);
    //Poor filtering is used here
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE,
      GL_COMPARE_REF_TO_TEXTURE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
  }
} //namespace scene