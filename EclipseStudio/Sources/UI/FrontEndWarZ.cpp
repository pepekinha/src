#include "r3dPCH.h"
#include "r3d.h"

#include "GameCommon.h"
#include "r3dDebug.h"

#include "FrontendWarZ.h" 
#include "GameCode\UserFriends.h"
#include "GameCode\UserRewards.h"
#include "GameCode\UserSkills.h"
#include "GameCode\UserClans.h"
#include "GameCode\UserSettings.h"

#include "CkHttpRequest.h"
#include "CkHttpResponse.h"
#include "backend/HttpDownload.h"
#include "backend/WOBackendAPI.h"
#include "HUDDisplay.h"

#include "../rendering/Deffered/CommonPostFX.h"
#include "../rendering/Deffered/PostFXChief.h"

#include "multiplayer/MasterServerLogic.h"
#include "multiplayer/LoginSessionPoller.h"

#include "../ObjectsCode/weapons/WeaponArmory.h"
#include "../ObjectsCode/weapons/Weapon.h"
#include "../ObjectsCode/weapons/Ammo.h"
#include "../ObjectsCode/weapons/Gear.h"
#include "../ObjectsCode/ai/AI_Player.h"
#include "../ObjectsCode/ai/AI_PlayerAnim.h"
#include "../ObjectsCode/Gameplay/UIWeaponModel.h"
#include "GameLevel.h"
#include "Scaleform/Src/Render/D3D9/D3D9_Texture.h"
#include "../../Eternity/Source/r3dEternityWebBrowser.h"

#include "m_LoadingScreen.h"

#include "HWInfo.h"

#include "shellapi.h"
#include "SteamHelper.h"
#include "../Editors/CameraSpotsManager.h"

// for IcmpSendEcho
#undef NTDDI_VERSION
#define NTDDI_VERSION NTDDI_WINXP
#include <iphlpapi.h>
#include <icmpapi.h>
#pragma comment(lib, "iphlpapi.lib")

extern	char		_p2p_masterHost[MAX_PATH];
extern	int		_p2p_masterPort;
extern HUDDisplay*	hudMain;

char	Login_PassedUser[256] = "";
char	Login_PassedPwd[256] = "";
char	Login_PassedAuth[256] = "";
static int LoginMenuExitFlag = 0;

void writeGameOptionsFile();
extern r3dScreenBuffer*	Scaleform_RenderToTextureRT;

float getRatio(float num1, float num2)
{
	if(num1 == 0)
		return 0.0f;
	if(num2 == 0)
		return num1;
	
	return num1/num2;
}

const char* getTimePlayedString(int timePlayed) 
{
	int seconds = timePlayed%60;
	int minutes = (timePlayed/60)%60;
	int hours = (timePlayed/3600)%24;
	int days = (timePlayed/86400);

	static char tmpStr[64];
	sprintf(tmpStr, "%d:%02d:%02d", days, hours, minutes);
	return tmpStr;
}

const wchar_t* getReputationString(int Reputation)
{
	const wchar_t* algnmt = gLangMngr.getString("$rep_civilian");
	if(Reputation >= ReputationPoints::Paragon)
		algnmt = gLangMngr.getString("$rep_paragon");
	else if(Reputation >= ReputationPoints::Vigilante)
		algnmt = gLangMngr.getString("$rep_vigilante");
	else if(Reputation >= ReputationPoints::Guardian)
		algnmt = gLangMngr.getString("$rep_guardian");
	else if(Reputation >= ReputationPoints::Lawman)
		algnmt = gLangMngr.getString("$rep_lawmen");
	else if(Reputation >= ReputationPoints::Deputy)
		algnmt = gLangMngr.getString("$rep_deputy");	
	else if(Reputation >= ReputationPoints::Constable)
		algnmt = gLangMngr.getString("$rep_constable");
	else if(Reputation >= ReputationPoints::Civilian)
		algnmt = gLangMngr.getString("$rep_civilian");
	else if(Reputation <= ReputationPoints::Villain)
		algnmt = gLangMngr.getString("$rep_villain");
	else if(Reputation <= ReputationPoints::Assassin)
		algnmt = gLangMngr.getString("$rep_assassin");
	else if(Reputation <= ReputationPoints::Hitman)
		algnmt = gLangMngr.getString("$rep_hitman");
	else if(Reputation <= ReputationPoints::Bandit)
		algnmt = gLangMngr.getString("$rep_bandit");
	else if(Reputation <= ReputationPoints::Outlaw)
		algnmt = gLangMngr.getString("$rep_outlaw");
	else if(Reputation <= ReputationPoints::Thug)
		algnmt = gLangMngr.getString("$rep_thug");

	return algnmt;
}

FrontendWarZ::FrontendWarZ(const char * movieName)
: UIMenu(movieName)
, r3dIResource(r3dIntegrityGuardian())
{
	extern bool g_bDisableP2PSendToHost;
	g_bDisableP2PSendToHost = true;

	RTScaleformTexture = NULL;
	needReInitScaleformTexture = false;

	prevGameResult = GRESULT_Unknown;

	CancelQuickJoinRequest = false;
  	exitRequested_      = false;
  	needExitByGameJoin_ = false;
	needReturnFromQuickJoin = false;
	m_ReloadProfile = false;

	masterConnectTime_ = -1;
		
	m_Player = 0;
	m_needPlayerRenderingRequest = 0;
	m_CreateHeroID = 0;
	m_CreateBodyIdx = 0;
	m_CreateHeadIdx = 0;
	m_CreateLegsIdx = 0;
	
	m_joinGameServerId = 0;
	m_joinGamePwd[0]   = 0;

	loginThread = NULL;
	loginAnswerCode = ANS_Unactive;

	m_browseGamesMode = 0;

	m_leaderboardPage = 1;
	m_leaderboardPageCount = 1;
}

FrontendWarZ::~FrontendWarZ()
{
	r3d_assert(loginThread == NULL);

	if(m_Player)
	{
		GameWorld().DeleteObject(m_Player);

		extern void DestroyGame(); // destroy game only if player was loaded. to prevent double call to destroy game
		DestroyGame();
	}

	extern bool g_bDisableP2PSendToHost;
	g_bDisableP2PSendToHost = false;

	WorldLightSystem.Destroy();
}

unsigned int WINAPI FrontendWarZ::LoginProcessThread(void* in_data)
{
	r3dThreadAutoInstallCrashHelper crashHelper;
	FrontendWarZ* This = (FrontendWarZ*)in_data;

	r3d_assert(This->loginAnswerCode == ANS_Unactive);
	This->loginAnswerCode = ANS_Processing;
	gUserProfile.CustomerID = 0;

	CWOBackendReq req("api_lobuginek.aspx");
	req.AddParam("username", Login_PassedUser);
	req.AddParam("password", Login_PassedPwd);

	if(!req.Issue())
	{
		r3dOutToLog("Login FAILED, code: %d\n", req.resultCode_);
		This->loginAnswerCode = req.resultCode_ == 8 ? ANS_Timeout : ANS_Error;
		return 0;
	}

	int n = sscanf(req.bodyStr_, "%d %d %d", 
		&gUserProfile.CustomerID, 
		&gUserProfile.SessionID,
		&gUserProfile.AccountStatus);
	if(n != 3)
	{
		r3dOutToLog("Login: bad answer\n");
		This->loginAnswerCode = ANS_Error;
		return 0;
	}
	//r3dOutToLog("CustomerID: %d\n",gUserProfile.CustomerID);

	if(gUserProfile.CustomerID == 0)
		This->loginAnswerCode = ANS_BadPassword;
	else if(gUserProfile.AccountStatus >= 200)
		This->loginAnswerCode = ANS_Frozen;
	else
		This->loginAnswerCode = ANS_Logged;

	return 0;
}

void FrontendWarZ_LoginProcessThread(void *in_data)
{
	FrontendWarZ::LoginProcessThread(in_data);
}

unsigned int WINAPI FrontendWarZ::LoginAuthThread(void* in_data)
{
	r3dThreadAutoInstallCrashHelper crashHelper;
	FrontendWarZ* This = (FrontendWarZ*)in_data;

	r3d_assert(This->loginAnswerCode == ANS_Unactive);
	This->loginAnswerCode = ANS_Processing;
	r3d_assert(gUserProfile.CustomerID);
	r3d_assert(gUserProfile.SessionID);

	CWOBackendReq req(&gUserProfile, "api_LoginSessionPoller.aspx");
	if(req.Issue() == true)
	{
		This->loginAnswerCode = ANS_Logged;
		return true;
	}

	gUserProfile.CustomerID    = 0;
	gUserProfile.SessionID     = 0;
	gUserProfile.AccountStatus = 0;

	r3dOutToLog("LoginAuth: %d\n", req.resultCode_);
	This->loginAnswerCode = ANS_BadPassword;
	return 0;
}

bool FrontendWarZ::DecodeAuthParams()
{
	r3d_assert(Login_PassedAuth[0]);

	CkString s1;
	s1 = Login_PassedAuth;
	s1.base64Decode("utf-8");

	char* authToken = (char*)s1.getAnsi();
	for(size_t i=0; i<strlen(authToken); i++)
		authToken[i] = authToken[i] ^ 0x64;

	DWORD CustomerID = 0;
	DWORD SessionID = 0;
	DWORD AccountStatus = 0;
	int n = sscanf(authToken, "%d:%d:%d", &CustomerID, &SessionID, &AccountStatus);
	if(n != 3)
		return false;

	gUserProfile.CustomerID    = CustomerID;
	gUserProfile.SessionID     = SessionID;
	gUserProfile.AccountStatus = AccountStatus;
	return true;
}

void FrontendWarZ::LoginCheckAnswerCode()
{
	if(loginAnswerCode == ANS_Unactive)
		return;
		
	if(loginAnswerCode == ANS_Processing)
		return;
		
	// wait for thread to finish
	if(::WaitForSingleObject(loginThread, 1000) == WAIT_TIMEOUT)
		r3d_assert(0);
	
	CloseHandle(loginThread);
	loginThread = NULL;
	
	Scaleform::GFx::Value vars[3];
	switch(loginAnswerCode)
	{
	case ANS_Timeout:
		loginMsgBoxOK_Exit = true;
		vars[0].SetStringW(gLangMngr.getString("LoginMenu_CommError"));
		vars[1].SetBoolean(true);
		vars[2].SetString("ERROR");
		gfxMovie.Invoke("_root.api.showInfoMsg", vars, 3);
		break;
	case ANS_Error:
		loginMsgBoxOK_Exit = true;
		vars[0].SetStringW(gLangMngr.getString("LoginMenu_WrongLoginAnswer"));
		vars[1].SetBoolean(true);
		vars[2].SetString("ERROR");
		gfxMovie.Invoke("_root.api.showInfoMsg", vars, 3);
		break;
	case ANS_Logged:
		LoginMenuExitFlag = 1; 
		break;

	case ANS_BadPassword:
		loginMsgBoxOK_Exit = true;
		vars[0].SetStringW(gLangMngr.getString("LoginMenu_LoginFailed"));
		vars[1].SetBoolean(true);
		vars[2].SetString("ERROR");
		gfxMovie.Invoke("_root.api.showInfoMsg", vars, 3);
		break;

	case ANS_Frozen:
		loginMsgBoxOK_Exit = true;
		vars[0].SetStringW(gLangMngr.getString("LoginMenu_AccountFrozen"));
		vars[1].SetBoolean(true);
		vars[2].SetString("ERROR");
		gfxMovie.Invoke("_root.api.showInfoMsg", vars, 2);
		break;
	}
}

void FrontendWarZ::initLoginStep(const wchar_t* loginErrorMsg)
{
	LoginMenuExitFlag = 0;
	loginProcessStartTime = r3dGetTime();

	// show info message and render it one time
	gfxMovie.Invoke("_root.api.showLoginMsg", gLangMngr.getString("LoggingIn"));

	if(loginErrorMsg)
	{
		loginMsgBoxOK_Exit = true;
		Scaleform::GFx::Value vars[3];
		vars[0].SetStringW(loginErrorMsg);
		vars[1].SetBoolean(true);
		vars[2].SetString("ERROR");
		gfxMovie.Invoke("_root.api.showInfoMsg", vars, 3);
		return;
	}

	if( r3dRenderer->DeviceAvailable )
	{
		// advance movie by 5 frames, so info screen will fade in and show
		Scaleform::GFx::Movie* pMovie = gfxMovie.GetMovie();

		pMovie->Advance((1.0f/pMovie->GetFrameRate()) * 5);

		r3dRenderer->StartFrame();
		r3dRenderer->StartRender(1);

		r3dRenderer->SetRenderingMode(R3D_BLEND_ALPHA | R3D_BLEND_NZ);

		gfxMovie.UpdateAndDraw();

		r3dRenderer->Flush();
		CurRenderPipeline->Finalize() ;
		r3dRenderer->EndFrame();
	}
	r3dRenderer->EndRender( true );

	if(!loginErrorMsg)
	{
		// if we have encoded login session information
		if(Login_PassedAuth[0])
		{
			if(DecodeAuthParams())
			{
				r3d_assert(loginThread == NULL);
				loginThread = (HANDLE)_beginthreadex(NULL, 0, &LoginAuthThread, this, 0, NULL);
				if(loginThread == NULL)
					r3dError("Failed to begin thread");
			}
			return;
		}
#ifndef FINAL_BUILD
		if(Login_PassedUser[0] == 0 || Login_PassedPwd[0] == 0)
		{
			r3dscpy(Login_PassedUser, d_login->GetString());
			r3dscpy(Login_PassedPwd, d_password->GetString());
			if(strlen(Login_PassedUser)<2 || strlen(Login_PassedPwd)<2)
			{
				r3dError("you should set login as d_login <user> d_password <pwd> in local.ini");
				// programmers only can do this:
				//r3dError("you should set login as '-login <user> -pwd <pwd> in command line");
			}
		}
#endif

		loginThread = (HANDLE)_beginthreadex(NULL, 0, &LoginProcessThread, this, 0, NULL);
	}
}

static volatile LONG gProfileIsAquired = 0;
static volatile LONG gProfileOK = 0;
static volatile float gTimeWhenProfileLoaded = 0;
static volatile LONG gProfileLoadStage = 0;

extern CHWInfo g_HardwareInfo;

static void SetLoadStage(const char* stage)
{
	const static char* sname = NULL;
	static float stime = 0;
#ifndef FINAL_BUILD	
	if(sname) 
	{
		r3dOutToLog("SetLoadStage: %4.2f sec in %s\n", r3dGetTime() - stime, sname);
	}
#endif

	sname = stage;
	stime = r3dGetTime();
	gProfileLoadStage++;
}

static void LoadFrontendGameData(FrontendWarZ* UI)
{
	//
	// load shooting gallery
	//
	SetLoadStage("FrontEnd Lighting Level");
	{
		extern void DoLoadGame(const char* LevelFolder, int MaxPlayers, bool unloadPrev, bool isMenuLevel );
		DoLoadGame(r3dGameLevel::GetHomeDir(), 4, true, true );
	}

	//
	// create player and FPS weapon
	//
	SetLoadStage("Player Model");
	{
		obj_Player* plr = (obj_Player *)srv_CreateGameObject("obj_Player", "Player", r3dPoint3D(0,0,0));
		plr->PlayerState = PLAYER_IDLE;
		plr->bDead = 0;
		plr->CurLoadout = gUserProfile.ProfileData.ArmorySlots[0];
		plr->m_disablePhysSkeleton = true;
		plr->m_fPlayerRotationTarget = plr->m_fPlayerRotation = 0;

		// we need it to be created as a networklocal character for physics.
		plr->NetworkLocal = true;
		plr->OnCreate();
		plr->NetworkLocal = false;
		// work around for loading fps model sometimes instead of proper tps model
		plr->UpdateLoadoutSlot(plr->CurLoadout);
		// switch player to UI idle mode
		plr->uberAnim_->IsInUI = true;
		plr->uberAnim_->AnimPlayerState = -1;
		plr->uberAnim_->anim.StopAll();	// prevent animation blending on loadout switch
		plr->SyncAnimation(true);
		UI->SetLoadedThings(plr);
	}
}

static bool ActualGetProfileData(FrontendWarZ* UI)
{
	gProfileLoadStage = 0;

	SetLoadStage("ApiGetShopData");
	if(gUserProfile.ApiGetShopData() != 0)
		return false;
		
	// get game rewards from server.
	SetLoadStage("ApiGameRewards");
	if(g_GameRewards == NULL)
		g_GameRewards = new CGameRewards();
	if(!g_GameRewards->loaded_) {
		if(g_GameRewards->ApiGetDataGameRewards() != 0) {
			return false;
		}
	}
		
	// update items info only once and do not check for errors
	static bool gotCurItemsData = false;
	SetLoadStage("ApiGetItemsInfo");
	if(!gotCurItemsData) {
		gotCurItemsData = true;
		gUserProfile.ApiGetItemsInfo();
	}

	SetLoadStage("GetProfile");
	if(gUserProfile.GetProfile() != 0)
		return false;

	// load player only after profile
	// need to load game data first, because of DestroyGame() in destructor
	LoadFrontendGameData(UI);

	if(gUserProfile.ProfileDataDirty > 0)
	{
		//@TODO: set dirty profile flag, repeat getting profile
		r3dOutToLog("@@@@@@@@@@ProfileDataDirty: %d\n", gUserProfile.ProfileDataDirty);
	}

	SetLoadStage("ApiSteamGetShop");
	if(gSteam.inited_)
		gUserProfile.ApiSteamGetShop();

	// retreive friends status
/*	SetLoadStage("ApiFriendGetStats");
	gUserProfile.friends->friendsPrev_.clear();
	if(!gUserProfile.friends->gotNewData)
		gLoginSessionPoller.ForceTick();
	const float waitEnd = r3dGetTime() + 20.0f;
	while(r3dGetTime() < waitEnd)
	{
		if(gUserProfile.friends->gotNewData) 
		{
			// fetch your friends statistics
			gUserProfile.ApiFriendGetStats(0);
			break;
		}
	} */


	// send HW report if necessary
	/*SetLoadStage("HWReport");
	if(FrontendWarZ::frontendFirstTimeInit)
	{
		if(NeedUploadReport(g_HardwareInfo))
		{
			CWOBackendReq req(&gUserProfile, "api_ReportHWInfo_Customer.aspx");
			char buf[1024];
			sprintf(buf, "%I64d", g_HardwareInfo.uniqueId);
			req.AddParam("r00", buf);
			req.AddParam("r10", g_HardwareInfo.CPUString);
			req.AddParam("r11", g_HardwareInfo.CPUBrandString);
			sprintf(buf, "%d", g_HardwareInfo.CPUFreq);
			req.AddParam("r12", buf);
			sprintf(buf, "%d", g_HardwareInfo.TotalMemory);
			req.AddParam("r13", buf);

			sprintf(buf, "%d", g_HardwareInfo.DisplayW);
			req.AddParam("r20", buf);
			sprintf(buf, "%d", g_HardwareInfo.DisplayH);
			req.AddParam("r21", buf);
			sprintf(buf, "%d", g_HardwareInfo.gfxErrors);
			req.AddParam("r22", buf);
			sprintf(buf, "%d", g_HardwareInfo.gfxVendorId);
			req.AddParam("r23", buf);
			sprintf(buf, "%d", g_HardwareInfo.gfxDeviceId);
			req.AddParam("r24", buf);
			req.AddParam("r25", g_HardwareInfo.gfxDescription);

			req.AddParam("r30", g_HardwareInfo.OSVersion);

			if(!req.Issue())
			{
				r3dOutToLog("Failed to upload HW Info\n");
			}
			else
			{
				// mark that we reported it
				HKEY hKey;
				int hr;
				hr = RegCreateKeyEx(HKEY_CURRENT_USER, 
					"Software\\Arktos Entertainment Group\\TheWarZ", 
					0, 
					NULL,
					REG_OPTION_NON_VOLATILE, 
					KEY_ALL_ACCESS,
					NULL,
					&hKey,
					NULL);
				if(hr == ERROR_SUCCESS)
				{
					__int64 repTime = _time64(NULL);
					DWORD size = sizeof(repTime);

					hr = RegSetValueEx(hKey, "UpdaterTime2", NULL, REG_QWORD, (BYTE*)&repTime, size);
					RegCloseKey(hKey);
				}
			}
		}
	}*/

	SetLoadStage(NULL);
	return true;
}

static unsigned int WINAPI GetProfileDataThread( void * FrontEnd )
{
	r3dThreadAutoInstallCrashHelper crashHelper;
	r3dRandInitInTread rand_in_thread;

	try 
	{
		gProfileOK = 0;
		if(ActualGetProfileData((FrontendWarZ*)FrontEnd))
		{
			gProfileOK = 1;
			gTimeWhenProfileLoaded = r3dGetTime();
		}
	}
	catch(const char* err)
	{
		// catch r3dError
		r3dOutToLog("GetProfileData error: %s\n", err);
	}
		
	InterlockedExchange( &gProfileIsAquired, 1 );

	return 0;
}

//////////////////////////////////////////////////////////////////////////

void FrontendWarZ::D3DCreateResource()
{
	needReInitScaleformTexture = true;
}

//////////////////////////////////////////////////////////////////////////

static float aquireProfileStart = 0;
static HANDLE handleGetProfileData = 0;

bool FrontendWarZ::Initialize()
{
	extern int g_CCBlackWhite;
	extern float g_fCCBlackWhitePwr;

	g_CCBlackWhite = false;
	g_fCCBlackWhitePwr = 0.0f;

	bindRTsToScaleForm();
	frontendStage = 0;
	loginMsgBoxOK_Exit = false;

// check for bad values in contrast, brightness
// 	if(r_contrast->GetFloat() < r_contrast->GetMinVal() || r_contrast->GetFloat() > r_contrast->GetMaxVal())
// 		r_contrast->SetFloat(0.5f);
// 	if(r_brightness->GetFloat() < r_brightness->GetMinVal() || r_brightness->GetFloat() > r_brightness->GetMaxVal())
// 		r_brightness->SetFloat(0.5f);
	
	if(g_mouse_sensitivity->GetFloat() < g_mouse_sensitivity->GetMinVal() || g_mouse_sensitivity->GetFloat() > g_mouse_sensitivity->GetMaxVal())
		g_mouse_sensitivity->SetFloat(0.5f);
	if(s_sound_volume->GetFloat() < s_sound_volume->GetMinVal() || s_sound_volume->GetFloat() > s_sound_volume->GetMaxVal())
		s_sound_volume->SetFloat(1.0f);
	if(s_music_volume->GetFloat() < s_music_volume->GetMinVal() || s_music_volume->GetFloat() > s_music_volume->GetMaxVal())
		s_music_volume->SetFloat(1.0f);
	if(s_comm_volume->GetFloat() < s_comm_volume->GetMinVal() || s_comm_volume->GetFloat() > s_comm_volume->GetMaxVal())
		s_comm_volume->SetFloat(1.0f);

	// reacquire the menu.
	gfxMovie.SetKeyboardCapture();

	r_film_tone_a->SetFloat(0.15f);
	r_film_tone_b->SetFloat(0.50f);
	r_film_tone_c->SetFloat(0.10f);
	r_film_tone_d->SetFloat(0.20f);
	r_film_tone_e->SetFloat(0.02f);
	r_film_tone_f->SetFloat(0.30f);
	r_exposure_bias->SetFloat(0.5f);
	r_white_level->SetFloat(11.2f);

	gClientLogic().Reset(); // reset game finished, otherwise player will not update and will not update its skelet and will not render

	gfxMovie.SetCurentRTViewport( Scaleform::GFx::Movie::SM_ExactFit );

#define MAKE_CALLBACK(FUNC) new r3dScaleformMovie::TGFxEICallback<FrontendWarZ>(this, &FrontendWarZ::FUNC)
	gfxMovie.RegisterEventHandler("eventPlayGame", MAKE_CALLBACK(eventPlayGame));
	gfxMovie.RegisterEventHandler("eventCancelQuickGameSearch", MAKE_CALLBACK(eventCancelQuickGameSearch));
	gfxMovie.RegisterEventHandler("eventQuitGame", MAKE_CALLBACK(eventQuitGame));
	gfxMovie.RegisterEventHandler("eventCreateCharacter", MAKE_CALLBACK(eventCreateCharacter));
	gfxMovie.RegisterEventHandler("eventDeleteChar", MAKE_CALLBACK(eventDeleteChar));
	gfxMovie.RegisterEventHandler("eventReviveChar", MAKE_CALLBACK(eventReviveChar));
	gfxMovie.RegisterEventHandler("eventReviveCharMoney", MAKE_CALLBACK(eventReviveCharMoney));
	gfxMovie.RegisterEventHandler("eventLearnSkill", MAKE_CALLBACK(eventLearnSkill));
	gfxMovie.RegisterEventHandler("eventBuyItem", MAKE_CALLBACK(eventBuyItem));	
	gfxMovie.RegisterEventHandler("eventBackpackFromInventory", MAKE_CALLBACK(eventBackpackFromInventory));	
	gfxMovie.RegisterEventHandler("eventBackpackToInventory", MAKE_CALLBACK(eventBackpackToInventory));	
	gfxMovie.RegisterEventHandler("eventBackpackGridSwap", MAKE_CALLBACK(eventBackpackGridSwap));	
	gfxMovie.RegisterEventHandler("eventSetSelectedChar", MAKE_CALLBACK(eventSetSelectedChar));	
	gfxMovie.RegisterEventHandler("eventOpenBackpackSelector", MAKE_CALLBACK(eventOpenBackpackSelector));	
	gfxMovie.RegisterEventHandler("eventChangeBackpack", MAKE_CALLBACK(eventChangeBackpack));
	gfxMovie.RegisterEventHandler("eventChangeOutfit", MAKE_CALLBACK(eventChangeOutfit));



	gfxMovie.RegisterEventHandler("eventOptionsReset", MAKE_CALLBACK(eventOptionsReset));
	gfxMovie.RegisterEventHandler("eventOptionsApply", MAKE_CALLBACK(eventOptionsApply));
	gfxMovie.RegisterEventHandler("eventOptionsControlsReset", MAKE_CALLBACK(eventOptionsControlsReset));
	gfxMovie.RegisterEventHandler("eventOptionsControlsApply", MAKE_CALLBACK(eventOptionsControlsApply));
	gfxMovie.RegisterEventHandler("eventOptionsLanguageSelection", MAKE_CALLBACK(eventOptionsLanguageSelection));
	gfxMovie.RegisterEventHandler("eventOptionsControlsRequestKeyRemap", MAKE_CALLBACK(eventOptionsControlsRequestKeyRemap));

	gfxMovie.RegisterEventHandler("eventSetCurrentBrowseChannel", MAKE_CALLBACK(eventSetCurrentBrowseChannel));
	gfxMovie.RegisterEventHandler("eventCreateChangeCharacter", MAKE_CALLBACK(eventCreateChangeCharacter));	
	gfxMovie.RegisterEventHandler("eventCreateCancel", MAKE_CALLBACK(eventCreateCancel));	

	gfxMovie.RegisterEventHandler("eventRequestPlayerRender", MAKE_CALLBACK(eventRequestPlayerRender));	
	gfxMovie.RegisterEventHandler("eventMsgBoxCallback", MAKE_CALLBACK(eventMsgBoxCallback));	
	
	//gfxMovie.RegisterEventHandler("initButtons", MAKE_CALLBACK(initButtons));//frontend
	gfxMovie.RegisterEventHandler("eventBrowseGamesRequestFilterStatus", MAKE_CALLBACK(eventBrowseGamesRequestFilterStatus));	
	gfxMovie.RegisterEventHandler("eventBrowseGamesSetFilter", MAKE_CALLBACK(eventBrowseGamesSetFilter));	
	gfxMovie.RegisterEventHandler("eventBrowseGamesJoin", MAKE_CALLBACK(eventBrowseGamesJoin));	
	gfxMovie.RegisterEventHandler("eventBrowseGamesOnAddToFavorites", MAKE_CALLBACK(eventBrowseGamesOnAddToFavorites));	
	gfxMovie.RegisterEventHandler("eventBrowseGamesRequestList", MAKE_CALLBACK(eventBrowseGamesRequestList));
	gfxMovie.RegisterEventHandler("eventTrialUpgradeAccount", MAKE_CALLBACK(eventTrialUpgradeAccount));	// PasswordCallBack

	gfxMovie.RegisterEventHandler("eventRequestMyClanInfo", MAKE_CALLBACK(eventRequestMyClanInfo));	
	gfxMovie.RegisterEventHandler("eventRequestClanList", MAKE_CALLBACK(eventRequestClanList));	
	gfxMovie.RegisterEventHandler("eventCreateClan", MAKE_CALLBACK(eventCreateClan));	
	gfxMovie.RegisterEventHandler("eventClanAdminDonateGC", MAKE_CALLBACK(eventClanAdminDonateGC));	
	gfxMovie.RegisterEventHandler("eventClanAdminAction", MAKE_CALLBACK(eventClanAdminAction));	
	gfxMovie.RegisterEventHandler("eventClanLeaveClan", MAKE_CALLBACK(eventClanLeaveClan));	
	gfxMovie.RegisterEventHandler("eventClanDonateGCToClan", MAKE_CALLBACK(eventClanDonateGCToClan));	
	gfxMovie.RegisterEventHandler("eventRequestClanApplications", MAKE_CALLBACK(eventRequestClanApplications));	
	gfxMovie.RegisterEventHandler("eventClanApplicationAction", MAKE_CALLBACK(eventClanApplicationAction));	
	gfxMovie.RegisterEventHandler("eventClanInviteToClan", MAKE_CALLBACK(eventClanInviteToClan));	
	gfxMovie.RegisterEventHandler("eventClanRespondToInvite", MAKE_CALLBACK(eventClanRespondToInvite));	
	gfxMovie.RegisterEventHandler("eventClanBuySlots", MAKE_CALLBACK(eventClanBuySlots));	
	gfxMovie.RegisterEventHandler("eventClanApplyToJoin", MAKE_CALLBACK(eventClanApplyToJoin));	

	gfxMovie.RegisterEventHandler("eventRequestLeaderboardData", MAKE_CALLBACK(eventRequestLeaderboardData));	
	
	// BETA
	gfxMovie.RegisterEventHandler("eventRequestMyServerList", MAKE_CALLBACK(eventRequestMyServerList));
	gfxMovie.RegisterEventHandler("eventRequestGCTransactionData", MAKE_CALLBACK(eventRequestGCTransactionData));
	gfxMovie.RegisterEventHandler("eventRentServerUpdatePrice", MAKE_CALLBACK(eventRentServerUpdatePrice));
	gfxMovie.RegisterEventHandler("eventRentServer", MAKE_CALLBACK(eventRentServer));
	gfxMovie.RegisterEventHandler("eventRenameCharacter", MAKE_CALLBACK(eventRenameCharacter)); //Change Name

	gfxMovie.RegisterEventHandler("eventShowSurvivorsMap", MAKE_CALLBACK(eventShowSurvivorsMap));	
	gfxMovie.RegisterEventHandler("eventStorePurchaseGDCallback", MAKE_CALLBACK(eventStorePurchaseGDCallback));
	gfxMovie.RegisterEventHandler("eventStorePurchaseGD", MAKE_CALLBACK(eventStorePurchaseGD));
	gfxMovie.RegisterEventHandler("eventMarketplaceActive", MAKE_CALLBACK(eventMarketplaceActive));
	return true;
}

void FrontendWarZ::postLoginStepInit(EGameResult gameResult)
{
	frontendStage = 1;
	prevGameResult = gameResult;

	gProfileIsAquired = 0;
	aquireProfileStart = r3dGetTime();
	handleGetProfileData = (HANDLE)_beginthreadex(NULL, 0, &GetProfileDataThread, this, 0, 0);
	if(handleGetProfileData == 0)
		r3dError("Failed to begin thread");

	// show info message and render it one time
	gfxMovie.Invoke("_root.api.showLoginMsg", gLangMngr.getString("RetrievingProfileData"));

	if( r3dRenderer->DeviceAvailable )
	{
		// advance movie by 5 frames, so info screen will fade in and show
		Scaleform::GFx::Movie* pMovie = gfxMovie.GetMovie();

		pMovie->Advance((1.0f/pMovie->GetFrameRate()) * 5);

		r3dRenderer->StartFrame();
		r3dRenderer->StartRender(1);

		r3dRenderer->SetRenderingMode(R3D_BLEND_ALPHA | R3D_BLEND_NZ);

		gfxMovie.UpdateAndDraw();

		r3dRenderer->Flush();
		CurRenderPipeline->Finalize() ;
		r3dRenderer->EndFrame();
	}
	r3dRenderer->EndRender( true );
	r3dRenderer->TryToRestoreDevice();

	// init things to load game level
	r3dGameLevel::SetHomeDir("WZ_FrontEndLighting");
	extern void InitGame_Start();
	InitGame_Start();
}

void FrontendWarZ::bindRTsToScaleForm()
{
	RTScaleformTexture = gfxMovie.BoundRTToImage("merc_rendertarget", Scaleform_RenderToTextureRT->AsTex2D(), (int)Scaleform_RenderToTextureRT->Width, (int)Scaleform_RenderToTextureRT->Height);
}


bool FrontendWarZ::Unload()
{
#if ENABLE_WEB_BROWSER
	d_show_browser->SetBool(false);
	g_pBrowserManager->SetSize(4, 4);
#endif

	return UIMenu::Unload();
}

extern void InputUpdate();
int FrontendWarZ::Update()
{
	struct EnableDisableDistanceCull
	{
		EnableDisableDistanceCull()
		{
			oldValue = r_allow_distance_cull->GetInt();
			r_allow_distance_cull->SetInt( 0 );
		}

		~EnableDisableDistanceCull()
		{
			r_allow_distance_cull->SetInt( oldValue );
		}

		int oldValue;

	} enableDisableDistanceCull; (void)enableDisableDistanceCull;

	if(needReInitScaleformTexture)
	{
		if (RTScaleformTexture && Scaleform_RenderToTextureRT)
			RTScaleformTexture->Initialize(Scaleform_RenderToTextureRT->AsTex2D());
		needReInitScaleformTexture = false;
	}


	if(gSteam.inited_)
		SteamAPI_RunCallbacks();

	InputUpdate();

	{
		r3dPoint3D soundPos(0,0,0), soundDir(0,0,1), soundUp(0,1,0);
		SoundSys.Update(soundPos, soundDir, soundUp);
	}

	if(frontendStage == 0) // login stage
	{
		// run temp drawing loop
		extern void tempDoMsgLoop();
		tempDoMsgLoop();

		float elapsedTime = r3dGetTime() - loginProcessStartTime;
		float progress = R3D_CLAMP(elapsedTime/2.0f, 0.0f, 1.0f); 
		if(loginMsgBoxOK_Exit)
			progress = 0;

		gfxMovie.Invoke("_root.api.updateLoginMsg", progress);

		r3dStartFrame();
		if( r3dRenderer->DeviceAvailable )
		{
			r3dRenderer->StartFrame();
			r3dRenderer->StartRender(1);

			r3dRenderer->SetRenderingMode(R3D_BLEND_ALPHA | R3D_BLEND_NZ);

			gfxMovie.UpdateAndDraw();

			r3dRenderer->Flush();
			CurRenderPipeline->Finalize() ;
			r3dRenderer->EndFrame();
		}

		r3dRenderer->EndRender( true );

		// process d3d device queue, keeping 20fps for rendering
		if( r3dRenderer->DeviceAvailable )
		{
			float endTime = r3dGetTime() + (1.0f / 20);
			while(r3dGetTime() < endTime)
			{
				extern bool ProcessDeviceQueue( float chunkTimeStart, float maxDuration ) ;
				ProcessDeviceQueue(r3dGetTime(), 0.05f);
			}
		}

		r3dEndFrame();

		LoginCheckAnswerCode();
		if(loginThread == NULL)
		{
			bool IsNeedExit();
			if(IsNeedExit())
				return FrontEndShared::RET_Exit;

			if(LoginMenuExitFlag == 1) 
				return FrontEndShared::RET_LoggedIn;
			else if(LoginMenuExitFlag == -1) // error logging in
				return FrontEndShared::RET_Exit;
		}
		
		return 0;
	}

	// we're still retreiving profile
	if(handleGetProfileData != 0 && gProfileIsAquired == 0)
	{
		// run temp drawing loop
		extern void tempDoMsgLoop();
		tempDoMsgLoop();
		
		// replace message with loading stage info
		static int oldStage = -1;
		if(oldStage != gProfileLoadStage)
		{
			oldStage = gProfileLoadStage;

			wchar_t dots[32] = L"";
			for(int i=0; i<gProfileLoadStage; i++) dots[i] = L'.';
			dots[gProfileLoadStage] = 0;
			
			wchar_t info[1024];
			StringCbPrintfW(info, sizeof(info), L"%s\n%s", gLangMngr.getString("RetrievingProfileData"), dots);
			
			//updateInfoMsgText(info);
		}
		{
			float progress = gProfileLoadStage/8.0f;
			gfxMovie.Invoke("_root.api.updateLoginMsg", progress);
		}

		// NOTE: WARNING: DO NOT TOUCH GameWorld() or anything here - background loading thread in progress!
		r3dStartFrame();
		if( r3dRenderer->DeviceAvailable )
		{
			r3dRenderer->StartFrame();
			r3dRenderer->StartRender(1);

			r3dRenderer->SetRenderingMode(R3D_BLEND_ALPHA | R3D_BLEND_NZ);

			gfxMovie.UpdateAndDraw();

			r3dRenderer->Flush();
			CurRenderPipeline->Finalize() ;
			r3dRenderer->EndFrame();
		}

		r3dRenderer->EndRender( true );

		// process d3d device queue, keeping 20fps for rendering
		if( r3dRenderer->DeviceAvailable )
		{
			float endTime = r3dGetTime() + (1.0f / 20);
			while(r3dGetTime() < endTime)
			{
				extern bool ProcessDeviceQueue( float chunkTimeStart, float maxDuration ) ;
				ProcessDeviceQueue(r3dGetTime(), 0.05f);
			}
		}
		
		r3dEndFrame();

		// update browser, so that by the time we get profile our welcome back screen will be ready to show page
#if ENABLE_WEB_BROWSER
		g_pBrowserManager->Update();
#endif

		return 0;
	}

	if(handleGetProfileData != 0)
	{
		// profile is acquired
		r3d_assert(gProfileIsAquired);
		
		if(!gProfileOK)
		{
			r3dOutToLog("Couldn't get profile data! stage: %d\n", gProfileLoadStage);
			return FrontEndShared::RET_Diconnected;
		}

		CloseHandle(handleGetProfileData);
		handleGetProfileData = 0;

		r3dOutToLog( "Acquired base profile data for %f\n", r3dGetTime() - aquireProfileStart );
		if(gUserProfile.AccountStatus >= 200)
		{
			return FrontEndShared::RET_Banned;
		}
		
		r3dResetFrameTime();

		extern void InitGame_Finish();
		InitGame_Finish();

		//
		if (gUserProfile.ProfileDataDirty == 0)
			initFrontend();
		else
		{
			m_ReloadProfile = true;
			m_ReloadTimer = r3dGetTime();

			Scaleform::GFx::Value var[2];

			var[0].SetStringW(gLangMngr.getString("Waiting for profile to finish updating..."));
			var[1].SetBoolean(false);
			gfxMovie.Invoke("_root.api.showInfoMsg", var, 2);		
		}
	}

	if (m_ReloadProfile)
	{
		int time = int (r3dGetTime() - m_ReloadTimer);


		if (time > 10)
		{
			if(gUserProfile.GetProfile() != 0)
				return false;

			if (gUserProfile.ProfileDataDirty == 0)
			{
				m_ReloadProfile = false;
				gfxMovie.Invoke("_root.api.hideInfoMsg", "");		
				initFrontend();
			}
			else
			{
				m_ReloadTimer = r3dGetTime();
			}
		}

		return 0; // frontend isn't ready yet, just keep looping until profile will be ready
	}

	// at the moment we must have finished initializing things in background
	r3d_assert(handleGetProfileData == 0);

	if(m_waitingForKeyRemap != -1)
	{
		// query input manager for any input
		bool conflictRemapping = false;
		if(InputMappingMngr->attemptRemapKey((r3dInputMappingMngr::KeybordShortcuts)m_waitingForKeyRemap, conflictRemapping))
		{
			Scaleform::GFx::Value var[2];
			var[0].SetNumber(m_waitingForKeyRemap);
			var[1].SetString(InputMappingMngr->getKeyName((r3dInputMappingMngr::KeybordShortcuts)m_waitingForKeyRemap));
			gfxMovie.Invoke("_root.api.updateKeyboardMapping", var, 2);
			m_waitingForKeyRemap = -1;

			void writeInputMap();
			writeInputMap();

			if(conflictRemapping)
			{
				Scaleform::GFx::Value var[2];
				var[0].SetStringW(gLangMngr.getString("ConflictRemappingKeys"));
				var[1].SetBoolean(true);
				gfxMovie.Invoke("_root.api.showInfoMsg", var, 2);		
			}
		}
	}

	if(gUserProfile.ProfileData.ArmorySlots[gUserProfile.SelectedCharID].Alive == 0) // dead
	{	
		int	timeToReviveInSec = 3600;
		// for now, use hard coded revive time
		if(gUserProfile.ProfileData.AccountType == 5 )
			{
				timeToReviveInSec = 600; // ViruZ (Premium 10 Minutos)
			}
		else if(gUserProfile.ProfileData.AccountType == 0 )
			{
				timeToReviveInSec = 1200; // ViruZ (Legend 20 Minutos)
			}
		else {
				timeToReviveInSec = 2400; // ViruZ (Normal 40 Minutos)
			 }

		Scaleform::GFx::Value var[3];

		int timeLeftToRevive = R3D_MAX(gUserProfile.ProfileData.ArmorySlots[gUserProfile.SelectedCharID].SecToRevive - int(r3dGetTime() - gTimeWhenProfileLoaded), 0);
		var[0].SetInt(timeLeftToRevive);
		int perc = 100-int((float(timeLeftToRevive)/float(timeToReviveInSec))*100.0f);
		var[1].SetInt(perc);
//#ifdef FINAL_BUILD
//		var[2].SetBoolean(timeLeftToRevive==0);
//#else
		var[2].SetBoolean(true);
//#endif
		gfxMovie.Invoke("_root.api.updateDeadTimer", var, 3);
	}

#if ENABLE_WEB_BROWSER
	g_pBrowserManager->Update();
#endif

	settingsChangeFlags_ = 0;

	r3dMouse::Show();

	extern void tempDoMsgLoop();
	tempDoMsgLoop();

	m_Player->UpdateTransform();
	r3dPoint3D size = m_Player->GetBBoxLocal().Size;

	float distance = GetOptimalDist(size, 22.5f);

	r3dPoint3D camPos(0, size.y * 1.0f, distance);
	r3dPoint3D playerPosHome(0, 0.38f, 0);
	r3dPoint3D playerPosCreate(0, 0.38f, 0);

	float backupFOV = gCam.FOV;
	gCam = camPos;
	gCam.vPointTo = (r3dPoint3D(0, 1, 0) - gCam).NormalizeTo();
	gCam.FOV = 45;

	gCam.SetPlanes(0.01f, 200.0f);
	if(m_needPlayerRenderingRequest==1) // home
		m_Player->SetPosition(playerPosHome);	
	else if(m_needPlayerRenderingRequest==2) // create
		m_Player->SetPosition(playerPosCreate);
	else if(m_needPlayerRenderingRequest==3) // play game screen
		m_Player->SetPosition(playerPosCreate);

	m_Player->m_fPlayerRotationTarget = m_Player->m_fPlayerRotation = 0;

	GameWorld().StartFrame();
	r3dRenderer->SetCamera( gCam, true );

	GameWorld().Update();

	async_.ProcessAsyncOperation(this, gfxMovie);
	market_.update();

	gfxMovie.SetCurentRTViewport( Scaleform::GFx::Movie::SM_ExactFit );

	r3dStartFrame();

	if( r3dRenderer->DeviceAvailable )
	{
		r3dRenderer->StartFrame();

		r3dRenderer->StartRender(1);

		//r3d_assert(m_pBackgroundPremiumTex);
		r3dRenderer->SetRenderingMode(R3D_BLEND_ALPHA | R3D_BLEND_NZ);
		r3dColor backgroundColor = r3dColor::white;

		if(m_needPlayerRenderingRequest)
			drawPlayer() ;

		gfxMovie.UpdateAndDraw();

		r3dRenderer->Flush();

		CurRenderPipeline->Finalize() ;

		r3dRenderer->EndFrame();
	}

	r3dRenderer->EndRender( true );

	if( r3dRenderer->DeviceAvailable )
	{
		r3dUpdateScreenShot();
		if(Keyboard->WasPressed(kbsPrtScr))
			r3dToggleScreenShot();
	}

	GameWorld().EndFrame();
	r3dEndFrame();

	if( needUpdateSettings_ )
	{
		UpdateSettings();
		needUpdateSettings_ = false;
	}

	if(gMasterServerLogic.IsConnected() && !async_.Processing() && !market_.processing())
	{
		if(r3dGetTime() > masterConnectTime_ + _p2p_idleTime)
		{
			masterConnectTime_ = -1;
			gMasterServerLogic.Disconnect();
		}
		
		if(gMasterServerLogic.shuttingDown_)
		{
			gMasterServerLogic.Disconnect();
			
			Scaleform::GFx::Value var[2];
			var[0].SetStringW(gLangMngr.getString("MSShutdown1"));
			var[1].SetBoolean(true);
			gfxMovie.Invoke("_root.api.showInfoMsg", var, 2);		
		}
	}

	if(!async_.Processing() && !market_.processing())
	{
		bool IsNeedExit();
		if(IsNeedExit())
			return FrontEndShared::RET_Exit;
		
		if(exitRequested_)
			return FrontEndShared::RET_Exit;

		if(!gLoginSessionPoller.IsConnected()) {
			//@TODO: set var, display message and exit
			r3dError("double login");
		}

		if(needExitByGameJoin_)
		{
			if(!gUserProfile.ProfileData.ArmorySlots[gUserProfile.SelectedCharID].Alive)
			{
				needExitByGameJoin_ = false;

				Scaleform::GFx::Value var[2];
				var[0].SetStringW(gLangMngr.getString("$FR_PLAY_GAME_SURVIVOR_DEAD"));
				var[1].SetBoolean(true);
				gfxMovie.Invoke("_root.api.showInfoMsg", var, 2);		
				return 0;
			}
			return FrontEndShared::RET_JoinGame;
		}
	}

	return 0;
}

void FrontendWarZ::drawPlayer()
{
	struct BeginEndEvent
	{
		BeginEndEvent()
		{
			D3DPERF_BeginEvent( 0, L"FrontendUI::drawPlayer" ) ;
		}
		
		~BeginEndEvent()
		{
			D3DPERF_EndEvent() ;
		}
	} beginEndEvent ;

	CurRenderPipeline->PreRender();
	CurRenderPipeline->Render();

	CurRenderPipeline->AppendPostFXes();

	{
#if 0
		PFX_Fill::Settings efsts ;

		efsts.Value = float4( r_gameui_exposure->GetFloat(), 0.f, 0.f, 0.f ) ;

		gPFX_Fill.PushSettings( efsts ) ;
		g_pPostFXChief->AddFX( gPFX_Fill, PostFXChief::RTT_SCENE_EXPOSURE0, PostFXChief::RTT_AVG_SCENE_LUMA );
		gPFX_Fill.PushSettings( efsts ) ;
		g_pPostFXChief->AddFX( gPFX_Fill, PostFXChief::RTT_SCENE_EXPOSURE1, PostFXChief::RTT_AVG_SCENE_LUMA );

		g_pPostFXChief->AddFX( gPFX_ConvertToLDR );
		g_pPostFXChief->AddSwapBuffers();
#endif

		PFX_Fill::Settings fsts;

		fsts.ColorWriteMask = D3DCOLORWRITEENABLE_ALPHA;			

		gPFX_Fill.PushSettings( fsts );

		g_pPostFXChief->AddFX( gPFX_Fill, PostFXChief::RTT_PINGPONG_LAST, PostFXChief::RTT_DIFFUSE_32BIT );

		PFX_StencilToMask::Settings ssts;

		ssts.Value = float4( 0, 0, 0, 1 );

		gPFX_StencilToMask.PushSettings( ssts );

		g_pPostFXChief->AddFX( gPFX_StencilToMask, PostFXChief::RTT_PINGPONG_LAST );

		{
			r3dScreenBuffer* buf = g_pPostFXChief->GetBuffer( PostFXChief::RTT_PINGPONG_LAST ) ;
			r3dScreenBuffer* buf_scaleform = g_pPostFXChief->GetBuffer( PostFXChief::RTT_UI_CHARACTER_32BIT ) ;

			PFX_Copy::Settings sts ;

			sts.TexScaleX = 1.0f;
			sts.TexScaleY = 1.0f;
			sts.TexOffsetX = 0.0f;
			sts.TexOffsetY = 0.0f;

			gPFX_Copy.PushSettings( sts ) ;

			g_pPostFXChief->AddFX( gPFX_Copy, PostFXChief::RTT_UI_CHARACTER_32BIT ) ;
		}

		g_pPostFXChief->Execute( false, true );
	}

	r3dRenderer->SetVertexShader();
	r3dRenderer->SetPixelShader();
}

void FrontendWarZ::addClientSurvivor(const wiCharDataFull& slot, int slotIndex)
{
	Scaleform::GFx::Value var[23];
	char tmpGamertag[128];
	if(slot.ClanID != 0)
		sprintf(tmpGamertag, "[%s] %s", slot.ClanTag, slot.Gamertag);
	else
		r3dscpy(tmpGamertag, slot.Gamertag);
	var[0].SetString(tmpGamertag);
	var[1].SetNumber(slot.Health);
	var[2].SetNumber(slot.Stats.XP);
	var[3].SetNumber(slot.Stats.TimePlayed);
	var[4].SetNumber(slot.Hardcore);
	var[5].SetNumber(slot.HeroItemID);
	var[6].SetNumber(slot.HeadIdx);
	var[7].SetNumber(slot.BodyIdx);
	var[8].SetNumber(slot.LegsIdx);
	var[9].SetNumber(slot.Alive);
	var[10].SetNumber(slot.Hunger);
	var[11].SetNumber(slot.Thirst);
	var[12].SetNumber(slot.Toxic);
	var[13].SetNumber(slot.BackpackID);
	var[14].SetNumber(slot.BackpackSize);

	var[15].SetNumber(0);		// weight
	var[16].SetNumber(slot.Stats.KilledZombies);		// zombies Killed
	var[17].SetNumber(slot.Stats.KilledBandits);		// bandits killed
	var[18].SetNumber(slot.Stats.KilledSurvivors);		// civilians killed
	var[19].SetStringW(getReputationString(slot.Stats.Reputation));	// alignment
	//nome no mapa
	//const wiCharDataFull& slot1 = gUserProfile.ProfileData.ArmorySlots[gUserProfile.SelectedCharID];
	int mapid = slot.GameMapId;
	const wchar_t* AtMap = gLangMngr.getString("$map_semspawn");
	
	if (mapid == 2) 
	{
		AtMap = gLangMngr.getString("$map_colorado");
		//var[20].SetString("COLORADO");
	}
	else if (mapid == 3)
	{
		AtMap = gLangMngr.getString("$map_cliffside");
		
	}
	else if (mapid == 5)
	{
		AtMap = gLangMngr.getString("$map_colorado_pve");
		
	}
	else if (mapid == 6)
	{
		AtMap = gLangMngr.getString("$map_atlanta");
		
	}
	else if (mapid == 7)
	{
		AtMap = gLangMngr.getString("$map_viruzarena");
		
	}
	else if (mapid == 8)
	{
		AtMap = gLangMngr.getString("$map_valley");
		
	}

	var[20].SetStringW(AtMap);	// last Map
	var[21].SetBoolean(slot.GameFlags & wiCharDataFull::GAMEFLAG_NearPostBox);

	var[22].SetNumber(slot.Stats.SkillXPPool);

	gfxMovie.Invoke("_root.api.addClientSurvivor", var, 23);

	addBackpackItems(slot, slotIndex);
}

void FrontendWarZ::addBackpackItems(const wiCharDataFull& slot, int slotIndex)
{
	Scaleform::GFx::Value var[8];
	for (int a = 0; a < slot.BackpackSize; a++)
	{
		if (slot.Items[a].InventoryID != 0)
		{
			var[0].SetInt(slotIndex);
			var[1].SetInt(a);
			var[2].SetUInt(uint32_t(slot.Items[a].InventoryID));
			var[3].SetUInt(slot.Items[a].itemID);
			var[4].SetInt(slot.Items[a].quantity);
			var[5].SetInt(slot.Items[a].Var1);
			var[6].SetInt(slot.Items[a].Var2);
			char tmpStr[128] = {0};
			getAdditionalDescForItem(slot.Items[a].itemID, slot.Items[a].Var1, slot.Items[a].Var2, tmpStr);
			var[7].SetString(tmpStr);
			gfxMovie.Invoke("_root.api.addBackpackItem", var, 8);
		}
	}
}
void FrontendWarZ::InitButtons()
{
	Scaleform::GFx::Value vars[7];
	vars[0].SetBoolean(false);
	vars[1].SetBoolean(true);
	vars[2].SetBoolean(true);
	vars[3].SetBoolean(true);
	vars[4].SetBoolean(false);
	vars[5].SetBoolean(false);
	vars[6].SetBoolean(false);
	gfxMovie.Invoke("_root.api.Main.BrowseGamesChannelsAnim.initButtons", vars, 7);
}

void FrontendWarZ::initFrontend()
{
	market_.initialize(&gfxMovie);
	initInventoryItems();

	// send survivor info
	Scaleform::GFx::Value var[20];
	for(int i=0; i< gUserProfile.ProfileData.NumSlots; ++i)
	{
		addClientSurvivor(gUserProfile.ProfileData.ArmorySlots[i], i);
	}

	updateSurvivorTotalWeight(gUserProfile.SelectedCharID);

	for(int i=0; i<r3dInputMappingMngr::KS_NUM; ++i)
	{
		Scaleform::GFx::Value args[2];
		args[0].SetStringW(gLangMngr.getString(InputMappingMngr->getMapName((r3dInputMappingMngr::KeybordShortcuts)i)));
		args[1].SetString(InputMappingMngr->getKeyName((r3dInputMappingMngr::KeybordShortcuts)i));
		gfxMovie.Invoke("_root.api.addKeyboardMapping", args, 2);
	}

	SyncGraphicsUI();

	gfxMovie.SetVariable("_root.api.SelectedChar", gUserProfile.SelectedCharID);
	m_Player->uberAnim_->anim.StopAll();	
	m_Player->UpdateLoadoutSlot(gUserProfile.ProfileData.ArmorySlots[gUserProfile.SelectedCharID]);

	gfxMovie.Invoke("_root.api.showSurvivorsScreen", "");

	gfxMovie.Invoke("_root.api.setLanguage", g_user_language->GetString());
	gfxMovie.SetVariable("_root.api.ChangeName_Price",1000); // 1000 is price //Change Name

	InitButtons();
	
	if (gUserProfile.ProfileData.AccountType = 5 )
	gfxMovie.SetVariable("_root.api.Main.SurvivorsAnim.Survivors.PremiumAcc.visible", true);

	// init clan icons
	// important: DO NOT CHANGE THE ORDER OF ICONS!!! EVER!
	{
		gfxMovie.Invoke("_root.api.addClanIcon", "$Data/Menu/clanIcons/clan_survivor.dds");
		gfxMovie.Invoke("_root.api.addClanIcon", "$Data/Menu/clanIcons/clan_bandit.dds");
		gfxMovie.Invoke("_root.api.addClanIcon", "$Data/Menu/clanIcons/clan_lawman.dds");
		// add new icons at the end!
	}
	{
		//public function addClanSlotBuyInfo(buyIdx:uint, price:uint, numSlots:uint)
		Scaleform::GFx::Value var[3];
		for(int i=0; i<6; ++i)
		{
			var[0].SetUInt(i);
			var[1].SetUInt(gUserProfile.ShopClanAddMembers_GP[i]);
			var[2].SetUInt(gUserProfile.ShopClanAddMembers_Num[i]);
			gfxMovie.Invoke("_root.api.addClanSlotBuyInfo", var, 3);
		}
	}

	gfxMovie.Invoke("_root.api.hideLoginMsg", "");

	m_waitingForKeyRemap = -1;
	m_needPlayerRenderingRequest = 1; // by default when FrontendInit we are in home screen, so show player

	{
		Scaleform::GFx::Value vars[3];
		vars[1].SetBoolean(true);
		vars[2].SetString("ERROR");
		switch(prevGameResult)
		{
		case GRESULT_Failed_To_Join_Game:
			vars[0].SetStringW(gLangMngr.getString("FailedToJoinGame"));
			gfxMovie.Invoke("_root.api.showInfoMsg", vars, 3);
			break;
		case GRESULT_Timeout:
			vars[0].SetStringW(gLangMngr.getString("TimeoutJoiningGame"));
			gfxMovie.Invoke("_root.api.showInfoMsg", vars, 3);
			break;
		case GRESULT_StillInGame:
			vars[0].SetStringW(gLangMngr.getString("FailedToJoinGameStillInGame"));
			gfxMovie.Invoke("_root.api.showInfoMsg", vars, 3);
			break;
		case GRESULT_noPremium:
			vars[0].SetStringW(gLangMngr.getString("noPremiumResp"));
			gfxMovie.Invoke("_root.api.showInfoMsg", vars, 3);
			break;
		}
	}
	
	{
		//public function addSkillInfo(arg1:uint, arg2:String, arg3:String, arg4:String, arg5:String, arg6:uint):*
		//loc1, "skill" + loc1, "desc" + loc1, "skill/skill.png", "skill/skillBW.png", 100 + loc1 * 10
		//this.SkillData.push({"id":arg1, "name":arg2, "desc":arg3, "icon":arg4, "iconBW":arg5, "cost":arg6});
		Scaleform::GFx::Value var[6];
		for(int i=0; i<34; ++i)
		{
			char skill[256];
			char skillbw[256];
			char skillname[256];
			char skilldesc[256];
			sprintf(skill, "$Data/Menu/skillIcons/Skill%d.dds", i);
			sprintf(skillbw, "$Data/Menu/skillIcons/Skill%dBW.dds", i);
			sprintf(skillname, "SkillName%d", i);
			sprintf(skilldesc, "SkillDesc%d", i);
			var[0].SetInt(i);
			var[1].SetStringW(gLangMngr.getString(skillname));
			var[2].SetStringW(gLangMngr.getString(skilldesc));
			var[3].SetString(skill);
			var[4].SetString(skillbw);
			var[5].SetInt(1000 + i * 100);
			gfxMovie.Invoke("_root.api.addSkillInfo", var, 6);
		}
		updateInventoryAndSkillItems();
	}

	{
		//addRentServer_MapInfo(0, "Colorado");
		Scaleform::GFx::Value var[2];
		var[0].SetInt(0);
		var[1].SetString("Colorado");
		gfxMovie.Invoke("_root.api.addRentServer_MapInfo", var, 2);

		var[0].SetInt(1);
		var[1].SetString("Cliffside");
		gfxMovie.Invoke("_root.api.addRentServer_MapInfo", var, 2);
		//Scaleform::GFx::Value var[2];
		var[0].SetInt(0);
		var[1].SetString("S1");
		gfxMovie.Invoke("_root.api.addRentServer_StrongholdInfo", var, 2);

		//Scaleform::GFx::Value var[2];
		var[0].SetInt(0);
		var[1].SetString("US");
		gfxMovie.Invoke("_root.api.addRentServer_RegionInfo", var, 2);

		//Scaleform::GFx::Value var[2];
		var[0].SetInt(1);
		var[1].SetString("EU");
		gfxMovie.Invoke("_root.api.addRentServer_RegionInfo", var, 2);

		Scaleform::GFx::Value var3[3];
		var3[0].SetInt(32);
		var3[1].SetString("32");
		var3[2].SetBoolean(false);
		gfxMovie.Invoke("_root.api.addRentServer_SlotsInfo", var3, 3);

		//Scaleform::GFx::Value var3[3];
		var3[0].SetInt(64);
		var3[1].SetString("64");
		var3[2].SetBoolean(true);
		gfxMovie.Invoke("_root.api.addRentServer_SlotsInfo", var3, 3);

		var3[0].SetInt(128);
		var3[1].SetString("128");
		var3[2].SetBoolean(true);
		gfxMovie.Invoke("_root.api.addRentServer_SlotsInfo", var3, 3);

		var3[0].SetInt(256);
		var3[1].SetString("256");
		var3[2].SetBoolean(true);
		gfxMovie.Invoke("_root.api.addRentServer_SlotsInfo", var3, 3);

		var3[0].SetInt(1024);
		var3[1].SetString("1024");
		var3[2].SetBoolean(false);
		gfxMovie.Invoke("_root.api.addRentServer_SlotsInfo", var3, 3);

	}
	{
		Scaleform::GFx::Value var[3];
		var[0].SetInt(0);
		var[1].SetInt(0);
		var[2].SetString("1");
		gfxMovie.Invoke("_root.api.addRentServer_RentInfo", var, 3);

		var[0].SetInt(1);
		var[1].SetInt(1);
		var[2].SetString("1");
		gfxMovie.Invoke("_root.api.addRentServer_RentInfo", var, 3);

		var[0].SetInt(2);
		var[1].SetInt(2);
		var[2].SetString("2");
		gfxMovie.Invoke("_root.api.addRentServer_RentInfo", var, 3);

		var[0].SetInt(3);
		var[1].SetInt(3);
		var[2].SetString("3");
		gfxMovie.Invoke("_root.api.addRentServer_RentInfo", var, 3);

		var[0].SetInt(4);
		var[1].SetInt(4);
		var[2].SetString("6");
		gfxMovie.Invoke("_root.api.addRentServer_RentInfo", var, 3);

	}
	{
		Scaleform::GFx::Value var[2];
		var[0].SetInt(0);
		var[1].SetString("NO");
		gfxMovie.Invoke("_root.api.addRentServer_PVEInfo", var, 2);

		var[0].SetInt(1);
		var[1].SetString("YES");
		gfxMovie.Invoke("_root.api.addRentServer_PVEInfo", var, 2);
	}
	
	prevGameResult = GRESULT_Unknown;
}

unsigned int WINAPI FrontendWarZ::as_ConvertGDThread(void* in_data)
{
	r3dThreadAutoInstallCrashHelper crashHelper;
	FrontendWarZ* This = (FrontendWarZ*)in_data;

	This->async_.DelayServerRequest();
	int apiCode = gUserProfile.ApiConvertGCToGD(This->currentvalue,This->convertvalue);
	
	if(apiCode == 50){
		This->async_.SetAsyncError(0, gLangMngr.getString("Connevt Failed"));
		return 0;
	}

	return 1;
}
void FrontendWarZ::eventPlayGame(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
{
	if(gUserProfile.ProfileData.NumSlots == 0)
		return;
		
	async_.StartAsyncOperation(this, &FrontendWarZ::as_PlayGameThread);
}

void FrontendWarZ::eventCancelQuickGameSearch(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
{
	CancelQuickJoinRequest = true;
}

void FrontendWarZ::eventQuitGame(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
{
	exitRequested_ = true;
}

void FrontendWarZ::eventCreateCharacter(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
{
	r3dscpy(m_CreateGamerTag, args[0].GetString()); // gamertag
	m_CreateHeroID = args[1].GetInt(); // hero
	m_CreateGameMode = args[2].GetInt(); // mode
	m_CreateHeadIdx = args[3].GetInt(); // bodyID
	m_CreateBodyIdx = args[4].GetInt(); // headID
	m_CreateLegsIdx = args[5].GetInt(); // legsID

	if(strpbrk(m_CreateGamerTag, " !@#$%^&*()-=+_<>,./?'\":;|{}[]")!=NULL) // do not allow this symbols
	{
		Scaleform::GFx::Value var[2];
		var[0].SetStringW(gLangMngr.getString("CharacterNameCannotContaintSpecialSymbols"));
		var[1].SetBoolean(true);
		gfxMovie.Invoke("_root.api.showInfoMsg", var, 2);
		return;
	}

	if(gUserProfile.ProfileData.NumSlots >= 5) 
	{
		Scaleform::GFx::Value var[2];
		var[0].SetStringW(gLangMngr.getString("CannotCreateMoreThan5Char"));
		var[1].SetBoolean(true);
		gfxMovie.Invoke("_root.api.showInfoMsg", var, 2);
		return;
	}

	Scaleform::GFx::Value var[2];
	var[0].SetStringW(gLangMngr.getString("OneMomentPlease"));
	var[1].SetBoolean(false);
	gfxMovie.Invoke("_root.api.showInfoMsg", var, 2);

	async_.StartAsyncOperation(this, &FrontendWarZ::as_CreateCharThread, &FrontendWarZ::OnCreateCharSuccess);
}

void FrontendWarZ::eventDeleteChar(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
{
	Scaleform::GFx::Value var[2];
	var[0].SetStringW(gLangMngr.getString("OneMomentPlease"));
	var[1].SetBoolean(false);
	gfxMovie.Invoke("_root.api.showInfoMsg", var, 2);

	async_.StartAsyncOperation(this, &FrontendWarZ::as_DeleteCharThread, &FrontendWarZ::OnDeleteCharSuccess);
}
void FrontendWarZ::eventReviveCharMoney(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
{
	//gfxMovie.SetVariable("_root.api.Main.PopUpEarlyRevival.visible",false);
	if (gUserProfile.ProfileData.GamePoints < 100)
	{
		Scaleform::GFx::Value var[2];
		var[0].SetStringW(gLangMngr.getString("NotEnoughGP"));
		var[1].SetBoolean(true);
		gfxMovie.Invoke("_root.api.showInfoMsg", var, 2);
		return;
	}
	
	{

		Scaleform::GFx::Value var[2];
		var[0].SetStringW(gLangMngr.getString("OneMomentPlease"));
		var[1].SetBoolean(false);
		gfxMovie.Invoke("_root.api.showInfoMsg", var, 2);
		async_.StartAsyncOperation(this, &FrontendWarZ::as_ReviveCharThread, &FrontendWarZ::OnReviveCharSuccess);
	}
}
void FrontendWarZ::eventReviveChar(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
{
	//gfxMovie.SetVariable("_root.api.Main.PopUpEarlyRevival.Value.Value.text","20");
	if (gUserProfile.ProfileData.ArmorySlots[gUserProfile.SelectedCharID].SecToRevive > 0 && gUserProfile.ProfileData.GamePoints > 99)
	{
		gfxMovie.SetVariable("_root.api.EarlyRevival_Price",100);
		gfxMovie.SetVariable("_root.api.Main.PopUpEarlyRevival.Title.text","Early Revival");
		gfxMovie.SetVariable("_root.api.Main.PopUpEarlyRevival.Text.text","if you want to Early revive you need pay");
		gfxMovie.Invoke("_root.api.Main.PopUpEarlyRevival.showPopUp",0);
		return;
	}

	int timeLeftToRevive = R3D_MAX(gUserProfile.ProfileData.ArmorySlots[gUserProfile.SelectedCharID].SecToRevive - int(r3dGetTime() - gTimeWhenProfileLoaded), 0);
	Scaleform::GFx::Value var[2];
	var[0].SetStringW(gLangMngr.getString("OneMomentPlease"));
	var[1].SetBoolean(false);
	gfxMovie.Invoke("_root.api.showInfoMsg", var, 2);




	async_.StartAsyncOperation(this, &FrontendWarZ::as_ReviveCharThread, &FrontendWarZ::OnReviveCharSuccess);
}

void FrontendWarZ::eventBuyItem(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
{
	market_.eventBuyItem(pMovie, args, argCount);
}

void FrontendWarZ::OnLearnSkillSuccess()
{

	const wiCharDataFull& slot = gUserProfile.ProfileData.ArmorySlots[gUserProfile.SelectedCharID];

	Scaleform::GFx::Value var[2];	

	char tmpGamertag[128];
	if(slot.ClanID != 0)
		sprintf(tmpGamertag, "[%s] %s", slot.ClanTag, slot.Gamertag);
	else
		r3dscpy(tmpGamertag, slot.Gamertag);


	var[0].SetString(tmpGamertag);
	var[1].SetInt(skillid);
	

	gfxMovie.Invoke("_root.api.setSkillLearnedSurvivor", var, 2);

	updateInventoryAndSkillItems();
	const wiCharDataFull& slot2 = gUserProfile.ProfileData.ArmorySlots[gUserProfile.SelectedCharID];
	Scaleform::GFx::Value var2[11];
	var2[0].SetString(tmpGamertag);
	var2[1].SetNumber(slot2.Health);
	var2[2].SetNumber(slot2.Stats.XP);
	var2[3].SetNumber(slot2.Stats.TimePlayed);
	var2[4].SetNumber(slot2.Alive);
	var2[5].SetNumber(slot2.Hunger);
	var2[6].SetNumber(slot2.Thirst);
	var2[7].SetNumber(slot2.Toxic);
	var2[8].SetNumber(slot2.BackpackID);
	var2[9].SetNumber(slot2.BackpackSize);
	var2[10].SetNumber(slot2.Stats.SkillXPPool);
	gfxMovie.Invoke("_root.api.updateClientSurvivor", var2, 11);
	
	gfxMovie.Invoke("_root.api.hideInfoMsg", "");
	gfxMovie.Invoke("_root.api.Main.SkillTree.refreshSkillTree", "");

	return;
}

unsigned int WINAPI FrontendWarZ::as_LearnSkilLThread(void* in_data)
{
	//Skillsystem
	r3dThreadAutoInstallCrashHelper crashHelper;
	FrontendWarZ* This = (FrontendWarZ*)in_data;

	This->async_.DelayServerRequest();
	int apiCode = gUserProfile.ApiLearnSkill(This->skillid, This->CharID);

	
	if(apiCode == 50){
		This->async_.SetAsyncError(0, gLangMngr.getString("FailedToLearnSkill"));
		return 0;
	}
	
		

	return 1;
}

void FrontendWarZ::eventRentServer(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
{
	Scaleform::GFx::Value var[2];
	var[0].SetString("Contact ViruZ Team");
	var[1].SetBoolean(true);
	gfxMovie.Invoke("_root.api.showInfoMsg", var, 2);
}
//Change Name
void FrontendWarZ::eventRenameCharacter(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
{
//args[0].GetString();
    Scaleform::GFx::Value var[2];
    var[0].SetStringW(gLangMngr.getString("Please Wait..."));
    var[1].SetBoolean(false);
    gfxMovie.Invoke("_root.api.showInfoMsg", var, 2);

    gUserProfile.GetProfile();
    updateInventoryAndSkillItems();

        char tmpGamertag[128];
        if(gUserProfile.ProfileData.ArmorySlots[gUserProfile.SelectedCharID].ClanID != 0)
            sprintf(tmpGamertag, "[%s] %s", gUserProfile.ProfileData.ArmorySlots[gUserProfile.SelectedCharID].ClanTag, gUserProfile.ProfileData.ArmorySlots[gUserProfile.SelectedCharID].Gamertag);
        else
            r3dscpy(tmpGamertag, gUserProfile.ProfileData.ArmorySlots[gUserProfile.SelectedCharID].Gamertag);


		//const char* oldname = gUserProfile.ProfileData.ArmorySlots[gUserProfile.SelectedCharID].Gamertag;
        int apiCode = gUserProfile.ApiChangeName(args[0].GetString());
        if(apiCode == 50){
        gfxMovie.Invoke("_root.api.hideInfoMsg", "");
		var[0].SetString("Change Name Failed");
		var[1].SetBoolean(true);
		gfxMovie.Invoke("_root.api.showInfoMsg", var, 2);
        return;
    }

    var[0].SetString(tmpGamertag);
    var[1].SetString(args[0].GetString());
    gfxMovie.Invoke("_root.api.changeSurvivorName", var, 2);


    gfxMovie.Invoke("_root.api.hideInfoMsg", "");


    updateInventoryAndSkillItems();

    Scaleform::GFx::Value vars[1];
    vars[0].SetInt(gUserProfile.ProfileData.GamePoints);
    gfxMovie.Invoke("_root.api.setGC", vars, 1);

    /*var[0].SetInt(gUserProfile.ProfileData.GameDollars);
    gfxMovie.Invoke("_root.api.setDollars", vars, 1);*/
}

void FrontendWarZ::eventRequestMyServerList(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
{
	gfxMovie.Invoke("_root.api.Main.PlayGameMyServers.showServerList", "");
}

void FrontendWarZ::eventRequestGCTransactionData(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
{
	// TODO ADD LOGIC
	Scaleform::GFx::Value var[5];
	var[0].SetInt(1);
	var[1].SetString("07/12/2013");
	var[2].SetString("ViruZ");
	var[3].SetString("+60.10");
	var[4].SetString("1500.10");
	gfxMovie.Invoke("_root.api.Main.Marketplace.addTransactionData", var, 5);

	gfxMovie.Invoke("_root.api.Main.Marketplace.showTransactionsPopup", "");
}

void FrontendWarZ::eventRentServerUpdatePrice(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
{
	//send("eventRentServerUpdatePrice", arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11);
	//warz.events.FrontEndEvents.eventRentServerUpdatePrice(this.isGameServerRenting, this.currentMapID,
	// this.currentRegionID, this.currentSlotID, this.currentPeriodID, this.ServerName.Value.text,
	// this.Password.Value.text, this.isPVEServer, this.isShowNameplates, this.isShowCrosshair, this.isShowTracers);
	Scaleform::GFx::Value var[1];
	var[0].SetInt(1000);
	gfxMovie.Invoke("_root.api.Main.RentServerPopup.updateServerPrice", var, 1);
}

bool FrontendWarZ::ConnectToMasterServer()
{
	masterConnectTime_ = r3dGetTime();
	if(gMasterServerLogic.badClientVersion_)
		return false;
	if(gMasterServerLogic.IsConnected())
		return true;
	
	gMasterServerLogic.Disconnect();
	if(!gMasterServerLogic.StartConnect(_p2p_masterHost, _p2p_masterPort))
	{
		async_.SetAsyncError(0, gLangMngr.getString("NoConnectionToMasterServer"));
		return false;
	}

	const float endTime = r3dGetTime() + 30.0f;
	while(r3dGetTime() < endTime)
	{
		::Sleep(10);
		//if(gMasterServerLogic.IsConnected())
		//	return true;

		if(gMasterServerLogic.versionChecked_ && gMasterServerLogic.badClientVersion_)
			return false;
			
		// if we received server id, connection is ok.
		if(gMasterServerLogic.masterServerId_)
		{
			r3d_assert(gMasterServerLogic.versionChecked_);
			return true;
		}
		
		// early timeout by enet connect fail
		if(!gMasterServerLogic.net_->IsStillConnecting())
			break;
	}
	
	async_.SetAsyncError(8, gLangMngr.getString("TimeoutToMasterServer"));
	return false;
}

bool FrontendWarZ::ParseGameJoinAnswer()
{
	r3d_assert(gMasterServerLogic.gameJoinAnswered_);
	
	switch(gMasterServerLogic.gameJoinAnswer_.result)
	{
	case GBPKT_M2C_JoinGameAns_s::rOk:
		needExitByGameJoin_ = true;
		return true;
	case GBPKT_M2C_JoinGameAns_s::rNoGames:
		async_.SetAsyncError(0, gLangMngr.getString("JoinGameNoGames"));
		return false;
	case GBPKT_M2C_JoinGameAns_s::rGameFull:
		async_.SetAsyncError(0, gLangMngr.getString("GameIsFull"));
		return false;
	case GBPKT_M2C_JoinGameAns_s::rGameFinished:
		async_.SetAsyncError(0, gLangMngr.getString("GameIsAlmostFinished"));
		return false;
	case GBPKT_M2C_JoinGameAns_s::rGameNotFound:
		async_.SetAsyncError(0, gLangMngr.getString("GameNotFound"));
		return false;
	case GBPKT_M2C_JoinGameAns_s::rWrongPassword:
		async_.SetAsyncError(0, gLangMngr.getString("WrongPassword"));
		return false;
	case GBPKT_M2C_JoinGameAns_s::rLevelTooLow:
		async_.SetAsyncError(0, gLangMngr.getString("GameTooLow"));
		return false;
	case GBPKT_M2C_JoinGameAns_s::rLevelTooHigh:
		async_.SetAsyncError(0, gLangMngr.getString("GameTooHigh"));
		return false;
	case GBPKT_M2C_JoinGameAns_s::rJoinDelayActive:
		async_.SetAsyncError(0, gLangMngr.getString("JoinDelayActive"));
		return false;
	}

	wchar_t buf[128];
	swprintf(buf, 128, gLangMngr.getString("UnableToJoinGameCode"), gMasterServerLogic.gameJoinAnswer_.result);
	async_.SetAsyncError(0, buf);
	return  false;
}

unsigned int WINAPI FrontendWarZ::as_PlayGameThread(void* in_data)
{
	r3dThreadAutoInstallCrashHelper crashHelper;
	FrontendWarZ* This_ = (FrontendWarZ*)in_data;

	This_->async_.DelayServerRequest();
	if(!This_->ConnectToMasterServer())
		return 0;
		
	NetPacketsGameBrowser::GBPKT_C2M_QuickGameReq_s n;
	n.CustomerID = gUserProfile.CustomerID;
#ifndef FINAL_BUILD
	n.gameMap    = d_use_test_map->GetInt();
#else
	n.gameMap	 = 0xFF;
#endif
	if(gUserSettings.BrowseGames_Filter.region_us)
		n.region = GBNET_REGION_US_West;
	else if(gUserSettings.BrowseGames_Filter.region_eu)
		n.region = GBNET_REGION_Europe;
	else if(gUserSettings.BrowseGames_Filter.region_ru)
	n.region = GBNET_REGION_Russia;	
	else
		n.region = GBNET_REGION_Unknown;
		
	gMasterServerLogic.SendJoinQuickGame(n);
	
	const float endTime = r3dGetTime() + 60.0f;
	while(r3dGetTime() < endTime)
	{
		::Sleep(10);

		if(This_->CancelQuickJoinRequest)
		{
			This_->CancelQuickJoinRequest = false;
			return 0;
		}

		if(!gMasterServerLogic.IsConnected())
			break;

		if(gMasterServerLogic.gameJoinAnswered_)
		{
			if(!This_->ParseGameJoinAnswer())
				This_->needReturnFromQuickJoin = true;
			return 1;
		}
	}
		
	This_->async_.SetAsyncError(0, gLangMngr.getString("TimeoutJoiningGame"));
	This_->needReturnFromQuickJoin = true;
	return 0;
}

unsigned int WINAPI FrontendWarZ::as_JoinGameThread(void* in_data)
{
	r3dThreadAutoInstallCrashHelper crashHelper;
	FrontendWarZ* This_ = (FrontendWarZ*)in_data;

	This_->async_.DelayServerRequest();
	if(!This_->ConnectToMasterServer())
		return 0;
		
	gMasterServerLogic.SendJoinGame(This_->m_joinGameServerId, This_->passstring);
	//gMasterServerLogic.SendJoinGame(This_->m_joinGameServerId, This_->m_joinGamePwd);
	
	const float endTime = r3dGetTime() + 60.0f;
	while(r3dGetTime() < endTime)
	{
		::Sleep(10);

		if(This_->CancelQuickJoinRequest)
		{
			This_->CancelQuickJoinRequest = false;
			return 0;
		}

		if(!gMasterServerLogic.IsConnected())
			break;

		if(gMasterServerLogic.gameJoinAnswered_)
		{
			This_->ParseGameJoinAnswer();
			return 1;
		}
	}
		
	This_->async_.SetAsyncError(0, gLangMngr.getString("TimeoutJoiningGame"));
	return 0;
}

unsigned int WINAPI FrontendWarZ::as_CreateCharThread(void* in_data)
{
	r3dThreadAutoInstallCrashHelper crashHelper;
	FrontendWarZ* This = (FrontendWarZ*)in_data;
	
	This->async_.DelayServerRequest();
	
	int apiCode = gUserProfile.ApiCharCreate(This->m_CreateGamerTag, This->m_CreateGameMode, This->m_CreateHeroID, This->m_CreateHeadIdx, This->m_CreateBodyIdx, This->m_CreateLegsIdx);
	if(apiCode != 0)
	{
		if(apiCode == 9)
		{
			This->async_.SetAsyncError(0, gLangMngr.getString("ThisNameIsAlreadyInUse"));
		}
		else
			This->async_.SetAsyncError(apiCode, gLangMngr.getString("CreateCharacterFail"));
		return 0;
	}

	return 1;
}

void FrontendWarZ::OnCreateCharSuccess()
{
	gfxMovie.Invoke("_root.api.hideInfoMsg", "");

	Scaleform::GFx::Value var[20];

	int	i = gUserProfile.ProfileData.NumSlots - 1;
	{
		addClientSurvivor(gUserProfile.ProfileData.ArmorySlots[i], i);
	}

	var[0].SetInt(i);
	gfxMovie.Invoke("_root.api.createCharSuccessful", var, 1);

	gUserProfile.SelectedCharID = i;
	m_Player->uberAnim_->anim.StopAll();	// prevent animation blending on loadout switch
	m_Player->UpdateLoadoutSlot(gUserProfile.ProfileData.ArmorySlots[gUserProfile.SelectedCharID]);
	return;
}

unsigned int WINAPI FrontendWarZ::as_DeleteCharThread(void* in_data)
{
	r3dThreadAutoInstallCrashHelper crashHelper;
	FrontendWarZ* This = (FrontendWarZ*)in_data;
	
	This->async_.DelayServerRequest();
	
	int apiCode = gUserProfile.ApiCharDelete();
	if(apiCode != 0)
	{
		if(apiCode == 7)
			This->async_.SetAsyncError(0, gLangMngr.getString("CannotDeleteCharThatIsClanLeader"));
		else
			This->async_.SetAsyncError(apiCode, gLangMngr.getString("FailedToDeleteChar"));
		return 0;
	}

	return 1;
}

void FrontendWarZ::OnDeleteCharSuccess()
{
	gfxMovie.Invoke("_root.api.hideInfoMsg", "");

	gfxMovie.Invoke("_root.api.deleteCharSuccessful", "");

	gUserProfile.SelectedCharID = 0;
	m_Player->uberAnim_->anim.StopAll();	// prevent animation blending on loadout switch
	m_Player->UpdateLoadoutSlot(gUserProfile.ProfileData.ArmorySlots[gUserProfile.SelectedCharID]);
}

unsigned int WINAPI FrontendWarZ::as_ReviveCharThread(void* in_data)
{
	r3dThreadAutoInstallCrashHelper crashHelper;
	FrontendWarZ* This = (FrontendWarZ*)in_data;
	
	This->async_.DelayServerRequest();
	
	int apiCode = gUserProfile.ApiCharRevive();

if(apiCode != 0)
	{
		if(apiCode == 6)
		{
			This->async_.SetAsyncError(0, gLangMngr.getString("FailedToReviveCharGC"));
		}
		else
			This->async_.SetAsyncError(0, gLangMngr.getString("FailedToReviveChar"));
		return 0;
	}

	return 1;
}


void FrontendWarZ::OnReviveCharSuccess()
{


	// sync what server does. after revive you are allowed to access global inventory
	gUserProfile.ProfileData.ArmorySlots[gUserProfile.SelectedCharID].GameFlags |= wiCharDataFull::GAMEFLAG_NearPostBox;

	Scaleform::GFx::Value var[1];
	gfxMovie.Invoke("_root.api.reviveCharSuccessful", "");

	// Update User GC
	var[0].SetInt(gUserProfile.ProfileData.GamePoints);
	gfxMovie.Invoke("_root.api.setGC", var, 1);

	//Viruz Update kill : Update
	updateInventoryAndSkillItems();
	const wiCharDataFull& slot2 = gUserProfile.ProfileData.ArmorySlots[gUserProfile.SelectedCharID];
	Scaleform::GFx::Value var2[11];
	var2[0].SetString(slot2.Gamertag);
	var2[1].SetNumber(slot2.Health);
	var2[2].SetNumber(slot2.Stats.XP);
	var2[3].SetNumber(slot2.Stats.TimePlayed);
	var2[4].SetNumber(slot2.Alive);
	var2[5].SetNumber(slot2.Hunger);
	var2[6].SetNumber(slot2.Thirst);
	var2[7].SetNumber(slot2.Toxic);
	var2[8].SetNumber(slot2.BackpackID);
	var2[9].SetNumber(slot2.BackpackSize);
	var2[10].SetNumber(slot2.Stats.SkillXPPool);
	gfxMovie.Invoke("_root.api.updateClientSurvivor", var2, 11);
	gfxMovie.Invoke("_root.api.hideInfoMsg", "");
	gfxMovie.Invoke("_root.api.Main.SkillTree.refreshSkillTree", "");
	//r3dOutToLog("reload skills : Update Health");
	gfxMovie.Invoke("_root.api.hideInfoMsg", "");
	return;
}

void FrontendWarZ::initInventoryItems()
{
	updateInventoryAndSkillItems ();
}

void FrontendWarZ::updateInventoryAndSkillItems()
{
	Scaleform::GFx::Value var[7];
	// clear inventory DB
	gfxMovie.Invoke("_root.api.clearInventory", NULL, 0);

	// add all items
	for(uint32_t i=0; i<gUserProfile.ProfileData.NumItems; ++i)
	{
		var[0].SetUInt(uint32_t(gUserProfile.ProfileData.Inventory[i].InventoryID));
		var[1].SetUInt(gUserProfile.ProfileData.Inventory[i].itemID);
		var[2].SetNumber(gUserProfile.ProfileData.Inventory[i].quantity);
		var[3].SetNumber(gUserProfile.ProfileData.Inventory[i].Var1);
		var[4].SetNumber(gUserProfile.ProfileData.Inventory[i].Var2);
		bool isConsumable = false;
		{
			const WeaponConfig* wc = g_pWeaponArmory->getWeaponConfig(gUserProfile.ProfileData.Inventory[i].itemID);
			if(wc && wc->category == storecat_UsableItem && wc->m_isConsumable)
				isConsumable = true;
		}
		var[5].SetBoolean(isConsumable);
		char tmpStr[128] = {0};
		getAdditionalDescForItem(gUserProfile.ProfileData.Inventory[i].itemID, gUserProfile.ProfileData.Inventory[i].Var1, gUserProfile.ProfileData.Inventory[i].Var2, tmpStr);
		var[6].SetString(tmpStr);
		gfxMovie.Invoke("_root.api.addInventoryItem", var, 7);
		}
	for(int i=0;i<=4; i++)
		{
				const wiCharDataFull& slot = gUserProfile.ProfileData.ArmorySlots[i];
				char tmpGamertag[128];
				if(slot.ClanID != 0)
					sprintf(tmpGamertag, "[%s] %s", slot.ClanTag, slot.Gamertag);
				else
					r3dscpy(tmpGamertag, slot.Gamertag);


			Scaleform::GFx::Value var2[2];
			if(slot.Stats.skillid0 == 1){
				var2[0].SetString(tmpGamertag);
				var2[1].SetInt(0);
				gfxMovie.Invoke("_root.api.setSkillLearnedSurvivor", var2, 2);
			}
			if(slot.Stats.skillid1 == 1){
				var2[0].SetString(tmpGamertag);
				var2[1].SetInt(1);
				gfxMovie.Invoke("_root.api.setSkillLearnedSurvivor", var2, 2);
			}
			if(slot.Stats.skillid2 == 1){
				var2[0].SetString(tmpGamertag);
				var2[1].SetInt(2);
				gfxMovie.Invoke("_root.api.setSkillLearnedSurvivor", var2, 2);
			}
			if(slot.Stats.skillid3 == 1){
				var2[0].SetString(tmpGamertag);
				var2[1].SetInt(3);
				gfxMovie.Invoke("_root.api.setSkillLearnedSurvivor", var2, 2);
			}
			if(slot.Stats.skillid4 == 1){
				var2[0].SetString(tmpGamertag);
				var2[1].SetInt(4);
				gfxMovie.Invoke("_root.api.setSkillLearnedSurvivor", var2, 2);
			}
			if(slot.Stats.skillid5 == 1){
				var2[0].SetString(tmpGamertag);
				var2[1].SetInt(5);
				gfxMovie.Invoke("_root.api.setSkillLearnedSurvivor", var2, 2);
			}
			if(slot.Stats.skillid6 == 1){
				var2[0].SetString(tmpGamertag);
				var2[1].SetInt(6);
				gfxMovie.Invoke("_root.api.setSkillLearnedSurvivor", var2, 2);
			}
			if(slot.Stats.skillid7 == 1){
				var2[0].SetString(tmpGamertag);
				var2[1].SetInt(7);
				gfxMovie.Invoke("_root.api.setSkillLearnedSurvivor", var2, 2);
			}
			if(slot.Stats.skillid8 == 1){
				var2[0].SetString(tmpGamertag);
				var2[1].SetInt(8);
				gfxMovie.Invoke("_root.api.setSkillLearnedSurvivor", var2, 2);
			}
			if(slot.Stats.skillid9 == 1){
				var2[0].SetString(tmpGamertag);
				var2[1].SetInt(9);
				gfxMovie.Invoke("_root.api.setSkillLearnedSurvivor", var2, 2);
			}
			if(slot.Stats.skillid10 == 1){
				var2[0].SetString(tmpGamertag);
				var2[1].SetInt(10);
				gfxMovie.Invoke("_root.api.setSkillLearnedSurvivor", var2, 2);
			}
			if(slot.Stats.skillid11 == 1){
				var2[0].SetString(tmpGamertag);
				var2[1].SetInt(11);
				gfxMovie.Invoke("_root.api.setSkillLearnedSurvivor", var2, 2);
			}
			if(slot.Stats.skillid12 == 1){
				var2[0].SetString(tmpGamertag);
				var2[1].SetInt(12);
				gfxMovie.Invoke("_root.api.setSkillLearnedSurvivor", var2, 2);
			}
			if(slot.Stats.skillid13 == 1){
				var2[0].SetString(tmpGamertag);
				var2[1].SetInt(13);
				gfxMovie.Invoke("_root.api.setSkillLearnedSurvivor", var2, 2);
			}
			if(slot.Stats.skillid14 == 1){
				var2[0].SetString(tmpGamertag);
				var2[1].SetInt(14);
				gfxMovie.Invoke("_root.api.setSkillLearnedSurvivor", var2, 2);
			}
			if(slot.Stats.skillid15 == 1){
				var2[0].SetString(tmpGamertag);
				var2[1].SetInt(15);
				gfxMovie.Invoke("_root.api.setSkillLearnedSurvivor", var2, 2);
			}
			if(slot.Stats.skillid16 == 1){
				var2[0].SetString(tmpGamertag);
				var2[1].SetInt(16);
				gfxMovie.Invoke("_root.api.setSkillLearnedSurvivor", var2, 2);
			}
			if(slot.Stats.skillid17 == 1){
				var2[0].SetString(tmpGamertag);
				var2[1].SetInt(17);
				gfxMovie.Invoke("_root.api.setSkillLearnedSurvivor", var2, 2);
			}
			if(slot.Stats.skillid18 == 1){
				var2[0].SetString(tmpGamertag);
				var2[1].SetInt(18);
				gfxMovie.Invoke("_root.api.setSkillLearnedSurvivor", var2, 2);
			}
			if(slot.Stats.skillid19 == 1){
				var2[0].SetString(tmpGamertag);
				var2[1].SetInt(19);
				gfxMovie.Invoke("_root.api.setSkillLearnedSurvivor", var2, 2);
			}
			if(slot.Stats.skillid20 == 1){
				var2[0].SetString(tmpGamertag);
				var2[1].SetInt(20);
				gfxMovie.Invoke("_root.api.setSkillLearnedSurvivor", var2, 2);
			}
			if(slot.Stats.skillid21 == 1){
				var2[0].SetString(tmpGamertag);
				var2[1].SetInt(21);
				gfxMovie.Invoke("_root.api.setSkillLearnedSurvivor", var2, 2);
			}
			if(slot.Stats.skillid22 == 1){
				var2[0].SetString(tmpGamertag);
				var2[1].SetInt(22);
				gfxMovie.Invoke("_root.api.setSkillLearnedSurvivor", var2, 2);
			}
			if(slot.Stats.skillid23 == 1){
				var2[0].SetString(tmpGamertag);
				var2[1].SetInt(23);
				gfxMovie.Invoke("_root.api.setSkillLearnedSurvivor", var2, 2);
			}
			if(slot.Stats.skillid24 == 1){
				var2[0].SetString(tmpGamertag);
				var2[1].SetInt(24);
				gfxMovie.Invoke("_root.api.setSkillLearnedSurvivor", var2, 2);
			}
			if(slot.Stats.skillid25 == 1){
				var2[0].SetString(tmpGamertag);
				var2[1].SetInt(25);
				gfxMovie.Invoke("_root.api.setSkillLearnedSurvivor", var2, 2);
			}
			if(slot.Stats.skillid26 == 1){
				var2[0].SetString(tmpGamertag);
				var2[1].SetInt(26);
				gfxMovie.Invoke("_root.api.setSkillLearnedSurvivor", var2, 2);
			}
			if(slot.Stats.skillid27 == 1){
				var2[0].SetString(tmpGamertag);
				var2[1].SetInt(27);
				gfxMovie.Invoke("_root.api.setSkillLearnedSurvivor", var2, 2);
			}
			if(slot.Stats.skillid28 == 1){
				var2[0].SetString(tmpGamertag);
				var2[1].SetInt(28);
				gfxMovie.Invoke("_root.api.setSkillLearnedSurvivor", var2, 2);
			}
			if(slot.Stats.skillid29 == 1){
				var2[0].SetString(tmpGamertag);
				var2[1].SetInt(29);
				gfxMovie.Invoke("_root.api.setSkillLearnedSurvivor", var2, 2);
			}
			if(slot.Stats.skillid30 == 1){
				var2[0].SetString(tmpGamertag);
				var2[1].SetInt(30);
				gfxMovie.Invoke("_root.api.setSkillLearnedSurvivor", var2, 2);
			}
			if(slot.Stats.skillid31 == 1){
				var2[0].SetString(tmpGamertag);
				var2[1].SetInt(31);
				gfxMovie.Invoke("_root.api.setSkillLearnedSurvivor", var2, 2);
			}
			if(slot.Stats.skillid32 == 1){
				var2[0].SetString(tmpGamertag);
				var2[1].SetInt(32);
				gfxMovie.Invoke("_root.api.setSkillLearnedSurvivor", var2, 2);
			}
			if(slot.Stats.skillid33 == 1){
				var2[0].SetString(tmpGamertag);
				var2[1].SetInt(33);
				gfxMovie.Invoke("_root.api.setSkillLearnedSurvivor", var2, 2);
			}

		}
}
void FrontendWarZ::eventBackpackFromInventory(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
{
	const wiCharDataFull& slot = gUserProfile.ProfileData.ArmorySlots[gUserProfile.SelectedCharID];
	if(!(slot.GameFlags & wiCharDataFull::GAMEFLAG_NearPostBox))
		return;

	m_inventoryID = args[0].GetUInt();
	m_gridTo = args[1].GetInt();
	m_Amount = args[2].GetInt();

	Scaleform::GFx::Value var[2];
	var[0].SetStringW(gLangMngr.getString("OneMomentPlease"));
	var[1].SetBoolean(false);
	gfxMovie.Invoke("_root.api.showInfoMsg", var, 2);

	uint32_t itemID = 0;
	for(uint32_t i=0; i<gUserProfile.ProfileData.NumItems; ++i)
	{
		if(gUserProfile.ProfileData.Inventory[i].InventoryID == m_inventoryID)
		{
			itemID = gUserProfile.ProfileData.Inventory[i].itemID;
			break;
		}
	}

	// check to see if there is anything in backpack, and if there is, then we need to firstly move that item to inventory
	if(slot.Items[m_gridTo].itemID != 0 && slot.Items[m_gridTo].itemID!=itemID)
	{
		m_gridFrom = m_gridTo;
		m_Amount2 = slot.Items[m_gridTo].quantity;

		// check weight
		float totalWeight = gUserProfile.ProfileData.ArmorySlots[gUserProfile.SelectedCharID].getTotalWeight();
		const BaseItemConfig* bic = g_pWeaponArmory->getConfig(slot.Items[m_gridTo].itemID);
		if(bic)
			totalWeight -= bic->m_Weight*slot.Items[m_gridTo].quantity;
	
		bic = g_pWeaponArmory->getConfig(itemID);
		if(bic)
			totalWeight += bic->m_Weight*m_Amount;
				//Skillsystem
		if(slot.Stats.skillid2 == 1){
			totalWeight *= 0.9f;
			if(slot.Stats.skillid6 == 1)
				totalWeight *= 0.7f;
		}

		const BackpackConfig* bc = g_pWeaponArmory->getBackpackConfig(gUserProfile.ProfileData.ArmorySlots[gUserProfile.SelectedCharID].BackpackID);
		r3d_assert(bc);
		
		if(totalWeight > bc->m_maxWeight)
		{
			Scaleform::GFx::Value var[2];
			var[0].SetStringW(gLangMngr.getString("FR_PAUSE_TOO_MUCH_WEIGHT"));
			var[1].SetBoolean(true);
			var[2].SetString("");
			gfxMovie.Invoke("_root.api.showInfoMsg", var, 3);
			return;
		}

		async_.StartAsyncOperation(this, &FrontendWarZ::as_BackpackFromInventorySwapThread, &FrontendWarZ::OnBackpackFromInventorySuccess);
	}
	else
	{
		// check weight
		float totalWeight = gUserProfile.ProfileData.ArmorySlots[gUserProfile.SelectedCharID].getTotalWeight();
		if(slot.Stats.skillid2 == 1){
			totalWeight *= 0.9f;
			if(slot.Stats.skillid6 == 1)
				totalWeight *= 0.7f;
		}
		const BaseItemConfig* bic = g_pWeaponArmory->getConfig(itemID);
		if(bic)
			totalWeight += bic->m_Weight*m_Amount;

		const BackpackConfig* bc = g_pWeaponArmory->getBackpackConfig(gUserProfile.ProfileData.ArmorySlots[gUserProfile.SelectedCharID].BackpackID);
		r3d_assert(bc);
		
		if(totalWeight > bc->m_maxWeight)
		{
			Scaleform::GFx::Value var[2];
			var[0].SetStringW(gLangMngr.getString("FR_PAUSE_TOO_MUCH_WEIGHT"));
			var[1].SetBoolean(true);
			var[2].SetString("");
			gfxMovie.Invoke("_root.api.showInfoMsg", var, 3);
			return;
		}

		async_.StartAsyncOperation(this, &FrontendWarZ::as_BackpackFromInventoryThread, &FrontendWarZ::OnBackpackFromInventorySuccess);
	}
}

unsigned int WINAPI FrontendWarZ::as_BackpackFromInventorySwapThread(void* in_data)
{
	r3dThreadAutoInstallCrashHelper crashHelper;
	FrontendWarZ* This = (FrontendWarZ*)in_data;

	This->async_.DelayServerRequest();

	// move item in backpack to inventory
	int apiCode = gUserProfile.ApiBackpackToInventory(This->m_gridFrom, This->m_Amount2);
	if(apiCode != 0)
	{
		if(apiCode==7)
			This->async_.SetAsyncError(0, gLangMngr.getString("GameSessionHasNotClosedYet"));
		else
			This->async_.SetAsyncError(apiCode, gLangMngr.getString("BackpackToInventoryFail"));
		return 0;
	}

	apiCode = gUserProfile.ApiBackpackFromInventory(This->m_inventoryID, This->m_gridTo, This->m_Amount);
	if(apiCode != 0)
	{
		if(apiCode==7)
			This->async_.SetAsyncError(0, gLangMngr.getString("GameSessionHasNotClosedYet"));
		else
			This->async_.SetAsyncError(apiCode, gLangMngr.getString("FailedToFindBackpack"));
		return 0;
	}

	return 1;
}


unsigned int WINAPI FrontendWarZ::as_BackpackFromInventoryThread(void* in_data)
{
	r3dThreadAutoInstallCrashHelper crashHelper;
	FrontendWarZ* This = (FrontendWarZ*)in_data;
	
	This->async_.DelayServerRequest();
	
	int apiCode = gUserProfile.ApiBackpackFromInventory(This->m_inventoryID, This->m_gridTo, This->m_Amount);
	if(apiCode != 0)
	{
		if(apiCode==7)
			This->async_.SetAsyncError(0, gLangMngr.getString("GameSessionHasNotClosedYet"));
		else
			This->async_.SetAsyncError(apiCode, gLangMngr.getString("BackpackFromInventoryFail"));
		return 0;
	}

	return 1;
}

void FrontendWarZ::OnBackpackFromInventorySuccess()
{
	Scaleform::GFx::Value var[8];
	gfxMovie.Invoke("_root.api.clearBackpack", "");
	int	slot = gUserProfile.SelectedCharID;

	addBackpackItems(gUserProfile.ProfileData.ArmorySlots[slot], slot);

	updateInventoryAndSkillItems ();

	updateSurvivorTotalWeight(slot);

	gfxMovie.Invoke("_root.api.hideInfoMsg", "");
	gfxMovie.Invoke("_root.api.backpackFromInventorySuccess", "");
	return;
}

void FrontendWarZ::updateSurvivorTotalWeight(int survivor)
{
    wiCharDataFull& slot = gUserProfile.ProfileData.ArmorySlots[survivor];

    char tmpGamertag[128];
    if(slot.ClanID != 0)
        sprintf(tmpGamertag, "[%s] %s", slot.ClanTag, slot.Gamertag);
    else
        r3dscpy(tmpGamertag, slot.Gamertag);

    float totalWeight = slot.getTotalWeight();
	if(slot.Stats.skillid2 == 1){
		totalWeight *= 0.9f;
		if(slot.Stats.skillid6 == 1)
			totalWeight *= 0.7f;
	}
    Scaleform::GFx::Value var[2];
    var[0].SetString(tmpGamertag);
    var[1].SetNumber(totalWeight);
    gfxMovie.Invoke("_root.api.updateClientSurvivorWeight", var, 2);
}

void FrontendWarZ::eventBackpackToInventory(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
{
	const wiCharDataFull& slot = gUserProfile.ProfileData.ArmorySlots[gUserProfile.SelectedCharID];
	if(!(slot.GameFlags & wiCharDataFull::GAMEFLAG_NearPostBox))
		return;

	m_gridFrom = args[0].GetInt();
	m_Amount = args[1].GetInt();

	Scaleform::GFx::Value var[2];
	var[0].SetStringW(gLangMngr.getString("OneMomentPlease"));
	var[1].SetBoolean(false);
	gfxMovie.Invoke("_root.api.showInfoMsg", var, 2);

	async_.StartAsyncOperation(this, &FrontendWarZ::as_BackpackToInventoryThread, &FrontendWarZ::OnBackpackToInventorySuccess);
}

unsigned int WINAPI FrontendWarZ::as_BackpackToInventoryThread(void* in_data)
{
	r3dThreadAutoInstallCrashHelper crashHelper;
	FrontendWarZ* This = (FrontendWarZ*)in_data;
	
	This->async_.DelayServerRequest();
	
	int apiCode = gUserProfile.ApiBackpackToInventory(This->m_gridFrom, This->m_Amount);
	if(apiCode != 0)
	{
		if(apiCode==7)
			This->async_.SetAsyncError(0, gLangMngr.getString("GameSessionHasNotClosedYet"));
		else
			This->async_.SetAsyncError(apiCode, gLangMngr.getString("BackpackToInventoryFail"));
		return 0;
	}

	return 1;
}

void FrontendWarZ::OnBackpackToInventorySuccess()
{
	Scaleform::GFx::Value var[8];
	gfxMovie.Invoke("_root.api.clearBackpack", "");
	int	slot = gUserProfile.SelectedCharID;

	addBackpackItems(gUserProfile.ProfileData.ArmorySlots[slot], slot);

	updateInventoryAndSkillItems ();

	updateSurvivorTotalWeight(slot);

	gfxMovie.Invoke("_root.api.hideInfoMsg", "");
	gfxMovie.Invoke("_root.api.backpackToInventorySuccess", "");


	return;
}

void FrontendWarZ::eventBackpackGridSwap(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
{
	m_gridFrom = args[0].GetInt();
	m_gridTo = args[1].GetInt();

	Scaleform::GFx::Value var[2];
	var[0].SetStringW(gLangMngr.getString("OneMomentPlease"));
	var[1].SetBoolean(false);
	gfxMovie.Invoke("_root.api.showInfoMsg", var, 2);

	async_.StartAsyncOperation(this, &FrontendWarZ::as_BackpackGridSwapThread, &FrontendWarZ::OnBackpackGridSwapSuccess);
}

void FrontendWarZ::eventSetSelectedChar(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
{
	gUserProfile.SelectedCharID = args[0].GetInt();
	m_Player->uberAnim_->anim.StopAll();	// prevent animation blending on loadout switch
	m_Player->UpdateLoadoutSlot(gUserProfile.ProfileData.ArmorySlots[gUserProfile.SelectedCharID]);
}

void FrontendWarZ::eventOpenBackpackSelector(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
{
	r3d_assert(argCount == 0);
	Scaleform::GFx::Value var[2];

	// clear
	gfxMovie.Invoke("_root.api.clearBackpacks", "");

	std::vector<uint32_t> uniqueBackpacks; // to filter identical backpack
	
	int backpackSlotIDInc = 0;
	// add backpack content info
	{
		wiCharDataFull& slot = gUserProfile.ProfileData.ArmorySlots[gUserProfile.SelectedCharID];

		for (int a = 0; a < slot.BackpackSize; a++)
		{
			if (slot.Items[a].itemID != 0)
			{
				if(std::find<std::vector<uint32_t>::iterator, uint32_t>(uniqueBackpacks.begin(), uniqueBackpacks.end(), slot.Items[a].itemID) != uniqueBackpacks.end())
					continue;
				
				const BackpackConfig* bpc = g_pWeaponArmory->getBackpackConfig(slot.Items[a].itemID);
				if(bpc)
				{
					// add backpack info
					var[0].SetInt(backpackSlotIDInc++);
					var[1].SetUInt(slot.Items[a].itemID);
					gfxMovie.Invoke("_root.api.addBackpack", var, 2);

					uniqueBackpacks.push_back(slot.Items[a].itemID);
				}
			}
		}
	}
	// add inventory info
	for(uint32_t i=0; i<gUserProfile.ProfileData.NumItems; ++i)
	{
		if(std::find<std::vector<uint32_t>::iterator, uint32_t>(uniqueBackpacks.begin(), uniqueBackpacks.end(), gUserProfile.ProfileData.Inventory[i].itemID) != uniqueBackpacks.end())
			continue;

		const BackpackConfig* bpc = g_pWeaponArmory->getBackpackConfig(gUserProfile.ProfileData.Inventory[i].itemID);
		if(bpc)
		{
			// add backpack info
			var[0].SetInt(backpackSlotIDInc++);
			var[1].SetUInt(gUserProfile.ProfileData.Inventory[i].itemID);
			gfxMovie.Invoke("_root.api.addBackpack", var, 2);

			uniqueBackpacks.push_back(gUserProfile.ProfileData.Inventory[i].itemID);
		}
	}

	gfxMovie.Invoke("_root.api.showChangeBackpack", "");
}

void FrontendWarZ::eventChangeBackpack(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
{
	r3d_assert(argCount == 2);
	uint32_t itemID = args[1].GetUInt();

	// find inventory id with that itemID
	__int64 inventoryID = 0;
	wiCharDataFull& slot = gUserProfile.ProfileData.ArmorySlots[gUserProfile.SelectedCharID];
	//antidupe
	int slotFrom = -1;
	for(int b=0; b<slot.BackpackSize; ++b)
	{
		if(slot.Items[b].itemID == itemID)
		{


		if(slot.Items[b].quantity > 1)
		{
			//r3dMouse::Show();

		Scaleform::GFx::Value vars[3];
		vars[0].SetString("ViruZ Antidupe System!");
		vars[1].SetBoolean("Okey");
		vars[2].SetString("Warning");
		gfxMovie.Invoke("_root.api.showInfoMsg", vars, 3);
		return;
			
		}

			slotFrom = b;
			break;
		}
	} 

	if(slotFrom == -1)
		return;
	//fim do antidupe

	for (int a = 0; a < slot.BackpackSize; a++)
	{
		if (slot.Items[a].itemID == itemID)
		{
			inventoryID = slot.Items[a].InventoryID;
			break;
		}
	}
	if(inventoryID == 0)
	{
		for(uint32_t i=0; i<gUserProfile.ProfileData.NumItems; ++i)
		{
			if(gUserProfile.ProfileData.Inventory[i].itemID == itemID)
			{
				inventoryID = gUserProfile.ProfileData.Inventory[i].InventoryID;
				break;
			}
		}
	}

	if(inventoryID == 0)
	{
		Scaleform::GFx::Value vars[3];
		vars[0].SetString("Failed to find backpack!");
		vars[1].SetBoolean(true);
		vars[2].SetString("ERROR");
		gfxMovie.Invoke("_root.api.showInfoMsg", vars, 3);
		return;
	}

	Scaleform::GFx::Value var[2];
	var[0].SetStringW(gLangMngr.getString("OneMomentPlease"));
	var[1].SetBoolean(false);
	gfxMovie.Invoke("_root.api.showInfoMsg", var, 2);		

	mChangeBackpack_inventoryID = inventoryID;
	async_.StartAsyncOperation(this, &FrontendWarZ::as_BackpackChangeThread, &FrontendWarZ::OnBackpackChangeSuccess);
}

unsigned int WINAPI FrontendWarZ::as_BackpackChangeThread(void* in_data)
{
	r3dThreadAutoInstallCrashHelper crashHelper;
	FrontendWarZ* This = (FrontendWarZ*)in_data;

	This->async_.DelayServerRequest();

	int apiCode = gUserProfile.ApiChangeBackpack(This->mChangeBackpack_inventoryID);
	if(apiCode != 0)
	{
		This->async_.SetAsyncError(apiCode, gLangMngr.getString("FailedToFindBackpack"));
		return 0;
	}

	return 1;
}


void FrontendWarZ::OnBackpackChangeSuccess()
{
	gfxMovie.Invoke("_root.api.hideInfoMsg", "");

	wiCharDataFull& slot = gUserProfile.ProfileData.ArmorySlots[gUserProfile.SelectedCharID];
	//leo
	char tmpGamertag[128];
    if(slot.ClanID != 0)
        sprintf(tmpGamertag, "[%s] %s", slot.ClanTag, slot.Gamertag);
    else
        r3dscpy(tmpGamertag, slot.Gamertag); //leo fix backpack cnhage in clan

	Scaleform::GFx::Value var[11];
	var[0].SetString(tmpGamertag);
	var[1].SetNumber(slot.Health);
	var[2].SetNumber(slot.Stats.XP);
	var[3].SetNumber(slot.Stats.TimePlayed);
	var[4].SetNumber(slot.Alive);
	var[5].SetNumber(slot.Hunger);
	var[6].SetNumber(slot.Thirst);
	var[7].SetNumber(slot.Toxic);
	var[8].SetNumber(slot.BackpackID);
	var[9].SetNumber(slot.BackpackSize);
	var[10].SetNumber(slot.Stats.SkillXPPool);
	gfxMovie.Invoke("_root.api.updateClientSurvivor", var, 11);

	addBackpackItems(slot, gUserProfile.SelectedCharID);
	updateInventoryAndSkillItems();

	updateSurvivorTotalWeight(gUserProfile.SelectedCharID);

	gfxMovie.Invoke("_root.api.changeBackpackSuccess", "");
}

void FrontendWarZ::eventSetCurrentBrowseChannel(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
{
	r3d_assert(argCount == 2);
	CurrentBrowse = args[0].GetUInt();

	gfxMovie.Invoke("_root.api.Main.showScreen","ServerBrowse");
	/*Scaleform::GFx::Value vars1[3];
	vars1[1].SetBoolean(true);
	vars1[2].SetString("NOTICE");
	vars1[0].SetInt(CurrentBrowse);
	gfxMovie.Invoke("_root.api.showInfoMsg", vars1, 3);  	*/

	//  r3dOutToLog("%d",CurrentBrowse);
}
void FrontendWarZ::eventChangeOutfit(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
{
	r3d_assert(argCount == 4);
	int headIdx = args[1].GetInt();
	int bodyIdx = args[2].GetInt();
	int legsIdx = args[3].GetInt();


	gUserProfile.ApiChangeOutfit(headIdx, bodyIdx, legsIdx);


	gUserProfile.ProfileData.ArmorySlots[gUserProfile.SelectedCharID].HeadIdx = headIdx;
	gUserProfile.ProfileData.ArmorySlots[gUserProfile.SelectedCharID].BodyIdx = bodyIdx;
	gUserProfile.ProfileData.ArmorySlots[gUserProfile.SelectedCharID].LegsIdx = legsIdx;

	m_Player->uberAnim_->anim.StopAll();
	m_Player->UpdateLoadoutSlot(m_Player->CurLoadout);
}

void FrontendWarZ::eventCreateChangeCharacter(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
{
	PlayerCreateLoadout.HeroItemID = args[0].GetInt();
	PlayerCreateLoadout.HeadIdx = args[1].GetInt();
	PlayerCreateLoadout.BodyIdx = args[2].GetInt();
	PlayerCreateLoadout.LegsIdx = args[3].GetInt();

	m_Player->uberAnim_->anim.StopAll();	// prevent animation blending on loadout switch
	m_Player->UpdateLoadoutSlot(PlayerCreateLoadout);
}

void FrontendWarZ::eventCreateCancel(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
{
	m_Player->uberAnim_->anim.StopAll();	// prevent animation blending on loadout switch
	m_Player->UpdateLoadoutSlot(gUserProfile.ProfileData.ArmorySlots[gUserProfile.SelectedCharID]);
}

void FrontendWarZ::eventRequestPlayerRender(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
{
	r3d_assert(argCount==1);
	m_needPlayerRenderingRequest = args[0].GetInt();
}

unsigned int WINAPI FrontendWarZ::as_BackpackGridSwapThread(void* in_data)
{
	r3dThreadAutoInstallCrashHelper crashHelper;
	FrontendWarZ* This = (FrontendWarZ*)in_data;
	
	This->async_.DelayServerRequest();
	
	int apiCode = gUserProfile.ApiBackpackGridSwap(This->m_gridFrom, This->m_gridTo);
	if(apiCode != 0)
	{
		if(apiCode==7)
			This->async_.SetAsyncError(0, gLangMngr.getString("GameSessionHasNotClosedYet"));
		else
			This->async_.SetAsyncError(apiCode, gLangMngr.getString("SwitchBackpackSameBackpacks"));
		return 0;
	}

	return 1;
}

void FrontendWarZ::OnBackpackGridSwapSuccess()
{
	Scaleform::GFx::Value var[8];

	gfxMovie.Invoke("_root.api.clearBackpack", "");
	int	slot = gUserProfile.SelectedCharID;

	addBackpackItems(gUserProfile.ProfileData.ArmorySlots[slot], slot);

	updateSurvivorTotalWeight(slot);

	gfxMovie.Invoke("_root.api.hideInfoMsg", "");
	gfxMovie.Invoke("_root.api.backpackGridSwapSuccess", "");
	return;
}

void FrontendWarZ::eventOptionsLanguageSelection(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
{
	r3d_assert(args);
	r3d_assert(argCount == 1);

	const char* newLang = args[0].GetString();

	if(strcmp(newLang, g_user_language->GetString())==0)
		return; // same language

#ifdef FINAL_BUILD
	{
		Scaleform::GFx::Value var[2];
		var[0].SetStringW(L"LOCALIZATIONS ARE COMING SOON");
		var[1].SetBoolean(true);
		pMovie->Invoke("_root.api.showInfoMsg", var, 2);		
		return;
	}
#endif

	g_user_language->SetString(newLang);

	Scaleform::GFx::Value var[2];
	var[0].SetStringW(gLangMngr.getString("NewLanguageSetAfterRestart"));
	var[1].SetBoolean(true);
	pMovie->Invoke("_root.api.showInfoMsg", var, 2);		

	// write to ini file
	writeGameOptionsFile();
}

void FrontendWarZ::AddSettingsChangeFlag( DWORD flag )
{
	settingsChangeFlags_ |= flag;
}

static int compRes( const void* r1, const void* r2 )
{
	const r3dDisplayResolution* rr1 = (const r3dDisplayResolution*)r1 ;
	const r3dDisplayResolution* rr2 = (const r3dDisplayResolution*)r2 ;

	return rr1->Width - rr2->Width; // sort resolutions by width
}

void FrontendWarZ::SyncGraphicsUI()
{
	const DisplayResolutions& reses = r3dRenderer->GetDisplayResolutions();

	DisplayResolutions supportedReses ;

	for( uint32_t i = 0, e = reses.Count(); i < e; i ++ )
	{
		const r3dDisplayResolution& r = reses[ i ];
		float aspect = (float)r.Width / r.Height ;
		supportedReses.PushBack( r );
	}

	if(supportedReses.Count() == 0)
		r3dError("Couldn't find any supported video resolutions. Bad video driver?!\n");

	qsort( &supportedReses[ 0 ], supportedReses.Count(), sizeof supportedReses[ 0 ], compRes );

	gfxMovie.Invoke("_root.api.clearScreenResolutions", "");
	for( uint32_t i = 0, e = supportedReses.Count() ; i < e; i ++ )
	{
		char resString[ 128 ] = { 0 };
		const r3dDisplayResolution& r = supportedReses[ i ] ;
		_snprintf( resString, sizeof resString - 1, "%dx%d", r.Width, r.Height );
		gfxMovie.Invoke("_root.api.addScreenResolution", resString);
	}

	int width	= r_width->GetInt();
	int height	= r_height->GetInt();

	int desktopWidth, desktopHeight ;
	r3dGetDesktopDimmensions( &desktopWidth, &desktopHeight );

	if( !r_ini_read->GetBool() )
	{
		if( desktopWidth < width || desktopHeight < height )
		{
			width = desktopWidth ;
			height = desktopHeight ;
		}
	}

	bool finalResSet = false ;
	int selectedRes = 0;
	for( uint32_t i = 0, e = supportedReses.Count() ; i < e; i ++ )
	{
		const r3dDisplayResolution& r = supportedReses[ i ] ;
		if( width == r.Width && height == r.Height )
		{
			selectedRes = i;
			finalResSet = true;
			break;
		}
	}

	if( !finalResSet )
	{
		int bestSum = 0 ;

		for( uint32_t i = 0, e = supportedReses.Count() ; i < e; i ++ )
		{
			const r3dDisplayResolution& r = supportedReses[ i ] ;

			if( width >= r.Width && 
				height >= r.Height )
			{
				if( r.Width + r.Height > bestSum )
				{
					selectedRes = i;
					bestSum = r.Width + r.Height ;
					finalResSet = true ;
				}
			}
		}
	}

	if( !finalResSet )
	{
		int bestSum = 0x7fffffff ;

		// required mode too small, find smallest mode..
		for( uint32_t i = 0, e = supportedReses.Count() ; i < e; i ++ )
		{
			const r3dDisplayResolution& r = supportedReses[ i ] ;

			if( r.Width + r.Height < bestSum )
			{
				finalResSet = true ;

				selectedRes = i;
				bestSum = r.Width + r.Height ;
			}
		}
	}

	Scaleform::GFx::Value var[30];
	var[0].SetNumber(selectedRes);
	var[1].SetNumber( r_overall_quality->GetInt());
	var[2].SetNumber( ((r_gamma_pow->GetFloat()-2.2f)+1.0f)/2.0f);
	var[3].SetNumber( 0.0f );
	var[4].SetNumber( s_sound_volume->GetFloat());
	var[5].SetNumber( s_music_volume->GetFloat());
	var[6].SetNumber( s_comm_volume->GetFloat());
	var[7].SetNumber( g_tps_camera_mode->GetInt());
	var[8].SetNumber( g_enable_voice_commands->GetBool());
	var[9].SetNumber( r_antialiasing_quality->GetInt());
	var[10].SetNumber( r_ssao_quality->GetInt());
	var[11].SetNumber( r_terrain_quality->GetInt());
	var[12].SetNumber( r_decoration_quality->GetInt() ); 
	var[13].SetNumber( r_water_quality->GetInt());
	var[14].SetNumber( r_shadows_quality->GetInt());
	var[15].SetNumber( r_lighting_quality->GetInt());
	var[16].SetNumber( r_particles_quality->GetInt());
	var[17].SetNumber( r_mesh_quality->GetInt());
	var[18].SetNumber( r_anisotropy_quality->GetInt());
	var[19].SetNumber( r_postprocess_quality->GetInt());
	var[20].SetNumber( r_texture_quality->GetInt());
	var[21].SetNumber( g_vertical_look->GetBool());
	var[22].SetNumber( 0 ); // not used
	var[23].SetNumber( g_mouse_wheel->GetBool());
	var[24].SetNumber( g_mouse_sensitivity->GetFloat());
	var[25].SetNumber( g_mouse_acceleration->GetBool());
	var[26].SetNumber( g_toggle_aim->GetBool());
	var[27].SetNumber( g_toggle_crouch->GetBool());
	//var[28].SetString( "LOCKED"); // Fullscreen lock
	var[28].SetNumber( r_fullscreen->GetInt());
	var[29].SetNumber( r_vsync_enabled->GetInt()+1);

	gfxMovie.Invoke("_root.api.setOptions", var, 30);
 
	gfxMovie.Invoke("_root.api.reloadOptions", "");
}

void FrontendWarZ::eventOptionsReset(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
{
	r3d_assert(args);
	r3d_assert(argCount == 0);

	// get options
	g_tps_camera_mode->SetInt(0);
	g_enable_voice_commands			->SetBool(true);

	int old_fullscreen = r_fullscreen->GetInt();
	r_fullscreen->SetBool(true);

	int old_vsync = r_vsync_enabled->GetInt();
	r_vsync_enabled			->SetInt(0);

	if(old_fullscreen!=r_fullscreen->GetInt() || old_vsync!=r_vsync_enabled->GetInt())
	{
		// show message telling player that to change windows\fullscreen he have to restart game
		// todo: make fullscreen/window mode switch on the fly?
		Scaleform::GFx::Value var[2];
		var[0].SetStringW(gLangMngr.getString("RestartGameForChangesToTakeEffect"));
		var[1].SetBoolean(true);
		gfxMovie.Invoke("_root.api.showInfoMsg", var, 2);		
	}

	switch( r3dGetDeviceStrength() )
	{
		case S_WEAK:
			r_overall_quality->SetInt(1);
			break;
		case S_MEDIUM:
			r_overall_quality->SetInt(2);
			break;
		case S_STRONG:
			r_overall_quality->SetInt(3);
			break;
		case S_ULTRA:
			r_overall_quality->SetInt(4);
			break;
		default:
			r_overall_quality->SetInt(1);
			break;
	}

	DWORD settingsChangedFlags = 0;
	GraphicSettings settings;

	switch( r_overall_quality->GetInt() )
	{
		case 1:
			FillDefaultSettings( settings, S_WEAK );
			settingsChangedFlags = SetDefaultSettings( S_WEAK );
			break;
		case 2:
			FillDefaultSettings( settings, S_MEDIUM );
			settingsChangedFlags = SetDefaultSettings( S_MEDIUM );
			break;
		case 3:
			FillDefaultSettings( settings, S_STRONG );
			settingsChangedFlags = SetDefaultSettings( S_STRONG );
			break;
		case 4:
			FillDefaultSettings( settings, S_ULTRA );
			settingsChangedFlags = SetDefaultSettings( S_ULTRA );
			break;
		case 5:
			{
				settings.mesh_quality			= (int)args[17].GetNumber();
				settings.texture_quality		= (int)args[20].GetNumber();
				settings.terrain_quality		= (int)args[11].GetNumber();
				settings.water_quality			= (int)args[13].GetNumber();
				settings.shadows_quality		= (int)args[14].GetNumber();
				settings.lighting_quality		= (int)args[15].GetNumber();
				settings.particles_quality		= (int)args[16].GetNumber();
				settings.decoration_quality		= (int)args[12].GetNumber();
				settings.anisotropy_quality		= (int)args[18].GetNumber();
				settings.postprocess_quality	= (int)args[19].GetNumber();
				settings.ssao_quality			= (int)args[10].GetNumber();
				SaveCustomSettings( settings );
			}
			break;
		default:
			r3d_assert( false );
	}

	// AA is separate and can be changed at any overall quality level
	settings.antialiasing_quality	= 0;

	settingsChangedFlags |= GraphSettingsToVars( settings );

	AddSettingsChangeFlag( settingsChangedFlags );

	// clamp brightness and contrast, otherwise if user set it to 0 the screen will be white
	//r_brightness			->SetFloat(0.5f);
	//r_contrast				->SetFloat(0.5f);
	r_gamma_pow->SetFloat(2.2f);

	s_sound_volume			->SetFloat(1.0f);
	s_music_volume			->SetFloat(1.0f);
	s_comm_volume			->SetFloat(1.0f);

	SetNeedUpdateSettings();

	// write to ini file
	writeGameOptionsFile();
	SyncGraphicsUI();
}

void FrontendWarZ::eventOptionsApply(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
{
	r3d_assert(args);
	r3d_assert(argCount == 23);

	// get options
	g_tps_camera_mode->SetInt((int)args[7].GetNumber());
	g_enable_voice_commands			->SetBool( !!(int)args[8].GetNumber() );

	const char* res = args[0].GetString();
	int width = 1280, height = 720;
	sscanf(res, "%dx%d", &width, &height );

	r_width->SetInt( width );
	r_height->SetInt( height );

	int old_fullscreen = r_fullscreen->GetInt();
	r_fullscreen->SetInt( (int)args[21].GetNumber() );

	int old_vsync = r_vsync_enabled->GetInt();
	r_vsync_enabled			->SetInt((int)args[22].GetNumber()-1);

	if(old_fullscreen!=r_fullscreen->GetInt() || old_vsync!=r_vsync_enabled->GetInt())
	{
		// show message telling player that to change windows\fullscreen he have to restart game
		// todo: make fullscreen/window mode switch on the fly?
		Scaleform::GFx::Value var[2];
		var[0].SetStringW(gLangMngr.getString("RestartGameForChangesToTakeEffect"));
		var[1].SetBoolean(true);
		gfxMovie.Invoke("_root.api.showInfoMsg", var, 2);		
	}

	r_overall_quality		->SetInt( (int)args[1].GetNumber());

	DWORD settingsChangedFlags = 0;
	GraphicSettings settings;

	switch( r_overall_quality->GetInt() )
	{
		case 1:
			FillDefaultSettings( settings, S_WEAK );
			settingsChangedFlags = SetDefaultSettings( S_WEAK );
			break;
		case 2:
			FillDefaultSettings( settings, S_MEDIUM );
			settingsChangedFlags = SetDefaultSettings( S_MEDIUM );
			break;
		case 3:
			FillDefaultSettings( settings, S_STRONG );
			settingsChangedFlags = SetDefaultSettings( S_STRONG );
			break;
		case 4:
			FillDefaultSettings( settings, S_ULTRA );
			settingsChangedFlags = SetDefaultSettings( S_ULTRA );
			break;
		case 5:
			{
				settings.mesh_quality			= (int)args[17].GetNumber();
				settings.texture_quality		= (int)args[20].GetNumber();
				settings.terrain_quality		= (int)args[11].GetNumber();
				settings.water_quality			= (int)args[13].GetNumber();
				settings.shadows_quality		= (int)args[14].GetNumber();
				settings.lighting_quality		= (int)args[15].GetNumber();
				settings.particles_quality		= (int)args[16].GetNumber();
				settings.decoration_quality		= (int)args[12].GetNumber();
				settings.anisotropy_quality		= (int)args[18].GetNumber();
				settings.postprocess_quality	= (int)args[19].GetNumber();
				settings.ssao_quality			= (int)args[10].GetNumber();
				SaveCustomSettings( settings );
			}
			break;
		default:
			r3d_assert( false );
	}

	// AA is separate and can be changed at any overall quality level
	settings.antialiasing_quality	= (int)args[9].GetNumber();

	settingsChangedFlags |= GraphSettingsToVars( settings );

	AddSettingsChangeFlag( settingsChangedFlags );

	// clamp brightness and contrast, otherwise if user set it to 0 the screen will be white
	//r_brightness			->SetFloat( R3D_CLAMP((float)args[2].GetNumber(), 0.25f, 0.75f) );
	//r_contrast				->SetFloat( R3D_CLAMP((float)args[3].GetNumber(), 0.25f, 0.75f) );
	r_gamma_pow->SetFloat(2.2f + (float(args[2].GetNumber())*2.0f-1.0f));

	s_sound_volume			->SetFloat( R3D_CLAMP((float)args[4].GetNumber(), 0.0f, 1.0f) );
	s_music_volume			->SetFloat( R3D_CLAMP((float)args[5].GetNumber(), 0.0f, 1.0f) );
	s_comm_volume			->SetFloat( R3D_CLAMP((float)args[6].GetNumber(), 0.0f, 1.0f) );


	SetNeedUpdateSettings();
	SyncGraphicsUI();

	// write to ini file
	writeGameOptionsFile();

	// if we changed resolution, we need to reset scaleform, otherwise visual artifacts will show up
//	needScaleformReset = true;
}

void FrontendWarZ::SetNeedUpdateSettings()
{
	needUpdateSettings_ = true;
}

void FrontendWarZ::UpdateSettings()
{
	r3dRenderer->UpdateSettings( ) ;

	gfxMovie.SetCurentRTViewport( Scaleform::GFx::Movie::SM_ExactFit );

	Mouse->SetRange( r3dRenderer->HLibWin );

	void applyGraphicsOptions( uint32_t settingsFlags );

	applyGraphicsOptions( settingsChangeFlags_ );

	gfxMovie.UpdateTextureMatrices("merc_rendertarget", (int)r3dRenderer->ScreenW, (int)r3dRenderer->ScreenH);
}

void FrontendWarZ::eventOptionsControlsRequestKeyRemap(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
{
	r3d_assert(args);
	r3d_assert(argCount == 1);

	int remapIndex = (int)args[0].GetNumber();
	r3d_assert(m_waitingForKeyRemap == -1);
	
	r3d_assert(remapIndex>=0 && remapIndex<r3dInputMappingMngr::KS_NUM);
	m_waitingForKeyRemap = remapIndex;
}

void FrontendWarZ::eventOptionsControlsReset(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
{
	r3d_assert(args);
	r3d_assert(argCount == 0);

//	InputMappingMngr->resetKeyMappingsToDefault();
	for(int i=0; i<r3dInputMappingMngr::KS_NUM; ++i)
	{
		Scaleform::GFx::Value args[2];
		args[0].SetStringW(gLangMngr.getString(InputMappingMngr->getMapName((r3dInputMappingMngr::KeybordShortcuts)i)));
		args[1].SetString(InputMappingMngr->getKeyName((r3dInputMappingMngr::KeybordShortcuts)i));
		gfxMovie.Invoke("_root.api.setKeyboardMapping", args, 2);
	}
	void writeInputMap();
	writeInputMap();

	// update those to match defaults in Vars.h
	g_vertical_look			->SetBool(false);
	g_mouse_wheel			->SetBool(true);
	g_mouse_sensitivity		->SetFloat(1.0f);
	g_mouse_acceleration	->SetBool(false);
	g_toggle_aim			->SetBool(false);
	g_toggle_crouch			->SetBool(false);

	// write to ini file
	writeGameOptionsFile();
	SyncGraphicsUI();
}

void FrontendWarZ::eventOptionsControlsApply(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
{
	r3d_assert(args);
	r3d_assert(argCount == 7);

	g_vertical_look			->SetBool( !!(int)args[0].GetNumber() );
	g_mouse_wheel			->SetBool( !!(int)args[2].GetNumber() );
	g_mouse_sensitivity		->SetFloat( (float)args[3].GetNumber() );
	g_mouse_acceleration	->SetBool( !!(int)args[4].GetNumber() );
	g_toggle_aim			->SetBool( !!(int)args[5].GetNumber() );
	g_toggle_crouch			->SetBool( !!(int)args[6].GetNumber() );

	// write to ini file
	writeGameOptionsFile();

	SyncGraphicsUI();
}

void FrontendWarZ::eventMsgBoxCallback(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
{
	if(loginMsgBoxOK_Exit)
	{
		loginMsgBoxOK_Exit = false;
		LoginMenuExitFlag = -1;
	}
	if(needReturnFromQuickJoin)
	{
		gfxMovie.Invoke("_root.api.Main.showScreen", "PlayGame");
		needReturnFromQuickJoin = false;
	}
}

void FrontendWarZ::eventBrowseGamesRequestFilterStatus(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
{
	r3d_assert(args);
	r3d_assert(argCount == 0);

	// setBrowseGamesOptions(regus:Boolean, regeu:Boolean, regru:Boolean, filt_gw:Boolean, filt_sh:Boolean, filt_empt:Boolean,
	//filt_full:Boolean, opt_trac:Boolean, opt_nm:Boolean, opt_ch:Boolean, opt_enabled:Boolean, opt_passworded:Boolean)
	Scaleform::GFx::Value var[16];
	var[0].SetBoolean(gUserSettings.BrowseGames_Filter.region_us);
	var[1].SetBoolean(gUserSettings.BrowseGames_Filter.region_eu);
	var[2].SetBoolean(gUserSettings.BrowseGames_Filter.region_ru);
	var[3].SetBoolean(false);
	var[4].SetBoolean(true);
	var[5].SetBoolean(true);
	var[6].SetBoolean(true);
	var[7].SetBoolean(true);
	var[8].SetBoolean(true);
	var[9].SetBoolean(true);
	var[10].SetBoolean(true);
	var[11].SetBoolean(true);
	var[12].SetBoolean("");
	var[13].SetBoolean(true);
	var[14].SetBoolean(true);
	var[15].SetUInt(0);
	
	gfxMovie.Invoke("_root.api.setBrowseGamesOptions", var, 16);
}

void FrontendWarZ::eventBrowseGamesSetFilter(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
{
	//regus:Boolean, regeu:Boolean, regru:Boolean, filt_gw:Boolean, filt_sh:Boolean, filt_empt:Boolean, filt_full:Boolean,
	//opt_trac:Boolean, opt_nm:Boolean, opt_ch:Boolean, nameFilter:String, opt_enable:Boolean, opt_Password:Boolean
	r3d_assert(args);
	r3d_assert(argCount == 16);
	/*gUserSettings.BrowseGames_Filter.region_us = args[0].GetBool();
	gUserSettings.BrowseGames_Filter.region_eu = args[1].GetBool();
	gUserSettings.BrowseGames_Filter.region_ru = args[2].GetBool();
	gUserSettings.BrowseGames_Filter.gameworld = args[3].GetBool();
	gUserSettings.BrowseGames_Filter.stronghold = args[4].GetBool();
	gUserSettings.BrowseGames_Filter.hideempty = args[5].GetBool();
	gUserSettings.BrowseGames_Filter.hidefull = args[6].GetBool();
	gUserSettings.BrowseGames_Filter.tracers = args[7].GetBool();
	gUserSettings.BrowseGames_Filter.nameplates = args[8].GetBool();
	gUserSettings.BrowseGames_Filter.crosshair = args[9].GetBool();*/
	//r3dscpy(passstring,args[11].GetString());
//	passstring = args[11].GetString();
	//gUserSettings.BrowseGames_Filter.enabled = args[11].GetBool();
	//gUserSettings.BrowseGames_Filter.passworded = args[12].GetBool();

	//gUserSettings.saveSettings();
	/*char message[128] = {0};
	sprintf(message,"Set Password Successfully : %s",passstring);
	   Scaleform::GFx::Value vars1[3];
	    vars1[1].SetBoolean(true);
		vars1[2].SetString("SYSTEM NOTICE");
		vars1[0].SetString(message);
			gfxMovie.Invoke("_root.api.showInfoMsg", vars1, 3); */
}
void FrontendWarZ::eventMarketplaceActive(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
{
	gfxMovie.SetVariable("_root.api.Main.Marketplace.Marketplace.Tab7.visible", true);
	//gfxMovie.SetVariable("_root.api.Main.Marketplace.Marketplace.Tutorial.visible", true);
}
void FrontendWarZ::eventStorePurchaseGD(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
{
	if(currentvalue > gUserProfile.ProfileData.GamePoints)
	{
		Scaleform::GFx::Value var[2];
		var[0].SetStringW(gLangMngr.getString("NotEnoughGP"));
		var[1].SetBoolean(true);
		gfxMovie.Invoke("_root.api.showInfoMsg", var, 2);
		return;
	}

	Scaleform::GFx::Value var[2];
	var[0].SetStringW(gLangMngr.getString("Converting Please Wait..."));
	var[1].SetBoolean(false);
	gfxMovie.Invoke("_root.api.showInfoMsg", var, 2);

	async_.StartAsyncOperation(this, &FrontendWarZ::as_ConvertGDThread, &FrontendWarZ::OnConvertGDSuccess);
}
void FrontendWarZ::OnConvertGDSuccess()
{
	 Scaleform::GFx::Value var[1];
	var[0].SetInt(gUserProfile.ProfileData.GamePoints);
	gfxMovie.Invoke("_root.api.setGC", var, 1);

	var[0].SetInt(gUserProfile.ProfileData.GameDollars);
	gfxMovie.Invoke("_root.api.setDollars", var, 1);

	gfxMovie.Invoke("_root.api.hideInfoMsg", "");
}
void FrontendWarZ::eventStorePurchaseGDCallback(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
{
	r3d_assert(args);
	r3d_assert(argCount == 1);
	convertvalue = args[0].GetInt() * 4;
	currentvalue = args[0].GetInt();
	Scaleform::GFx::Value vars1[1];
	vars1[0].SetInt(convertvalue);
	gfxMovie.Invoke("_root.api.Main.PurchaseGC.setGCValue", vars1, 1);
}
r3dPoint2D getMinimapPos1(const r3dPoint3D& pos , int mapid)
{
	r3dPoint3D worldOrigin = GameWorld().m_MinimapOrigin;
	r3dPoint3D worldSize = GameWorld().m_MinimapSize;
	float left_corner_x = 0;
	float bottom_corner_y = 0; 
	float x_size;
	float y_size;

	if (mapid == 2) //Colorado
	{
		x_size = 8192;
		y_size = 8192;
	}
	else if (mapid == 3 || mapid == 4) // Cliffside
	{
		x_size = 2000;
		y_size = 1900;
	}
	else if (mapid == 5) // Colorado V1
	{
		x_size = 8192;
		y_size = 8192;
	}
	else if (mapid == 6) // ATLANTA
	{
		x_size = 950;
		y_size = 950;
	}
	else if (mapid == 7) // ViruZ_pvp
	{
		x_size = 400;
		y_size = 550;
	}
	else if (mapid == 8) // Valley
	{
		x_size = 410;
		y_size = 410;
	}

	float x = R3D_CLAMP((pos.x-left_corner_x)/x_size, 0.0f, 1.0f);
	float y = 1.0f-R3D_CLAMP((pos.z-bottom_corner_y)/y_size, 0.0f, 1.0f);

	return r3dPoint2D(x, y);
}
void FrontendWarZ::eventShowSurvivorsMap(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
{
	const wiCharDataFull& slot1 = gUserProfile.ProfileData.ArmorySlots[gUserProfile.SelectedCharID];
	if (slot1.Alive == 0 || slot1.Alive == 3)
	{
		Scaleform::GFx::Value vars1[3];

		vars1[1].SetBoolean(true);
		vars1[2].SetString("SYSTEM NOTICE");
		vars1[0].SetString("Please spawn character on the map");
		gfxMovie.Invoke("_root.api.showInfoMsg", vars1, 3); 
		return;
	}
	char sFullPath[512];
	char sFullPathImg[512];
	//char mapdir[512];
	//sprintf(mapdir,"Levels\WZ_Colorado");
	int mapid = slot1.GameMapId;
	if (mapid == 2) //Colorado
	{
		sprintf(sFullPath,"Levels\\WZ_Colorado\\minimap.dds");
	}
	else if (mapid == 3 || mapid == 4) // Cliffside servertest
	{
		sprintf(sFullPath,"Levels\\WZ_Cliffside\\minimap.dds");
	}
	else if (mapid == 5) // Colorado V1
	{
		sprintf(sFullPath,"Levels\\WZ_Colorado_pve\\minimap.dds");
	}
	else if (mapid == 6) // ATLANTA
	{
		sprintf(sFullPath,"Levels\\WZ_Atlanta\\minimap.dds");
	}
	else if (mapid == 7) // Viruz_pvp
	{
		sprintf(sFullPath,"Levels\\WZ_ViruZ_pvp\\minimap.dds");
	}
	else if (mapid == 8) // Valley
	{
		sprintf(sFullPath,"Levels\\WZ_Valley\\minimap.dds");
	}

	sprintf(sFullPathImg, "$%s", sFullPath); // use '$' char to indicate absolute path

	if(r3dFileExists(sFullPath))
	{

		r3dOutToLog("File Found\n");





		Scaleform::GFx::Value vars[1];
		vars[0].SetString(sFullPathImg);
		gfxMovie.Invoke("_root.api.Main.SurvivorsAnim.loadSurvivorMap", vars,1);
		r3dPoint2D mapPos = getMinimapPos1(slot1.GamePos,mapid);
		Scaleform::GFx::Value var[3];
		var[0].SetNumber(mapPos.x);
		var[1].SetNumber(mapPos.y);
		var[2].SetString(slot1.Gamertag);
		gfxMovie.Invoke("_root.api.Main.SurvivorsAnim.addSurvivorPinToMap", var, 3);
		r3dOutToLog("%.2f , %.2f",mapPos.x,mapPos.y);

		r3dOutToLog("%s\n",sFullPathImg);
		gfxMovie.SetVariable("_root.api.Main.SurvivorsAnim.Survivors.PopupLastMap.visible", true);
	}
	else
	{

		r3dOutToLog("File Not Found");
	}
}
void FrontendWarZ::eventTrialUpgradeAccount(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
{
	r3d_assert(args);
	r3d_assert(argCount == 1);
	r3dscpy(passstring,args[0].GetString());

	Scaleform::GFx::Value var[3];
	var[0].SetStringW(gLangMngr.getString("WaitConnectingToServer"));
	var[1].SetBoolean(false);
	var[2].SetString("");
	gfxMovie.Invoke("_root.api.showInfoMsg", var, 3);


	async_.StartAsyncOperation(this, &FrontendWarZ::as_JoinGameThread);

}
void FrontendWarZ::eventBrowseGamesJoin(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
{

	r3d_assert(args);
	r3d_assert(argCount == 1);
	/*Scaleform::GFx::Value vars[2];
	vars[0].SetStringW(gLangMngr.getString("ENTER PASSWORD"));
	vars[1].SetString("JOIN SERVER");
	//vars[2].SetString("");
gfxMovie.Invoke("_root.api.Main.MsgBox.showPasswordInputMsg", vars , 2);*/
if (CurrentBrowse == 3)
{
	m_joinGameServerId = args[0].GetInt();
	r3d_assert(m_joinGameServerId > 0);
	gfxMovie.Invoke("_root.api.showTrialUpgradeWindow", "");
	return;
}
	
	// gameID:int
	

	if(gUserProfile.ProfileData.NumSlots == 0)
		return;
		
	m_joinGameServerId = args[0].GetInt();
    //m_ispassword = args[1].GetInt();
	r3d_assert(m_joinGameServerId > 0);
	//r3d_assert(m_ispassword > 0);
	//r3dOutToLog("Password : %d\n",m_ispassword);
//if (m_ispassword == 32)
//{

	//gUserSettings.addGameToFavorite(m_joinGameServerId);
	//gUserSettings.saveSettings();
	
	Scaleform::GFx::Value var[3];
	var[0].SetStringW(gLangMngr.getString("WaitConnectingToServer"));
	var[1].SetBoolean(false);
	var[2].SetString("");
	gfxMovie.Invoke("_root.api.showInfoMsg", var, 3);

	async_.StartAsyncOperation(this, &FrontendWarZ::as_JoinGameThread);
//}
//else
//{
	//Scaleform::GFx::Value vars[1];
	//vars[0].SetStringW(gLangMngr.getString("$FR_EnterPasswordToJoinGame"));

    //gfxMovie.Invoke("_root.api.Main.MsgBox.showPasswordInputMsg", vars , 1);
//}
}
void FrontendWarZ::eventBrowseGamesOnAddToFavorites(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
{
	// gameID:int
	r3d_assert(args);
	r3d_assert(argCount == 1);

	uint32_t gameID = (uint32_t)args[0].GetInt();
	gUserSettings.addGameToFavorite(gameID);
	gUserSettings.saveSettings();
}
void FrontendWarZ::eventBrowseGamesRequestList(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
{
	// Browse Text Seting
	/*gfxMovie.SetVariable("_root.api.Main.BrowseGamesAnim.ServBrowse.Filters.TitleFilters.text","FILTERS:");
	gfxMovie.SetVariable("_root.api.Main.BrowseGamesAnim.ServBrowse.Filters.TitleServerOptions.text","SERVER OPTIONS:");
	gfxMovie.SetVariable("_root.api.Main.BrowseGamesAnim.ServBrowse.SlotlistTop.BtnSort1.Text.text","SERVER NAME");
	gfxMovie.SetVariable("_root.api.Main.BrowseGamesAnim.ServBrowse.SlotlistTop.BtnSort2.Text.text","MODE");
	gfxMovie.SetVariable("_root.api.Main.BrowseGamesAnim.ServBrowse.SlotlistTop.BtnSort3.Text.text","MAP");
	gfxMovie.SetVariable("_root.api.Main.BrowseGamesAnim.ServBrowse.SlotlistTop.BtnSort4.Text.text","PING");
	gfxMovie.SetVariable("_root.api.Main.BrowseGamesAnim.ServBrowse.SlotlistTop.BtnSort5.Text.text","Player");*/
	// type:String (browse, recent, favorites)
	r3d_assert(args);
	r3d_assert(argCount == 4);

	Scaleform::GFx::Value var[3];
	var[0].SetStringW(gLangMngr.getString("FetchingGamesListFromServer"));
	var[1].SetBoolean(false);
	var[2].SetString("");
	gfxMovie.Invoke("_root.api.showInfoMsg", var, 3);

	if(strcmp(args[0].GetString(), "browse")==0)
	{
		m_browseGamesMode = 0;
		async_.StartAsyncOperation(this, &FrontendWarZ::as_BrowseGamesThread, &FrontendWarZ::OnGameListReceived);
	}
	else
	{
		if(strcmp(args[0].GetString(), "recent")==0)
			m_browseGamesMode = 1;
		else
			m_browseGamesMode = 2;

		// this works only if we already have a list of games from server. but, browse games shows by default in mode 0, so we should always have a list
		gfxMovie.Invoke("_root.api.hideInfoMsg", "");
		processNewGameList();	
		gfxMovie.Invoke("_root.api.Main.BrowseGamesAnim.showGameList", "");
	}
}

unsigned int WINAPI FrontendWarZ::as_BrowseGamesThread(void* in_data)
{
	r3dThreadAutoInstallCrashHelper crashHelper;
	FrontendWarZ* This = (FrontendWarZ*)in_data;
	
	This->async_.DelayServerRequest();
	if(!This->ConnectToMasterServer())
		return 0;

	gMasterServerLogic.RequestGameList();

	const float endTime = r3dGetTime() + 120.0f;
	while(r3dGetTime() < endTime)
	{
		::Sleep(10);
		if(gMasterServerLogic.gameListReceived_)
		{
			This->ProcessSupervisorPings();
			return 1;
		}

		if(!gMasterServerLogic.IsConnected())
			break;
	}

	This->async_.SetAsyncError(0, gLangMngr.getString("FailedReceiveGameList"));
	return 0;
}

void FrontendWarZ::OnGameListReceived()
{
	gfxMovie.Invoke("_root.api.hideInfoMsg", "");
	processNewGameList();	
	gfxMovie.Invoke("_root.api.Main.BrowseGamesAnim.showGameList", "");
}

void FrontendWarZ::processNewGameList()
{
	int numGames = (int)gMasterServerLogic.games_.size();

	int gameCounter = 0;
	for(int i=0; i<numGames; i++) 
	{
		const GBPKT_M2C_GameData_s& gd = gMasterServerLogic.games_[i];
		const GBGameInfo& ginfo = gd.info;

		// process filters
		/*if(m_browseGamesMode == 0)
		{
			if(!gUserSettings.BrowseGames_Filter.gameworld) // hack. only gameworlds available right now
			{
				continue; 
			}

			if(gUserSettings.BrowseGames_Filter.region_us && (gd.info.region != GBNET_REGION_US_East && gd.info.region != GBNET_REGION_US_West))
				continue;
			if(gUserSettings.BrowseGames_Filter.region_eu && gd.info.region != GBNET_REGION_Europe)
				continue;





			if(gUserSettings.BrowseGames_Filter.hideempty && gd.curPlayers == 0)
				continue;
			if(gUserSettings.BrowseGames_Filter.hidefull && gd.curPlayers == gd.info.maxPlayers)
				continue;

		}
		else if(m_browseGamesMode == 1) // recent
		{
			if(!gUserSettings.isInRecentGamesList(ginfo.gameServerId))
				continue;
		}
		else if(m_browseGamesMode == 2) // favorite
		{
			if(!gUserSettings.isInFavoriteGamesList(ginfo.gameServerId))
				continue;
		}
		else
			r3d_assert(false); // shouldn't happen*/
		// finished filters

		int ping = GetGamePing(gd.superId);
		if(ping > 0)
			ping = R3D_CLAMP(ping + random(10)-5, 1, 1000);
		ping = R3D_CLAMP(ping/10, 1, 100); // UI accepts ping from 0 to 100 and shows bars instead of actual number

		
		Scaleform::GFx::Value var[15];

		//viruz nova - addGameToList "id":arg1, "name":arg2, "mode":arg3, "map":arg4, "tracers":arg5, "nametags":arg6, "crosshair":arg7, "players":arg8, "ping":arg9, "movie":null, "favorite":arg10, "isPassword":arg11, "isTimeLimit":arg12, "trialsAllowed":arg13, "donate":arg14, "disableWeapon":arg15	
		
		//OFFICIALS SERVERS
	if (CurrentBrowse == 2)
	{
		if (!ginfo.ispass && !ginfo.ispre && !ginfo.isfarm)
		{
		var[0].SetNumber(ginfo.gameServerId);
		var[1].SetString(ginfo.name);
		var[2].SetString("VIRUZWORLD");
		switch (ginfo.mapId)
		 {
		case GBGameInfo::MAPID_WZ_Colorado:
				var[3].SetString("COLORADO");
				break;
		case GBGameInfo::MAPID_WZ_Cliffside:
				var[3].SetString("CLIFFSIDE");
				break;
		case GBGameInfo::MAPID_WZ_Colorado_pve:
				var[3].SetString("COLORADO PVE");
				break;
		case GBGameInfo::MAPID_WZ_Atlanta:
				var[3].SetString("ATLANTA");
				break;
		case GBGameInfo::MAPID_WZ_ViruZ_pvp:
				var[3].SetString("VIRUZ ARENA");
				break;
		case GBGameInfo::MAPID_WZ_Valley:
				var[3].SetString("VALLEY");
				break;
				
		default:
				var[3].SetString("MAPTEST");
				break;
		}
		var[4].SetBoolean(true);
		var[5].SetBoolean(true);
		var[6].SetBoolean(true);
		char players[16];
		sprintf(players, "%d/%d", gd.curPlayers, ginfo.maxPlayers);
		var[7].SetString(players);
		var[8].SetInt(ping);
		var[9].SetBoolean(gUserSettings.isInFavoriteGamesList(ginfo.gameServerId));
		var[10].SetString(ginfo.pwdchar); // isPassword
		var[11].SetBoolean(true); // TimeLimit
		var[12].SetBoolean(false); // TiralAllowJoin
		var[13].SetBoolean(true);
		var[14].SetBoolean(false);

		gfxMovie.Invoke("_root.api.Main.BrowseGamesAnim.addGameToList", var, 15);
		}
		//PREMIUM SERVERS
	else if (CurrentBrowse == 5)
	{
		if (!ginfo.ispass && !ginfo.ispre && ginfo.isfarm)
		{
		var[0].SetNumber(ginfo.gameServerId);
		var[1].SetString(ginfo.name);
		var[2].SetString("VIRUZWORLD");
		
		switch (ginfo.mapId)
		{
		case GBGameInfo::MAPID_WZ_Colorado:
				var[3].SetString("COLORADO");
				break;
		case GBGameInfo::MAPID_WZ_Cliffside:
				var[3].SetString("CLIFFSIDE");
				break;
		case GBGameInfo::MAPID_WZ_Colorado_pve:
				var[3].SetString("COLORADO PVE");
				break;
		case GBGameInfo::MAPID_WZ_Atlanta:
				var[3].SetString("ATLANTA");
				break;
		case GBGameInfo::MAPID_WZ_ViruZ_pvp:
				var[3].SetString("VIRUZ ARENA");
				break;
        case GBGameInfo::MAPID_WZ_Valley:
				var[3].SetString("VALLEY");
				break;


		default:
				var[3].SetString("MAPTEST");
				break;
		}
		var[4].SetBoolean(true);
		var[5].SetBoolean(true);
		var[6].SetBoolean(true);
		char players[16];
		sprintf(players, "%d/%d", gd.curPlayers, ginfo.maxPlayers);
		var[7].SetString(players);
		var[8].SetInt(ping);
		var[9].SetBoolean(gUserSettings.isInFavoriteGamesList(ginfo.gameServerId));
		var[10].SetString(ginfo.pwdchar); // isPassword
		var[11].SetBoolean(true); // TimeLimit
		var[12].SetBoolean(false); // TiralAllowJoin
		var[13].SetBoolean(true);
		var[14].SetBoolean(false);
		
		gfxMovie.Invoke("_root.api.Main.BrowseGamesAnim.addGameToList", var, 15);
		}
	}
}
		
	//PASSWORDED SERVERS
	else if (CurrentBrowse == 3)
	{
		if (ginfo.ispass)
		{
		var[0].SetNumber(ginfo.gameServerId);
		var[1].SetString(ginfo.name);
		var[2].SetString("VIRUZWORLD");
		switch (ginfo.mapId)
		{
		case GBGameInfo::MAPID_WZ_Colorado:
				var[3].SetString("COLORADO");
				break;
		case GBGameInfo::MAPID_WZ_Cliffside:
				var[3].SetString("CLIFFSIDE");
				break;
		case GBGameInfo::MAPID_WZ_Colorado_pve:
				var[3].SetString("COLORADO PVE");
				break;
		case GBGameInfo::MAPID_WZ_Atlanta:
				var[3].SetString("ATLANTA");
				break;
		case GBGameInfo::MAPID_WZ_ViruZ_pvp:
				var[3].SetString("VIRUZ ARENA");
				break;
        case GBGameInfo::MAPID_WZ_Valley:
				var[3].SetString("VALLEY");
				break;


		default:
				var[3].SetString("MAPTEST");
				break;
		}
		var[4].SetBoolean(true);
		var[5].SetBoolean(true);
		var[6].SetBoolean(true);
		char players[16];
		sprintf(players, "%d/%d", gd.curPlayers, ginfo.maxPlayers);
		var[7].SetString(players);
		var[8].SetInt(ping);
		var[9].SetBoolean(gUserSettings.isInFavoriteGamesList(ginfo.gameServerId));
		var[10].SetString(ginfo.pwdchar); // isPassword
		var[11].SetBoolean(true); // TimeLimit
		var[12].SetBoolean(false); // TiralAllowJoin
		var[13].SetBoolean(true);
		var[14].SetBoolean(false);

		gfxMovie.Invoke("_root.api.Main.BrowseGamesAnim.addGameToList", var, 15);
		}
	}
	else if (CurrentBrowse == 4)
		{
			/*Scaleform::GFx::Value vars1[3];
			if (!gUserProfile.ProfileData.isPunisher)
			{
			   vars1[1].SetBoolean(true);
			vars1[2].SetString("NOTICE");
			vars1[0].SetString("ONLY FOR PUNISHER ACCOUNT");
			gfxMovie.Invoke("_root.api.showInfoMsg", vars1, 3);  
		   }
		   else
		   {*/
		 if (ginfo.ispre || ginfo.isfarm)
			{
		var[0].SetNumber(ginfo.gameServerId);
		var[1].SetString(ginfo.name);
		var[2].SetString("VIRUZWORLD");
		switch (ginfo.mapId)
		{

		case GBGameInfo::MAPID_WZ_Colorado:
				var[3].SetString("COLORADO");
				break;
		case GBGameInfo::MAPID_WZ_Cliffside:
				var[3].SetString("CLIFFSIDE");
				break;
		case GBGameInfo::MAPID_WZ_Colorado_pve:
				var[3].SetString("COLORADO PVE");
				break;
		case GBGameInfo::MAPID_WZ_Atlanta:
				var[3].SetString("ATLANTA");
				break;
		case GBGameInfo::MAPID_WZ_ViruZ_pvp:
				var[3].SetString("VIRUZ ARENA");
				break;
		case GBGameInfo::MAPID_WZ_Valley:
				var[3].SetString("VALLEY");
				break;


		default:
				var[3].SetString("MAPTEST");
				break;
		}
		var[4].SetBoolean(true);
		var[5].SetBoolean(true);
		var[6].SetBoolean(true);
		char players[16];
		sprintf(players, "%d/%d", gd.curPlayers, ginfo.maxPlayers);
		var[7].SetString(players);
		var[8].SetInt(ping);
		var[9].SetBoolean(gUserSettings.isInFavoriteGamesList(ginfo.gameServerId));
		var[10].SetString(ginfo.pwdchar); // isPassword
		var[11].SetBoolean(true); // TimeLimit
		var[12].SetBoolean(false); // TiralAllowJoin
		var[13].SetBoolean(true);
		var[14].SetBoolean(false);
		gfxMovie.Invoke("_root.api.Main.BrowseGamesAnim.addGameToList", var, 15);
			   }
		}
	}
}

int FrontendWarZ::GetSupervisorPing(DWORD ip)
{
	HANDLE hIcmpFile = IcmpCreateFile();
	if(hIcmpFile == INVALID_HANDLE_VALUE) {
		r3dOutToLog("IcmpCreatefile returned error: %d\n", GetLastError());
		return -1;
	}    

	char  sendData[32]= "Data Buffer";
	DWORD replySize   = sizeof(ICMP_ECHO_REPLY) + sizeof(sendData);
	void* replyBuf    = (void*)_alloca(replySize);
	
	// send single ping with 1000ms, without payload as it alert most firewalls
	DWORD sendResult = IcmpSendEcho(hIcmpFile, ip, sendData, 0, NULL, replyBuf, replySize, 1000);
#ifndef FINAL_BUILD	
	if(sendResult == 0) {
		char ips[128];
		r3dscpy(ips, inet_ntoa(*(in_addr*)&ip));
		r3dOutToLog("PING failed to %s : %d\n", ips, GetLastError());
	}
#endif

	IcmpCloseHandle(hIcmpFile);

	if(sendResult == 0) {
		//r3dOutToLog("IcmpSendEcho returned error: %d\n", GetLastError());
		return -2;
	}

	PICMP_ECHO_REPLY pEchoReply = (PICMP_ECHO_REPLY)replyBuf;
	if(pEchoReply->Status == IP_SUCCESS)
	{
		return pEchoReply->RoundTripTime;
	}

	//r3dOutToLog("IcmpSendEcho returned status %d\n", pEchoReply->Status);
	return -3;
}

void FrontendWarZ::ProcessSupervisorPings()
{
	memset(&superPings_, 0, sizeof(superPings_));

	for(size_t i = 0; i < gMasterServerLogic.supers_.size(); ++i)
	{
		const GBPKT_M2C_SupervisorData_s& super = gMasterServerLogic.supers_[i];
		if(super.ID >= R3D_ARRAYSIZE(superPings_))
		{
#ifndef FINAL_BUILD		
			r3dError("Too Many servers, please contact support@thewarz.com");
#endif
			continue;
		}

		int ping = GetSupervisorPing(super.ip);
		superPings_[super.ID] = ping ? ping : 1;
	}
}

int FrontendWarZ::GetGamePing(DWORD superId)
{
	// high word of gameId is supervisor Id
	r3d_assert(superId < R3D_ARRAYSIZE(superPings_));
	return superPings_[superId];
}

void FrontendWarZ::eventRequestMyClanInfo(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
{
	r3d_assert(argCount==0);

	setClanInfo();
	gfxMovie.Invoke("_root.api.Main.Clans.showClanList", "");
}

void FrontendWarZ::setClanInfo()
{
	wiCharDataFull& slot = gUserProfile.ProfileData.ArmorySlots[gUserProfile.SelectedCharID];
	CUserClans* clans = gUserProfile.clans[gUserProfile.SelectedCharID];

	// fill clanCurData_.clanID and invites/application list.
	// TODO: implement timer to avoid API spamming? or check async every N seconds...
	clans->ApiGetClanStatus();
	
	if(clans->clanCurData_.ClanID != slot.ClanID)
	{
		slot.ClanID = clans->clanCurData_.ClanID;
		// we joined or left clan. do something
	}


	// if we don't have clan data yet - retrieve it. NOTE: need switch to async call
	if(slot.ClanID && clans->clanInfo_.ClanID == 0)
	{
		clans->ApiClanGetInfo(slot.ClanID, &clans->clanInfo_, &clans->clanMembers_);
	}
	
	{
		//		public function setClanInfo(clanID:uint, isAdmin:Boolean, name:String, availableSlots:uint, clanReserve:uint)
		Scaleform::GFx::Value var[6];
		var[0].SetUInt(slot.ClanID); // if ClanID is zero, then treated by UI as user is not in clan
		var[1].SetBoolean(slot.ClanRank<=1); // should be true only for admins of the clan (creator=0 and officers=1)
		var[2].SetString(clans->clanInfo_.ClanName);
		var[3].SetUInt(clans->clanInfo_.MaxClanMembers-clans->clanInfo_.NumClanMembers);
		var[4].SetUInt(clans->clanInfo_.ClanGP);
		var[5].SetUInt(clans->clanInfo_.ClanEmblemID);
		gfxMovie.Invoke("_root.api.setClanInfo", var, 6);
	}

	{
		Scaleform::GFx::Value var[10];
		for(CUserClans::TClanMemberList::iterator iter=clans->clanMembers_.begin(); iter!=clans->clanMembers_.end(); ++iter)
		{
			CUserClans::ClanMember_s& memberInfo = *iter;
			//public function addClanMemberInfo(customerID:uint, name:String, exp:uint, time:String, rep:String, kzombie:uint, ksurvivor:uint, kbandits:uint, donatedgc:uint)
			var[0].SetUInt(memberInfo.CharID);
			var[1].SetString(memberInfo.gamertag);
			var[2].SetUInt(memberInfo.stats.XP);
			var[3].SetString(getTimePlayedString(memberInfo.stats.TimePlayed));
			var[4].SetStringW(getReputationString(memberInfo.stats.Reputation));
			var[5].SetUInt(memberInfo.stats.KilledZombies);
			var[6].SetUInt(memberInfo.stats.KilledSurvivors);
			var[7].SetUInt(memberInfo.stats.KilledBandits);
			var[8].SetUInt(memberInfo.ContributedGP);
			var[9].SetUInt(memberInfo.ClanRank);
			gfxMovie.Invoke("_root.api.addClanMemberInfo", var, 10);
		}
	}
	checkForInviteFromClan();
}

void FrontendWarZ::eventRequestClanList(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
{
	r3d_assert(argCount == 2);

	uint32_t sortID = args[0].GetUInt();
	uint32_t unknown = args[1].GetUInt();

	gfxMovie.Invoke("_root.api.Main.Clans.clearClanInfo", 0);

	// async it
	CUserClans* clans = new CUserClans;
	clans->ApiClanGetLeaderboard();
	Scaleform::GFx::Value var[7];
	for(std::list<CUserClans::ClanInfo_s>::iterator iter=clans->leaderboard_.begin(); iter!=clans->leaderboard_.end(); ++iter)
	{
		CUserClans::ClanInfo_s& clanInfo = *iter;
		//public function addClanInfo(clanID:uint, name:String, creator:String, xp:uint, numMembers:uint, description:String, icon:String)
		var[0].SetUInt(clanInfo.ClanID);
		var[1].SetString(clanInfo.ClanName);
		var[2].SetString(clanInfo.OwnerGamertag);
		var[3].SetUInt(clanInfo.ClanXP);
		var[4].SetUInt(clanInfo.NumClanMembers);
		var[5].SetString(clanInfo.ClanLore);
		var[6].SetUInt(clanInfo.ClanEmblemID);
		gfxMovie.Invoke("_root.api.Main.Clans.addClanInfo", var, 7);
	}
	delete clans;

	gfxMovie.Invoke("_root.api.Main.Clans.populateClanList", 0);
}

void FrontendWarZ::eventCreateClan(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
{
	r3d_assert(argCount == 6);

	if(gUserProfile.ProfileData.NumSlots == 0)
	{
		Scaleform::GFx::Value var[2];
		var[0].SetStringW(gLangMngr.getString("YouNeedCharBeforeCreatingClan"));
		var[1].SetBoolean(true);
		gfxMovie.Invoke("_root.api.showInfoMsg", var, 2);
		return;
	}

	//eventCreateClan(name:String, tag:String, desc:String, nameColor:int, tagColor:int, iconID:int)
	r3dscpy(clanCreateParams.ClanName, args[0].GetString());
	r3dscpy(clanCreateParams.ClanTag, args[1].GetString());
	clanCreateParam_Desc = args[2].GetString();
	clanCreateParams.ClanNameColor = args[3].GetInt();
	clanCreateParams.ClanTagColor = args[4].GetInt();
	clanCreateParams.ClanEmblemID = args[5].GetInt();

	if(strpbrk(clanCreateParams.ClanName, "!@#$%^&*()-=+_<>,./?'\":;|{}[]")!=NULL) // do not allow this symbols
	{
		Scaleform::GFx::Value var[2];
		var[0].SetStringW(gLangMngr.getString("ClanNameNoSpecSymbols"));
		var[1].SetBoolean(true);
		gfxMovie.Invoke("_root.api.showInfoMsg", var, 2);
		return;
	}
	if(strpbrk(clanCreateParams.ClanTag, "!@#$%^&*()-=+_<>,./?'\":;|{}[]")!=NULL) // do not allow this symbols
	{
		Scaleform::GFx::Value var[2];
		var[0].SetStringW(gLangMngr.getString("ClanTagNoSpecSymbols"));
		var[1].SetBoolean(true);
		gfxMovie.Invoke("_root.api.showInfoMsg", var, 2);
		return;
	}

	/*int pos = 0;
	while((pos= clanCreateParam_Desc.find('<'))!=-1)
		clanCreateParam_Desc.replace(pos, 1, "&lt;");
	while((pos = clanCreateParam_Desc.find('>'))!=-1)
		clanCreateParam_Desc.replace(pos, 1, "&gt;");*/

	Scaleform::GFx::Value var[2];
	var[0].SetStringW(gLangMngr.getString("OneMomentPlease"));
	var[1].SetBoolean(false);
	gfxMovie.Invoke("_root.api.showInfoMsg", var, 2);

	async_.StartAsyncOperation(this, &FrontendWarZ::as_CreateClanThread, &FrontendWarZ::OnCreateClanSuccess);
}

unsigned int WINAPI FrontendWarZ::as_CreateClanThread(void* in_data)
{
	r3dThreadAutoInstallCrashHelper crashHelper;
	FrontendWarZ* This = (FrontendWarZ*)in_data;

	CUserClans* clans = gUserProfile.clans[gUserProfile.SelectedCharID];
	int api = clans->ApiClanCreate(This->clanCreateParams);
	if(api!=0)
	{
		switch (api)
		{
		case 1:
			This->async_.SetAsyncError(0, gLangMngr.getString("ClanNameNoSpecSymbols"));
			break;
		case 2:
			This->async_.SetAsyncError(0, gLangMngr.getString("ClanTagNoSpecSymbols"));
			break;
		case 27:
			This->async_.SetAsyncError(0, gLangMngr.getString("ClanError_Code27"));
			break;
		case 28:
			This->async_.SetAsyncError(0, gLangMngr.getString("ClanError_Code28"));
			break;
		case 29:
			This->async_.SetAsyncError(0, gLangMngr.getString("ClanError_Code29"));
			break;
		default:
			This->async_.SetAsyncError(api, gLangMngr.getString("FailedToCreateClan"));
			break;
		}
		return 0;
	}
	return 1;
}

void FrontendWarZ::OnCreateClanSuccess()
{
	gfxMovie.Invoke("_root.api.hideInfoMsg", "");
	CUserClans* clans = gUserProfile.clans[gUserProfile.SelectedCharID];
	int api = clans->ApiClanSetLore(clanCreateParam_Desc.c_str());
	if(api!=0)
	{
		r3dOutToLog("failed to set clan desc, api=%d\n", api);
		r3d_assert(false);
	}

	setClanInfo();
	gfxMovie.Invoke("_root.api.Main.showScreen", "MyClan");
}

void FrontendWarZ::eventClanAdminDonateGC(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
{
	r3d_assert(argCount == 2);
	uint32_t charID = args[0].GetUInt();
	uint32_t numGC = args[1].GetUInt();
	CUserClans* clans = gUserProfile.clans[gUserProfile.SelectedCharID];

	if(clans->clanInfo_.ClanGP < int(numGC))
	{
		Scaleform::GFx::Value var[2];
		var[0].SetStringW(gLangMngr.getString("ClanReserveNotEnoughGC"));
		var[1].SetBoolean(true);
		gfxMovie.Invoke("_root.api.showInfoMsg", var, 2);
		return;
	}

	int api = clans->ApiClanDonateGPToMember(charID, numGC);
	if(api != 0)
	{
		r3dOutToLog("Failed to donate to member, api=%d\n", api);

		Scaleform::GFx::Value var[2];
		switch (api)
		{
		case 23:
			var[0].SetStringW(gLangMngr.getString("ClanError_Code23"));
			break;
		default:
			var[0].SetStringW(gLangMngr.getString("FailToDonateGCToClanMember"));
			break;
		}
		var[1].SetBoolean(true);
		gfxMovie.Invoke("_root.api.showInfoMsg", var, 2);
	}

	char tmpStr[32]; sprintf(tmpStr, "%d GC", clans->clanInfo_.ClanGP);
	gfxMovie.SetVariable("_root.api.Main.ClansMyClan.MyClan.OptionsBlock3.GC.text", tmpStr);
}

void FrontendWarZ::refreshClanUIMemberList()
{
	setClanInfo();
	gfxMovie.Invoke("_root.api.Main.ClansMyClan.showClanMembers", "");
}

void FrontendWarZ::eventClanAdminAction(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
{
	r3d_assert(argCount == 2);
	uint32_t charID = args[0].GetUInt();
	const char* actionType = args[1].GetString(); // promote, demote, kick
	CUserClans* clans = gUserProfile.clans[gUserProfile.SelectedCharID];
	if(strcmp(actionType, "promote") == 0)
	{
		CUserClans::ClanMember_s* member = clans->GetMember(charID);
		r3d_assert(member);
		if(member->ClanRank>0)
		{
			int newRank = member->ClanRank;
			if(newRank > 2)
				newRank = 1;
			else
				newRank = newRank-1;
			int api = clans->ApiClanSetRank(charID, newRank);
			if(api != 0)
			{
				r3dOutToLog("Failed to promote rank, api=%d\n", api);

				Scaleform::GFx::Value var[2];
				switch (api)
				{
				case 23:
					var[0].SetStringW(gLangMngr.getString("ClanError_Code23"));
					break;
				default:
					var[0].SetStringW(gLangMngr.getString("FailToPromote"));
					break;
				}
				var[1].SetBoolean(true);
				gfxMovie.Invoke("_root.api.showInfoMsg", var, 2);
			}
			else
			{
				if(newRank == 0) // promoted someone else to leader -> demote itself
				{
					wiCharDataFull& slot = gUserProfile.ProfileData.ArmorySlots[gUserProfile.SelectedCharID];
					slot.ClanRank = 1;
					CUserClans::ClanMember_s* m = clans->GetMember(slot.LoadoutID);
					if(m)
						m->ClanRank = 1;
				}
				refreshClanUIMemberList();
			}
		}
		else
		{
			Scaleform::GFx::Value var[2];
			var[0].SetStringW(gLangMngr.getString("MemberAlreadyHasHighestRank"));
			var[1].SetBoolean(true);
			gfxMovie.Invoke("_root.api.showInfoMsg", var, 2);
		}
	}
	if(strcmp(actionType, "demote") == 0)
	{
		CUserClans::ClanMember_s* member = clans->GetMember(charID);
		r3d_assert(member);
		if(member->ClanRank<2)
		{
			int api = clans->ApiClanSetRank(charID, member->ClanRank+1);
			if(api != 0)
			{
				r3dOutToLog("Failed to demote rank, api=%d\n", api);

				Scaleform::GFx::Value var[2];
				switch (api)
				{
				case 23:
					var[0].SetStringW(gLangMngr.getString("ClanError_Code23"));
					break;
				default:
					var[0].SetStringW(gLangMngr.getString("FailToDemote"));
					break;
				}
				var[1].SetBoolean(true);
				gfxMovie.Invoke("_root.api.showInfoMsg", var, 2);
			}
			else
				refreshClanUIMemberList();
		}
		else
		{
			Scaleform::GFx::Value var[2];
			var[0].SetStringW(gLangMngr.getString("MemberAlreadyHasLowestRank"));
			var[1].SetBoolean(true);
			gfxMovie.Invoke("_root.api.showInfoMsg", var, 2);
		}
	}
	if(strcmp(actionType, "kick") == 0)
	{
		if(clans->GetMember(charID)== NULL)
		{
			Scaleform::GFx::Value var[2];
			var[0].SetStringW(gLangMngr.getString("NoSuchClanMember"));
			var[1].SetBoolean(true);
			gfxMovie.Invoke("_root.api.showInfoMsg", var, 2);
			return;
		}

		int api = clans->ApiClanKick(charID);
		if(api != 0)
		{
			Scaleform::GFx::Value var[2];
			switch (api)
			{
			case 6:
				var[0].SetStringW(gLangMngr.getString("YouCannotKickYourself"));
				break;
			case 23:
				var[0].SetStringW(gLangMngr.getString("ClanError_Code23"));
				break;
			default:
				r3dOutToLog("Failed to kick, api=%d\n", api);
				var[0].SetStringW(gLangMngr.getString("FailToKickMember"));
				break;
			}
			var[1].SetBoolean(true);
			gfxMovie.Invoke("_root.api.showInfoMsg", var, 2);
		}
		else
		{
			Scaleform::GFx::Value var[2];
			var[0].SetStringW(gLangMngr.getString("ClanMemberWasKickedFromClan"));
			var[1].SetBoolean(true);
			gfxMovie.Invoke("_root.api.showInfoMsg", var, 2);
			refreshClanUIMemberList();
		}
	}
}

void FrontendWarZ::eventClanLeaveClan(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
{
	r3d_assert(argCount == 0);
	CUserClans* clans = gUserProfile.clans[gUserProfile.SelectedCharID];
	int api = clans->ApiClanLeave();
	if(api != 0)
	{
		r3dOutToLog("Failed to leave clan, api=%d\n", api);

		Scaleform::GFx::Value var[2];
		var[0].SetStringW(gLangMngr.getString("FailToLeaveClan"));
		var[1].SetBoolean(true);
		gfxMovie.Invoke("_root.api.showInfoMsg", var, 2);
	}
	else
	{
		gfxMovie.Invoke("_root.api.Main.showScreen", "Clans");
	}
}

void FrontendWarZ::eventClanDonateGCToClan(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
{
	r3d_assert(argCount == 1);
	uint32_t amount = args[0].GetUInt();

	if(amount == 0)
	{
		Scaleform::GFx::Value var[2];
		var[0].SetStringW(gLangMngr.getString("ClanError_AmountToDonateShouldBeMoreThanZero"));
		var[1].SetBoolean(true);
		gfxMovie.Invoke("_root.api.showInfoMsg", var, 2);
		return;
	}
	if(int(amount) > gUserProfile.ProfileData.GamePoints)
	{
		Scaleform::GFx::Value var[2];
		var[0].SetStringW(gLangMngr.getString("NotEnoughGP"));
		var[1].SetBoolean(true);
		gfxMovie.Invoke("_root.api.showInfoMsg", var, 2);
		return;
	}

	CUserClans* clans = gUserProfile.clans[gUserProfile.SelectedCharID];
	int api = clans->ApiClanDonateGPToClan(amount);
	if(api != 0)
	{
		r3dOutToLog("Failed to donate to clan, api=%d\n", api);

		Scaleform::GFx::Value var[2];
		var[0].SetStringW(gLangMngr.getString("FailToDonateGCToClan"));
		var[1].SetBoolean(true);
		gfxMovie.Invoke("_root.api.showInfoMsg", var, 2);
		return;
	}
	char tmpStr[32]; sprintf(tmpStr, "%d GC", clans->clanInfo_.ClanGP);
	gfxMovie.SetVariable("_root.api.Main.ClansMyClan.MyClan.OptionsBlock3.GC.text", tmpStr);
}

void FrontendWarZ::eventRequestClanApplications(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
{
	r3d_assert(argCount == 0);

	CUserClans* clans = gUserProfile.clans[gUserProfile.SelectedCharID];

	clans->ApiGetClanStatus();

	Scaleform::GFx::Value var[9];
	for(std::list<CUserClans::ClanApplication_s>::iterator it = clans->clanApplications_.begin(); it!=clans->clanApplications_.end(); ++it)
	{
		//public function addApplication(appID:uint, appText:String, name:String, exp:uint, stime:String, rep:String, kz:uint, ks:uint, kb:uint)
		CUserClans::ClanApplication_s& appl = *it;
		var[0].SetUInt(appl.ClanApplID);
		var[1].SetString(appl.Note.c_str());
		var[2].SetString(appl.Gamertag.c_str());
		var[3].SetUInt(appl.stats.XP);
		var[4].SetString(getTimePlayedString(appl.stats.TimePlayed));
		var[4].SetStringW(getReputationString(appl.stats.Reputation));
		var[6].SetUInt(appl.stats.KilledZombies);
		var[7].SetUInt(appl.stats.KilledSurvivors);
		var[8].SetUInt(appl.stats.KilledBandits);
		gfxMovie.Invoke("_root.api.Main.ClansMyClanApps.addApplication", var, 9);
	}

	gfxMovie.Invoke("_root.api.Main.ClansMyClanApps.showApplications", "");
}

void FrontendWarZ::eventClanApplicationAction(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
{
	r3d_assert(argCount == 2);
	uint32_t applicationID = args[0].GetUInt();
	bool isAccepted = args[1].GetBool();

	CUserClans* clans = gUserProfile.clans[gUserProfile.SelectedCharID];
	int api = clans->ApiClanApplyAnswer(applicationID, isAccepted);
	if(api != 0)
	{
		r3dOutToLog("Failed to answer application, api=%d\n", api);

		Scaleform::GFx::Value var[2];
		switch (api)
		{
		case 20:
			var[0].SetStringW(gLangMngr.getString("ClanError_Code20"));
			break;
		case 21:
			var[0].SetStringW(gLangMngr.getString("ClanError_Code21"));
			break;
		case 23:
			var[0].SetStringW(gLangMngr.getString("ClanError_Code23"));
			break;
		default:
			var[0].SetStringW(gLangMngr.getString("FailToAnswerApplication"));
			break;
		}
		var[1].SetBoolean(true);
		gfxMovie.Invoke("_root.api.showInfoMsg", var, 2);
	}
}

void FrontendWarZ::eventClanInviteToClan(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
{
	r3d_assert(argCount == 1);
	const char* playerNameToInvite = args[0].GetString();

	CUserClans* clans = gUserProfile.clans[gUserProfile.SelectedCharID];
	
	int api = clans->ApiClanSendInvite(playerNameToInvite);
	if(api != 0)
	{
		r3dOutToLog("Failed to send invite, api=%d\n", api);

		Scaleform::GFx::Value var[2];
		switch (api)
		{
		case 20:
			var[0].SetStringW(gLangMngr.getString("ClanError_Code20"));
			break;
		case 21:
			var[0].SetStringW(gLangMngr.getString("ClanError_Code21"));
			break;
		case 22:
			var[0].SetStringW(gLangMngr.getString("ClanError_Code22"));
			break;
		case 23:
			var[0].SetStringW(gLangMngr.getString("ClanError_Code23"));
			break;
		case 24:
			var[0].SetStringW(gLangMngr.getString("ClanError_Code24"));
			break;
		default:
			var[0].SetStringW(gLangMngr.getString("FailToSendInvite"));
			break;
		}
		var[1].SetBoolean(true);
		gfxMovie.Invoke("_root.api.showInfoMsg", var, 2);
	}
	else
	{
		Scaleform::GFx::Value var[2];
		var[0].SetStringW(gLangMngr.getString("InviteSentSuccess"));
		var[1].SetBoolean(true);
		gfxMovie.Invoke("_root.api.showInfoMsg", var, 2);
	}
}

void FrontendWarZ::eventClanRespondToInvite(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
{
	r3d_assert(argCount == 2);
	uint32_t inviteID = args[0].GetUInt();
	bool isAccepted = args[1].GetBool();

	CUserClans* clans = gUserProfile.clans[gUserProfile.SelectedCharID];
	int api = clans->ApiClanAnswerInvite(inviteID, isAccepted);
	// remove this invite from the list
	{
		struct clanInviteSearch
		{
			uint32_t inviteID;

			clanInviteSearch(uint32_t id): inviteID(id) {};

			bool operator()(const CUserClans::ClanInvite_s &a)
			{
				return a.ClanInviteID == inviteID;
			}
		};

		clanInviteSearch prd(inviteID);
		clans->clanInvites_.erase(std::find_if(clans->clanInvites_.begin(), clans->clanInvites_.end(), prd));
	}
	if(api != 0)
	{
		r3dOutToLog("Failed to accept invite, api=%d\n", api);

		Scaleform::GFx::Value var[2];
		switch (api)
		{
		case 20:
			var[0].SetStringW(gLangMngr.getString("ClanError_Code20"));
			break;
		default:
			var[0].SetStringW(gLangMngr.getString("FailAcceptInvite"));
			break;
		}
		var[1].SetBoolean(true);
		gfxMovie.Invoke("_root.api.showInfoMsg", var, 2);
	}
	else if(isAccepted)
	{
		setClanInfo();
		gfxMovie.Invoke("_root.api.Main.showScreen", "MyClan");
	}
	else if(!isAccepted)
	{
		checkForInviteFromClan();
	}
}

void FrontendWarZ::checkForInviteFromClan()
{
	CUserClans* clans = gUserProfile.clans[gUserProfile.SelectedCharID];

	if(!clans->clanInvites_.empty())
	{
		CUserClans::ClanInvite_s& invite = clans->clanInvites_[0];
		//		public function showClanInvite(inviteID:uint, clanName:String, numMembers:uint, desc:String, iconID:uint)
		Scaleform::GFx::Value var[5];
		var[0].SetUInt(invite.ClanInviteID);
		var[1].SetString(invite.ClanName.c_str());
		var[2].SetUInt(0); // todo: need data
		var[3].SetString(""); // todo: need data
		var[4].SetUInt(invite.ClanEmblemID);
		gfxMovie.Invoke("_root.api.showClanInvite", var, 5);
	}
}

void FrontendWarZ::eventClanBuySlots(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
{
	r3d_assert(argCount == 1);
	uint32_t buyIdx = args[0].GetUInt();
	
	CUserClans* clans = gUserProfile.clans[gUserProfile.SelectedCharID];

	int api = clans->ApiClanBuyAddMembers(buyIdx);
	if(api != 0)
	{
		r3dOutToLog("Failed to buy slots, api=%d\n", api);

		Scaleform::GFx::Value var[2];
		var[0].SetStringW(gLangMngr.getString("FailToBuyMoreSlots"));
		var[1].SetBoolean(true);
		gfxMovie.Invoke("_root.api.showInfoMsg", var, 2);
		return;
	}

	gfxMovie.SetVariable("_root.api.Main.ClansMyClan.MyClan.OptionsBlock2.Slots.text", clans->clanInfo_.MaxClanMembers);
}

void FrontendWarZ::eventClanApplyToJoin(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
{
	r3d_assert(argCount == 2);
	uint32_t clanID = args[0].GetUInt();
	const char* applText = args[1].GetString();
	CUserClans* clans = gUserProfile.clans[gUserProfile.SelectedCharID];

	wiCharDataFull& slot = gUserProfile.ProfileData.ArmorySlots[gUserProfile.SelectedCharID];
	if(slot.ClanID != 0)
	{
		Scaleform::GFx::Value var[2];
		var[0].SetStringW(gLangMngr.getString("YouAreAlreadyInClan"));
		var[1].SetBoolean(true);
		gfxMovie.Invoke("_root.api.showInfoMsg", var, 2);
		return;
	}

	int api = clans->ApiClanApplyToJoin(clanID, applText);
	if(api != 0)
	{
		r3dOutToLog("Failed to apply to clan, api=%d\n", api);

		Scaleform::GFx::Value var[2];
		switch (api)
		{
		case 24:
			var[0].SetStringW(gLangMngr.getString("ClanError_Code24"));
			break;
		case 25:
			var[0].SetStringW(gLangMngr.getString("ClanError_Code25"));
			break;
		default:
			var[0].SetStringW(gLangMngr.getString("FailApplyToClan"));
			break;
		}
		var[1].SetBoolean(true);
		gfxMovie.Invoke("_root.api.showInfoMsg", var, 2);
	}
	else
	{
		Scaleform::GFx::Value var[2];
		var[0].SetStringW(gLangMngr.getString("SuccessApplyToClan"));
		var[1].SetBoolean(true);
		gfxMovie.Invoke("_root.api.showInfoMsg", var, 2);
	}
}

void FrontendWarZ::eventRequestLeaderboardData(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
{
	r3d_assert(argCount == 3);

	int hardcore = args[0].GetInt();
	r3d_assert(hardcore == 0 || hardcore == 1);

	int type = args[1].GetInt();
	r3d_assert(type >= 0 && type <= 6);

	int pageType = args[2].GetInt();
	r3d_assert(pageType >= 0 && pageType <= 3);

	const int rowsPerPage = 1; 
	switch (pageType)
	{
	case 0:
	case 1: // fall-through
		m_leaderboardPage = 1;
		break;
	case 2: // Previous
		m_leaderboardPage = std::max(1, m_leaderboardPage - 1);
		break;
	case 3: // Next
		m_leaderboardPage = std::min(m_leaderboardPageCount, m_leaderboardPage + 1);
		break;
	}

	gfxMovie.Invoke("_root.api.Main.LeaderboardAnim.clearLeaderboardList", 0);

	int curPos = 1;
	gUserProfile.ApiGetLeaderboard(hardcore, type, m_leaderboardPage, curPos, m_leaderboardPageCount);

	Scaleform::GFx::Value var[4];
	for(std::vector<CClientUserProfile::LBEntry_s>::iterator it = gUserProfile.m_lbData[type].begin(); it != gUserProfile.m_lbData[type].end(); ++it)
	{
		//public function addLeaderboardData(param1:uint, param2:String, param3:Boolean, param4:String)
		CClientUserProfile::LBEntry_s& lbe = *it;
		var[0].SetUInt(curPos++);
		var[1].SetString(lbe.gamertag);
		var[2].SetBoolean(lbe.alive ? true : false);

		std::stringstream ss;
		if (type == 1)
		{
			ss << getTimePlayedString(lbe.data);
		}
		else
		{
			ss << lbe.data;
		}
		std::string temp = ss.str();
		var[3].SetString(temp.c_str());

		gfxMovie.Invoke("_root.api.Main.LeaderboardAnim.addLeaderboardData", var, 4);
	}

	switch (type)
	{
	case 0:
		gfxMovie.Invoke("_root.api.Main.LeaderboardAnim.setLeaderboardText", gLangMngr.getString("$FR_LB_TOP_XP"));
		break;
	case 1:
		gfxMovie.Invoke("_root.api.Main.LeaderboardAnim.setLeaderboardText", gLangMngr.getString("$FR_LB_TOP_ST"));
		break;
	case 2:
		gfxMovie.Invoke("_root.api.Main.LeaderboardAnim.setLeaderboardText", gLangMngr.getString("$FR_LB_TOP_KZ"));
		break;
	case 3:
		gfxMovie.Invoke("_root.api.Main.LeaderboardAnim.setLeaderboardText", gLangMngr.getString("$FR_LB_TOP_KS"));
		break;
	case 4:
		gfxMovie.Invoke("_root.api.Main.LeaderboardAnim.setLeaderboardText", gLangMngr.getString("$FR_LB_TOP_KB"));
		break;
	case 5: // fall-through
	case 6:
		gfxMovie.Invoke("_root.api.Main.LeaderboardAnim.setLeaderboardText", gLangMngr.getString("$FR_LB_TOP_RT"));
		break;
	default:
		r3d_assert(false);
	}

	gfxMovie.Invoke("_root.api.Main.LeaderboardAnim.populateLeaderboard", 0);
}

void FrontendWarZ::eventLearnSkill(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
{
   	// Skillsystem
	skillid = args[0].GetUInt();
	const wiCharDataFull& slot = gUserProfile.ProfileData.ArmorySlots[gUserProfile.SelectedCharID];
	CharID = slot.LoadoutID;

	Scaleform::GFx::Value var[2];
	var[0].SetStringW(gLangMngr.getString("OneMomentPlease"));
	var[1].SetBoolean(false);
	gfxMovie.Invoke("_root.api.showInfoMsg", var, 2);

	async_.StartAsyncOperation(this, &FrontendWarZ::as_LearnSkilLThread, &FrontendWarZ::OnLearnSkillSuccess);
}