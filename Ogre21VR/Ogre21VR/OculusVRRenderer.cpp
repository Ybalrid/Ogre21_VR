#include "OculusVRRenderer.hpp"

OculusVRRenderer::OculusVRRenderer(int OpenGLMajor, int OpenGLMinor) : VRRenderer{ OpenGLMajor, OpenGLMinor },
mirrorTexture{ nullptr },
textureSwapchain{ nullptr },
layers{ nullptr },
currentFrameDisplayTime{ 0 },
frameCounter{ 0 },
currentIndex{ 0 },
renderTextureGLID{ 0 },
oculusRenderTextureGLID{ 0 },
fbo{ 0 }
{
	ovr_Initialize(nullptr);

	std::stringstream clientIdentifier;
	clientIdentifier << "EngineName: Ogre\n";
	clientIdentifier << "EngineVersion: 2.1 GL3+\n";

	ovr_IdentifyClient(clientIdentifier.str().c_str());

	if (ovr_Create(&session, &luid) != ovrSuccess)
	{
		running = false;
		ovr_Shutdown();
		return;
	}

	hmdDesc = ovr_GetHmdDesc(session);
}

OculusVRRenderer::~OculusVRRenderer()
{
	ovr_Destroy(session);
	ovr_Shutdown();
}

void OculusVRRenderer::renderAndSubmitFrame()
{
	ovr_GetTextureSwapChainCurrentIndex(session, textureSwapchain, &currentIndex);
	ovr_GetTextureSwapChainBufferGL(session, textureSwapchain, currentIndex, &oculusRenderTextureGLID);

	//Texture should be written at this point
	getOgreRoot()->renderOneFrame();

	messagePump();

	glCopyImageSubData(renderTextureGLID, GL_TEXTURE_2D, 0, 0, 0, 0,
					   oculusRenderTextureGLID, GL_TEXTURE_2D, 0, 0, 0, 0,
					   bufferSize.w, bufferSize.h, 1);

	layers = &layer.Header;
	ovr_CommitTextureSwapChain(session, textureSwapchain);
	ovr_SubmitFrame(session, 0, nullptr, &layers, 1);
}

void OculusVRRenderer::updateTracking()
{
	ts = ovr_GetTrackingState(session, currentFrameDisplayTime = ovr_GetPredictedDisplayTime(session, 0), ovrTrue);

	pose = ts.HeadPose.ThePose;

	ovr_CalcEyePoses(pose, offset.data(), layer.RenderPose);
	cameraRig->setOrientation(oculusToOgreQuat(pose.Orientation));
	cameraRig->setPosition(oculusToOgreVect3(pose.Position));
}

Ogre::Quaternion OculusVRRenderer::oculusToOgreQuat(const ovrQuatf& q) { return Ogre::Quaternion{ q.w, q.x, q.y, q.z }; }
Ogre::Vector3 OculusVRRenderer::oculusToOgreVect3(const ovrVector3f& v) { return Ogre::Vector3{ v.x, v.y, v.z }; }

void OculusVRRenderer::initVRHardware()
{
	const auto texSizeL = ovr_GetFovTextureSize(session, ovrEye_Left, hmdDesc.DefaultEyeFov[0], 1);
	const auto texSizeR = ovr_GetFovTextureSize(session, ovrEye_Right, hmdDesc.DefaultEyeFov[1], 1);

	bufferSize.w = texSizeL.w + texSizeR.w;
	bufferSize.h = std::max(texSizeL.h, texSizeR.h);

	ovrTextureSwapChainDesc textureSwapChainDesc = {};
	textureSwapChainDesc.Type = ovrTexture_2D;
	textureSwapChainDesc.ArraySize = 1;
	textureSwapChainDesc.Format = OVR_FORMAT_R8G8B8A8_UNORM_SRGB;
	textureSwapChainDesc.Width = bufferSize.w;
	textureSwapChainDesc.Height = bufferSize.h;
	textureSwapChainDesc.MipLevels = 1;
	textureSwapChainDesc.SampleCount = 1;
	textureSwapChainDesc.StaticImage = ovrFalse;

	if (ovr_CreateTextureSwapChainGL(session, &textureSwapChainDesc, &textureSwapchain) != ovrSuccess)
		throw std::runtime_error("texture swap-chain cannot be created!");

	ovr_GetTextureSwapChainBufferGL(session, textureSwapchain, 0, &renderTextureGLID);

	rttTexture = getOgreRoot()->getTextureManager()->
		createManual(rttTextureName,
					 Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME,
					 Ogre::TEX_TYPE_2D, bufferSize.w, bufferSize.h, 0,
					 Ogre::PF_R8G8B8A8, Ogre::TU_RENDERTARGET);

	rttTexture->getCustomAttribute("GLID", &renderTextureGLID);

	auto compositor = getOgreRoot()->getCompositorManager2();

	if (!compositor->hasWorkspaceDefinition(stereoscopicCompositor))
		compositor->createBasicWorkspaceDef(stereoscopicCompositor, backgroundColor);

	Ogre::uint8 modifierMask, executionMask;
	Ogre::Vector4 OffsetScale;

	modifierMask = 0x01;
	executionMask = 0x01;
	OffsetScale = Ogre::Vector4{ 0, 0, 0.5f, 1 };
	compositor->addWorkspace(smgr, rttTexture->getBuffer()->getRenderTarget(), stereoCameras[0],
							 stereoscopicCompositor, true, -1, OffsetScale, modifierMask, executionMask);

	modifierMask = 0x02;
	executionMask = 0x02;
	OffsetScale = Ogre::Vector4{ 0.5f, 0, 0.5f, 1 };
	compositor->addWorkspace(smgr, rttTexture->getBuffer()->getRenderTarget(), stereoCameras[1],
							 stereoscopicCompositor, true, -1, OffsetScale, modifierMask, executionMask);

	//Populate OVR structures
	EyeRenderDesc[0] = ovr_GetRenderDesc(session, ovrEye_Left, hmdDesc.DefaultEyeFov[0]);
	EyeRenderDesc[1] = ovr_GetRenderDesc(session, ovrEye_Right, hmdDesc.DefaultEyeFov[1]);
	offset[0] = EyeRenderDesc[0].HmdToEyeOffset;
	offset[1] = EyeRenderDesc[1].HmdToEyeOffset;

	stereoCameras[0]->setPosition(oculusToOgreVect3(offset[0]));
	stereoCameras[1]->setPosition(oculusToOgreVect3(offset[1]));

	//Create a layer with our single swaptexture on it. Each side is an eye.
	layer.Header.Type = ovrLayerType_EyeFov;
	layer.Header.Flags = 0;
	layer.ColorTexture[0] = textureSwapchain;
	layer.ColorTexture[1] = textureSwapchain;
	layer.Fov[0] = EyeRenderDesc[0].Fov;
	layer.Fov[1] = EyeRenderDesc[1].Fov;
	ovrRecti leftRect, rightRect;
	leftRect.Size = bufferSize;
	leftRect.Size.w /= 2;
	rightRect = leftRect;
	leftRect.Pos.x = 0;
	leftRect.Pos.y = 0;
	rightRect.Pos.x = bufferSize.w / 2;
	rightRect.Pos.y = 0;
	layer.Viewport[0] = leftRect;
	layer.Viewport[1] = rightRect;

	setCorrectProjectionMatrix();
}

void OculusVRRenderer::setCorrectProjectionMatrix()
{
	const std::array<ovrMatrix4f, ovrEye_Count> oculusProjectionMatrix
	{
		ovrMatrix4f_Projection(EyeRenderDesc[ovrEye_Left].Fov, nearClippingDistance, farClippingDistance, 0),
		ovrMatrix4f_Projection(EyeRenderDesc[ovrEye_Right].Fov, nearClippingDistance, farClippingDistance, 0)
	};

	std::array<Ogre::Matrix4, 2> ogreProjectionMatrix{};

	for (const auto& eye : { 0, 1 })
	{
		for (auto x : { 0, 1, 2, 3 })
			for (auto y : { 0, 1, 2, 3 })
				ogreProjectionMatrix[eye][x][y] = oculusProjectionMatrix[eye].M[x][y];

		stereoCameras[eye]->setCustomProjectionMatrix(true, ogreProjectionMatrix[eye]);
	}
}