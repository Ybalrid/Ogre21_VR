#pragma once

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

class VRRenderer
{
public:
	virtual ~VRRenderer() {}

	VRRenderer() :
		root{ nullptr },
		threads{ uint8_t(std::thread::hardware_concurrency() / 2) },
		monoscopicCompositor{ "MonoscopicWorspace" },
		stereoscopicCompositor{ "StereoscopicWorkspace" },
		running{ false },
		smgr{ nullptr },
		backgroundColor{ 0.2f, 0.4f, 0.6f }
	{
		initOgre();
	}

	void initOgre()
	{
		Ogre::String pluginFile;
#ifdef _DEBUG
		pluginFile = "plugins_d.cfg";
#else
		pluginFile = "plugins.cfg";
#endif
		root = std::make_unique<Ogre::Root>(pluginFile);
		root->setRenderSystem(root->getRenderSystemByName("OpenGL 3+ Rendering Subsystem"));
		root->initialise(false);
		window = root->createRenderWindow("Window", 1024, 768, false, nullptr);

		smgr = root->createSceneManager(Ogre::ST_GENERIC, threads, Ogre::INSTANCING_CULLING_THREADED);

		cameraRig = smgr->getRootSceneNode()->createChildSceneNode();

		attachCameraToRig(stereoCameras[0] = smgr->createCamera("LeftEyeVR"));
		attachCameraToRig(stereoCameras[1] = smgr->createCamera("RightEyeVR"));
		attachCameraToRig(monoCamera = smgr->createCamera("MonoCamera"));

		//Some stereo disparity. this value should be changed by the child class anyway
		stereoCameras[0]->setPosition(-0.063f / 2, 0, 0);
		stereoCameras[0]->setPosition(+0.063f / 2, 0, 0);

		auto compositor = root->getCompositorManager2();
		if (!compositor->hasWorkspaceDefinition(monoscopicCompositor)) compositor->createBasicWorkspaceDef(monoscopicCompositor, backgroundColor);
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

	static constexpr const char* const SL{ "GLSL" };

protected:

	const Ogre::IdString monoscopicCompositor, stereoscopicCompositor;

	bool running;
	Ogre::RenderWindow* window;
	Ogre::SceneManager* smgr;

	std::array<Ogre::Camera*, 2> stereoCameras;
	Ogre::Camera* monoCamera;

	Ogre::SceneNode* cameraRig;

	Ogre::ColourValue backgroundColor;
};

class OculusVRRenderer : public VRRenderer
{
public:
	OculusVRRenderer() : VRRenderer{}
	{
	}

	void renderAndSubmitFrame() override
	{
		if (window->isClosed())
		{
			running = false;
			return;
		}

		getOgreRoot()->renderOneFrame();

		messagePump();
	}

	void updateTracking() override
	{
	}

	void initVRHardware() override
	{
	}
};