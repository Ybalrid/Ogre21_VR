#pragma once

#include "VRRenderer.hpp"

//Oculus Library
#include <OVR_CAPI.h>
#include <OVR_CAPI_GL.h>
#include <Extras/OVR_Math.h>

///VRRenderer implementation for the Oculus Rift
class OculusVRRenderer : public VRRenderer
{
public:
	///Construct the Oculus Renderer
	OculusVRRenderer(int OpenGLMajor = 4, int OpenGLMinor = 3);
	///Destruct the Oculus Renderer
	virtual ~OculusVRRenderer();
	///Render a frame to the render buffer and copy it to the Oculus Texture swapchain
	void renderAndSubmitFrame() override;
	///Update the tracking
	void updateTracking() override;
	///Convert an Oculus Quaternion to an Ogre quaternion
	static Ogre::Quaternion oculusToOgreQuat(const ovrQuatf& q);
	///Convert an Oculus Vector 3D to an Ogre Vector 3D
	static Ogre::Vector3 oculusToOgreVect3(const ovrVector3f& v);
	///Initialize the Oculus rendering
	void initVRHardware() override;
	///Get the projection matrix from the ovr rendering
	void setCorrectProjectionMatrix() override;

private:
	ovrSession session;
	ovrHmdDesc hmdDesc;
	ovrGraphicsLuid luid;
	ovrSizei bufferSize, hmdSize;
	ovrMirrorTexture mirrorTexture;
	ovrLayerEyeFov layer;
	ovrTextureSwapChain textureSwapchain;
	std::array<ovrVector3f, 2> offset;
	ovrPosef pose;
	ovrTrackingState ts;
	ovrLayerHeader* layers;
	ovrSessionStatus sessionStatus;
	ovrEyeRenderDesc EyeRenderDesc[2];

	double currentFrameDisplayTime;
	unsigned long long frameCounter;
	int currentIndex;

	static constexpr const char* const rttTextureName{ "RTT_TEX_HMD_BUFFER" };

	GLuint renderTextureGLID;
	GLuint oculusRenderTextureGLID;
	GLuint fbo;
};