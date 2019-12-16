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

#include <QOpenGLExtraFunctions>
#include <QOpenGLWidget>

namespace Ak {
class KinectWidget final
    : public QOpenGLWidget
    , protected QOpenGLExtraFunctions
{
    Q_OBJECT

public:
    explicit KinectWidget(QWidget* parent) noexcept;

    ~KinectWidget() noexcept override;

    /**
     * Sets what types of data should be rendered.
     * @param depthImage   True to render depth image.
     * @param colourImage  True to render colour image.
     * @param irImage      True to render IR image.
     * @param bodyShadow   True to render body shadow.
     * @param bodySkeleton True to render body skeleton.
     */
    void setRenderOptions(bool depthImage, bool colourImage, bool irImage, bool bodyShadow, bool bodySkeleton) noexcept;

    /**
     * Updates the calibration information for the camera
     * @param calibration The calibration data.
     */
    void updateCalibration(const KinectCalibration& calibration) noexcept;

public slots:

    /**
     * Slot used to receive thread safe, asynchronous image/position information.
     * @param depthImage  The depth image data.
     * @param colourImage The colour image data.
     * @param irImage     The IR image data.
     * @param shadowImage The body shadow image data.
     * @param joints      The joint data.
     */
    void dataSlot(KinectImage depthImage, KinectImage colourImage, KinectImage irImage, KinectImage shadowImage,
        KinectJoints joints) noexcept;

    /** Slot used to receive thread safe, asynchronous render update notifications. */
    void refreshRenderSlot() noexcept;

    /** Slot used to receive thread safe, asynchronous calibration update notifications. */
    void refreshCalibrationSlot() noexcept;

signals:
    /**
     * Signal used to pass asynchronous thread safe error messages.
     * @param message The message.
     */
    void errorSignal(const QString& message);

    /** Signal used to pass asynchronous thread safe render update notifications. */
    void refreshRenderSignal();

    /** Signal used to pass asynchronous thread safe calibration update notifications. */
    void refreshCalibrationSignal();

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

    bool m_depthImage = true;
    bool m_colourImage = false;
    bool m_irImage = false;
    bool m_bodyShadowImage = true;
    bool m_bodySkeletonImage = true;

    // Render shaders
    GLuint m_depthProgram = 0;
    GLuint m_colourProgram = 0;
    GLuint m_irProgram = 0;
    GLuint m_shadowProgram = 0;
    GLuint m_skeletonProgram = 0;

    // Screen quad
    GLuint m_quadVAO = 0;
    GLuint m_quadVBO = 0;
    GLuint m_quadIBO = 0;

    // Image textures
    GLuint m_depthTexture = 0;
    GLuint m_colourTexture = 0;
    GLuint m_irTexture = 0;
    GLuint m_shadowTexture = 0;

    struct SkeletonData
    {
        glm::mat4 m_mat1;
        glm::mat4 m_mat2;
        float m_confidence;
    };

    // Sphere data
    GLuint m_sphereVAO = 0;
    GLuint m_sphereVBO = 0;
    GLuint m_sphereIBO = 0;
    GLsizei m_sphereElements = 0;
    GLuint m_sphereInstanceBO = 0;
    std::vector<SkeletonData> m_sphereTransforms;

    // Cylinder data
    GLuint m_cylinderVAO = 0;
    GLuint m_cylinderVBO = 0;
    GLuint m_cylinderIBO = 0;
    GLsizei m_cylinderElements = 0;
    GLuint m_cylinderInstanceBO = 0;
    std::vector<SkeletonData> m_cylinderTransforms;

    // Camera data
    GLuint m_inverseResUBO = 0;
    GLuint m_cameraUBO = 0;
    GLuint m_transformUBO = 0;
    GLuint m_imageUBO = 0;

    // Calibration data
    KinectCalibration m_calibration;

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

    /**
     * Generates a sphere mesh.
     * @param tessU    The horizontal tessellation.
     * @param tessV    The vertical tessellation.
     * @param vertexBO The vertex buffer to write to.
     * @param indexBO  The index buffer to write to.
     * @returns The number of indices in the sphere.
     */
    GLsizei generateSphere(uint32_t tessU, uint32_t tessV, GLuint vertexBO, GLuint indexBO) noexcept;

    /**
     * Generates a cylinder mesh.
     * @param tessU    The horizontal tessellation.
     * @param vertexBO The vertex buffer to write to.
     * @param indexBO  The index buffer to write to.
     * @returns The number of indices in the sphere.
     */
    GLsizei generateCylinder(uint32_t tessU, GLuint vertexBO, GLuint indexBO) noexcept;
};
} // namespace Ak
