#pragma once
#include <Windows.h>
#include <GL/gl3w.h>
#include <GLFW/glfw3.h>

#define GLFW_EXPOSE_NATIVE_WIN32
#define GLFW_EXPOSE_NAVIVE_WGL
#include <GLFW/glfw3native.h>

#include <iostream>
#include <memory>
#include <array>
#include <thread>

#include <OGRE/Ogre.h>
#include <OGRE/OgreSceneNode.h>
#include <OGRE/OgreCamera.h>
#include <OGRE/OgreMeshManager.h>
#include <OGRE/OgreMeshManager2.h>
#include <OGRE/OgreMesh.h>
#include <OGRE/OgreMesh2.h>
#include <OGRE/Compositor/OgreCompositorManager2.h>
#include <OGRE/Compositor/OgreCompositorWorkspaceDef.h>
#include <OGRE/Hlms/Pbs/OgreHlmsPbs.h>
#include <OGRE/Hlms/Unlit/OgreHlmsUnlit.h>
#include <OGRE/OgreHlmsManager.h>
#include <OGRE/OgreHlms.h>

#include <OVR_CAPI.h>
#include <OVR_CAPI_GL.h>
#include <Extras/OVR_Math.h>

auto logToOgre = [](const std::string& message) { Ogre::LogManager::getSingleton().logMessage(message); };

class VRRenderer
{
public:
	virtual ~VRRenderer()
	{
	}

	VRRenderer() :
		root{ nullptr },
		threads{ uint8_t(std::thread::hardware_concurrency() / 2) },
		monoscopicCompositor{ "MonoscopicWorspace" },
		stereoscopicCompositor{ "StereoscopicWorkspace" },
		running{ false },
		smgr{ nullptr },
		backgroundColor{ 0.2f, 0.4f, 0.6f },
		AALevel{ 4 }
	{
		width = 1024;
		height = 768;
		windowName = "Window";

		initOgre();

		if (gl3wInit())
		{
			throw std::runtime_error("call to gl3wInit is unsuccessful");
		}

		if (!gl3wIsSupported(4, 3))
		{
			logToOgre("Cant call GL4.3 funcitons... :'(");
			throw std::runtime_error("Cannot call glCopyImageSubData because GL 4.3 is not supported");
		}

		logToOgre(std::string("OpenGL : ") + (char*)glGetString(GL_VERSION));
	}

	void initOgre()
	{
		Ogre::String pluginFile;
#ifdef _DEBUG
		pluginFile = "plugins_d.cfg";
#else
		pluginFile = "plugins.cfg";
#endif
		root = std::make_unique<Ogre::Root>(pluginFile, "dummy.cfg");
		root->setRenderSystem(root->getRenderSystemByName("OpenGL 3+ Rendering Subsystem"));
		root->initialise(false);

		Ogre::NameValuePairList windowParameters;

		//Use GLFW to create the window and the opengl context
		glfwInit();
		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
		glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

		GLFWwindow* glfWindow = glfwCreateWindow(width, height, windowName.c_str(), nullptr, nullptr);
		glfwMakeContextCurrent(glfWindow);

		//HGLRC context{ glfwGet
		HGLRC context{ wglGetCurrentContext() };
		HWND handle{ glfwGetWin32Window(glfWindow) };

		windowParameters["externalWindowHandle"] = std::to_string(size_t(handle));
		windowParameters["externalGLContext"] = std::to_string(size_t(context));

		window = root->createRenderWindow(windowName, width, height, false, &windowParameters);

		smgr = root->createSceneManager(Ogre::ST_GENERIC, threads, Ogre::INSTANCING_CULLING_THREADED);

		cameraRig = smgr->getRootSceneNode()->createChildSceneNode();

		attachCameraToRig(stereoCameras[0] = smgr->createCamera("LeftEyeVR"));
		attachCameraToRig(stereoCameras[1] = smgr->createCamera("RightEyeVR"));
		attachCameraToRig(monoCamera = smgr->createCamera("MonoCamera"));

		//Some stereo disparity. this value should be changed by the child class anyway
		stereoCameras[0]->setPosition(-0.063f / 2, 0, 0);
		stereoCameras[0]->setPosition(+0.063f / 2, 0, 0);

		auto compositor = root->getCompositorManager2();
		if (!compositor->hasWorkspaceDefinition(monoscopicCompositor))
			compositor->createBasicWorkspaceDef(monoscopicCompositor, backgroundColor);

		compositor->addWorkspace(smgr, window, monoCamera, monoscopicCompositor, true);

		//everything is right :
		running = true;
	}

	virtual void initVRHardware() = 0;

	virtual void renderAndSubmitFrame() = 0;
	virtual void updateTracking() = 0;

	static void messagePump()
	{
		Ogre::WindowEventUtilities::messagePump();
	}

	bool isRunning()
	{
		return running;
	}

	Ogre::SceneManager* getSmgr()
	{
		return smgr;
	}

	decltype(auto) loadV1mesh(Ogre::String meshName)
	{
		return Ogre::v1::MeshManager::getSingleton()
			.load(meshName,
				  Ogre::ResourceGroupManager::AUTODETECT_RESOURCE_GROUP_NAME,
				  Ogre::v1::HardwareBuffer::HBU_STATIC,
				  Ogre::v1::HardwareBuffer::HBU_STATIC);
	}

	decltype(auto) asV2mesh(Ogre::String meshName,
							Ogre::String ResourceGroup = Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME,
							Ogre::String sufix = " V2",
							bool halfPos = true,
							bool halfTextCoords = true,
							bool qTangents = true)
	{
		//Get the V1 mesh
		auto v1mesh = loadV1mesh(meshName);

		//Convert it as a V2 mesh
		auto mesh = Ogre::MeshManager::getSingletonPtr()->createManual(meshName + sufix, ResourceGroup);
		mesh->importV1(v1mesh.get(), halfPos, halfTextCoords, qTangents);

		//Unload the useless V1 mesh
		v1mesh->unload();
		v1mesh.setNull();

		//Return the shared pointer to the new mesh
		return mesh;
	}

	static void declareHlmsLibrary(const Ogre::String&& path)
	{
#ifdef _DEBUG
		if (std::string(SL) != "GLSL" || std::string(Ogre::Root::getSingleton().getRenderSystem()->getName()) != "OpenGL 3+ Rendering Subsystem")
			throw std::runtime_error("This function is OpenGL only. Please use the RenderSytem_GL3+ in the Ogre configuration!");
#endif
		auto hlmsFolder = path;

		//The hlmsFolder can come from a configuration file where it could be "empty" or set to "." or lacking the trailing "/"
		if (hlmsFolder.empty()) hlmsFolder = "./";
		else if (hlmsFolder[hlmsFolder.size() - 1] != '/') hlmsFolder += "/";

		//Get the hlmsManager (not a singleton by itself, but accessible via Root)
		auto hlmsManager = Ogre::Root::getSingleton().getHlmsManager();

		//Define the shader library to use for HLMS
		auto library = Ogre::ArchiveVec();
		auto archiveLibrary = Ogre::ArchiveManager::getSingletonPtr()->load(hlmsFolder + "Hlms/Common/" + SL, "FileSystem", true);
		library.push_back(archiveLibrary);

		//Define "unlit" and "PBS" (physics based shader) HLMS
		auto archiveUnlit = Ogre::ArchiveManager::getSingletonPtr()->load(hlmsFolder + "Hlms/Unlit/" + SL, "FileSystem", true);
		auto archivePbs = Ogre::ArchiveManager::getSingletonPtr()->load(hlmsFolder + "Hlms/Pbs/" + SL, "FileSystem", true);
		auto hlmsUnlit = OGRE_NEW Ogre::HlmsUnlit(archiveUnlit, &library);
		auto hlmsPbs = OGRE_NEW Ogre::HlmsPbs(archivePbs, &library);
		hlmsManager->registerHlms(hlmsUnlit);
		hlmsManager->registerHlms(hlmsPbs);
}

	Ogre::Root* getOgreRoot() const
	{
		return root.get();
	}

private:

	void attachCameraToRig(Ogre::Camera* camera)
	{
		if (auto parent = camera->getParentSceneNode())
			parent->detachObject(camera);

		if (cameraRig)
			cameraRig->attachObject(camera);
	}

	std::unique_ptr <Ogre::Root> root;
	uint8_t threads;
	int width;
	int height;
	std::string windowName;
	static constexpr const char* const SL{ "GLSL" };
	GLFWwindow* glfWindow;

protected:

	const Ogre::IdString monoscopicCompositor, stereoscopicCompositor;

	bool running;
	Ogre::RenderWindow* window;
	Ogre::SceneManager* smgr;
	std::array<Ogre::Camera*, 2> stereoCameras;
	Ogre::Camera* monoCamera;
	Ogre::SceneNode* cameraRig;
	Ogre::ColourValue backgroundColor;
	uint8_t AALevel;
};

class OculusVRRenderer : public VRRenderer
{
public:
	OculusVRRenderer() : VRRenderer{},
		mirrorTexture{ nullptr },
		textureSwapchain{ nullptr },
		layers{ nullptr },
		currentFrameDisplayTime{ 0 },
		frameCounter{ 0 },
		currentIndex{ 0 },
		renderTextureGLID{ 0 },
		oculusRenderTextureGLID{ 0 }
	{
		ovr_Initialize(nullptr);

		std::stringstream clientIdentifier;
		clientIdentifier << "EngineName: no f---ing engine\n";
		clientIdentifier << "EngineVersion: 666\n";

		ovr_IdentifyClient(clientIdentifier.str().c_str());

		if (ovr_Create(&session, &luid) != ovrSuccess)
		{
			running = false;
			ovr_Shutdown();
			return;
		}

		hmdDesc = ovr_GetHmdDesc(session);
	}

	~OculusVRRenderer()
	{
		ovr_Destroy(session);
		ovr_Shutdown();
	}

	int toto = 0;
	void renderAndSubmitFrame() override
	{
		toto++;
		if (toto == 42)
		{
			rttTexture->getBuffer()->getRenderTarget()->writeContentsToFile("shouldBeBlue.bmp");
		}
		if (window->isClosed())
		{
			running = false;
			return;
		}

		//Texture should be written at this point
		getOgreRoot()->renderOneFrame();

		messagePump();

		ovr_GetTextureSwapChainCurrentIndex(session, textureSwapchain, &currentIndex);
		ovr_GetTextureSwapChainBufferGL(session, textureSwapchain, currentIndex, &oculusRenderTextureGLID);

		std::cerr << "Frame " << frameCounter << ": copy from " << renderTextureGLID << " to " << oculusRenderTextureGLID << '\n';

		glCopyImageSubData(renderTextureGLID, GL_TEXTURE_2D, 0, 0, 0, 0,
						   oculusRenderTextureGLID, GL_TEXTURE_2D, 0, 0, 0, 0,
						   bufferSize.w, bufferSize.h, 1);

		layers = &layer.Header;

		ovr_CommitTextureSwapChain(session, textureSwapchain);
		ovr_SubmitFrame(session, /*frameCounter*/0, nullptr, &layers, 1);
	}

	void updateTracking() override
	{
		//get tracking information
		//apply it to the camera

		ts = ovr_GetTrackingState(session, currentFrameDisplayTime = ovr_GetPredictedDisplayTime(session, /*++frameCounter*/0), ovrTrue);

		ovr_CalcEyePoses(pose, offset.data(), layer.RenderPose);
		cameraRig->setOrientation(oculusToOgreQuat(pose.Orientation));
		cameraRig->setPosition(oculusToOgreVect3(pose.Position));
	}

	Ogre::Quaternion oculusToOgreQuat(const ovrQuatf& q)
	{
		return Ogre::Quaternion{ q.w, q.x, q.y, q.z };
	}

	Ogre::Vector3 oculusToOgreVect3(const ovrVector3f& v)
	{
		return Ogre::Vector3{ v.x, v.y, v.z };
	}

	void initVRHardware() override
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
		{
			//TODO handle problem here
			throw std::runtime_error("texture swapchain cannot be created!");
		}

		auto textureManager = getOgreRoot()->getTextureManager();

		rttTexture = textureManager->createManual(rttTextureName, Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME, Ogre::TEX_TYPE_2D, bufferSize.w, bufferSize.h, 0,
												  Ogre::PF_R8G8B8A8, Ogre::TU_RENDERTARGET);

		rttTexture->getCustomAttribute("GLID", &renderTextureGLID);

		auto compositor = getOgreRoot()->getCompositorManager2();

		if (!compositor->hasWorkspaceDefinition(stereoscopicCompositor)) compositor->createBasicWorkspaceDef(stereoscopicCompositor, backgroundColor);

		Ogre::uint8 modifierMask, executionMask;
		Ogre::Vector4 OffsetScale;

		modifierMask = 0x01;
		executionMask = 0x01;
		OffsetScale = Ogre::Vector4{ 0, 0, 0.5f, 1 };
		compositor->addWorkspace(smgr, rttTexture->getBuffer()->getRenderTarget(), stereoCameras[0], stereoscopicCompositor, true, -1, OffsetScale, modifierMask, executionMask);

		modifierMask = 0x02;
		executionMask = 0x02;
		OffsetScale = Ogre::Vector4{ 0.5f, 0, 0.5f, 1 };
		compositor->addWorkspace(smgr, rttTexture->getBuffer()->getRenderTarget(), stereoCameras[1], stereoscopicCompositor, true, -1, OffsetScale, modifierMask, executionMask);

		//Populate OVR structures
		EyeRenderDesc[0] = ovr_GetRenderDesc(session, ovrEye_Left, hmdDesc.DefaultEyeFov[0]);
		EyeRenderDesc[1] = ovr_GetRenderDesc(session, ovrEye_Right, hmdDesc.DefaultEyeFov[1]);
		offset[0] = EyeRenderDesc[0].HmdToEyeOffset;
		offset[1] = EyeRenderDesc[1].HmdToEyeOffset;

		//Create a layer with our single swaptexture on it. Each side is an eye.
		layer.Header.Type = ovrLayerType_EyeFov;
		layer.Header.Flags = 0;
		layer.ColorTexture[0] = textureSwapchain;
		layer.ColorTexture[1] = textureSwapchain;
		layer.Fov[0] = EyeRenderDesc[0].Fov;
		layer.Fov[1] = EyeRenderDesc[1].Fov;

		//Define the two viewports dimensions :
		ovrRecti leftRect, rightRect;
		leftRect.Size = bufferSize;													//same size than the buffer
		leftRect.Size.w /= 2;
		//leftRect.Size.w -= (frontierWidth / 2);										//but half the width
		rightRect = leftRect;														//The two rects are of the same size, but not at the same position

																					//Give OVR the position of the 2 viewports
		ovrVector2i leftPos, rightPos;
		leftPos.x = 0;																//The left one start at the bottom left corner
		leftPos.y = 0;
		rightPos = leftPos;
		rightPos.x = bufferSize.w - (bufferSize.w / 2) /*+ (frontierWidth / 2)*/;		//But the right start at half the buffer width
		leftRect.Pos = leftPos;
		rightRect.Pos = rightPos;

		//Assign the defined viewport to the layer
		layer.Viewport[0] = leftRect;
		layer.Viewport[1] = rightRect;
	}

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

	Ogre::TexturePtr rttTexture;
	static constexpr const char* const rttTextureName{ "RTT_TEX_HMD_BUFFER" };

	//TODO this should be a GLuint. Change that when some GL
	//typedef unsigned int GLuint;

	GLuint renderTextureGLID;
	GLuint oculusRenderTextureGLID;
};