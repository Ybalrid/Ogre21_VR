#pragma once

#include <memory>
#include <OGRE/Ogre.h>

class VRRenderer
{
public:
	virtual ~VRRenderer() {}

	VRRenderer() :
		running{ false },
		root{ nullptr }
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
		window = root->createRenderWindow("Window", 800, 600, false, nullptr);

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

private:
	std::unique_ptr <Ogre::Root> root;
protected:
	bool running;
	Ogre::RenderWindow* window;
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
		messagePump();
	}

	void updateTracking() override
	{
	}

	void initVRHardware() override
	{
	}
};