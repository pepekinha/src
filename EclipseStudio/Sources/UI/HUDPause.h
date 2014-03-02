#pragma once

#include "APIScaleformGfx.h"
#include "UIItemInventory.h"

class HUDPause
{
	bool	isActive_;
	bool	isInit;

	float	DisconnectAt; // time

private:
	r3dScaleformMovie gfxMovie;

	r3dScaleformMovie* prevKeyboardCaptureMovie;

	void	setMinimapPosition(const r3dPoint3D& pos, const r3dPoint3D& dir);

public:
	HUDPause();
	~HUDPause();

	bool 	Init();
	bool 	Unload();

	bool	IsInited() const { return isInit; }

	void 	Update();
	void 	Draw();

	bool	isActive() const { return isActive_; }
	void	Activate();
	void	Deactivate();

	void	showInventory();
	void	showMap();

	void	setTime(__int64 utcTime);

	void	reloadBackpackInfo();

	void	updateSurvivorTotalWeight();


private:
	void	eventBackpackGridSwap(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);
	void	eventBackpackDrop(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);
	void	eventBackpackUnloadClip(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);
	void	eventBackpackUseItem(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);
	void	eventChangeBackpack(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);
	void	eventMsgBoxCallback(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);
	void	eventBackToGame(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);
	void	eventQuitGame(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);
	void	eventShowMap(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);
	void	eventOptionsControlsRequestKeyRemap(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);
	void	eventOptionsControlsReset(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);
	void	eventOptionsApply(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);
	void	eventSendCallForHelp(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);

	void	AddSettingsChangeFlag( DWORD flag );
	void	SetNeedUpdateSettings();

	int		m_waitingForKeyRemap;

	UIItemInventory itemInventory_;
};
