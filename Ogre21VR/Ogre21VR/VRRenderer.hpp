#pragma once
#include <Windows.h>

//OpenGL extension loading
#include <GL/gl3w.h>

//GLFW
#include <GLFW/glfw3.h>

//Native windows access (for getting the handle and the context)
#define GLFW_EXPOSE_NATIVE_WIN32
#define GLFW_EXPOSE_NAVIVE_WGL
#include <GLFW/glfw3native.h>

//C++ standard libraries
#include <iostream>
#include <memory>
#include <array>
#include <thread>

//Ogre 2 libraries
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
#include <OGRE/OgreItem.h>

///VRRenderer abstract class
class VRRenderer
{
public:
	///Construct a renderer
	VRRenderer(int openGLMajor = 4, int openGLMinor = 3);
	///Destruct a renderer
	virtual ~VRRenderer();

	///Initialize the VR hardware
	virtual void initVRHardware() = 0;
	///Render one frame and submit it to the VR system
	virtual void renderAndSubmitFrame() = 0;
	///Get tracking information and move the cameras accordingly
	virtual void updateTracking() = 0;

	///Set the near clipping distance
	void setNearClippingDistance(double d);
	///Set the far clipping distance
	void setFarClippingDistance(double d);

	void updateEvents();
	///Return true while the application should be running
	bool isRunning();
	///Return the scene manager
	Ogre::SceneManager* getSmgr();

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

	///Declare the HLMS library
	static void declareHlmsLibrary(const Ogre::String&& path);
	///Return the root object
	Ogre::Root* getOgreRoot() const;
	///This method is called to set the wanted projection matrix
	virtual void setCorrectProjectionMatrix() = 0;

private:
	///This load OpenGL "core" functions
	void loadOpenGLFunctions();
	///Initialize Ogre using a GLFW function
	void initOgre();
	///put all the camera attached to one single "camera rig" node
	void attachCameraToRig(Ogre::Camera* camera);

	std::unique_ptr<Ogre::Root> root;
	uint8_t threads;
	size_t width;
	size_t height;
	std::string windowName;
	static constexpr const char* const SL{ "GLSL" };
	GLFWwindow* glfwWindow;
	const int glMajor, glMinor;

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

	double nearClippingDistance;
	double farClippingDistance;

	Ogre::TexturePtr rttTexture;
};