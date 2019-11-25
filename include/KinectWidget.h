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

#include <QOpenGLFunctions>
#include <QOpenGLWidget>

namespace Ak {
class KinectWidget final
    : public QOpenGLWidget
    , protected QOpenGLFunctions
{
public:
    explicit KinectWidget(QWidget* parent) noexcept;

    ~KinectWidget() noexcept override;

protected:
    void initializeGL() noexcept override;

    void resizeGL(int width, int height) noexcept override;

    void paintGL() noexcept override;

private:
    void cleanup() noexcept;
};
} // namespace Ak
