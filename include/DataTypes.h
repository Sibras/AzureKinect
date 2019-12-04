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

    Position(const float x, const float y, const float z);

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

    Quaternion(const float x, const float y, const float z, const float w);

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

    Joint(const Position& position, const Quaternion& rotation, const bool confident);

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

    Bone(const Position& joint1, const Position& joint2, const bool confident);

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
} // namespace Ak
