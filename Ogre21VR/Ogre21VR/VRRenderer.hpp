#pragma once

class VRRenderer
{
public:
	virtual ~VRRenderer() {}

	VRRenderer() :
		running{ false }
	{
		initOgre();
	}

	void initOgre()
	{
		//everything is right :
		running = true;
	}

	virtual void renderAndSubmitFrame() = 0;
	virtual void updateTracking() = 0;

	bool isRunning()
	{
		return running;
	}

private:
protected:
	bool running;
};

class OculusVRRenderer : public VRRenderer
{
public:
	OculusVRRenderer() : VRRenderer{}
	{
	}

	void renderAndSubmitFrame() override
	{
	}

	void updateTracking() override
	{
	}
};