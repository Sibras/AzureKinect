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

#include <QOpenGLFunctions_4_3_Core>
#include <QOpenGLWidget>

namespace Ak {
class KinectWidget final
    : public QOpenGLWidget
    , protected QOpenGLFunctions_4_3_Core
{
    Q_OBJECT
public:
    explicit KinectWidget(QWidget* parent) noexcept;

    ~KinectWidget() noexcept override;

public slots:

    /**
     * Slot used to receive thread safe, asynchronous image information.
     * @param [in] imageData The depth image data array.
     * @param      width     The width.
     * @param      height    The height.
     * @param      stride    The stride.
     */
    void imageSlot(char* imageData, unsigned width, unsigned height, unsigned stride) noexcept;

signals:
    /**
     * Signal used to pass asynchronous thread safe error messages.
     * @param message The message.
     */
    void errorSignal(const QString& message);

protected:
    void initializeGL() noexcept override;

    void resizeGL(int width, int height) noexcept override;

    void paintGL() noexcept override;

private:
    // View port setting
    GLint m_viewportX = 0;
    GLint m_viewportY = 0;
    GLsizei m_viewportW = 0;
    GLsizei m_viewportH = 0;

    GLuint m_depthProgram = 0;

    // Screen quad
    GLuint m_quadVAO = 0;
    GLuint m_quadVBO = 0;
    GLuint m_quadIBO = 0;

    GLuint m_depthTexture = 0;

    // Cmera data
    GLuint m_inverseResUBO = 0;
    GLuint m_cameraUBO = 0;

    /** Cleanup any OpenGL resources */
    void cleanup() noexcept;

    /**
     * Loads a shader
     * @param [out] shader     The returned shader.
     * @param       shaderType Type of the shader (vertex, fragment supported).
     * @param       shaderCode The shader code.
     * @returns True if it succeeds, false if it fails.
     */
    bool loadShader(GLuint& shader, GLenum shaderType, const GLchar* shaderCode) noexcept;

    /**
     * Links shaders into final program.
     * @param [out] shader         The shader program.
     * @param       vertexShader   The vertex shader.
     * @param       fragmentShader The fragment shader.
     * @returns True if it succeeds, false if it fails.
     */
    bool loadShaders(GLuint& shader, GLuint vertexShader, GLuint fragmentShader) noexcept;
};
} // namespace Ak