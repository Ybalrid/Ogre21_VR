#include "VRRenderer.hpp"

auto logToOgre = [](const std::string& message)
{
	Ogre::LogManager::getSingleton().logMessage(message);
};

VRRenderer::~VRRenderer()
{
	glfwTerminate();
}

VRRenderer::VRRenderer(int openGLMajor, int openGLMinor) :
	root{ nullptr },
	threads{ uint8_t(std::thread::hardware_concurrency() / 2) },
	width{ 1024 },
	height{ 768 },
	windowName{ "Window" },
	glMajor{ openGLMajor },
	glMinor{ openGLMinor },
	monoscopicCompositor{ "MonoscopicWorspace" },
	stereoscopicCompositor{ "StereoscopicWorkspace" },
	running{ false },
	smgr{ nullptr },
	backgroundColor{ 0.2f, 0.4f, 0.6f },
	AALevel{ 4 },
	nearClippingDistance{ 0.1 },
	farClippingDistance{ 1000 }
{
	initOgre();
	loadOpenGLFunctions();
}

void VRRenderer::loadOpenGLFunctions()
{
	//For some odd version, this thing returns something equivalent to `true` when it fails.
	if (gl3wInit()) { throw std::runtime_error("call to gl3wInit is unsuccessful"); }

	if (!gl3wIsSupported(4, 3))
	{
		logToOgre("Cant call GL4.3 functions... :'(");
		throw std::runtime_error("Not able to glCopyImageSubData because GL 4.3 is not supported.\n"
								 "You either need to update your drivers or buy a newer graphics card.\n"
								 "You need one that is quite fast for VR anyway.");
	}

	const auto getConstChar = [](const GLubyte* bytes) {return reinterpret_cast<const char*>(bytes); };

	logToOgre(std::string("OpenGL   : ") + getConstChar(glGetString(GL_VERSION)));
	logToOgre(std::string("GLSL     : ") + getConstChar(glGetString(GL_SHADING_LANGUAGE_VERSION)));
	logToOgre(std::string("Vendor   : ") + getConstChar(glGetString(GL_VENDOR)));
	logToOgre(std::string("Renderer : ") + getConstChar(glGetString(GL_RENDERER)));
}

void VRRenderer::initOgre()
{
	//Get the good "plugin" file to use according to the compilation mode
	Ogre::String pluginFile;
#ifdef _DEBUG
	pluginFile = "plugins_d.cfg";
#else
	pluginFile = "plugins.cfg";
#endif
	root = std::make_unique<Ogre::Root>(pluginFile);
	root->setRenderSystem(root->getRenderSystemByName("OpenGL 3+ Rendering Subsystem"));
	root->initialise(false);

	//This is needed to send special parameters to Ogre when creating a "render window"
	Ogre::NameValuePairList windowParameters;

	//Use GLFW to create the window and the opengl context
	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, glMajor);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, glMinor);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	//In order to create the context, we need to actually create a window on the screen
	glfwWindow = glfwCreateWindow(width, height, windowName.c_str(), nullptr, nullptr);
	//Now we can create the current context, running at the OpenGL version that WE CHOOSE
	glfwMakeContextCurrent(glfwWindow);

	//Get what we want out of this environment
	HGLRC context{ wglGetCurrentContext() };
	HWND handle{ glfwGetWin32Window(glfwWindow) };

	//populate theses parameters with the values above, as text, even for pointer types
	windowParameters["externalWindowHandle"] = std::to_string(size_t(handle));
	windowParameters["externalGLContext"] = std::to_string(size_t(context));

	//Create the window, the scene and the cameras
	window = root->createRenderWindow(windowName, width, height, false, &windowParameters);
	smgr = root->createSceneManager(Ogre::ST_GENERIC, threads, Ogre::INSTANCING_CULLING_THREADED);
	cameraRig = smgr->getRootSceneNode()->createChildSceneNode();
	attachCameraToRig(stereoCameras[0] = smgr->createCamera("LeftEyeVR"));
	attachCameraToRig(stereoCameras[1] = smgr->createCamera("RightEyeVR"));
	attachCameraToRig(monoCamera = smgr->createCamera("MonoCamera"));

	//Do some minor camera configuration :
	monoCamera->setNearClipDistance(nearClippingDistance);
	monoCamera->setFarClipDistance(farClippingDistance);
	monoCamera->setAutoAspectRatio(true);

	//Some stereo disparity. this value should be changed by the child class anyway, but it's nice to be able to test something.
	stereoCameras[0]->setPosition(-0.063f / 2, 0, 0);
	stereoCameras[0]->setPosition(+0.063f / 2, 0, 0);

	//Setup the rendering pipeline by creating a basic compositor workspace
	auto compositor = root->getCompositorManager2();
	if (!compositor->hasWorkspaceDefinition(monoscopicCompositor))
		compositor->createBasicWorkspaceDef(monoscopicCompositor, backgroundColor);
	compositor->addWorkspace(smgr, window, monoCamera, monoscopicCompositor, true);

	//everything is right :
	running = true;
}

void VRRenderer::setNearClippingDistance(double d)
{
	nearClippingDistance = d;
	setCorrectProjectionMatrix();
	monoCamera->setNearClipDistance(nearClippingDistance);
}

void VRRenderer::setFarClippingDistance(double d)
{
	farClippingDistance = d;
	setCorrectProjectionMatrix();
	monoCamera->setFarClipDistance(farClippingDistance);
}

void VRRenderer::updateEvents()
{
	Ogre::WindowEventUtilities::messagePump();
	glfwPollEvents();
	running = !glfwWindowShouldClose(glfwWindow);
}

bool VRRenderer::isRunning()
{
	return running;
}

Ogre::SceneManager* VRRenderer::getSmgr()
{
	return smgr;
}

void VRRenderer::declareHlmsLibrary(const Ogre::String&& path)
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

Ogre::Root* VRRenderer::getOgreRoot() const
{
	return root.get();
}

void VRRenderer::attachCameraToRig(Ogre::Camera* camera)
{
	if (auto parent = camera->getParentSceneNode())
		parent->detachObject(camera);

	if (cameraRig)
		cameraRig->attachObject(camera);
}