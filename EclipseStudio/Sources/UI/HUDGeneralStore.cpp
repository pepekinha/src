#include "r3dPCH.h"
#include "r3d.h"

#include "HUDGeneralStore.h"

#include "FrontendShared.h"

HUDGeneralStore::HUDGeneralStore() :
isActive_(false),
isInit_(false),
prevKeyboardCaptureMovie_(NULL)
{
}

HUDGeneralStore::~HUDGeneralStore()
{
}

void HUDGeneralStore::eventReturnToGame(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
{
	Deactivate();
}

void HUDGeneralStore::eventBuyItem(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
{
	market_.eventBuyItem(pMovie, args, argCount);
}

bool HUDGeneralStore::Init()
{
 	if(!gfxMovie_.Load("Data\\Menu\\WarZ_HUD_GeneralStore.swf", false)) 
	{
 		return false;
	}

#define MAKE_CALLBACK(FUNC) new r3dScaleformMovie::TGFxEICallback<HUDGeneralStore>(this, &HUDGeneralStore::FUNC)
 	gfxMovie_.RegisterEventHandler("eventReturnToGame", MAKE_CALLBACK(eventReturnToGame));
	gfxMovie_.RegisterEventHandler("eventBuyItem", MAKE_CALLBACK(eventBuyItem));

	gfxMovie_.SetCurentRTViewport(Scaleform::GFx::Movie::SM_ExactFit);

	market_.initialize(&gfxMovie_);

	isActive_ = false;
	isInit_ = true;
	return true;
}

bool HUDGeneralStore::Unload()
{
 	gfxMovie_.Unload();
	isActive_ = false;
	isInit_ = false;
	return true;
}

void HUDGeneralStore::Update()
{
	market_.update();
}

void HUDGeneralStore::Draw()
{
 	gfxMovie_.UpdateAndDraw();
}

void HUDGeneralStore::Deactivate()
{
	if (market_.processing())
	{
		return;
	}

	Scaleform::GFx::Value var[1];
	var[0].SetString("menu_close");
	gfxMovie_.OnCommandCallback("eventSoundPlay", var, 1);

	if(prevKeyboardCaptureMovie_)
	{
		prevKeyboardCaptureMovie_->SetKeyboardCapture();
		prevKeyboardCaptureMovie_ = NULL;
	}

	if(!g_cursor_mode->GetInt())
	{
		r3dMouse::Hide();
	}

	isActive_ = false;
}

void HUDGeneralStore::Activate()
{
	prevKeyboardCaptureMovie_ = gfxMovie_.SetKeyboardCapture(); // for mouse scroll events

	r3d_assert(!isActive_);
	r3dMouse::Show();
	isActive_ = true;

	gfxMovie_.Invoke("_root.api.showMarketplace", 0);

	Scaleform::GFx::Value var[1];
	var[0].SetString("menu_open");
	gfxMovie_.OnCommandCallback("eventSoundPlay", var, 1);
}