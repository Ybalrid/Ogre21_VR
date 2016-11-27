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
	OculusVRRenderer(int OpenGLMajor = 4, int OpenGLMinor = 3);
	virtual ~OculusVRRenderer();
	void renderAndSubmitFrame() override;
	void updateTracking() override;
	static Ogre::Quaternion oculusToOgreQuat(const ovrQuatf& q);
	static Ogre::Vector3 oculusToOgreVect3(const ovrVector3f& v);
	void initVRHardware() override;
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