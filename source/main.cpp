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

#include "AzureKinectWindow.h"

#include <QtWidgets/QApplication>

using namespace Ak;

namespace Ak {
constexpr auto logFile = "LogFile.log";

void logHandler(const std::string& message)
{
    static std::mutex s_logLock;
    std::lock_guard<std::mutex> lock(s_logLock);

    std::ofstream log(logFile, std::ios::app);
    if (log.is_open()) {
        log << message << std::endl;
        log.close();
    }
}
} // namespace Ak

int main(int argc, char* argv[])
{
    // Clear previous log
    std::remove(logFile);

    QApplication a(argc, argv);

    // Set global OpenGL settings
    QSurfaceFormat format;
    format.setDepthBufferSize(24);
    format.setStencilBufferSize(8);
    format.setVersion(4, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
    format.setSwapInterval(1);
#if _DEBUG
    format.setOption(QSurfaceFormat::DebugContext);
#endif
    QSurfaceFormat::setDefaultFormat(format);

    // Show window
    AzureKinectWindow w;
    w.show();
    return a.exec();
}
