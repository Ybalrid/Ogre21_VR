#include <memory>
#include "VRRenderer.hpp"

int main(void)
{
	std::unique_ptr<VRRenderer> Renderer = std::make_unique<OculusVRRenderer>();

	while (Renderer->isRunning())
	{
		Renderer->updateTracking();
		Renderer->renderAndSubmitFrame();
	}

	return 0;
}