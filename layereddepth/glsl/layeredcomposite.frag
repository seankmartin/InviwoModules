/*********************************************************************************
 *
 * Inviwo - Interactive Visualization Workshop
 *
 * Copyright (c) 2013-2018 Inviwo Foundation
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
#define REF_SAMPLING_INTERVAL 150.0

#include "utils/structs.glsl"
#include "utils/sampler2d.glsl"

uniform sampler2D tex0Color;
uniform sampler2D tex0Depth;
uniform sampler2D tex0Picking;

uniform sampler2D tex1Color;
uniform sampler2D tex1Depth;
uniform sampler2D tex1Picking;

uniform ImageParameters outportParameters;

void main() {
    vec2 texCoords = gl_FragCoord.xy * outportParameters.reciprocalDimensions;
    vec4 color0 = texture(tex0Color, texCoords);
    vec4 picking0 = texture(tex0Picking, texCoords);
    float depth0 = texture(tex0Depth, texCoords).r;
    vec4 color1 = texture(tex1Color, texCoords);
    vec4 picking1 = texture(tex1Picking, texCoords);
    float depth1 = texture(tex1Depth, texCoords).r;
    vec4 colorOut;
    vec4 pickingOut;
    float depthOut;

    // colorOut.a = color0.a + (1.0 - color0.a) * color1.a;
    // colorOut.rgb = color0.rgb + (1.0 - color0.a) * color1.rgb;
    // colorOut = color0 * color0.a + color1 * (1 - color0.a);
    // colorOut.rgb = color1.rbg * color.a + (1 - color0.r)
    //float tIncr = 0.004;
    //color0.a = 1.0 - pow(1.0 - color0.a, tIncr * REF_SAMPLING_INTERVAL);
    colorOut = (1.0 - color0.a) * color1 + color0;
    pickingOut = (picking1.a > 0 ? picking1 : (color1.a < 0.95 ? picking0 : vec4(0.0)));
    depthOut = min(depth0, depth1);

    FragData0 = colorOut;
    PickingData = pickingOut;
    gl_FragDepth = depthOut;
}