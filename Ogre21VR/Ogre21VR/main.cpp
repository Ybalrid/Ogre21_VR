#include <memory>
#include "OculusVRRenderer.hpp"

void win32stdConsole()
{
	AllocConsole();
	// ReSharper disable once CppDeprecatedEntity
	freopen("CONOUT$", "w", stdout);
	std::cerr.rdbuf(std::cout.rdbuf());
}

INT WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR strCmdLine, INT)
{
	win32stdConsole();
	std::unique_ptr<VRRenderer> Renderer = std::make_unique<OculusVRRenderer>(4, 3);

	Renderer->initVRHardware();
	Renderer->declareHlmsLibrary("HLMS");

	Ogre::ResourceGroupManager::getSingleton().addResourceLocation(".", "FileSystem");

	Ogre::ResourceGroupManager::getSingleton().initialiseAllResourceGroups();

	//load the V1 mesh file for Suzanne exported from Blender
	auto SuzanneMesh = Renderer->asV2mesh("Suzanne.mesh");
	auto smgr = Renderer->getSmgr();
	auto SuzanneNode = smgr->getRootSceneNode()->createChildSceneNode();
	auto SuzanneItem = smgr->createItem(SuzanneMesh);
	SuzanneNode->attachObject(SuzanneItem);
	SuzanneNode->setPosition(0, 0, -5);

	while (Renderer->isRunning())
	{
		Renderer->updateTracking();
		Renderer->renderAndSubmitFrame();
	}

	return 0;
}