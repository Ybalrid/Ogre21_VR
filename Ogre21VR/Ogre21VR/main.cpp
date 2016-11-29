#include <memory>
#include "OculusVRRenderer.hpp"

void win32stdConsole()
{
	AllocConsole();
	// ReSharper disable once CppDeprecatedEntity
	freopen("CONOUT$", "w", stdout);
	std::cerr.rdbuf(std::cout.rdbuf());
}

Ogre::Quaternion anim()
{
	return Ogre::Quaternion(Ogre::Degree(Ogre::Root::getSingleton().getTimer()->getMilliseconds() / 10), Ogre::Vector3::UNIT_Y);
}

INT WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR strCmdLine, INT)
{
	win32stdConsole();
	std::unique_ptr<VRRenderer> Renderer = std::make_unique<OculusVRRenderer>(4, 5);

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
	SuzanneNode->setPosition(0, -1, -5);

	auto SunLight = smgr->createLight();
	auto SunNode = smgr->getRootSceneNode()->createChildSceneNode();
	SunNode->attachObject(SunLight);
	SunLight->setType(Ogre::Light::LT_DIRECTIONAL);
	SunLight->setPowerScale(Ogre::Math::PI * 3);
	SunLight->setDirection(Ogre::Vector3(-1, -3, -1).normalisedCopy());

	while (Renderer->isRunning())
	{
		SuzanneNode->setOrientation(anim());
		Renderer->updateTracking();
		Renderer->renderAndSubmitFrame();
	}

	return 0;
}