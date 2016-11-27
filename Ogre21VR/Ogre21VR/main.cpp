#include <memory>
#include <GL/gl3w.h>

#include "VRRenderer.hpp"

//int main(int argc, char** argv)

void win32stdConsole()
{
	AllocConsole();
	// ReSharper disable once CppDeprecatedEntity
	auto f = freopen("CONOUT$", "w", stdout);
	std::cerr.rdbuf(std::cout.rdbuf());
}

INT WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR strCmdLine, INT)
{
	win32stdConsole();

	std::unique_ptr<VRRenderer> Renderer = std::make_unique<OculusVRRenderer>();

	Renderer->initVRHardware();

	//resources here

	Renderer->declareHlmsLibrary("HLMS");
	Ogre::ResourceGroupManager::getSingleton().initialiseAllResourceGroups();

	while (Renderer->isRunning())
	{
		Renderer->updateTracking();
		Renderer->renderAndSubmitFrame();
	}

	return 0;
}