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

#include <atomic>
#include <functional>
#include <k4abt.h>
#include <mutex>
#include <thread>

namespace Ak {
class AzureKinect
{
public:
    AzureKinect() noexcept = default;

    ~AzureKinect();

    AzureKinect(const AzureKinect& other) noexcept = delete;

    AzureKinect(AzureKinect&& other) noexcept = delete;

    AzureKinect& operator=(const AzureKinect& other) noexcept = delete;

    AzureKinect& operator=(AzureKinect&& other) noexcept = delete;

    /**
     * Initializes the azure kinect camera.
     * @param error (Optional) The callback used to signal errors.
     * @param ready (Optional) The callback used to signal camera is ready for operations.
     * @param image (Optional) The callback used to signal updated image data.
     * @returns True if it succeeds, false if it fails.
     */
    bool init(std::function<void(const std::string&)> error = nullptr, std::function<void()> ready = nullptr,
        std::function<void(uint8_t*, uint32_t, uint32_t, uint32_t)> image = nullptr) noexcept;

    /**
     * Notify to start acquisition.
     * @note This function is asynchronous and may not result in an immediate shutdown.
     * @param pid The player ID used to identify output data.
     */
    void start(uint32_t pid) noexcept;

    /** Notify to end acquisition.
     * @note This function is asynchronous and may not result in an immediate shutdown.
     */
    void stop() noexcept;

    /** Notify to shutdown.
     * @note This function is synchronous and will block until thread has completed.
     */
    void shutdown() noexcept;

    struct Position
    {
        float m_x, m_y, m_z;

        Position() = default;

        Position(const float x, const float y, const float z)
            : m_x(x)
            , m_y(y)
            , m_z(z)
        {}

        Position(const Position& other) = default;

        Position(Position&& other) noexcept = default;

        Position& operator=(const Position& other) = default;

        Position& operator=(Position&& other) noexcept = default;
    };

    struct Quaternion
    {
        float m_x, m_y, m_z, m_w;

        Quaternion() = default;

        Quaternion(const float x, const float y, const float z, const float w)
            : m_x(x)
            , m_y(y)
            , m_z(z)
            , m_w(w)
        {}

        Quaternion(const Quaternion& other) = default;

        Quaternion(Quaternion&& other) noexcept = default;

        Quaternion& operator=(const Quaternion& other) = default;

        Quaternion& operator=(Quaternion&& other) noexcept = default;
    };

    struct Joint
    {
        Position m_position;
        Quaternion m_rotation;
        bool m_confident;

        Joint() = default;

        Joint(const Position& position, const Quaternion& rotation, const bool confident)
            : m_position(position)
            , m_rotation(rotation)
            , m_confident(confident)
        {}

        Joint(const Position&& position, const Quaternion&& rotation, const bool&& confident)
            : m_position(position)
            , m_rotation(rotation)
            , m_confident(confident)
        {}

        Joint(const Joint& other) = default;

        Joint(Joint&& other) noexcept = default;

        Joint& operator=(const Joint& other) = default;

        Joint& operator=(Joint&& other) noexcept = default;
    };

    struct Bone
    {
        Position m_joint1;
        Position m_joint2;
        bool m_confident;

        Bone() = default;

        Bone(const Position& joint1, const Position& joint2, const bool confident)
            : m_joint1(joint1)
            , m_joint2(joint2)
            , m_confident(confident)
        {}

        Bone(const Position&& joint1, const Position&& joint2, const bool&& confident)
            : m_joint1(joint1)
            , m_joint2(joint2)
            , m_confident(confident)
        {}

        Bone(const Bone& other) = default;

        Bone(Bone&& other) noexcept = default;

        Bone& operator=(const Bone& other) = default;

        Bone& operator=(Bone&& other) noexcept = default;
    };

private:
    std::atomic_bool m_shutdown = false;
    std::atomic_bool m_run = false;
    std::atomic_bool m_run2 = false;
    std::atomic_uint32_t m_pid = 0;
    k4a_device_t m_device = nullptr;
    k4abt_tracker_t m_tracker = nullptr;
    std::thread m_captureThread;
    std::function<void(const std::string&)> m_errorCallback = nullptr;
    std::function<void(uint8_t*, uint32_t, uint32_t, uint32_t)> m_imageCallback = nullptr;

    [[nodiscard]] bool initCamera() noexcept;

    [[nodiscard]] bool initOutput() noexcept;

    void cleanupOutput() noexcept;

    /**
     * Run image acquisition and processing.
     * @note init() must be called before this function can be used.
     * @param ready (Optional) The callback used to signal camera is ready for operations.
     * @returns True if it succeeds, false if it fails.
     */
    [[nodiscard]] bool run(const std::function<void()>& ready = nullptr) noexcept;

    /** Cleanup any resources created during init(). */
    void cleanup() noexcept;
};
} // namespace Ak
