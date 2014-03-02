#include "r3dPCH.h"
#include "r3d.h"

#include "../../../Eternity/sf/Console/config.h"
#include "HUDActionUI.h"
#include "LangMngr.h"

#include "FrontendShared.h"

#include "../multiplayer/clientgamelogic.h"
#include "../ObjectsCode/AI/AI_Player.H"
#include "../ObjectsCode/weapons/Weapon.h"
#include "../ObjectsCode/weapons/WeaponArmory.h"
#include "../GameLevel.h"

HUDActionUI::HUDActionUI()
{
	isActive_ = false;
	isInit = false;
}

HUDActionUI::~HUDActionUI()
{
}

bool HUDActionUI::Init()
{
 	if(!gfxMovie.Load("Data\\Menu\\WarZ_HUD_PickupUI.swf", false)) 
 		return false;
 
	gfxMovie.SetCurentRTViewport( Scaleform::GFx::Movie::SM_NoScale );

	isActive_ = false;
	isInit = true;
	return true;
}

bool HUDActionUI::Unload()
{
 	gfxMovie.Unload();
	isActive_ = false;
	isInit = false;
	return true;
}

void HUDActionUI::Update()
{
}

void HUDActionUI::Draw()
{
 	gfxMovie.UpdateAndDraw();
}

void HUDActionUI::setScreenPos(int x, int y)
{
	x -= (int)r3dRenderer->ScreenW2;
	y -= (int)r3dRenderer->ScreenH2;

	Scaleform::GFx::Value Main;
	gfxMovie.GetMovie()->GetVariable(&Main, "_root.Main");
	if(!Main.IsUndefined())
	{
		Scaleform::GFx::Value::DisplayInfo dinfo;
		Main.GetDisplayInfo(&dinfo);

		//dinfo.SetYRotation(45);
		dinfo.SetX(x);
		dinfo.SetY(y);

		Main.SetDisplayInfo(dinfo);
	}
}

void HUDActionUI::Deactivate()
{
	isActive_ = false;
}

void HUDActionUI::Activate()
{
	r3d_assert(!isActive_);
	isActive_ = true;
}

void HUDActionUI::setText(const wchar_t* title, const wchar_t* msg, const char* letter)
{
	if(!isInit) return;
	gfxMovie.Invoke("_root.api.showPlateText", msg);
	gfxMovie.Invoke("_root.api.showPlateTitle", title);
	gfxMovie.Invoke("_root.api.setEBlockLetter", letter);
}

void HUDActionUI::enableRegularBlock()
{
	if(!isInit) return;
	gfxMovie.Invoke("_root.api.setEBlockToRegular", "");
}

void HUDActionUI::enableProgressBlock()
{
	if(!isInit) return;
	gfxMovie.Invoke("_root.api.setEBlockToHold", "");
}

void HUDActionUI::setProgress(int value)
{
	if(!isInit) return;
	gfxMovie.Invoke("_root.api.setEBlockProgress", value);
}
