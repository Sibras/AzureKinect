#pragma once
/**
 * Copyright Matthew Oliver
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <cstdint>
#include <glm/gtx/type_aligned.hpp>
#include <glm/vec3.hpp>

namespace Ak {
class KinectImage
{
public:
    KinectImage() = default;

    KinectImage(const KinectImage& other) = default;

    KinectImage(KinectImage&& other) noexcept = default;

    KinectImage& operator=(const KinectImage& other) = default;

    KinectImage& operator=(KinectImage&& other) noexcept = default;

    ~KinectImage() = default;

    KinectImage(uint8_t* image, int32_t width, int32_t height, int32_t stride);

    uint8_t* m_image;
    int32_t m_width;
    int32_t m_height;
    int32_t m_stride;
};

class Position
{
public:
    Position() = default;

    Position(float x, float y, float z);

    Position(const Position& other) = default;

    Position(Position&& other) noexcept = default;

    Position& operator=(const Position& other) = default;

    Position& operator=(Position&& other) noexcept = default;

    glm::vec3 m_position;
};

class Quaternion
{
public:
    Quaternion() = default;

    Quaternion(float x, float y, float z, float w);

    Quaternion(const Quaternion& other) = default;

    Quaternion(Quaternion&& other) noexcept = default;

    Quaternion& operator=(const Quaternion& other) = default;

    Quaternion& operator=(Quaternion&& other) noexcept = default;

    glm::vec4 m_rotation;
};

class Joint
{
public:
    Joint() = default;

    Joint(const Position& position, const Quaternion& rotation, bool confident);

    Joint(const Position&& position, const Quaternion&& rotation, const bool&& confident);

    Joint(const Joint& other) = default;

    Joint(Joint&& other) noexcept = default;

    Joint& operator=(const Joint& other) = default;

    Joint& operator=(Joint&& other) noexcept = default;

    Position m_position;
    Quaternion m_rotation;
    bool m_confident;
};

class Bone
{
public:
    Bone() = default;

    Bone(const Position& joint1, const Position& joint2, bool confident);

    Bone(const Position&& joint1, const Position&& joint2, const bool&& confident);

    Bone(const Bone& other) = default;

    Bone(Bone&& other) noexcept = default;

    Bone& operator=(const Bone& other) = default;

    Bone& operator=(Bone&& other) noexcept = default;

    Position m_joint1;
    Position m_joint2;
    bool m_confident;
};

class KinectJoints
{
public:
    KinectJoints() = default;

    KinectJoints(const KinectJoints& other) = default;

    KinectJoints(KinectJoints&& other) noexcept = default;

    KinectJoints& operator=(const KinectJoints& other) = default;

    KinectJoints& operator=(KinectJoints&& other) noexcept = default;

    ~KinectJoints() = default;

    KinectJoints(Joint* joints, uint32_t length);

    Joint* m_joints;
    uint32_t m_length;
};

class CustomVertex
{
public:
    CustomVertex() = default;

    CustomVertex(const CustomVertex& other) = default;

    CustomVertex(CustomVertex&& other) noexcept = default;

    CustomVertex& operator=(const CustomVertex& other) = default;

    CustomVertex& operator=(CustomVertex&& other) noexcept = default;

    ~CustomVertex() = default;

    CustomVertex(const glm::vec3& position, const glm::vec3& normal);

    glm::vec3 m_position;
    glm::vec3 m_normal;
};

class BrownConradyTransform
{
public:
    BrownConradyTransform() = default;

    BrownConradyTransform(const glm::vec2&& c, const glm::vec2&& f, const glm::vec2&& k14, const glm::vec2&& k25,
        const glm::vec2&& k36, const glm::vec2&& p);

    BrownConradyTransform(const BrownConradyTransform& other) = default;

    BrownConradyTransform(BrownConradyTransform&& other) noexcept = default;

    BrownConradyTransform& operator=(const BrownConradyTransform& other) = default;

    BrownConradyTransform& operator=(BrownConradyTransform&& other) noexcept = default;

    glm::vec2 m_c;
    glm::vec2 m_f;
    glm::vec2 m_k14;
    glm::vec2 m_k25;
    glm::vec2 m_k36;
    glm::vec2 m_p;
};

class KinectCalibration
{
public:
    BrownConradyTransform m_depthBC;
    BrownConradyTransform m_colourBC;
    BrownConradyTransform m_irBC;
    glm::mat4 m_jointToDepth;
    glm::mat4 m_jointToColour;
    glm::mat4 m_jointToIR;
    glm::vec2 m_depthFOV;
    glm::vec2 m_colourFOV;
    glm::vec2 m_irFOV;
    glm::ivec2 m_depthDimensions;
    glm::ivec2 m_colourDimensions;
    glm::ivec2 m_irDimensions;
};
} // namespace Ak
