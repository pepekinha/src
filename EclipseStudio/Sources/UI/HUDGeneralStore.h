#pragma once

#include "APIScaleformGfx.h"
#include "UIMarket.h"

class HUDGeneralStore
{
public:
	HUDGeneralStore();
	~HUDGeneralStore();

	bool Init();
	bool Unload();

	bool IsInited() const { return isInit_; }

	void Update();
	void Draw();

	bool isActive() const { return isActive_; }
	void Activate();
	void Deactivate();

	void eventReturnToGame(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);
	void eventBuyItem(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);

private:
	r3dScaleformMovie gfxMovie_;
	r3dScaleformMovie* prevKeyboardCaptureMovie_;

	bool isActive_;
	bool isInit_;

	UIMarket market_;
};