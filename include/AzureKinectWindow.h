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

#include "AzureKinect.h"
#include "ui_AzureKinect.h"

#include <QIntValidator>
#include <QtWidgets/QMainWindow>

namespace Ak {
class AzureKinectWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit AzureKinectWindow(QWidget* parent = Q_NULLPTR) noexcept;

public slots:
    void startSlot() noexcept;

    void exitSlot() noexcept;

private:
    Ui::AzureKinectClass m_ui;
    QValidator* m_validatorPID = nullptr;
    bool m_started = false;
    AzureKinect m_kinect;

    void closeEvent(QCloseEvent* event) noexcept override;

    void errorCallback(const std::string& message) noexcept;

    void readyCallback() const noexcept;
};
} // namespace Ak
