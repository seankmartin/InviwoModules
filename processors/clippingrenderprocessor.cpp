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

#include <modules/layereddepth/processors/clippingrenderprocessor.h>

#include <modules/opengl/texture/textureutils.h>
#include <modules/opengl/shader/shaderutils.h>
#include <modules/opengl/openglutils.h>
#include <modules/opengl/rendering/meshdrawergl.h>
#include <inviwo/core/datastructures/geometry/simplemesh.h>

namespace inviwo {

// The Class Identifier has to be globally unique. Use a reverse DNS naming scheme
const ProcessorInfo clippingRenderProcessor::processorInfo_{
    "org.inviwo.clippingRenderProcessor",      // Class identifier
    "Clipping Render Processor",                // Display name
    "Rendering",              // Category
    CodeState::Experimental,  // Code state
    Tags::GL,               // Tags
};
const ProcessorInfo clippingRenderProcessor::getProcessorInfo() const { return processorInfo_; }

clippingRenderProcessor::clippingRenderProcessor()
    : Processor()
    , inport_("inport")
    , entryPort_("entry")
    , exitPort_("exit")
    , camera_("camera", "Camera", vec3(0.0f, 0.0f, -2.0f), vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 1.0f, 0.0f), &inport_)
    , trackball_(&camera_)
    , planeNormal_("normal", "Plane normal", vec3(0.0f), vec3(-100.0f), vec3(100.0f))
    , useCameraNormalAsPlane_("camNormal", "Use camera normal as plane normal", true)
    , planeDistance_("distance", "Plane distance along normal", 0.0f, -10.0f, 10.0f)
    , planeReverseDistance_("reverse_distance", "Reverse plane distance along normal", 0.0f, -10.0f, 10.0f)
    , shader_("clippingrenderprocessor.vert", "clippingrenderprocessor.frag")
    , faceShader_("facerender.vert", "facerender.frag")
    {
    shader_.onReload([this]() { invalidate(InvalidationLevel::InvalidResources); });
    faceShader_.onReload([this]() { invalidate(InvalidationLevel::InvalidResources); });

    addPort(inport_);
    addPort(entryPort_, "ImagePortGroup1");
    addPort(exitPort_, "ImagePortGroup1");
    addProperty(planeDistance_);
    addProperty(planeReverseDistance_);
    addProperty(planeNormal_);
    addProperty(useCameraNormalAsPlane_);
    addProperty(camera_);
    addProperty(trackball_);

    planeNormal_.setReadOnly(useCameraNormalAsPlane_.get());
    entryPort_.addResizeEventListener(&camera_);
    useCameraNormalAsPlane_.onChange(
        [this]() { onAlignPlaneNormalToCameraNormalToggled(); });
}

clippingRenderProcessor::~clippingRenderProcessor() {
}

void clippingRenderProcessor::onAlignPlaneNormalToCameraNormalToggled() {
    planeNormal_.setReadOnly(useCameraNormalAsPlane_.get());
    if (useCameraNormalAsPlane_.get()) {           
        planeNormal_.set(glm::normalize(camera_.getLookTo() - camera_.getLookFrom()));
    }
}

void clippingRenderProcessor::initializeResources() {
    shader_.build();
    faceShader_.build();
}

void clippingRenderProcessor::InviwoPlaneIntersectionPoints(std::vector<vec3> &out_points, const Plane& worldSpacePlane) {
    auto geom = inport_.getData();
    auto worldToData = geom->getCoordinateTransformer().getWorldToDataMatrix();
    auto worldToDataNormal = glm::transpose(glm::inverse(worldToData));
    auto dataSpacePos = vec3(worldToData * vec4(worldSpacePlane.getPoint(), 1.0));
    auto dataSpaceNormal =
        glm::normalize(vec3(worldToDataNormal * vec4(worldSpacePlane.getNormal(), 0.0)));
    Plane plane(dataSpacePos, dataSpaceNormal);

    if (auto simple = dynamic_cast<const SimpleMesh*>(inport_.getData().get())) {
        const std::vector<vec3>* vertexList = &simple->getVertexList()->getRAMRepresentation()->getDataContainer();
        
        // Check it is parallelepiped
        if(vertexList->size() == 8) {
            unsigned int num_edges = 12;
            vec3 rayDir;
            vec3 rayOrig;
            vec3 rayEnd;

            // Build a set of edges to test as start->end
            // Parallelepiped is laid out as:
            //      2-----3
            //     /|    /|          y
            //    6-+---7 |          |
            //    | 0---+-1          o--x
            //    |/    |/          /
            //    4-----5          z

            unsigned int points[24] = {
                0, 1, 2, 3, 4, 5, 6, 7, // Test edges facing along x axis
                0, 2, 1, 3, 4, 6, 5, 7, // Test edges facing along y axis
                0, 4, 1, 5, 3, 7, 2, 6 // Test edges facing along z axis
            };
            
            for(unsigned int i = 0; i < num_edges; ++i) {   
                rayOrig = vertexList->at(points[2 * i]);
                rayEnd = vertexList->at(points[2 * i + 1]);
                rayDir = rayEnd - rayOrig;

                if(plane.isInside(rayOrig)) {
                    if(plane.isInside(rayEnd)) {
                        //Both lie inside the plane, no intersection point
                    }
                    else { //the end lies outside the plane so there is intersection
                        vec3 intersection = 
                        plane
                                .getIntersection(rayOrig, rayEnd)
                                .intersection_;
                        out_points.push_back(intersection);
                    }
                }
                else {
                    if(plane.isInside(rayEnd)) { // the end lies inside the plane
                        vec3 intersection = 
                        plane
                                .getIntersection(rayOrig, rayEnd)
                                .intersection_;
                        out_points.push_back(intersection);
                    }
                    //Otherwise the both lie outside of the plane so there is no need to do anything
                }
            }

            // Sort the points
            if (out_points.size() == 0)
                return;
            const vec3 origin = out_points[0];
        
            std::sort(out_points.begin(), out_points.end(), [&](const vec3 &lhs, const vec3 &rhs) -> bool {
                vec3 v = glm::cross((lhs - origin), (rhs - origin));
                return glm::dot(v, dataSpaceNormal) < 0;
            });
        }
        else {
            throw Exception("Unsupported mesh type, only parallelepipeds are supported");
        }
    }
    else {
        throw Exception("Unsupported mesh type, only simple meshes are supported");
    }
    
}

void clippingRenderProcessor::process() {
    if (useCameraNormalAsPlane_.get()) {           
        planeNormal_.set(glm::normalize(camera_.getLookTo() - camera_.getLookFrom()));
    }
    utilgl::DepthFuncState depthfunc(GL_ALWAYS);
    utilgl::PointSizeState pointsize(1.0f);

    mat4 projectionMatrix = inport_.getData().get()->getCoordinateTransformer(camera_.get()).getViewToClipMatrix();
    mat4 viewMatrix = inport_.getData().get()->getCoordinateTransformer(camera_.get()).getWorldToViewMatrix();
    mat4 worldMatrix = inport_.getData().get()->getCoordinateTransformer(camera_.get()).getDataToWorldMatrix();
    mat4 mvpMatrix = projectionMatrix * viewMatrix * worldMatrix;

    auto planeNormal = planeNormal_.get();
    auto planeReverseNormal = vec3(-planeNormal[0], -planeNormal[1], -planeNormal[2]);
    
    vec4 planeEquation = vec4(planeNormal[0], planeNormal[1], planeNormal[2], planeDistance_);
    vec4 reversePlaneEquation = vec4(planeReverseNormal[0], planeReverseNormal[1], planeReverseNormal[2], planeReverseDistance_);

    shader_.activate();
    shader_.setUniform("clipPlane", planeEquation);
    shader_.setUniform("reversePlane", reversePlaneEquation);
    shader_.setUniform("dataToClip", mvpMatrix);
    shader_.setUniform("worldMatrix", worldMatrix);
    auto drawer = MeshDrawerGL::getDrawObject(inport_.getData().get());

    // Draw the front faces
    {
        // Turn on clipping plane distances
        glEnable(GL_CLIP_DISTANCE0);
        glEnable(GL_CLIP_DISTANCE1);
        utilgl::activateAndClearTarget(entryPort_);
        utilgl::CullFaceState cull(GL_BACK);
        drawer.draw();    
    }
    
    shader_.deactivate();
    faceShader_.activate();
    faceShader_.setUniform("dataToClip", mvpMatrix);
    

    // Draw the front facing polygon intersection
    {
        std::vector<vec3> points;
        // Maximum 6 interesection points
        points.reserve(6);
        Plane forwardPlane = Plane(-planeDistance_.get() * planeNormal, planeNormal);
        InviwoPlaneIntersectionPoints(points, forwardPlane);

        auto ppd=std::make_shared<SimpleMesh>();
        ppd->setIndicesInfo(DrawType::Triangles, ConnectivityType::Fan);
        for(unsigned int i = 0; i < points.size(); ++i){
            ppd->addVertex(points[i], points[i], vec4(points[i], 1.0f));
            ppd->addIndex(i);
        }

        glDisable(GL_CLIP_DISTANCE0);
        glDisable(GL_CLIP_DISTANCE1);

        auto new_drawer = MeshDrawerGL::getDrawObject(ppd.get());
        new_drawer.draw();
    }
    
    faceShader_.deactivate();
    shader_.activate();

    auto drawer2 = MeshDrawerGL::getDrawObject(inport_.getData().get());
    // Draw the back faces
    {
        // Turn on clipping plane distances
        glEnable(GL_CLIP_DISTANCE0);
        glEnable(GL_CLIP_DISTANCE1);
        utilgl::activateAndClearTarget(exitPort_);
        utilgl::CullFaceState cull(GL_FRONT);
        drawer2.draw();
    }
    
    shader_.deactivate();
    faceShader_.activate();
    // Draw the back facing polygon intersection
    {
        std::vector<vec3> points;
        // Maximum 6 interesection points
        points.reserve(6);
        Plane backwardPlane = Plane(-planeReverseDistance_.get() * planeReverseNormal, planeReverseNormal);
        InviwoPlaneIntersectionPoints(points, backwardPlane);

        auto ppd=std::make_shared<SimpleMesh>();
        ppd->setIndicesInfo(DrawType::Triangles, ConnectivityType::Fan);
        for(unsigned int i = 0; i < points.size(); ++i){
            ppd->addVertex(points[i], points[i], vec4(points[i], 1.0f));
            ppd->addIndex(i);
        }

        glDisable(GL_CLIP_DISTANCE0);
        glDisable(GL_CLIP_DISTANCE1);

        auto new_drawer = MeshDrawerGL::getDrawObject(ppd.get());
        new_drawer.draw();
    }
    
    utilgl::deactivateCurrentTarget();
    faceShader_.deactivate();
}

}  // namespace inviwo
