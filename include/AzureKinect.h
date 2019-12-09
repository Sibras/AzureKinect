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

#include "DataTypes.h"

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

    using errorCallback = std::function<void(const std::string&)>;
    using readyCallback = std::function<void()>;
    using dataCallback = std::function<void(
        uint64_t, const KinectImage&, const KinectImage&, const KinectImage&, const KinectImage&, const KinectJoints&)>;

    /**
     * Initializes the azure kinect camera.
     * @param error (Optional) The callback used to signal errors.
     * @param ready (Optional) The callback used to signal camera is ready for operations.
     * @param data  (Optional) The callback used to signal updated image/position data.
     * @returns True if it succeeds, false if it fails.
     */
    bool init(errorCallback error = nullptr, readyCallback ready = nullptr, dataCallback data = nullptr) noexcept;

    /** Notify to shutdown.
     * @note This function is synchronous and will block until thread has completed.
     */
    void shutdown() noexcept;

private:
    std::atomic_bool m_shutdown = false;
    k4a_device_t m_device = nullptr;
    k4abt_tracker_t m_tracker = nullptr;
    std::thread m_captureThread;
    errorCallback m_errorCallback = nullptr;
    dataCallback m_dataCallback = nullptr;

    [[nodiscard]] bool initCamera() noexcept;

    /**
     * Run image acquisition and processing.
     * @note init() must be called before this function can be used.
     * @param ready (Optional) The callback used to signal camera is ready for operations.
     * @returns True if it succeeds, false if it fails.
     */
    [[nodiscard]] bool run(const readyCallback& ready = nullptr) noexcept;

    /** Cleanup any resources created during init(). */
    void cleanup() noexcept;
};
} // namespace Ak
