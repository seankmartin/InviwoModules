#include <modules/lookingglass/processors/lfentryexitpoints.h>

#include <inviwo/core/datastructures/camera.h>
#include <inviwo/core/datastructures/coordinatetransformer.h>
#include <inviwo/core/datastructures/image/image.h>
#include <modules/opengl/texture/textureutils.h>
#include <modules/opengl/shader/shaderutils.h>
#include <modules/opengl/openglutils.h>
#include <modules/opengl/rendering/meshdrawergl.h>

namespace inviwo {

// The Class Identifier has to be globally unique. Use a reverse DNS naming scheme
const ProcessorInfo lfentryexitpoints::processorInfo_{
    "org.inviwo.lfentryexitpoints",      // Class identifier
    "Looking Glass Entry Exit Points",                // Display name
    "Looking Glass",              // Category
    CodeState::Experimental,  // Code state
    Tags::GL,               // Tags
};
const ProcessorInfo lfentryexitpoints::getProcessorInfo() const { return processorInfo_; }

lfentryexitpoints::lfentryexitpoints()
    : Processor()
    , inport_("geometry")
    , entryPort_("entry", DataVec4UInt16::get())
    , exitPort_("exit", DataVec4UInt16::get())
    , camera_("camera", "Camera", vec3(0.0f, 0.0f, -2.0f), vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 1.0f, 0.0f), &inport_)
    , capNearClipping_("capNearClipping", "Cap near plane clipping", true)
    , shouldShear_("shouldShear", "Should Use Shear Projection", true)
    , trackball_(&camera_)
    , regionSizeProperty_("size", "Size", 5.0f, 0.0f, 10.0f)
    , verticalAngleProperty_("vertical_angle", "Vertical Angle", 0.0f, -60.0f, 60.0f, 0.1f)
    , viewConeProperty_("view_cone", "View cone", 40.0f, 0.0f, 90.0f, 0.1f)
    , useIndividualView_("individual_view", "Should show only one view", false)
    , imageDim_("dimensions", "Invidual view dimensions", ivec2(408, 226), ivec2(0, 0), ivec2(819, 455), ivec2(1, 1))
    , fullSize_("full_dimensions", "Full dimensions", ivec2(2040, 2034), ivec2(0, 0), ivec2(4096, 4096), ivec2(1, 1))
    , viewProp_("view", "View number", 0, 0, 44, 1)
    , entryExitShader_("lfentryexitpoints.vert", "lfentryexitpoints.frag")
    , nearClipShader_("img_identity.vert", "lfcapnearclipping.frag") 
    {
    entryExitShader_.onReload([this]() { invalidate(InvalidationLevel::InvalidResources); });
    nearClipShader_.onReload([this]() { invalidate(InvalidationLevel::InvalidResources); });

    addPort(inport_);
    addPort(entryPort_, "ImagePortGroup1");
    addPort(exitPort_, "ImagePortGroup1");
    addProperty(useIndividualView_);
    addProperty(imageDim_);
    addProperty(fullSize_);
    addProperty(viewProp_);
    addProperty(regionSizeProperty_);
    addProperty(viewConeProperty_);
    addProperty(verticalAngleProperty_);
    addProperty(capNearClipping_);
    addProperty(shouldShear_);
    addProperty(camera_);
    addProperty(trackball_);

    // Change this if default state is to show only one view
    viewProp_.setVisible(false);

    entryPort_.setHandleResizeEvents(false);
    exitPort_.setHandleResizeEvents(false);

    useIndividualView_.onChange(
        [this]() {onViewToggled(); });
    imageDim_.onChange(
        [this]() {onViewToggled(); });

    inverseMatrices_ = std::make_unique<std::vector<mat4>>();
    inverseMatrices_->reserve(45);
    fullSize_.setReadOnly(true);
}

void lfentryexitpoints::initializeResources() {
    entryExitShader_.build();
    nearClipShader_.build();
}

void lfentryexitpoints::onViewToggled() {
    if(useIndividualView_.get()) {
        entryPort_.setDimensions(imageDim_.get());
        exitPort_.setDimensions(imageDim_.get());
        fullSize_.set(imageDim_.get());
    }
    else {
        //if (imageDim_.get() == imageDim_.getMaxValue())
        //    fullSize_ = ivec2(4096, 4096);
        //else {
        int Xdim = imageDim_.get().x * 5;
        int Ydim = imageDim_.get().y * 9;
        fullSize_.set(ivec2(Xdim, Ydim));
        // }

        entryPort_.setDimensions(fullSize_.get());
        exitPort_.setDimensions(fullSize_.get());
    }
    viewProp_.setVisible(useIndividualView_.get());
}

void lfentryexitpoints::process() {
    // outport_.setData(myImage);
    inverseMatrices_->clear();
    {
        utilgl::DepthFuncState depthfunc(GL_ALWAYS);
        utilgl::PointSizeState pointsize(1.0f);

        entryExitShader_.activate();
        mat4 modelMatrix = inport_.getData().get()->getCoordinateTransformer(camera_.get()).getDataToClipMatrix();
        entryExitShader_.setUniform("dataToClip", modelMatrix);
        
        {
            // generate exit points
            utilgl::activateAndClearTarget(
                exitPort_, 
                ImageType::ColorDepth
            );
            utilgl::CullFaceState cull(GL_FRONT);
            drawViews();
            utilgl::deactivateCurrentTarget();
        }

        {
            // generate entry points
            if (!tmpEntry_ || tmpEntry_->getDimensions() != entryPort_.getData()->getDimensions() ||
                tmpEntry_->getDataFormat() != entryPort_.getData()->getDataFormat()) {
                tmpEntry_.reset(new Image(entryPort_.getData()->getDimensions(), entryPort_.getData()->getDataFormat()));
            }
            utilgl::activateAndClearTarget(
                *tmpEntry_, 
                ImageType::ColorDepth
            );

            utilgl::CullFaceState cull(GL_BACK);
            drawViews();
            entryExitShader_.deactivate();
            utilgl::deactivateCurrentTarget();
        }

        {
            // draw near plane to fix any clipped entry points
            // render an image plane aligned quad to cap the proxy geometry
            utilgl::activateAndClearTarget(entryPort_, ImageType::ColorDepth);
            nearClipShader_.activate();

            TextureUnitContainer units;
            utilgl::bindAndSetUniforms(nearClipShader_, units, *tmpEntry_, "entry", ImageType::ColorDepth);
            utilgl::bindAndSetUniforms(nearClipShader_, units, exitPort_, ImageType::ColorDepth);

            // the rendered plane is specified in camera coordinates
            // thus we must transform from camera to world to texture coordinates
            auto& camera = camera_.get();
            nearClipShader_.setUniform("nearDist", camera.getNearPlaneDist());

            drawNearPlanes();
            nearClipShader_.deactivate();
            utilgl::deactivateCurrentTarget();
        }
    }
}

void lfentryexitpoints::drawViews()
{
    auto drawer = MeshDrawerGL::getDrawObject(inport_.getData().get());

    mat4 projectionMatrix = inport_.getData().get()->getCoordinateTransformer(camera_.get()).getViewToClipMatrix();
    mat4 viewMatrix = inport_.getData().get()->getCoordinateTransformer(camera_.get()).getWorldToViewMatrix();
    mat4 worldMatrix = inport_.getData().get()->getCoordinateTransformer(camera_.get()).getDataToWorldMatrix();

    int view = viewProp_.get();
    ivec2 tileSize = imageDim_.get();
    float viewCone = viewConeProperty_.get();
    PerspectiveCamera* cam = (PerspectiveCamera*)&camera_.get();
    float size = regionSizeProperty_.get();
    float verticalAngle = verticalAngleProperty_.get();
    float adjustedSize = size / tanf(glm::radians(cam->getFovy() * 0.5f));
    float offsetX = 0;
    float offsetY = 0;
    
    if(useIndividualView_.get()) {
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
        mat4 mvpMatrix = currentProjectionMatrix * currentViewMatrix * worldMatrix;
        entryExitShader_.setUniform("dataToClip", mvpMatrix);
        inverseMatrices_->push_back(MatrixInvert(mvpMatrix));
        drawer.draw();
    }
    else {
        view = 0;
        for(int y = 0; y < 9; ++y)
        {
            for(int x = 0; x < 5; ++x)
            {
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
                mat4 mvpMatrix = currentProjectionMatrix * currentViewMatrix * worldMatrix;
                entryExitShader_.setUniform("dataToClip", mvpMatrix);
                inverseMatrices_->push_back(MatrixInvert(mvpMatrix));
                size2_t start(x * tileSize.x, y * tileSize.y);
                glViewport(start.x, start.y, tileSize.x, tileSize.y);
                drawer.draw();
                ++view;
            }
        }
        
        glViewport(0, 0, fullSize_.get().x, fullSize_.get().y);
    }
}

void lfentryexitpoints::drawNearPlanes() {
    ivec2 tileSize = imageDim_.get();
    vec4 viewport;
    
    if(useIndividualView_.get()) {
        nearClipShader_.setUniform("NDCToTextureMat", inverseMatrices_->at(0));
        viewport = vec4(0.f, 0.f, 1.f, 1.f);
        nearClipShader_.setUniform("viewport", viewport);
        utilgl::singleDrawImagePlaneRect();;
    }
    else {
        int count = 0;
        for(int y = 0; y < 9; ++y)
        {
            for(int x = 0; x < 5; ++x)
            {
                nearClipShader_.setUniform("NDCToTextureMat", inverseMatrices_->at(count));
                size2_t start(x * tileSize.x, y * tileSize.y);
                glViewport(start.x, start.y, tileSize.x, tileSize.y);
                viewport = vec4(start.x / (float)fullSize_.get().x, start.y / (float)fullSize_.get().y, tileSize.x / (float)fullSize_.get().x, tileSize.y / (float)fullSize_.get().y);
                nearClipShader_.setUniform("viewport", viewport);
                utilgl::singleDrawImagePlaneRect();
                ++count;
            }
        }
        
        glViewport(0, 0, fullSize_.get().x, fullSize_.get().y);
    }
}

}  // namespace inviwo
