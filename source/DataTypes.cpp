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

#include "DataTypes.h"

using namespace glm;

namespace Ak {
KinectImage::KinectImage(uint8_t* const image, const int32_t width, const int32_t height, const int32_t stride)
    : m_image(image)
    , m_width(width)
    , m_height(height)
    , m_stride(stride)
{}

Position::Position(const float x, const float y, const float z)
    : m_position(x, y, z)
{}

Quaternion::Quaternion(const float x, const float y, const float z, const float w)
    : m_rotation(x, y, z, w)
{}

Joint::Joint(const Position& position, const Quaternion& rotation, const bool confident)
    : m_position(position)
    , m_rotation(rotation)
    , m_confident(confident)
{}

Joint::Joint(const Position&& position, const Quaternion&& rotation, const bool&& confident)
    : m_position(position)
    , m_rotation(rotation)
    , m_confident(confident)
{}

Bone::Bone(const Position& joint1, const Position& joint2, const bool confident)
    : m_joint1(joint1)
    , m_joint2(joint2)
    , m_confident(confident)
{}

Bone::Bone(const Position&& joint1, const Position&& joint2, const bool&& confident)
    : m_joint1(joint1)
    , m_joint2(joint2)
    , m_confident(confident)
{}

KinectJoints::KinectJoints(Joint* joints, const uint32_t length)
    : m_joints(joints)
    , m_length(length)
{}

CustomVertex::CustomVertex(const vec3& position, const vec3& normal)
    : m_position(position)
    , m_normal(normal)
{}
} // namespace Ak
