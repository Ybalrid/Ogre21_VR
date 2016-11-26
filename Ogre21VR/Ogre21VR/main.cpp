#include <memory>
#include "VRRenderer.hpp"

int main(void)
{
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