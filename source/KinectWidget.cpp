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

#define _USE_MATH_DEFINES
#include "KinectWidget.h"

#include "AzureKinectWindow.h"

#include <QOpenGLContext>
#include <QResource>
#include <glm/gtc/matrix_transform.hpp>
using namespace glm;
using namespace std;

namespace Ak {
#if _DEBUG
static QString s_severity[] = {"High", "Medium", "Low", "Notification"};
static QString s_type[] = {"Error", "Deprecated", "Undefined", "Portability", "Performance", "Other"};
static QString s_source[] = {"OpenGL", "OS", "GLSL Compiler", "3rd Party", "Application", "Other"};
void APIENTRY debugCallback(const uint32_t source, const uint32_t type, uint32_t, const uint32_t severity, int32_t,
    const char* message, void* userParam)
{
    // Get the severity
    uint32_t sevID;
    switch (severity) {
        case GL_DEBUG_SEVERITY_HIGH:
            sevID = 0;
            break;
        case GL_DEBUG_SEVERITY_MEDIUM:
            sevID = 1;
            break;
        case GL_DEBUG_SEVERITY_LOW:
            sevID = 2;
            break;
        case GL_DEBUG_SEVERITY_NOTIFICATION:
        default:
            sevID = 3;
            break;
    }

    // Get the type
    uint32_t typeID;
    switch (type) {
        case GL_DEBUG_TYPE_ERROR:
            typeID = 0;
            break;
        case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
            typeID = 1;
            break;
        case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
            typeID = 2;
            break;
        case GL_DEBUG_TYPE_PORTABILITY:
            typeID = 3;
            break;
        case GL_DEBUG_TYPE_PERFORMANCE:
            typeID = 4;
            break;
        case GL_DEBUG_TYPE_OTHER:
        default:
            typeID = 5;
            break;
    }

    // Get the source
    uint32_t sourceID;
    switch (source) {
        case GL_DEBUG_SOURCE_API:
            sourceID = 0;
            break;
        case GL_DEBUG_SOURCE_WINDOW_SYSTEM:
            sourceID = 1;
            break;
        case GL_DEBUG_SOURCE_SHADER_COMPILER:
            sourceID = 2;
            break;
        case GL_DEBUG_SOURCE_THIRD_PARTY:
            sourceID = 3;
            break;
        case GL_DEBUG_SOURCE_APPLICATION:
            sourceID = 4;
            break;
        case GL_DEBUG_SOURCE_OTHER:
        default:
            sourceID = 5;
            break;
    }

    // Output to message callback
    if (severity == GL_DEBUG_SEVERITY_HIGH) {
        static_cast<KinectWidget*>(userParam)->errorSignal(QObject::tr("OpenGL Debug: Severity=") + s_severity[sevID] +
            QObject::tr(", Type=") + s_type[typeID] + QObject::tr(", Source=") + s_source[sourceID] +
            QObject::tr(" - ") + message);
    }
}
#endif

static array<pair<k4abt_joint_id_t, k4abt_joint_id_t>, 31> s_boneList = {
    make_pair(K4ABT_JOINT_SPINE_CHEST, K4ABT_JOINT_SPINE_NAVAL), make_pair(K4ABT_JOINT_SPINE_NAVAL, K4ABT_JOINT_PELVIS),
    make_pair(K4ABT_JOINT_SPINE_CHEST, K4ABT_JOINT_NECK), make_pair(K4ABT_JOINT_NECK, K4ABT_JOINT_HEAD),
    make_pair(K4ABT_JOINT_HEAD, K4ABT_JOINT_NOSE), make_pair(K4ABT_JOINT_SPINE_CHEST, K4ABT_JOINT_CLAVICLE_LEFT),
    make_pair(K4ABT_JOINT_CLAVICLE_LEFT, K4ABT_JOINT_SHOULDER_LEFT),
    make_pair(K4ABT_JOINT_SHOULDER_LEFT, K4ABT_JOINT_ELBOW_LEFT),
    make_pair(K4ABT_JOINT_ELBOW_LEFT, K4ABT_JOINT_WRIST_LEFT), make_pair(K4ABT_JOINT_WRIST_LEFT, K4ABT_JOINT_HAND_LEFT),
    make_pair(K4ABT_JOINT_HAND_LEFT, K4ABT_JOINT_HANDTIP_LEFT),
    make_pair(K4ABT_JOINT_WRIST_LEFT, K4ABT_JOINT_THUMB_LEFT), make_pair(K4ABT_JOINT_PELVIS, K4ABT_JOINT_HIP_LEFT),
    make_pair(K4ABT_JOINT_HIP_LEFT, K4ABT_JOINT_KNEE_LEFT), make_pair(K4ABT_JOINT_KNEE_LEFT, K4ABT_JOINT_ANKLE_LEFT),
    make_pair(K4ABT_JOINT_ANKLE_LEFT, K4ABT_JOINT_FOOT_LEFT), make_pair(K4ABT_JOINT_NOSE, K4ABT_JOINT_EYE_LEFT),
    make_pair(K4ABT_JOINT_EYE_LEFT, K4ABT_JOINT_EAR_LEFT),
    make_pair(K4ABT_JOINT_SPINE_CHEST, K4ABT_JOINT_CLAVICLE_RIGHT),
    make_pair(K4ABT_JOINT_CLAVICLE_RIGHT, K4ABT_JOINT_SHOULDER_RIGHT),
    make_pair(K4ABT_JOINT_SHOULDER_RIGHT, K4ABT_JOINT_ELBOW_RIGHT),
    make_pair(K4ABT_JOINT_ELBOW_RIGHT, K4ABT_JOINT_WRIST_RIGHT),
    make_pair(K4ABT_JOINT_WRIST_RIGHT, K4ABT_JOINT_HAND_RIGHT),
    make_pair(K4ABT_JOINT_HAND_RIGHT, K4ABT_JOINT_HANDTIP_RIGHT),
    make_pair(K4ABT_JOINT_WRIST_RIGHT, K4ABT_JOINT_THUMB_RIGHT), make_pair(K4ABT_JOINT_PELVIS, K4ABT_JOINT_HIP_RIGHT),
    make_pair(K4ABT_JOINT_HIP_RIGHT, K4ABT_JOINT_KNEE_RIGHT),
    make_pair(K4ABT_JOINT_KNEE_RIGHT, K4ABT_JOINT_ANKLE_RIGHT),
    make_pair(K4ABT_JOINT_ANKLE_RIGHT, K4ABT_JOINT_FOOT_RIGHT), make_pair(K4ABT_JOINT_NOSE, K4ABT_JOINT_EYE_RIGHT),
    make_pair(K4ABT_JOINT_EYE_RIGHT, K4ABT_JOINT_EAR_RIGHT)};

KinectWidget::KinectWidget(QWidget* parent) noexcept
    : QOpenGLWidget(parent)
{}

KinectWidget::~KinectWidget() noexcept
{
    makeCurrent();
    cleanup();
    doneCurrent();
}

void KinectWidget::setRenderOptions(const bool depthImage, const bool colourImage, const bool irImage,
    const bool bodyShadow, const bool bodySkeleton) noexcept
{
    m_depthImage = depthImage;
    m_colourImage = colourImage;
    m_irImage = irImage;
    m_bodyShadowImage = bodyShadow;
    m_bodySkeletonImage = bodySkeleton;

    // Update required render settings
    emit refreshRenderSignal();
}

void KinectWidget::updateCalibration(const KinectCalibration& calibration) noexcept
{
    m_calibration = calibration;

    // Update the internal buffers for the correct size
    emit refreshCalibrationSignal();
    emit refreshRenderSignal();
}

void KinectWidget::dataSlot(const KinectImage depthImage, const KinectImage colourImage, const KinectImage irImage,
    const KinectImage shadowImage, const KinectJoints joints) noexcept
{
    if (m_depthImage) {
        // Copy depth image data
        if (depthImage.m_stride / 2 != depthImage.m_width) {
            glPixelStorei(GL_UNPACK_ROW_LENGTH, depthImage.m_stride / 2);
        }
        glBindTexture(GL_TEXTURE_2D, m_depthTexture);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, depthImage.m_width, depthImage.m_height, GL_DEPTH_COMPONENT,
            GL_UNSIGNED_SHORT, reinterpret_cast<const GLvoid*>(depthImage.m_image));
        if (depthImage.m_stride / 2 != depthImage.m_width) {
            glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        }
        glBindTexture(GL_TEXTURE_2D, 0);
    } else if (m_colourImage) {
        // Copy colour image data
        if (colourImage.m_stride / 4 != colourImage.m_width) {
            glPixelStorei(GL_UNPACK_ROW_LENGTH, colourImage.m_stride / 4);
        }
        glBindTexture(GL_TEXTURE_2D, m_colourTexture);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, colourImage.m_width, colourImage.m_height, GL_BGRA, GL_UNSIGNED_BYTE,
            reinterpret_cast<const GLvoid*>(colourImage.m_image));
        if (colourImage.m_stride / 4 != colourImage.m_width) {
            glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        }
        glBindTexture(GL_TEXTURE_2D, 0);
    } else if (m_irImage) {
        // Copy colour image data
        if (irImage.m_stride / 2 != irImage.m_width) {
            glPixelStorei(GL_UNPACK_ROW_LENGTH, irImage.m_stride / 2);
        }
        glBindTexture(GL_TEXTURE_2D, m_irTexture);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, irImage.m_width, irImage.m_height, GL_DEPTH_COMPONENT,
            GL_UNSIGNED_SHORT, reinterpret_cast<const GLvoid*>(irImage.m_image));
        if (irImage.m_stride / 2 != irImage.m_width) {
            glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        }
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    if (m_bodyShadowImage) {
        // Copy shadow image data
        glBindTexture(GL_TEXTURE_2D, m_shadowTexture);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, shadowImage.m_width, shadowImage.m_height, GL_RED, GL_UNSIGNED_BYTE,
            reinterpret_cast<const GLvoid*>(shadowImage.m_image));
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    if (m_bodySkeletonImage) {
        m_sphereTransforms.resize(0);
        m_cylinderTransforms.resize(0);
        // Copy skeleton data
        if (joints.m_length > 0) {
            // Get joint positions
            mat4 scale = glm::scale(mat4(1.0f), vec3(0.034f));
            mat4 spaceConvert(1.0f);
            if (m_colourImage) {
                // Need to convert from depth space to colour space
                // TODO: These values are currently hardcoded from k4a calibration, they should really be passed from
                // k4a in case any settings are changed Note: These values obtained from
                // calibration->extrinsics[source_camera(0)][target_camera(1)]
                spaceConvert = mat4(vec4(0.999987721f, 0.00121299317f, 0.00480594439f, 0.0f),
                    vec4(-0.000721473712f, 0.994887710f, -0.100984767f, 0.0f),
                    vec4(-0.00490386877f, 0.100980058f, 0.994876385f, 0.0f),
                    vec4(-32.1208534f * 0.001f, -2.06779432f * 0.001f, 3.97831678f * 0.001f, 1.0f));
            }
            auto pointer = joints.m_joints;
            for (uint32_t i = 0; i < joints.m_length; ++i, ++pointer) {
                mat transform(spaceConvert * translate(mat4(1.0f), pointer->m_position.m_position * 0.001f) * scale);
                m_sphereTransforms.emplace_back(DualMat4{transform, transpose(inverse(transform))});
            }

            // Get bone positions
            for (auto& bone : s_boneList) {
                const k4abt_joint_id_t joint1 = bone.first;
                const k4abt_joint_id_t joint2 = bone.second;

                if (joints.m_joints[joint1].m_confident && joints.m_joints[joint2].m_confident) {
                    const vec3 start = joints.m_joints[joint1].m_position.m_position * 0.001f;
                    const vec3 end = joints.m_joints[joint2].m_position.m_position * 0.001f;

                    vec3 axis = end - start;
                    float length = glm::length(axis) - (0.034f * 2.0f); // Subtract sphere radius
                    vec3 position = start + (axis * 0.5f);

                    // Determine translation based on centre of the two joints
                    mat4 translation = translate(mat4(1.0f), position);

                    // Rotate so that the cylinders z-axis aligns with the bone direction
                    vec3 zAxis(0.0f, 0.0f, 1.0f);
                    mat3 rotation(1.0f);
                    vec3 u1 = normalize(axis);
                    vec3 v = cross(zAxis, u1);
                    float sinTheta = glm::length(v);
                    if (sinTheta > 0.00001f) {
                        float cosTheta = dot(zAxis, u1);
                        float scale2 = 1.0f / (1.0f + cosTheta);
                        mat3 vx(vec3(0.0f, v[2], -v[1]), vec3(-v[2], 0.0f, v[0]), vec3(v[1], -v[0], 0.0f));
                        mat3 vx2 = vx * vx;
                        mat3 vx2Scaled = vx2 * scale2;
                        rotation += vx + vx2Scaled;
                    }

                    // Combine rotations
                    mat4 model = translation * mat4(rotation);

                    // Scale is based on the new length of the cylinder
                    mat4 scale2 = glm::scale(mat4(1.0f), vec3(0.014f, 0.014f, length));

                    mat4 transform = spaceConvert * model * scale2;
                    m_cylinderTransforms.emplace_back(DualMat4{transform, transpose(inverse(transform))});
                }
            }
            // TODO: Render differently based on confidence level
        }
    }

    // Signal that widget needs to be rendered with new data
    update();
}

void KinectWidget::refreshRenderSlot() noexcept
{
    // This means that the display image has changed so we must update values

    // Update the view/projection matrix
    const mat4 view = lookAt(vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 0.0f, 1.0f), vec3(0.0, -1.0, 0.0));
    mat4 projection;
    if (m_depthImage) {
        projection = perspective(
            radians(m_calibration.m_depthFOV.y), m_calibration.m_depthFOV.x / m_calibration.m_depthFOV.y, 0.01f, 30.0f);
    } else if (m_colourImage) {
        projection = perspective(radians(m_calibration.m_colourFOV.y),
            m_calibration.m_colourFOV.x / m_calibration.m_colourFOV.y, 0.01f, 30.0f);
    } else {
        projection = perspective(
            radians(m_calibration.m_irFOV.y), m_calibration.m_irFOV.x / m_calibration.m_irFOV.y, 0.01f, 30.0f);
    }
    projection[0][0] = -projection[0][0]; // Fix for mirroring
    mat4 viewProjection = projection * view;
    glBindBuffer(GL_UNIFORM_BUFFER, m_cameraUBO);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(mat4), &viewProjection, GL_DYNAMIC_DRAW);

    // Update transform buffer
    glBindBuffer(GL_UNIFORM_BUFFER, m_transformUBO);
    if (m_depthImage) {
        glBufferData(GL_UNIFORM_BUFFER, sizeof(BrownConradyTransform), &m_calibration.m_depthBC, GL_STATIC_DRAW);
    } else if (m_colourImage) {
        glBufferData(GL_UNIFORM_BUFFER, sizeof(BrownConradyTransform), &m_calibration.m_colourBC, GL_STATIC_DRAW);
    } else {
        glBufferData(GL_UNIFORM_BUFFER, sizeof(BrownConradyTransform), &m_calibration.m_irBC, GL_STATIC_DRAW);
    }

    vec2 inverseRes;
    if (m_depthImage) {
        inverseRes = 1.0f / vec2(m_calibration.m_depthDimensions);
    } else if (m_colourImage) {
        inverseRes = 1.0f / vec2(m_calibration.m_colourDimensions);
    } else if (m_irImage) {
        inverseRes = 1.0f / vec2(m_calibration.m_irDimensions);
    }
    glBindBuffer(GL_ARRAY_BUFFER, m_imageUBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vec2), &inverseRes, GL_STATIC_DRAW);

    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    // Update viewport in case of aspect ratio change
    resizeGL(width(), height());
}

void KinectWidget::refreshCalibrationSlot() noexcept
{
    const uint32_t maxSize = glm::max(m_calibration.m_depthDimensions.x * m_calibration.m_depthDimensions.y * 2,
        m_calibration.m_colourDimensions.x * m_calibration.m_colourDimensions.y * 4);

    // Resize depth texture
    glBindTexture(GL_TEXTURE_2D, m_depthTexture);
    glTexStorage2D(
        GL_TEXTURE_2D, 1, GL_DEPTH_COMPONENT16, m_calibration.m_depthDimensions.x, m_calibration.m_depthDimensions.y);
    std::vector<uint8_t> initialBlank(maxSize, 0);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_calibration.m_depthDimensions.x, m_calibration.m_depthDimensions.y,
        GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, reinterpret_cast<const GLvoid*>(initialBlank.data()));

    // Resize colour texture
    glBindTexture(GL_TEXTURE_2D, m_colourTexture);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, m_calibration.m_colourDimensions.x, m_calibration.m_colourDimensions.y);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_calibration.m_colourDimensions.x, m_calibration.m_colourDimensions.y,
        GL_BGRA, GL_UNSIGNED_BYTE, reinterpret_cast<const GLvoid*>(initialBlank.data()));

    // Resize IR texture
    glBindTexture(GL_TEXTURE_2D, m_irTexture);
    glTexStorage2D(
        GL_TEXTURE_2D, 1, GL_DEPTH_COMPONENT16, m_calibration.m_irDimensions.x, m_calibration.m_irDimensions.y);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_calibration.m_irDimensions.x, m_calibration.m_irDimensions.y,
        GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, reinterpret_cast<const GLvoid*>(initialBlank.data()));

    // Resize shadow texture
    glBindTexture(GL_TEXTURE_2D, m_shadowTexture);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_R8, m_calibration.m_depthDimensions.x, m_calibration.m_depthDimensions.y);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_calibration.m_depthDimensions.x, m_calibration.m_depthDimensions.y,
        GL_RED, GL_UNSIGNED_BYTE, reinterpret_cast<const GLvoid*>(initialBlank.data()));
}

void KinectWidget::initializeGL() noexcept
{
    initializeOpenGLFunctions();

#if _DEBUG
    // Allow for synchronous callbacks.
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);

    // Set up the debug info callback
    glDebugMessageCallback(reinterpret_cast<GLDEBUGPROC>(&debugCallback), this);

    // Set up the type of debug information we want to receive
    uint32_t uiUnusedIDs = 0;
    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, &uiUnusedIDs, GL_TRUE); // Enable all
    glDebugMessageControl(
        GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_NOTIFICATION, 0, nullptr, GL_FALSE); // Disable notifications
#endif

    // Connect cleanup handler
    connect(context(), &QOpenGLContext::aboutToBeDestroyed, this, &KinectWidget::cleanup, Qt::DirectConnection);

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f); // Set the cleared back buffer to black
    glCullFace(GL_BACK);                  // Set back-face culling
    glEnable(GL_CULL_FACE);               // Enable use of back/front face culling
    glEnable(GL_DEPTH_TEST);              // Enable use of depth testing
    glDisable(GL_STENCIL_TEST);           // Disable stencil test for speed

    // Generate the full screen quad
    glGenVertexArrays(1, &m_quadVAO);
    glGenBuffers(1, &m_quadVBO);
    glGenBuffers(1, &m_quadIBO);
    glBindVertexArray(m_quadVAO);

    // Create VBO data
    glBindBuffer(GL_ARRAY_BUFFER, m_quadVBO);
    GLfloat vertexData[] = {-1.0f, -1.0f, 1.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f};
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertexData), vertexData, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), nullptr);
    glEnableVertexAttribArray(0);

    // Create IBO data
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_quadIBO);
    GLubyte indexData[] = {0, 1, 3, 1, 2, 3};
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_quadIBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indexData), indexData, GL_STATIC_DRAW);
    glBindVertexArray(0);

    // Create depth texture
    glGenTextures(1, &m_depthTexture);
    glBindTexture(GL_TEXTURE_2D, m_depthTexture);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_DEPTH_COMPONENT16, 640, 576);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    std::vector<uint8_t> initialBlank(1280 * 720 * 10, 0);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 640, 576, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT,
        reinterpret_cast<const GLvoid*>(initialBlank.data()));

    // Create colour texture
    glGenTextures(1, &m_colourTexture);
    glBindTexture(GL_TEXTURE_2D, m_colourTexture);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 1280, 720);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1280, 720, GL_BGRA, GL_UNSIGNED_BYTE,
        reinterpret_cast<const GLvoid*>(initialBlank.data()));

    // Create IR texture
    glGenTextures(1, &m_irTexture);
    glBindTexture(GL_TEXTURE_2D, m_irTexture);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_DEPTH_COMPONENT16, 640, 576);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 640, 576, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT,
        reinterpret_cast<const GLvoid*>(initialBlank.data()));

    // Create shadow texture
    glGenTextures(1, &m_shadowTexture);
    glBindTexture(GL_TEXTURE_2D, m_shadowTexture);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_R8, 640, 576);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 640, 576, GL_RED, GL_UNSIGNED_BYTE,
        reinterpret_cast<const GLvoid*>(initialBlank.data()));

    // Create sphere object
    glGenVertexArrays(1, &m_sphereVAO);

    // Create VBO and IBOs
    glGenBuffers(1, &m_sphereVBO);
    glGenBuffers(1, &m_sphereIBO);

    // Bind the Sphere VAO
    glBindVertexArray(m_sphereVAO);

    // Create Sphere VBO and IBO data
    m_sphereElements = generateSphere(12, 6, m_sphereVBO, m_sphereIBO);

    // Create sphere instance buffer
    glGenBuffers(1, &m_sphereInstanceBO);
    glBindBuffer(GL_ARRAY_BUFFER, m_sphereInstanceBO);
    for (uint32_t i = 0; i < 8; i++) {
        // Set up the vertex attribute
        glVertexAttribPointer(
            2 + i, 4, GL_FLOAT, GL_FALSE, sizeof(DualMat4), reinterpret_cast<void*>(sizeof(vec4) * i));
        glEnableVertexAttribArray(2 + i);
        glVertexAttribDivisor(2 + i, 1);
    }
    m_sphereTransforms.reserve(32);

    // Create cylinder object
    glGenVertexArrays(1, &m_cylinderVAO);

    // Create VBO and IBOs
    glGenBuffers(1, &m_cylinderVBO);
    glGenBuffers(1, &m_cylinderIBO);

    // Bind the cylinder VAO
    glBindVertexArray(m_cylinderVAO);

    // Create cylinder VBO and IBO data
    m_cylinderElements = generateCylinder(12, m_cylinderVBO, m_cylinderIBO);

    // Create cylinder instance buffer
    glGenBuffers(1, &m_cylinderInstanceBO);
    glBindBuffer(GL_ARRAY_BUFFER, m_cylinderInstanceBO);
    for (uint32_t i = 0; i < 8; i++) {
        // Set up the vertex attribute
        glVertexAttribPointer(
            2 + i, 4, GL_FLOAT, GL_FALSE, sizeof(DualMat4), reinterpret_cast<void*>(sizeof(vec4) * i));
        glEnableVertexAttribArray(2 + i);
        glVertexAttribDivisor(2 + i, 1);
    }
    glBindVertexArray(0);
    m_sphereTransforms.reserve(32);

    // Setup inverse resolution
    glGenBuffers(1, &m_inverseResUBO);
    vec2 inverseRes = 1.0f / vec2(1280.0f, 720.0f);
    glBindBuffer(GL_UNIFORM_BUFFER, m_inverseResUBO);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(vec2), &inverseRes, GL_STATIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 2, m_inverseResUBO);

    // Create the viewProjection buffers
    glGenBuffers(1, &m_cameraUBO);
    glBindBufferBase(GL_UNIFORM_BUFFER, 1, m_cameraUBO);

    // Create image space transform buffer
    glGenBuffers(1, &m_transformUBO);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, m_transformUBO);

    // Create image resolution buffer
    glGenBuffers(1, &m_imageUBO);
    glBindBufferBase(GL_UNIFORM_BUFFER, 3, m_imageUBO);

    // Bind defaults to prevent potential contamination
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    // Load shaders
    GLuint vertexShader;
    if (!loadShader(vertexShader, GL_VERTEX_SHADER, (GLchar*)QResource(":/AzureKinect/FullScreenQuad.vert").data())) {
        return;
    }
    GLuint fragmentShader;
    if (!loadShader(fragmentShader, GL_FRAGMENT_SHADER, (GLchar*)QResource(":/AzureKinect/DepthImage.frag").data())) {
        return;
    }
    if (!loadShaders(m_depthProgram, vertexShader, fragmentShader)) {
        return;
    }
    glDeleteShader(fragmentShader);

    if (!loadShader(fragmentShader, GL_FRAGMENT_SHADER, (GLchar*)QResource(":/AzureKinect/ColourImage.frag").data())) {
        return;
    }
    if (!loadShaders(m_colourProgram, vertexShader, fragmentShader)) {
        return;
    }
    glDeleteShader(fragmentShader);

    if (!loadShader(fragmentShader, GL_FRAGMENT_SHADER, (GLchar*)QResource(":/AzureKinect/IRImage.frag").data())) {
        return;
    }
    if (!loadShaders(m_irProgram, vertexShader, fragmentShader)) {
        return;
    }
    glDeleteShader(fragmentShader);

    if (!loadShader(fragmentShader, GL_FRAGMENT_SHADER, (GLchar*)QResource(":/AzureKinect/ShadowImage.frag").data())) {
        return;
    }
    if (!loadShaders(m_shadowProgram, vertexShader, fragmentShader)) {
        return;
    }

    // Clean up unneeded shaders
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    if (!loadShader(vertexShader, GL_VERTEX_SHADER, (GLchar*)QResource(":/AzureKinect/Skeleton.vert").data())) {
        return;
    }
    if (!loadShader(fragmentShader, GL_FRAGMENT_SHADER, (GLchar*)QResource(":/AzureKinect/Skeleton.frag").data())) {
        return;
    }
    if (!loadShaders(m_skeletonProgram, vertexShader, fragmentShader)) {
        return;
    }

    // Clean up unneeded shaders
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
}

void KinectWidget::resizeGL(const int width, const int height) noexcept
{
    // Keep the viewport at a fixed 16:9 ratio for colour and (10/9 for depth/ir)
    const uint32_t widthRatio = m_colourImage ? 16 : 10;
    constexpr uint32_t heightRatio = 9;
    int32_t newWidth = width, newHeight = height;
    do {
        const int32_t widthScale = ((newHeight * widthRatio) / heightRatio);
        const int32_t heightScale = ((newWidth * heightRatio) / widthRatio);
        const int32_t widthOffset = heightScale >= newHeight ? widthScale : newWidth;
        const int32_t heightOffset = widthScale >= newWidth ? heightScale : newHeight;
        newWidth = widthOffset;
        newHeight = heightOffset;
    } while (newWidth * heightRatio - newHeight * widthRatio != 0);
    m_viewportX = (width - newWidth) / 2;
    m_viewportY = (height - newHeight) / 2;
    m_viewportW = newWidth;
    m_viewportH = newHeight;

    // Update inverse resolution
    struct ResolutionBuffer
    {
        vec2 m_inverseRes;
        vec2 m_windowsOffset;
    };
    ResolutionBuffer resBuffer = {
        1.0f / vec2(static_cast<float>(m_viewportW), static_cast<float>(m_viewportH)), vec2(m_viewportX, m_viewportY)};
    glBindBuffer(GL_UNIFORM_BUFFER, m_inverseResUBO);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(ResolutionBuffer), &resBuffer, GL_STATIC_DRAW);

    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

void KinectWidget::paintGL() noexcept
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Set correct viewport as it gets changed by Qt
    glViewport(m_viewportX, m_viewportY, m_viewportW, m_viewportH);

    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    if (m_depthImage) {
        // Render depth image
        glUseProgram(m_depthProgram);
        glBindVertexArray(m_quadVAO);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, m_depthTexture);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, nullptr);
    } else if (m_colourImage) {
        // Render colour image
        glUseProgram(m_colourProgram);
        glBindVertexArray(m_quadVAO);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, m_colourTexture);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, nullptr);
    } else if (m_irImage) {
        // Render IR image
        glUseProgram(m_irProgram);
        glBindVertexArray(m_quadVAO);
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, m_irTexture);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, nullptr);
    }

    if (m_bodyShadowImage && !m_colourImage) {
        // Render body shadow
        glEnable(GL_BLEND); // Enable blending
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glUseProgram(m_shadowProgram);
        glBindVertexArray(m_quadVAO);
        glActiveTexture(GL_TEXTURE4);
        glBindTexture(GL_TEXTURE_2D, m_shadowTexture);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, nullptr);
        glDisable(GL_BLEND);
    }

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);

    if (m_bodySkeletonImage) {
        // Render skeleton joints
        glEnable(GL_BLEND); // Enable blending
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glUseProgram(m_skeletonProgram);
        if (m_sphereTransforms.size() > 0) {
            glBindBuffer(GL_ARRAY_BUFFER, m_sphereInstanceBO);
            glBufferData(GL_ARRAY_BUFFER, sizeof(DualMat4) * m_sphereTransforms.size(),
                reinterpret_cast<const GLvoid*>(m_sphereTransforms.data()), GL_STATIC_DRAW);
            glBindVertexArray(m_sphereVAO);
            glDrawElementsInstanced(GL_TRIANGLES, m_sphereElements, GL_UNSIGNED_INT, nullptr,
                static_cast<GLsizei>(m_sphereTransforms.size()));
        }
        if (m_cylinderTransforms.size() > 0) {
            glBindBuffer(GL_ARRAY_BUFFER, m_cylinderInstanceBO);
            glBufferData(GL_ARRAY_BUFFER, sizeof(DualMat4) * m_cylinderTransforms.size(),
                reinterpret_cast<const GLvoid*>(m_cylinderTransforms.data()), GL_STATIC_DRAW);
            glBindVertexArray(m_cylinderVAO);
            glDrawElementsInstanced(GL_TRIANGLES, m_cylinderElements, GL_UNSIGNED_INT, nullptr,
                static_cast<GLsizei>(m_cylinderTransforms.size()));
        }
        glDisable(GL_BLEND);
    }
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void KinectWidget::cleanup() noexcept
{
    // Free all resources
    glDeleteProgram(m_depthProgram);
    glDeleteProgram(m_colourProgram);
    glDeleteProgram(m_irProgram);
    glDeleteProgram(m_shadowProgram);
    glDeleteProgram(m_skeletonProgram);

    glDeleteBuffers(1, &m_quadVBO);
    glDeleteBuffers(1, &m_quadIBO);
    glDeleteVertexArrays(1, &m_quadVAO);

    glDeleteTextures(1, &m_depthTexture);
    glDeleteTextures(1, &m_colourTexture);
    glDeleteTextures(1, &m_irTexture);
    glDeleteTextures(1, &m_shadowTexture);

    glDeleteBuffers(1, &m_sphereVBO);
    glDeleteBuffers(1, &m_sphereIBO);
    glDeleteVertexArrays(1, &m_sphereVAO);
    glDeleteBuffers(1, &m_sphereInstanceBO);

    glDeleteBuffers(1, &m_cylinderVBO);
    glDeleteBuffers(1, &m_cylinderIBO);
    glDeleteVertexArrays(1, &m_cylinderVAO);
    glDeleteBuffers(1, &m_cylinderInstanceBO);

    glDeleteBuffers(1, &m_inverseResUBO);
    glDeleteBuffers(1, &m_cameraUBO);
    glDeleteBuffers(1, &m_transformUBO);
    glDeleteBuffers(1, &m_imageUBO);
}

bool KinectWidget::loadShader(GLuint& shader, const GLenum shaderType, const GLchar* shaderCode) noexcept
{
    // Build and link the shader program
    shader = glCreateShader(shaderType);
    glShaderSource(shader, 1, &shaderCode, nullptr);
    glCompileShader(shader);

    // Check for errors
    GLint testReturn;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &testReturn);
    if (testReturn == GL_FALSE) {
        GLchar infolog[1024];
        int32_t errorLength;
        glGetShaderInfoLog(shader, 1024, &errorLength, infolog);
        emit errorSignal(tr("Failed to compile shader: ") + infolog + shaderCode);
        glDeleteShader(shader);
        return false;
    }
    return true;
}

bool KinectWidget::loadShaders(GLuint& shader, const GLuint vertexShader, const GLuint fragmentShader) noexcept
{
    // Link the shaders
    shader = glCreateProgram();
    glAttachShader(shader, vertexShader);
    glAttachShader(shader, fragmentShader);
    glLinkProgram(shader);

    // Check for error in link
    GLint testReturn;
    glGetProgramiv(shader, GL_LINK_STATUS, &testReturn);
    if (testReturn == GL_FALSE) {
        GLchar infolog[1024];
        int32_t errorLength;
        glGetShaderInfoLog(shader, 1024, &errorLength, infolog);
        emit errorSignal(tr("Failed to link shaders: ") + infolog);
        glDeleteProgram(shader);
        return false;
    }
    return true;
}

GLsizei KinectWidget::generateSphere(
    const uint32_t tessU, const uint32_t tessV, const GLuint vertexBO, const GLuint indexBO) noexcept
{
    // Init params
    const float dPhi = static_cast<float>(M_PI) / static_cast<float>(tessV);
    const float dTheta = static_cast<float>(M_PI + M_PI) / static_cast<float>(tessU);

    // Determine required parameters
    const uint32_t numVertices = (tessU * (tessV - 1)) + 2;
    const uint32_t numIndices = (tessU * 6) + (tessU * (tessV - 2) * 6);

    // Create the new primitive
    vector<CustomVertex> vertexBuffer;
    vertexBuffer.reserve(numVertices);
    vector<GLuint> indexBuffer;
    indexBuffer.reserve(numIndices);

    // Set the top vertex
    vertexBuffer.emplace_back(vec3(0.0f, 1.0f, 0.0f), vec3(0.0f, 1.0f, 0.0f));

    float phi = dPhi;
    for (uint32_t i = 0; i < tessV - 1; i++) {
        // Calculate initial value
        const float rSinPhi = sinf(phi);
        const float rCosPhi = cosf(phi);

        const float y = rCosPhi;

        float theta = 0.0f;
        for (uint32_t j = 0; j < tessU; j++) {
            // Calculate positions
            const float cosTheta = cosf(theta);
            const float sinTheta = sinf(theta);

            // Determine position
            const float x = rSinPhi * cosTheta;
            const float z = rSinPhi * sinTheta;

            // Create vertex
            vertexBuffer.emplace_back(vec3(x, y, z), vec3(x, y, z));
            theta += dTheta;
        }
        phi += dPhi;
    }

    // Set the bottom vertex
    vertexBuffer.emplace_back(vec3(0.0f, -1.0f, 0.0f), vec3(0.0f, -1.0f, 0.0f));

    // Create top
    for (GLuint j = 1; j <= tessU; j++) {
        // Top triangles all share same vertex point at pos 0
        indexBuffer.emplace_back(0);
        // Loop back to start if required
        indexBuffer.emplace_back(((j + 1) > tessU) ? 1 : j + 1);
        indexBuffer.emplace_back(j);
    }

    // Create inner triangles
    for (GLuint i = 0; i < tessV - 2; i++) {
        for (GLuint j = 1; j <= tessU; j++) {
            // Create indexes for each quad face (pair of triangles)
            indexBuffer.emplace_back(j + (i * tessU));
            // Loop back to start if required
            const GLuint index = ((j + 1) > tessU) ? 1 : j + 1;
            indexBuffer.emplace_back(index + (i * tessU));
            indexBuffer.emplace_back(j + ((i + 1) * tessU));

            indexBuffer.emplace_back(*(indexBuffer.end() - 2));
            // Loop back to start if required
            indexBuffer.emplace_back(index + ((i + 1) * tessU));
            indexBuffer.emplace_back(*(indexBuffer.end() - 3));
        }
    }

    // Create bottom
    for (GLuint j = 1; j <= tessU; j++) {
        // Bottom triangles all share same vertex point at pos numVertices - 1
        indexBuffer.emplace_back(j + ((tessV - 2) * tessU));
        // Loop back to start if required
        const GLuint index = ((j + 1) > tessU) ? 1 : j + 1;
        indexBuffer.emplace_back(index + ((tessV - 2) * tessU));
        indexBuffer.emplace_back(numVertices - 1);
    }

    // Fill Vertex Buffer Object
    glBindBuffer(GL_ARRAY_BUFFER, vertexBO);
    glBufferData(GL_ARRAY_BUFFER, vertexBuffer.size() * sizeof(CustomVertex), vertexBuffer.data(), GL_STATIC_DRAW);

    // Fill Index Buffer Object
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indexBuffer.size() * sizeof(GLuint), indexBuffer.data(), GL_STATIC_DRAW);

    // Specify location of data within buffer
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(CustomVertex), nullptr);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(CustomVertex),
        reinterpret_cast<const GLvoid*>(offsetof(CustomVertex, m_normal)));
    glEnableVertexAttribArray(1);

    return numIndices;
}

GLsizei KinectWidget::generateCylinder(const uint32_t tessU, const GLuint vertexBO, const GLuint indexBO) noexcept
{
    // Init params
    const float dTheta = static_cast<float>(M_PI + M_PI) / static_cast<float>(tessU);

    // Determine required parameters
    const uint32_t numVertices = tessU * 4 + 2;
    const uint32_t numIndices = (tessU * 6) + (tessU * 6);

    // Create the new primitive
    std::vector<CustomVertex> vertexBuffer;
    vertexBuffer.reserve(numVertices);
    std::vector<GLuint> indexBuffer;
    indexBuffer.reserve(numIndices);

    // Set the top vertex
    vertexBuffer.emplace_back(CustomVertex{vec3(0.0f, 0.0f, -0.5f), vec3(0.0f, 0.0f, -1.0f)});

    // Create top ring
    float theta = 0.0f;
    for (uint32_t j = 0; j < tessU; j++) {
        // Calculate positions
        const float cosTheta = cosf(theta);
        const float sinTheta = sinf(theta);

        // Determine position
        const float x = cosTheta;
        const float y = sinTheta;

        // Create vertex
        vertexBuffer.emplace_back(CustomVertex{vec3(x, y, -0.5f), vec3(0.0f, 0.0f, -1.0f)});
        theta += dTheta;
    }

    // Create inner rings
    float z = -0.5f;
    for (uint32_t i = 0; i < 2; i++) {
        theta = 0.0f;
        for (uint32_t j = 0; j < tessU; j++) {
            // Calculate positions
            const float cosTheta = cosf(theta);
            const float sinTheta = sinf(theta);

            // Determine position
            const float x = cosTheta;
            const float y = sinTheta;

            // Create vertex
            vertexBuffer.emplace_back(CustomVertex{vec3(x, y, z), normalize(vec3(x, y, 0.0f))});
            theta += dTheta;
        }
        z = 0.5f;
    }

    // Create bottom ring
    theta = 0.0f;
    for (uint32_t j = 0; j < tessU; j++) {
        // Calculate positions
        const float cosTheta = cosf(theta);
        const float sinTheta = sinf(theta);

        // Determine position
        const float x = cosTheta;
        const float y = sinTheta;

        // Create vertex
        vertexBuffer.emplace_back(CustomVertex{vec3(x, y, 0.5f), vec3(0.0f, 0.0f, 1.0f)});
        theta += dTheta;
    }

    // Set the bottom vertex
    vertexBuffer.emplace_back(CustomVertex{vec3(0.0f, 0.0f, 0.5f), vec3(0.0f, 0.0f, 1.0f)});

    // Create top
    for (GLuint j = 1; j <= tessU; j++) {
        // Top triangles all share same vertex point at pos 0
        indexBuffer.emplace_back(0);
        // Loop back to start if required
        indexBuffer.emplace_back(((j + 1) > tessU) ? 1 : j + 1);
        indexBuffer.emplace_back(j);
    }

    // Create inner triangles
    for (GLuint j = 1; j <= tessU; j++) {
        // Create indexes for each quad face (pair of triangles)
        indexBuffer.emplace_back(j + (tessU));
        // Loop back to start if required
        const GLuint index = ((j + 1) > tessU) ? 1 : j + 1;
        indexBuffer.emplace_back(index + (tessU));
        indexBuffer.emplace_back(j + (2 * tessU));

        indexBuffer.emplace_back(*(indexBuffer.end() - 2));
        // Loop back to start if required
        indexBuffer.emplace_back(index + (2 * tessU));
        indexBuffer.emplace_back(*(indexBuffer.end() - 3));
    }

    // Create bottom
    for (GLuint j = 1; j <= tessU; j++) {
        // Bottom triangles all share same vertex point at pos numVertices - 1
        indexBuffer.emplace_back(j + (3 * tessU));
        // Loop back to start if required
        const GLuint index = ((j + 1) > tessU) ? 1 : j + 1;
        indexBuffer.emplace_back(index + (3 * tessU));
        indexBuffer.emplace_back(numVertices - 1);
    }

    // Fill Vertex Buffer Object
    glBindBuffer(GL_ARRAY_BUFFER, vertexBO);
    glBufferData(GL_ARRAY_BUFFER, vertexBuffer.size() * sizeof(CustomVertex), vertexBuffer.data(), GL_STATIC_DRAW);

    // Fill Index Buffer Object
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indexBuffer.size() * sizeof(GLuint), indexBuffer.data(), GL_STATIC_DRAW);

    // Specify location of data within buffer
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(CustomVertex), nullptr);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(CustomVertex),
        reinterpret_cast<const GLvoid*>(offsetof(CustomVertex, m_normal)));
    glEnableVertexAttribArray(1);

    return numIndices;
}
} // namespace Ak
