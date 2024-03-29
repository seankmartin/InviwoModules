/*********************************************************************************
 *
 * Inviwo - Interactive Visualization Workshop
 *
 * Copyright (c) 2018 Inviwo Foundation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *********************************************************************************/

#include <modules/layereddepth/processors/multipleplaneprocessor.h>
#include <modules/opengl/texture/textureutils.h>
#include <modules/opengl/shader/shaderutils.h>
#include <modules/opengl/rendering/meshdrawergl.h>
#include <inviwo/core/datastructures/geometry/simplemesh.h>

namespace inviwo {

// The Class Identifier has to be globally unique. Use a reverse DNS naming scheme
const ProcessorInfo multipleplaneProcessor::processorInfo_{
    "org.inviwo.multipleplaneProcessor",      // Class identifier
    "Multipleplane Processor",                // Display name
    "Rendering",              // Category
    CodeState::Experimental,  // Code state
    Tags::GL,               // Tags
};
const ProcessorInfo multipleplaneProcessor::getProcessorInfo() const { return processorInfo_; }

multipleplaneProcessor::multipleplaneProcessor()
    : Processor()
    , inport_("inport")
    , outport_("outport")
    , camera_("camera", "Camera", vec3(0.0f, 0.0f, -2.0f), vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 1.0f, 0.0f))
    , trackball_(&camera_)
    , shouldShear_("shouldShear", "Should Use Shear Projection", true)
    , regionSizeProperty_("size", "Size", 5.0f, 0.0f, 10.0f)
    , verticalAngleProperty_("vertical_angle", "Vertical Angle", 0.0f, -60.0f, 60.0f, 0.1f)
    , viewConeProperty_("view_cone", "View cone", 40.0f, 0.0f, 90.0f, 0.1f)
    , shader_("multipleplaneprocessor.vert", "multipleplaneprocessor.frag")
    , numClips_("number", "Number of clips", 2, 1, 16, 1)
    , outportXDim_(4096)
    , outportYDim_(4096)
    , useIndividualView_("indvidual_view", "Should show only one view", false)
    , viewProp_("view", "View number", 0, 0, 44, 1)
    , xDim_("xdim", "Image width", 819, 256, 1024, 1)
    , yDim_("ydim", "Image height", 455, 256, 512, 1)
    {
    shader_.onReload([this]() { invalidate(InvalidationLevel::InvalidResources); });

    addPort(inport_);
    addPort(outport_);
    addProperty(useIndividualView_);
    addProperty(viewProp_);
    addProperty(numClips_);
    addProperty(xDim_);
    addProperty(yDim_);
    addProperty(regionSizeProperty_);
    addProperty(viewConeProperty_);
    addProperty(verticalAngleProperty_);
    addProperty(shouldShear_);
    addProperty(camera_);
    addProperty(trackball_);

    useIndividualView_.onChange(
        [this]() {onViewToggled(); });
    onViewToggled();
    outport_.setHandleResizeEvents(false);

    const char* sprite_location = "/home/sean/repos/modules/layereddepth/data/images/telescope.JPG";
    spriteTexture_ = std::make_unique<scene::Texture>();
    spriteTexture_->SetSlot(GL_TEXTURE0);
    spriteTexture_->LoadNoMip(sprite_location);
    glActiveTexture(GL_TEXTURE0);
}

void multipleplaneProcessor::onViewToggled() {
    if(useIndividualView_.get()) {
        outportXDim_ = xDim_;
        outportYDim_ = yDim_;
        width_ = outportXDim_;
        height_ = outportYDim_;
    }
    else {
        outportXDim_ = 4096;
        outportYDim_ = 4096;
        width_ = outportXDim_ / 5;
        height_ = outportYDim_ / 9;
    }
    viewProp_.setVisible(useIndividualView_.get());
    outport_.setDimensions(size2_t(outportXDim_, outportYDim_));    

    createVertexGrid(grid_, width_, height_);
    va_ = std::make_unique<VertexArray>();
    vb_ = std::make_shared<VertexBuffer>(
        grid_.get(), (unsigned int)sizeof(float) * 2 * width_ * height_
    );
    reverseVa_ = std::make_unique<VertexArray>();
    reverseGrid_ = std::make_unique<float[]>(width_ * height_ * 2);
    for (unsigned int i = 0; i < width_* height_; ++i) {
        reverseGrid_[2 * i] = grid_[(width_ * height_ * 2) - (2 * i + 2)];
        reverseGrid_[2 * i + 1] = grid_[(width_ * height_ * 2) - (2 * i + 1)];

    }
    reverseVb_ = std::make_unique<VertexBuffer>(
        reverseGrid_.get(), (unsigned int)sizeof(float) * 2 * width_ * height_
    );
    LogInfo("Grid last element is " << grid_[2* (width_ * height_ -1)] << " " << grid_[2* (width_ * height_ -1) + 1]);
    LogInfo("Reverse Grid last element is " << reverseGrid_[2* (width_ * height_ -1)] << " " << reverseGrid_[2* (width_ * height_ -1) + 1]);
    va_->Addbuffer_2f(vb_, 0);
    reverseVa_->Addbuffer_2f(reverseVb_, 0);
}

multipleplaneProcessor::~multipleplaneProcessor() {
    //delete[] grid_;
}


void multipleplaneProcessor::initializeResources() {
    shader_.build();
}

// Create a set of vertices in the pixel positions
// These will be in UV co-ordinates in 0, 1
void multipleplaneProcessor::createVertexGrid(std::unique_ptr<float[]> &grid, const unsigned int width, const unsigned int height) {
    float width_increment = 1.0f / width;
    float height_increment = 1.0f / height;
    float start_width = 0.5f * width_increment;
    float start_height = 0.5f * height_increment;
    grid = std::make_unique<float[]>(width * height * 2);
    for (unsigned int i = 0; i < width; ++i) {
      for (unsigned int j = 0; j < height; ++j) {
        grid[2 * (i * height + j)] = (float)(start_width + width_increment * i);
        grid[2 * (i * height + j) + 1] = (float)(start_height + height_increment * j);
      }
    }
}

void multipleplaneProcessor::process() {
    // Set up correct states
    glEnable(GL_BLEND);

    // glBlendFunc(Src_blend_factor, dest_blend_factor)
    // dest is the value already in the framebuffer
    glBlendFunc(GL_ONE_MINUS_DST_ALPHA, GL_ONE);
    //glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    // Set smoothing points information
    // Two options:
        // One - Render smoothed points as gaussian blurred pixels with bigger size
        // Two - Render square pixels with no blur at the exact size
    // glEnable(GL_POINT_SMOOTH);
    glDisable(GL_POINT_SMOOTH);
    glHint(GL_POINT_SMOOTH_HINT, GL_FASTEST);

    // Allow the size of a point to be specified in the shader
    glEnable(GL_PROGRAM_POINT_SIZE);
    glEnable(GL_POINT_SPRITE);
    glPointParameteri(GL_POINT_SPRITE_COORD_ORIGIN, GL_LOWER_LEFT);
    glClearColor(0.f, 0.f, 0.f, 0.f);

    //glDisable(GL_DEPTH_TEST);
    utilgl::DepthFuncState depthfunc(GL_ALWAYS);
    //glDepthFunc(GL_LESS); 

    // Set texture sampling to nearest
    // glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    // glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // Initialize shaders, textures, targets and uniforms
    shader_.activate();
    utilgl::activateAndClearTarget(outport_);
    shader_.setUniform("width", (int)width_);

    // Do preliminary calculations
    mat4 projectionMatrix = camera_.get().getProjectionMatrix();
    mat4 viewMatrix = camera_.get().getViewMatrix();

    size2_t tileSize(819, 455);
    float viewCone = viewConeProperty_.get();
    PerspectiveCamera* cam = (PerspectiveCamera*)&camera_.get();
    float size = regionSizeProperty_.get();
    float verticalAngle = verticalAngleProperty_.get();
    float adjustedSize = size / tanf(glm::radians(cam->getFovy() * 0.5f));
    float offsetX = 0;
    float offsetY = 0;

    int num_vertices = width_ * height_;
    mat4 vpMatrixInverse = (
        camera_.get().getInverseViewMatrix() *
        camera_.get().getInverseProjectionMatrix()
    );
    int view = viewProp_.get();

    // Could try making texture unit here
    //TextureUnit unit;
    spriteTexture_->Bind();
    GLint loc = glGetUniformLocation(shader_.getID(), "spriteTex");
    if (loc != -1) {
        glUniform1i(loc, spriteTexture_->GetID());
    }
    glActiveTexture(GL_TEXTURE0);
    // shader_.setUniform("sprite", spriteTexture_->GetID());
    if(useIndividualView_.get()) {
        if(view < 22) {
            va_->Bind();
        }
        else {
            reverseVa_->Bind();
        }
        for (int i = 0; i < numClips_; ++i) {
            TextureUnitContainer units;
            utilgl::bindAndSetUniforms(shader_, units, *inport_.getData()->at(i), "tex0",
                                       ImageType::ColorDepth);
            float angleAtView = -viewCone * 0.5f + (float)view / (45.0f - 1.0f) * viewCone;
            offsetX = adjustedSize * tanf(glm::radians(angleAtView));
            offsetY = adjustedSize * tanf(glm::radians(verticalAngle));

            mat4 currentViewMatrix = viewMatrix;
            currentViewMatrix[3][0] -= offsetX;
            currentViewMatrix[3][1] -= offsetY;
            
            mat4 currentProjectionMatrix = projectionMatrix;
            if (shouldShear_.get()) {
                currentProjectionMatrix[2][0] -= offsetX / (size * cam->getAspectRatio());
                currentProjectionMatrix[2][1] -= offsetY / size;
            }            
            mat4 vpMatrix = currentProjectionMatrix * currentViewMatrix;
            mat4 transformMatrix = vpMatrix * vpMatrixInverse;
            shader_.setUniform("transformMatrix", transformMatrix);
            glDrawArrays(GL_POINTS, 0, num_vertices);      
        }
    }
    else {
        for (int i = 0; i < numClips_; ++i) {
            TextureUnitContainer units;
            utilgl::bindAndSetUniforms(shader_, units, *inport_.getData()->at(i), "tex0",
                                       ImageType::ColorDepth);
            view = 0;
            
            bool forwardDir = true;
            for(int y = 0; y < 9; ++y)
            {
                for(int x = 0; x < 5; ++x)
                {   
                    if(view > 22) {
                        reverseVa_->Bind();
                        forwardDir = false;
                    }
                    else {
                        va_->Bind();    
                    }
                        
                    float angleAtView = -viewCone * 0.5f + (float)view / (45.0f - 1.0f) * viewCone;
                    offsetX = adjustedSize * tanf(glm::radians(angleAtView));
                    offsetY = adjustedSize * tanf(glm::radians(verticalAngle));

                    mat4 currentViewMatrix = viewMatrix;
                    currentViewMatrix[3][0] -= offsetX;
                    currentViewMatrix[3][1] -= offsetY;
                    
                    mat4 currentProjectionMatrix = projectionMatrix;
                    if (shouldShear_.get()) {
                        currentProjectionMatrix[2][0] -= offsetX / (size * cam->getAspectRatio());
                        currentProjectionMatrix[2][1] -= offsetY / size;
                    }            
                    mat4 vpMatrix = currentProjectionMatrix * currentViewMatrix;
                    mat4 transformMatrix = vpMatrix * vpMatrixInverse;
                    shader_.setUniform("transformMatrix", transformMatrix);
                    
                    size2_t start(x * tileSize.x, y * tileSize.y);
                    glViewport(start.x, start.y, tileSize.x, tileSize.y);
                    glDrawArrays(GL_POINTS, 0, num_vertices);   
                    
                    ++view;
                }
            }
        }
        glViewport(0, 0, 4096, 4096);
    }

    utilgl::deactivateCurrentTarget();
    shader_.deactivate();
    
    glDisable(GL_BLEND);
    //glEnable(GL_POINT_SMOOTH);
}

}  // namespace inviwo
