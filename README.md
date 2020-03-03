# AzureKinect

AzureKinect is a program for viewing and recording of data from an Azure Kinect camera. It can can view/record the direct image data from the camera as well as perform body tracking calculations and visualisation.

![Screenshot](https://raw.githubusercontent.com/Sibras/AzureKinect/assets/Skeleton.png)

## Features

  - View Azure Kinect camera depth image real-time output
  - View Azure Kinect camera 4k colour image real-time output
  - View Azure Kinect camera Infra-red image real-time output
  - Use GPU accelerated AI based body tracking
  - View the calculated body tracking region of interest and determined skeletal positions
  - Record the calculated body tracking data to a csv file
  - Record any/all of the Azure Kinect cameras to h264 in real-time
  - Use CPU or GPU accelerated h264 encoding
  - 60fps visualisation, recording and processing

## Requirements

  - Microsoft Azure Kinect camera
  - NVIDIA GPU RTX2070 or better
  - Windows 10 April 2018 (x64) or newer
  - 4+ core Skylake or better CPU
  - 8GB+ memory
  - Recording simultaneous streams in real-time may require additional CPU resources
  
## Building

Compilation uses Visual Studio. To build the program simply open the supplied project.

The following dependencies are required in order to build:
  - Visual Studio 2019 or newer
  - Qt5 SDK
  - Qt Visual Studio Tools addon
  - Nuget (used to install Azure Kinect SDK, glm and FFmpeg)
