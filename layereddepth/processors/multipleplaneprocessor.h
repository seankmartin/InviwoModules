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

#ifndef IVW_MULTIPLEPLANEPROCESSOR_H
#define IVW_MULTIPLEPLANEPROCESSOR_H

#include <modules/layereddepth/layereddepthmoduledefine.h>
#include <modules/layereddepth/helpers/vertexarray.h>
#include <modules/layereddepth/helpers/vertexbuffer.h>
#include <modules/layereddepth/helpers/texture.h>
#include <inviwo/core/common/inviwo.h>
#include <inviwo/core/processors/processor.h>
#include <inviwo/core/properties/ordinalproperty.h>
#include <inviwo/core/ports/imageport.h>
#include <modules/opengl/shader/shader.h>
#include <inviwo/core/datastructures/camera.h>
#include <inviwo/core/properties/cameraproperty.h>
#include <inviwo/core/interaction/cameratrackball.h>
#include <inviwo/core/properties/boolproperty.h>
#include <inviwo/core/ports/datainport.h>

namespace inviwo {

/** \docpage{org.inviwo.multipleplaneProcessor, multipleplane Processor}
 * ![](org.inviwo.multipleplaneProcessor.png?classIdentifier=org.inviwo.multipleplaneProcessor)
 * Explanation of how to use the processor.
 *
 * ### Inports
 *   * __<Inport1>__ <description>.
 *
 * ### Outports
 *   * __<Outport1>__ <description>.
 *
 * ### Properties
 *   * __<Prop1>__ <description>.
 *   * __<Prop2>__ <description>
 */

/**
 * \class multipleplaneProcessor
 * \brief VERY_BRIEFLY_DESCRIBE_THE_PROCESSOR
 * DESCRIBE_THE_PROCESSOR_FROM_A_DEVELOPER_PERSPECTIVE
 */
class IVW_MODULE_LAYEREDDEPTH_API multipleplaneProcessor : public Processor {
public:
    multipleplaneProcessor();
    virtual ~multipleplaneProcessor() override;

    virtual void process() override;
    virtual void initializeResources() override;

    virtual const ProcessorInfo getProcessorInfo() const override;
    static const ProcessorInfo processorInfo_;

    void createVertexGrid(std::unique_ptr<float[]> &grid, const unsigned int width, const unsigned int height);
private:
    void onViewToggled();
    DataInport<std::vector<std::shared_ptr<Image>>> inport_;
    ImageOutport outport_;
    
    CameraProperty camera_;
    PerspectiveCamera new_camera_;
    CameraTrackball trackball_;

    BoolProperty shouldShear_;
    FloatProperty regionSizeProperty_;
    FloatProperty verticalAngleProperty_;
    FloatProperty viewConeProperty_;

    Shader shader_;

    IntProperty numClips_;
    std::unique_ptr<float[]> grid_;
    std::unique_ptr<float[]> reverseGrid_;
    unsigned int outportXDim_;
    unsigned int outportYDim_;
    unsigned int width_;
    unsigned int height_;
    
    BoolProperty useIndividualView_;
    IntProperty viewProp_;
    IntProperty xDim_;
    IntProperty yDim_;

    std::unique_ptr<VertexArray> va_;
    std::shared_ptr<VertexBuffer> vb_;

    std::unique_ptr<VertexArray> reverseVa_;
    std::shared_ptr<VertexBuffer> reverseVb_;

    std::unique_ptr<scene::Texture> spriteTexture_;
};

}  // namespace inviwo

#endif  // IVW_MULTIPLEPLANEPROCESSOR_H
