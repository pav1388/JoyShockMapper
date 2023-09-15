#pragma once

#include "JoyShockMapper.h"
#include "PlatformDefinitions.h"

struct Indicator
{
	uint8_t led;
	uint8_t rgb[3];
};

class Gamepad
{

public:
	typedef function<void(uint8_t largeMotor, uint8_t smallMotor, Indicator indicator)> Callback;

protected:
	Gamepad() { }

public:
	static Gamepad* getNew(ControllerScheme scheme, Callback notification = nullptr);
	virtual ~Gamepad() { }

	virtual bool isInitialized(string* errorMsg = nullptr) const = 0;
	inline string getError() const
	{
		return _errorMsg;
	}

	virtual void setButton(KeyCode btn, bool pressed) = 0;
	virtual void setLeftStick(float x, float y) = 0;
	virtual void setRightStick(float x, float y) = 0;
	virtual void setStick(float x, float y, bool isLeft) = 0;
	virtual void setLeftTrigger(float) = 0;
	virtual void setRightTrigger(float) = 0;
	virtual void setGyro(float accelX, float accelY, float accelZ, float gyroX, float gyroY, float gyroZ) = 0;
	virtual void setTouchState(optional<FloatXY> press1, optional<FloatXY> press2) = 0;
	virtual void update() = 0;

	virtual ControllerScheme getType() const = 0;

protected:
	string _errorMsg;
};
