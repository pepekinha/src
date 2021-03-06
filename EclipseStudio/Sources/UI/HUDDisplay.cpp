#include "r3dPCH.h"
#include "r3dProtect.h"

#include "HUDDisplay.h"
#include "HUDCraft.h" //craft

#include "ObjectsCode/Gameplay/BasePlayerSpawnPoint.h"
#include "ObjectsCode/Gameplay/obj_Grave.h" //Gravestone
#include "../multiplayer/clientgamelogic.h"
#include "../ObjectsCode/ai/AI_Player.H"
#include "../ObjectsCode/weapons/Weapon.h"
#include "../ObjectsCode/weapons/WeaponArmory.h"

#include "HUDPause.h"

extern HUDPause*	hudPause;

struct NameHashFunc_T
{
	inline int operator () ( const char * szKey )
	{
		return r3dHash::MakeHash( szKey );
	}
};
static HashTableDynamic<const char*, FixedString256, NameHashFunc_T, 1024> dictionaryHash_;


HUDDisplay :: HUDDisplay()
{
	Inited = false;
	chatVisible = false;
	chatInputActive = false;
	chatVisibleUntilTime = 0;
	lastChatMessageSent = 0;
	playersListVisible = false;
	bloodAlpha = 0.0f;
	writeNoteSavedSlotIDFrom = 0;
	timeoutForNotes = 0;
	timeoutNoteReadAbuseReportedHideUI = 0;
	RangeFinderUIVisible = false;

	if(dictionaryHash_.Size() == 0)
	{
		r3dFile* f = r3d_open( "Data/LangPack/dictionary.txt", "rb" );
		if (!f->IsValid())
		{
			f = r3d_open( "Data/LangPack/dictionary.dat", "rb" );
		}

		if (f->IsValid())
		{
			char tmpStr[256];
			while(fgets(tmpStr, 256, f) != NULL)
			{
				size_t len = strlen(tmpStr);
				for(size_t i=0; i<len; ++i)
				{
					if(tmpStr[i]==13 || tmpStr[i]==10)
						tmpStr[i]=0;
				}
				dictionaryHash_.Add(tmpStr, tmpStr);	
			}
			fclose(f);
		}
	}
}

HUDDisplay :: ~HUDDisplay()
{
	dictionaryHash_.Clear();
}

bool HUDDisplay::Init()
{

	
	Cooldown = 0.0f;
	isgroups = false; // ViruZ Groups
 
	if(!gfxHUD.Load("Data\\Menu\\WarZ_HUD.swf", true)) 
		return false;
	if(!gfxBloodStreak.Load("Data\\Menu\\WarZ_BloodStreak.swf", false))
		return false;
	if(!gfxRangeFinder.Load("Data\\Menu\\WarZ_HUD_RangeFinder.swf", false))
		return false;

	gfxHUD.SetCurentRTViewport( Scaleform::GFx::Movie::SM_ExactFit );
	gfxBloodStreak.SetCurentRTViewport(Scaleform::GFx::Movie::SM_ExactFit);
	gfxRangeFinder.SetCurentRTViewport(Scaleform::GFx::Movie::SM_ExactFit);

#define MAKE_CALLBACK(FUNC) new r3dScaleformMovie::TGFxEICallback<HUDDisplay>(this, &HUDDisplay::FUNC)
	gfxHUD.RegisterEventHandler("eventChatMessage", MAKE_CALLBACK(eventChatMessage));
	gfxHUD.RegisterEventHandler("eventNoteWritePost", MAKE_CALLBACK(eventNoteWritePost));
	gfxHUD.RegisterEventHandler("eventNoteClosed", MAKE_CALLBACK(eventNoteClosed));
	gfxHUD.RegisterEventHandler("eventGraveNoteClosed", MAKE_CALLBACK(eventGraveNoteClosed));//Gravestone
	gfxHUD.RegisterEventHandler("eventNoteReportAbuse", MAKE_CALLBACK(eventNoteReportAbuse));
	gfxHUD.RegisterEventHandler("eventShowPlayerListContextMenu", MAKE_CALLBACK(eventShowPlayerListContextMenu));
	gfxHUD.RegisterEventHandler("eventPlayerListAction", MAKE_CALLBACK(eventPlayerListAction));

	{
		Scaleform::GFx::Value var[4];
		var[0].SetInt(0);
		var[1].SetString("PROXIMITY");
		var[2].SetBoolean(true);
		var[3].SetBoolean(true);
		gfxHUD.Invoke("_root.api.setChatTab", var, 4);
 		var[0].SetInt(1);
		var[1].SetString("GLOBAL");
		var[2].SetBoolean(false);
		var[3].SetBoolean(true);
		gfxHUD.Invoke("_root.api.setChatTab", var, 4);
		var[0].SetInt(2);
		var[1].SetString("CLAN");
		var[2].SetBoolean(false);
		var[3].SetBoolean(false);
		gfxHUD.Invoke("_root.api.setChatTab", var, 4);
		var[0].SetInt(3);
		var[1].SetString("GROUP");
		var[2].SetBoolean(false);
		var[3].SetBoolean(true);
		gfxHUD.Invoke("_root.api.setChatTab", var, 4);

		currentChatChannel = 0;
		var[0].SetInt(0);
		gfxHUD.Invoke("_root.api.setChatTabActive", var, 1);
	}

	setChatTransparency(R3D_CLAMP(g_ui_chat_alpha->GetFloat()/100.0f, 0.0f, 1.0f));

	Inited = true;

	weaponInfoVisible = -1;
	SafeZoneWarningVisible = false;
	
	if (gClientLogic().m_gameInfo.isfarm)
	{
	addChatMessage(1,"<SYSTEM>","You have joined farm server",2);
	}
	else if (gClientLogic().m_gameInfo.ispass)
	{
	addChatMessage(1,"<SYSTEM>","You have joined private server",2);
	}
	else if (gClientLogic().m_gameInfo.ispre)
	{
	addChatMessage(1,"<SYSTEM>","You have joined premium server",2);
	}
	else if (!gClientLogic().m_gameInfo.ispass && !gClientLogic().m_gameInfo.isfarm)
	{
	addChatMessage(1,"<SYSTEM>","You have joined officials server",2);
	}

	return true;
}

bool HUDDisplay::Unload()
{
	gfxHUD.Unload();
	gfxBloodStreak.Unload();
	gfxRangeFinder.Unload();

	Inited = false;
	return true;
}

void HUDDisplay::enableClanChannel()
{
	Scaleform::GFx::Value var[4];
	var[0].SetInt(2);
	var[1].SetString("CLAN");
	var[2].SetBoolean(false);
	var[3].SetBoolean(true);
	gfxHUD.Invoke("_root.api.setChatTab", var, 4);
}
//ViruZ Group
void HUDDisplay::enableGroupChannel()
{
	Scaleform::GFx::Value var[4];
	var[0].SetInt(3);
	var[1].SetString("GROUP");
	var[2].SetBoolean(false);
	var[3].SetBoolean(true);
	gfxHUD.Invoke("_root.api.setChatTab", var, 4);
}

int HUDDisplay::Update()
{
	if(!Inited)
		return 1;
	// cooldown novo

	int value;
    int timeLeft = int(ceilf(Cooldown - r3dGetTime()));
 
     if(Cooldown > 0)
     {
     //int timeLeftBar = int(ceilf(0.0f + r3dGetTime()));
 
    if (timeLeft == 9)
    {
      value = 10;
    }
    else if (timeLeft == 8)
    {
         value = 20;
    }
       else if (timeLeft == 7)
    {
         value = 30;
    }
       else if (timeLeft == 6)
    {
         value = 40;
    }
         else if (timeLeft == 5)
    {
         value = 50;
    }
         else if (timeLeft == 4)
    {
         value = 60;
    }
           else if (timeLeft == 3)
    {
         value = 70;
    }
           else if (timeLeft == 2)
    {
         value = 80;
    }
             else if (timeLeft == 1)
    {
         value = 90;
    }
   //          else if (timeLeft == 0)
   // {
 // value = 100;
 //   }
        if(timeLeft > 0)
      {
        setCooldown(currentslot,value,timeLeft);
      }
      else
      {
        Cooldown = 0.0f;
      }
     }
 
     //r3dOutToLog("%d",value);
     if (value == 90)
     {
       value = 100;
       timeLeft = 0;
       Cooldown = 0.0f;
       setCooldown(currentslot,100,timeLeft);
     }

	const ClientGameLogic& CGL = gClientLogic();

	if(r3dGetTime() > chatVisibleUntilTime && chatVisible && !chatInputActive)
	{
		showChat(false);
	}

	if(r3dGetTime() > timeoutNoteReadAbuseReportedHideUI && timeoutNoteReadAbuseReportedHideUI != 0)
	{
		r3dMouse::Hide();
		writeNoteSavedSlotIDFrom = 0;
		timeoutNoteReadAbuseReportedHideUI = 0;
		timeoutForNotes = r3dGetTime() + 0.5f;
		Scaleform::GFx::Value var[2];
		var[0].SetBoolean(false);
		var[1].SetString("");
		gfxHUD.Invoke("_root.api.showNoteRead", var, 2);
	}

	if(RangeFinderUIVisible)
	{
		r3dPoint3D dir;
		r3dScreenTo3D(r3dRenderer->ScreenW2, r3dRenderer->ScreenH2, &dir);

		PxRaycastHit hit;
		PhysicsCallbackObject* target = NULL;
		PxSceneQueryFilterData filter(PxFilterData(COLLIDABLE_STATIC_MASK|(1<<PHYSCOLL_NETWORKPLAYER), 0, 0, 0), PxSceneQueryFilterFlag::eSTATIC|PxSceneQueryFilterFlag::eDYNAMIC);
		g_pPhysicsWorld->raycastSingle(PxVec3(gCam.x, gCam.y, gCam.z), PxVec3(dir.x, dir.y, dir.z), 2000.0f, PxSceneQueryFlag::eDISTANCE, hit, filter);

		float distance = -1;
		if(hit.shape)
		{
			// sergey's design (range finder shows not real distance... have no idea what it actually shows)
			distance = hit.distance * (1.0f + R3D_MIN(1.0f, (R3D_MAX(0.0f, (hit.distance-200.0f)/1800.0f)))*0.35f);
		}
		gfxRangeFinder.Invoke("_root.Main.Distance.gotoAndStop", distance!=-1?"on":"off");	
		char tmpStr[16];
		sprintf(tmpStr, "%.1f", distance);
		gfxRangeFinder.SetVariable("_root.Main.Distance.Distance.Distance.text", tmpStr);

		const ClientGameLogic& CGL = gClientLogic();
		float compass = atan2f(CGL.localPlayer_->m_vVision.z, CGL.localPlayer_->m_vVision.x)/R3D_PI;
		compass = R3D_CLAMP(compass, -1.0f, 1.0f);

		float cmpVal = -(compass * 820);
		gfxRangeFinder.SetVariable("_root.Main.compass.right.x", cmpVal);
		gfxRangeFinder.SetVariable("_root.Main.compass.left.x", cmpVal-1632);

		if(!CGL.localPlayer_->m_isAiming)
			showRangeFinderUI(false); // in case if player switched weapon or anything happened
	}

	return 1;
}


int HUDDisplay::Draw()
{
	if(!Inited)
		return 1;
	{
		R3DPROFILE_FUNCTION("gfxBloodStreak.UpdateAndDraw");
		if(bloodAlpha > 0.0f)
			gfxBloodStreak.UpdateAndDraw();
	}
	{
		R3DPROFILE_FUNCTION("gfxRangeFinder.UpdateAndDraw");
		if(RangeFinderUIVisible)
			gfxRangeFinder.UpdateAndDraw();
	}
	{
		R3DPROFILE_FUNCTION("gfxHUD.UpdateAndDraw");
#ifndef FINAL_BUILD
		gfxHUD.UpdateAndDraw(d_disable_render_hud->GetBool());
#else
		gfxHUD.UpdateAndDraw();
#endif
	}

	return 1;
}

void HUDDisplay::setBloodAlpha(float alpha)
{
	if(!Inited) return;
	if(R3D_ABS(bloodAlpha-alpha)<0.01f) return;

	bloodAlpha = alpha;
	gfxBloodStreak.SetVariable("_root.blood.alpha", alpha);
}

void HUDDisplay::eventShowPlayerListContextMenu(r3dScaleformMovie* pMove, const Scaleform::GFx::Value* args, unsigned argCount)
{
	/*gfxHUD.Invoke("_root.api.setPlayerListContextMenuButton", 2, "");
	gfxHUD.Invoke("_root.api.setPlayerListContextMenuButton", 3, "");
	gfxHUD.Invoke("_root.api.setPlayerListContextMenuButton", 4, "$HUD_PlayerAction_Kick");
	gfxHUD.Invoke("_root.api.setPlayerListContextMenuButton", 5, "$HUD_PlayerAction_Ban");*/
    int isDev = gUserProfile.ProfileData.isDevAccount;
	Scaleform::GFx::Value var[3];
    /*
	var[0].SetInt(2);
	var[1].SetString("");
	var[2].SetInt(0);
	gfxHUD.Invoke("_root.api.setPlayerListContextMenuButton", var, 3);

	var[0].SetInt(3);
	var[1].SetString("");
	var[2].SetInt(0);
	gfxHUD.Invoke("_root.api.setPlayerListContextMenuButton", var, 3);

	var[0].SetInt(4);
	var[1].SetString("");
	var[2].SetInt(4);
	gfxHUD.Invoke("_root.api.setPlayerListContextMenuButton", var, 3);

	var[0].SetInt(5);
	var[1].SetString("");
	var[2].SetInt(5);
	gfxHUD.Invoke("_root.api.setPlayerListContextMenuButton", var, 3);
*/
	if(isDev){
		var[0].SetInt(1);
		var[1].SetString("$HUD_PlayerAction_Report");
		var[2].SetInt(1);
		gfxHUD.Invoke("_root.api.setPlayerListContextMenuButton", var, 3);

		var[0].SetInt(2);
		var[1].SetString("$HUD_PlayerAction_ToPlayer");
		var[2].SetInt(2);
		gfxHUD.Invoke("_root.api.setPlayerListContextMenuButton", var, 3);

		var[0].SetInt(3);
		var[1].SetString("$HUD_PlayerAction_ToMe");
		var[2].SetInt(3);
		gfxHUD.Invoke("_root.api.setPlayerListContextMenuButton", var, 3);

		var[0].SetInt(4);
		var[1].SetString("$HUD_PlayerAction_Kick");
		var[2].SetInt(4);
		gfxHUD.Invoke("_root.api.setPlayerListContextMenuButton", var, 3);

		var[0].SetInt(5);
		var[1].SetString("$HUD_PlayerAction_Ban");
		var[2].SetInt(5);
		gfxHUD.Invoke("_root.api.setPlayerListContextMenuButton", var, 3);

		var[0].SetInt(6);
		var[1].SetString("$HUD_PlayerAction_Ban");
		var[2].SetInt(6);
		gfxHUD.Invoke("_root.api.setPlayerListContextMenuButton", var, 3);
		
		//MENUS ADICIONAIS NA LISTA DE GM
		/*var[0].SetInt(7);
		var[1].SetString("$HUD_PlayerAction_Ban");
		var[2].SetInt(7);
		gfxHUD.Invoke("_root.api.setPlayerListContextMenuButton", var, 3);*/

	}else{
		//ViruZ Group
		
				
		var[0].SetInt(1);
		var[1].SetString("$HUD_PlayerAction_Report");
		var[2].SetInt(1);
		gfxHUD.Invoke("_root.api.setPlayerListContextMenuButton", var, 3);

		var[0].SetInt(2);
		var[1].SetString("INVITE FRIEND");
		var[2].SetInt(2);
		gfxHUD.Invoke("_root.api.setPlayerListContextMenuButton", var, 3);
		
		int index = 0;
		for(int i=0; i<R3D_ARRAYSIZE(gClientLogic().playerNames); i++)
		{
			if (gClientLogic().playerNames[i].isInvitePending == true)
			{
			var[0].SetInt(3);
			var[1].SetString("ACCEPT INVITE");
			var[2].SetInt(3);
			gfxHUD.Invoke("_root.api.setPlayerListContextMenuButton", var, 3);
		
			var[0].SetInt(4);
			var[1].SetString("DECLINE INVITE");
			var[2].SetInt(4);
			gfxHUD.Invoke("_root.api.setPlayerListContextMenuButton", var, 3);
			}
		}
		var[0].SetInt(5);
		var[1].SetString("");
		var[2].SetInt(0);
		gfxHUD.Invoke("_root.api.setPlayerListContextMenuButton", var, 3);

		var[0].SetInt(6);
		var[1].SetString("");
		var[2].SetInt(0);
		gfxHUD.Invoke("_root.api.setPlayerListContextMenuButton", var, 3);
		
		//MENUS ADICIONAIS LISTA PLAYER
		/*var[0].SetInt(7);
		var[1].SetString("");
		var[2].SetInt(0);
		gfxHUD.Invoke("_root.api.setPlayerListContextMenuButton", var, 3);*/
	}

	gfxHUD.Invoke("_root.api.showPlayerListContextMenu", "");
}

void HUDDisplay::eventPlayerListAction(r3dScaleformMovie* pMove, const Scaleform::GFx::Value* args, unsigned argCount)
{
	// REPORT
	// ""
	// ""
	// KICK
	// BAN
	int action = args[0].GetInt();
	const char* pName = args[1].GetString();
	//char msg[128];

	//sprintf(msg, "Action: %d, pName: %s", action, pName);
	//addChatMessage(0, "system", msg, 0);
	// END Close the List
	/*if(action == 1)
	{
		showChatInput();

		char ffReport[128];
		sprintf(ffReport, "FairFight %s ", pName);
		//gfxHUD.Invoke("_root.api.setChatActive", ffReport);

		chatVisible = true;
		Scaleform::GFx::Value var[3];
		var[0].SetBoolean(true);
		var[1].SetBoolean(true);
		var[2].SetString(ffReport);
		gfxHUD.Invoke("_root.api.showChat", var, 3);
		chatVisibleUntilTime = r3dGetTime() + 20.0f;
	}*/
	int isDev = gUserProfile.ProfileData.isDevAccount;
	if(isDev){

	if(action == 1)
	{
		showChatInput();

		char ffReport[128];
		sprintf(ffReport, "/report \"%s\" Reason: ", pName);
		gfxHUD.Invoke("_root.api.setChatActive", ffReport);

		chatVisible = true;
		Scaleform::GFx::Value var[3];
		var[0].SetBoolean(true);
		var[1].SetBoolean(true);
		var[2].SetString(ffReport);
		gfxHUD.Invoke("_root.api.showChat", var, 3);
	}
	 if(action == 2)
    {
        showChatInput();


        char cmGoto[128];
        sprintf(cmGoto, "/goto %s ", pName);
        //gfxHUD.Invoke("_root.api.setChatActive", ffReport);


        chatVisible = true;
        Scaleform::GFx::Value var[3];
        var[0].SetBoolean(true);
        var[1].SetBoolean(true);
        var[2].SetString(cmGoto);
        gfxHUD.Invoke("_root.api.showChat", var, 3);
        chatVisibleUntilTime = r3dGetTime() + 20.0f;
    }
	 if(action == 3)
    {
        showChatInput();


        char cmGoto[128];
        sprintf(cmGoto, "/tome %s ", pName);
        //gfxHUD.Invoke("_root.api.setChatActive", ffReport);


        chatVisible = true;
        Scaleform::GFx::Value var[3];
        var[0].SetBoolean(true);
        var[1].SetBoolean(true);
        var[2].SetString(cmGoto);
        gfxHUD.Invoke("_root.api.showChat", var, 3);
        chatVisibleUntilTime = r3dGetTime() + 20.0f;
    }
    if(action == 4)
    {
        showChatInput();


        char cmKick[128];
        sprintf(cmKick, "/kick %s ", pName);
        //gfxHUD.Invoke("_root.api.setChatActive", ffReport);


        chatVisible = true;
        Scaleform::GFx::Value var[3];
        var[0].SetBoolean(true);
        var[1].SetBoolean(true);
        var[2].SetString(cmKick);
        gfxHUD.Invoke("_root.api.showChat", var, 3);
        chatVisibleUntilTime = r3dGetTime() + 20.0f;
    }
    if(action == 5)
    {
        showChatInput();


        char cmBan[128];
        sprintf(cmBan, "/ban %s ", pName);
        //gfxHUD.Invoke("_root.api.setChatActive", ffReport);


        chatVisible = true;
        Scaleform::GFx::Value var[3];
        var[0].SetBoolean(true);
        var[1].SetBoolean(true);
        var[2].SetString(cmBan);
        gfxHUD.Invoke("_root.api.showChat", var, 3);
        chatVisibleUntilTime = r3dGetTime() + 20.0f;
    }
	}
	else //ViruZ Group
	{	
		if(action == 1)
		{
		showChatInput();

		char ffReport[128];
		sprintf(ffReport, "/report \"%s\" Reason: ", pName);
		gfxHUD.Invoke("_root.api.setChatActive", ffReport);

		chatVisible = true;
		Scaleform::GFx::Value var[3];
		var[0].SetBoolean(true);
		var[1].SetBoolean(true);
		var[2].SetString(ffReport);
		gfxHUD.Invoke("_root.api.showChat", var, 3);
		}
		if(action == 2)
		{
			
			const ClientGameLogic& CGL = gClientLogic();
			obj_Player* plr = CGL.localPlayer_;
			{
				if (plr)
				{ 		
					//plr->CurLoadout.Gamertag
					PKT_S2C_SendGroupInvite_s n;
					n.FromCustomerID = plr->CustomerID;
					sprintf(n.intogamertag,pName);
					r3dscpy(n.fromgamertag,plr->CurLoadout.Gamertag);
					p2pSendToHost(gClientLogic().localPlayer_, &n, sizeof(n));
					isgroups = true;
					return;
				}
			}
			
		}

		if(action == 3)
		{
			int index = 0;
			for(int i=0; i<R3D_ARRAYSIZE(gClientLogic().playerNames); i++)
			{
				if(gClientLogic().playerNames[i].Gamertag[0])
				{
					//	if (!gClientLogic().playerNames[i].isGroups)
					//	{
					if (gClientLogic().playerNames[i].isInvitePending)
					{
						obj_Player* plr = gClientLogic().localPlayer_;
						PKT_S2C_SendGroupAccept_s n1;

						n1.FromCustomerID = plr->CustomerID;
						r3dscpy(n1.fromgamertag,plr->CurLoadout.Gamertag);

						r3dscpy(n1.intogamertag,gClientLogic().playerNames[i].Gamertag);
						p2pSendToHost(plr, &n1, sizeof(n1));
						gClientLogic().playerNames[i].isInvitePending = false;
						isgroups = true;
						return;
					}
					else
					{
						//showMessage(gLangMngr.getString("Player Not Found"));
					}
					//	}
					//else
					//	{
					//showMessage(gLangMngr.getString("Player Allready Groups"));
					//}
				}
			}
		}
		if(action == 4)
		{
			int index = 0;
			for(int i=0; i<R3D_ARRAYSIZE(gClientLogic().playerNames); i++)
			{
				if(gClientLogic().playerNames[i].Gamertag[0])
				{
					if (gClientLogic().playerNames[i].isInvitePending)
					{
						obj_Player* plr = gClientLogic().localPlayer_;
						PKT_S2C_SendGroupNoAccept_s n1;

						n1.FromCustomerID = plr->CustomerID;
						r3dscpy(n1.fromgamertag,plr->CurLoadout.Gamertag);
						r3dscpy(n1.intogamertag,gClientLogic().playerNames[i].Gamertag);
						p2pSendToHost(plr, &n1, sizeof(n1));
						gClientLogic().playerNames[i].isInvitePending = false;
						isgroups = false;
						return;
					}
					else
					{
						//showMessage(gLangMngr.getString("Player Not Found"));
					}
				}
			}
		}
	}
}
void HUDDisplay::removeplayerfromgroup(const char* gamertag,bool legend)
{
	Scaleform::GFx::Value var[2];
	var[0].SetString(gamertag);
	gfxHUD.Invoke("_root.api.removePlayerFromGroup", var , 1);
}

void HUDDisplay::addplayertogroup(const char* gamertag,bool legend)
{
	Scaleform::GFx::Value var[3];
	var[0].SetString(gamertag);
	var[1].SetBoolean(legend);
	var[2].SetBoolean(false);
	gfxHUD.Invoke("_root.api.addPlayerToGroup", var , 3);
}
//ViruZ Group
void HUDDisplay::eventChatMessage(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
{
	chatInputActive = false;
	lastChatMessageSent = r3dGetTime();

	static char s_chatMsg[2048];
	int currentTabIndex = args[0].GetInt();
	r3dscpy(s_chatMsg, args[1].GetString());

	bool has_anything = false;

	size_t start_text=0;
	size_t argLen = strlen(s_chatMsg);
	if(argLen < 3)
		return;
		
	//ViruZ Group
	if(strncmp(s_chatMsg, "/accept", 6) == NULL)
		{
			char buf[256];
			char name[256];
			if(2 != sscanf(s_chatMsg, "%s %s", buf, &name))
			{
				addChatMessage(0, "<SYSTEM>", "/accept {name}", 0);
				return;
			}
			obj_Player* plr = gClientLogic().localPlayer_;
			PKT_S2C_SendGroupAccept_s n1;

			n1.FromCustomerID = plr->CustomerID;
			r3dscpy(n1.fromgamertag,plr->CurLoadout.Gamertag);

			r3dscpy(n1.intogamertag,name);
			p2pSendToHost(plr, &n1, sizeof(n1));
			isgroups = true;
		}

	if(strncmp(s_chatMsg, "/invite", 6) == NULL)
		{
			char buf[256];
			char name[256];
			if(2 != sscanf(s_chatMsg, "%s %s", buf, &name))
			{
				addChatMessage(0, "<SYSTEM>", "/invite {Exact player name}", 0);
				return;
			}
			obj_Player* plr = gClientLogic().localPlayer_;
			PKT_S2C_SendGroupInvite_s n;
			n.FromCustomerID = plr->CustomerID;
			sprintf(n.intogamertag,name);
			r3dscpy(n.fromgamertag,plr->CurLoadout.Gamertag);
			p2pSendToHost(plr, &n, sizeof(n));
			isgroups = true;
		}

	const wiCharDataFull& slot = gUserProfile.ProfileData.ArmorySlots[gUserProfile.SelectedCharID];
	if(strncmp(s_chatMsg, "/showgroups", 6) == NULL)
	{
		gfxHUD.Invoke("_root.api.refreshPlayerGroupList", "");
		addChatMessage(0, "<SYSTEM>", "Groups Show", 0);
		return;
	}

	if(strncmp(s_chatMsg, "/creategroups", 6) == NULL)
	{
		if (isgroups)
		{

			addChatMessage(0, "<SYSTEM>", "Allready groups", 0);
			return;

		}
		else
		{

			Scaleform::GFx::Value var[2];
			var[0].SetString(slot.Gamertag);
			var[1].SetBoolean(true);
			gfxHUD.Invoke("_root.api.addPlayerToGroup", var , 2);
			addChatMessage(0, "<SYSTEM>", "Groups Created", 0);
			isgroups = true;
			return;
		}
	}

	if(strncmp(s_chatMsg, "/leavegroup", 6) == NULL)
	{
		if (isgroups)
		{
			Scaleform::GFx::Value var[2];
			var[0].SetString(slot.Gamertag);
			gfxHUD.Invoke("_root.api.removePlayerFromGroup", var , 1);
			addChatMessage(0, "<SYSTEM>", "Group Disbanded", 0);
			isgroups = false;
			return;
		}
		else
		{
			addChatMessage(0, "<SYSTEM>", "No Group", 0);
			return;
		}
	}

	//ViruZ Group
	  if(gUserProfile.ProfileData.isDevAccount && strncmp(s_chatMsg, "/stime", 6) == NULL)
	{
		char buf[256];
		int hour, min;
		if(3 != sscanf(s_chatMsg, "%s %d %d", buf, &hour, &min))
		{
			addChatMessage(0, "<system>", "/stime {hour} {min}", 0);
			return;
		}

		__int64 gameUtcTime = gClientLogic().GetServerGameTime();
		struct tm* tm = _gmtime64(&gameUtcTime);
		r3d_assert(tm);
		
		// adjust server time to match supplied hour
		gClientLogic().gameStartUtcTime_ -= tm->tm_sec;
		gClientLogic().gameStartUtcTime_ -= (tm->tm_min) * 60;
		gClientLogic().gameStartUtcTime_ += (hour - tm->tm_hour) * 60 * 60;
		gClientLogic().gameStartUtcTime_ += (min) * 60;
		gClientLogic().lastShadowCacheReset_ = -1;
		
		addChatMessage(0, "<system>", "time changed", 0);
		return;
	}
//#endif

	char userName[64];
	gClientLogic().localPlayer_->GetUserName(userName);

	{
		PKT_C2C_ChatMessage_s n;
		n.userFlag = 0; // server will init it for others
		n.msgChannel = currentTabIndex;
		r3dscpy(n.msg, &s_chatMsg[start_text]);
		r3dscpy(n.gamertag, userName);
		p2pSendToHost(gClientLogic().localPlayer_, &n, sizeof(n));
	}

	uint32_t flags = 0;
	if(gUserProfile.ProfileData.AccountType==0)
		flags|=1;
	if(gUserProfile.ProfileData.isDevAccount)
		flags|=2;
	if(gUserProfile.ProfileData.AccountType==5) //Premium
		flags|=4;
	addChatMessage(currentTabIndex, userName, &s_chatMsg[start_text], flags);

	memset(s_chatMsg, 0, sizeof(s_chatMsg));
}

void HUDDisplay::addChatMessage(int tabIndex, const char* user, const char* text, uint32_t flags)
{
	if(!Inited) return;
	Scaleform::GFx::Value var[3];

	char tmpMsg[1024];
	const char* tabNames[] = {"[PROXIMITY]", "[GLOBAL]", "[CLAN]", "[GROUP]"};
	const char* tabNamesColor[] = {"#00A000", "#13bbeb", "#de13eb", "#ffa900"};
	const char* userNameColor[] = {"#ffffff", "#ffa800"};

	bool isUserLegend = (flags&1)?true:false;
	bool isUserDev = (flags&2)?true:false;
	bool isUserPremium = (flags&4)?true:false;

	const char* userColor = userNameColor[isUserLegend?1:0];
	const char* textColor = "#d0d0d0";
	const char* namePrefix = "";
	
	if(isUserDev)
	{
	 userColor = "#ff0000";
	 textColor = "#27ff27";
	 namePrefix = "&lt;DEV&gt;";
	}
	if (isUserLegend)
	{
      userColor = "#1E90FF";
      textColor = "#00BFFF";
      namePrefix = "&lt;LEGEND&gt;";
	}
	if (isUserPremium)
	{
      userColor = "#FFD700";
      textColor = "#EEE8AA";
      namePrefix = "&lt;PREMIUM&gt;";
	}
	
	// dirty stl :)
	std::string sUser = user;
	int pos = 0;
	while((pos= sUser.find('<'))!=-1)
		sUser.replace(pos, 1, "&lt;");
	while((pos = sUser.find('>'))!=-1)
		sUser.replace(pos, 1, "&gt;");

	std::string sMsg = text;
	while((pos = sMsg.find('<'))!=-1)
		sMsg.replace(pos, 1, "&lt;");
	while((pos = sMsg.find('>'))!=-1)
		sMsg.replace(pos, 1, "&gt;");

	// really simple profanity filter
	{
		int counter = 0;
		char profanityFilter[2048]={0};
		char clearString[2048]={0};
		r3dscpy(profanityFilter, sMsg.c_str());
		char* word = strtok(profanityFilter, " ");
		while(word)
		{
			if(dictionaryHash_.IsExists(word))
			{
				r3dscpy(&clearString[counter], "*** ");
				counter +=4;
			}
			else
			{
				r3dscpy(&clearString[counter], word);
				counter +=strlen(word);
				clearString[counter++] = ' ';
			}
			word = strtok(NULL, " ");
		}
		clearString[counter++] = 0;

		sMsg = clearString;
	}

	sprintf(tmpMsg, "<font color=\"%s\">%s</font> <font color=\"%s\">%s%s:</font> <font color=\"%s\">%s</font>", tabNamesColor[tabIndex], tabNames[tabIndex], userColor, namePrefix, sUser.c_str(), textColor, sMsg.c_str());

	var[0].SetString(tmpMsg);
	gfxHUD.Invoke("_root.api.receiveChat", var, 1);
}

void HUDDisplay::setVisibility(float percent)
{
	if(!Inited) return;
	gfxHUD.Invoke("_root.api.updateVisibility", percent);
}

void HUDDisplay::setHearing(float percent)
{
	if(!Inited) return;
	gfxHUD.Invoke("_root.api.updateHearingRadius", percent);
}

void HUDDisplay::setLifeParams(int food, int water, int health, int toxicity, int stamina)
{
	if(!Inited) return;
	Scaleform::GFx::Value var[5];

	// temp, for testing
#ifndef FINAL_BUILD
	if(d_ui_health->GetInt() >= 0)
		health = d_ui_health->GetInt();
	if(d_ui_toxic->GetInt() >= 0)
		toxicity = d_ui_toxic->GetInt();
	if(d_ui_water->GetInt() >= 0)
		water = d_ui_water->GetInt();
	if(d_ui_food->GetInt() >= 0)
		food = d_ui_food->GetInt();
	if(d_ui_stamina->GetInt() >= 0)
		stamina = d_ui_stamina->GetInt();
#endif

	// UI expects inverse values, so do 100-X (exception is toxicity)
	var[0].SetInt(100-food);
	var[1].SetInt(100-water);
	var[2].SetInt(100-health);
	var[3].SetInt(toxicity);
	var[4].SetInt(100-stamina);
	gfxHUD.Invoke("_root.api.setHeroCondition", var, 5);
}

void HUDDisplay::setWeaponInfo(int ammo, int clips, int firemode)
{
	if(!Inited) return;
	Scaleform::GFx::Value var[3];
	var[0].SetInt(ammo);
	var[1].SetInt(clips);
	if(firemode==1)
		var[2].SetString("one");
	else if(firemode ==2)
		var[2].SetString("three");
	else
		var[2].SetString("auto");
	gfxHUD.Invoke("_root.api.setWeaponInfo", var, 3);
}

void HUDDisplay::showWeaponInfo(int state)
{
	if(!Inited) return;
	if(state != weaponInfoVisible)
		gfxHUD.Invoke("_root.api.showWeaponInfo", state);
	weaponInfoVisible = state;
}

void HUDDisplay::setSlotInfo(int slotID, const char* name, int quantity, const char* icon)
{
	if(!Inited) return;
	Scaleform::GFx::Value var[4];
	var[0].SetInt(slotID);
	var[1].SetString(name);
	var[2].SetInt(quantity);
	var[3].SetString(icon);
	gfxHUD.Invoke("_root.api.setSlot", var, 4);
}

void HUDDisplay::updateSlotInfo(int slotID, int quantity)
{
	if(!Inited) return;
	Scaleform::GFx::Value var[2];
	var[0].SetInt(slotID);
	var[1].SetInt(quantity);
	gfxHUD.Invoke("_root.api.updateSlot", var, 2);
}

void HUDDisplay::showSlots(bool state)
{
	if(!Inited) return;
	gfxHUD.Invoke("_root.api.showSlots", state);
}

void HUDDisplay::setActiveSlot(int slotID)
{
	if(!Inited) return;
	gfxHUD.Invoke("_root.api.setActiveSlot", slotID);
}

void HUDDisplay::setActivatedSlot(int slotID)
{
	if(!Inited) return;
	gfxHUD.Invoke("_root.api.setActivatedSlot", slotID);
}

void HUDDisplay::showMessage(const wchar_t* text)
{
	if(!Inited) return;
	gfxHUD.Invoke("_root.api.showMsg", text);
}

void HUDDisplay::showChat(bool showChat, bool force)
{
	if(!Inited) return;
	if(chatVisible != showChat || force)
	{
		chatVisible = showChat;
		Scaleform::GFx::Value var[2];
		var[0].SetBoolean(showChat);
		var[1].SetBoolean(chatInputActive);
		gfxHUD.Invoke("_root.api.showChat", var, 2);
	}
	if(showChat)
		chatVisibleUntilTime = r3dGetTime() + 20.0f;
}

void HUDDisplay::showChatInput()
{
	if(!Inited) return;
	chatInputActive = true;
	showChat(true, true);
	gfxHUD.Invoke("_root.api.setChatActive", "");
}

void HUDDisplay::setChatTransparency(float alpha)
{
	if(!Inited) return;
	gfxHUD.Invoke("_root.api.setChatTransparency", alpha);
}

void HUDDisplay::setChatChannel(int index)
{
	if(!Inited) return;
	if(index <0 || index > 3) return;

	if(currentChatChannel != index)
	{
		currentChatChannel = index;
		Scaleform::GFx::Value var[1];
		var[0].SetInt(index);
		gfxHUD.Invoke("_root.api.setChatTabActive", var, 1);

		showChatInput();
	}
}

void HUDDisplay::clearPlayersList()
{
	if(!Inited) return;
	gfxHUD.Invoke("_root.api.clearPlayersList", "");
}

extern const wchar_t* getReputationString(int Reputation);
void HUDDisplay::addPlayerToList(int num, const char* name, int Reputation, bool isLegend, bool isDev, bool isPunisher, bool isInvitePending, bool isPremium)
{
	if(!Inited) return;
	Scaleform::GFx::Value var[10];
	var[0].SetInt(num);
	var[1].SetInt(num);
	
	// dirty stl :)
	std::string sUser = name;
	int pos = 0;
	while((pos= sUser.find('<'))!=-1)
		sUser.replace(pos, 1, "&lt;");
	while((pos = sUser.find('>'))!=-1)
		sUser.replace(pos, 1, "&gt;");
	
	var[2].SetString(sUser.c_str());

	const wchar_t* algnmt = getReputationString(Reputation);
	if(isDev)
		algnmt = L"";
	var[3].SetStringW(algnmt);//algnmt
	var[4].SetBoolean(isLegend);
	var[5].SetBoolean(isDev);
	var[6].SetBoolean(false);//isPunisher
	var[7].SetBoolean(isInvitePending); //isInvitePending era false //ViruZ Group
	var[8].SetBoolean(false);//VoipMuted
	var[9].SetBoolean(isPremium);
	gfxHUD.Invoke("_root.api.addPlayerToList", var, 10);
}

void HUDDisplay::showPlayersList(int flag)
{
	if(!Inited) return;
	playersListVisible = flag;
	gfxHUD.Invoke("_root.api.showPlayersList", flag);
}
//ViruZ Group
void HUDDisplay::groupmenu()
{
	Scaleform::GFx::Value var[1];
	var[0].SetBoolean(true);
	gfxHUD.Invoke("_root.api.main.groupMenu", var,1);
}

void HUDDisplay::showWriteNote(int slotIDFrom)
{
	if(!Inited) return;

	r3dMouse::Show();
	
	writeNoteSavedSlotIDFrom = slotIDFrom;

	Scaleform::GFx::Value var[1];
	var[0].SetBoolean(true);
	gfxHUD.Invoke("_root.api.showNoteWrite", var, 1);
}

void HUDDisplay::eventNoteWritePost(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
{
	r3d_assert(argCount == 1);

	r3dMouse::Hide();

	const char* Message = args[0].GetString();

	obj_Player* plr = gClientLogic().localPlayer_;
	r3d_assert(plr);

	PKT_C2S_CreateNote_s n;
	n.SlotFrom = (BYTE)writeNoteSavedSlotIDFrom;
	n.pos      = plr->GetPosition() + plr->GetvForw()*0.2f;
	n.ExpMins  = PKT_C2S_CreateNote_s::DEFAULT_PLAYER_NOTE_EXPIRE_TIME;
	r3dscpy(n.TextFrom, plr->CurLoadout.Gamertag);
	sprintf(n.TextSubj, Message); 
	p2pSendToHost(gClientLogic().localPlayer_, &n, sizeof(n));

	// local logic
	wiInventoryItem& wi = plr->CurLoadout.Items[writeNoteSavedSlotIDFrom];
	r3d_assert(wi.itemID && wi.quantity > 0);
	//local logic
	wi.quantity--;
	if(wi.quantity <= 0) {
		wi.Reset();
	}

	plr->OnBackpackChanged(writeNoteSavedSlotIDFrom);

	writeNoteSavedSlotIDFrom = 0;

	timeoutForNotes = r3dGetTime() + .5f;
}

//Gravestone
 void HUDDisplay::eventGraveNoteClosed(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
 {
   r3dMouse::Hide();
 
   writeNoteSavedSlotIDFrom = 0; 
   timeoutForNotes = r3dGetTime() + .5f;
 }

void HUDDisplay::eventNoteClosed(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
{
	r3dMouse::Hide();

	writeNoteSavedSlotIDFrom = 0;
	timeoutForNotes = r3dGetTime() + .5f;
}

//Gravestone
void HUDDisplay::showGraveNote(const char* plr,const char* plr2)
 {
 if(!Inited) return;
 
   r3dMouse::Show();
   writeNoteSavedSlotIDFrom = 1; // temp, to prevent mouse from hiding
   Scaleform::GFx::Value var[4];
   var[0].SetBoolean(true);
   var[1].SetString("R.I.P");
   var[2].SetString(plr);
   var[3].SetString(plr2);
   gfxHUD.Invoke("_root.api.showGraveNote", var, 4);
}

void HUDDisplay::showReadNote(const char* msg)
{
	if(!Inited) return;

	r3dMouse::Show();
	writeNoteSavedSlotIDFrom = 1; // temp, to prevent mouse from hiding
	Scaleform::GFx::Value var[2];
	var[0].SetBoolean(true);
	var[1].SetString(msg);
	gfxHUD.Invoke("_root.api.showNoteRead", var, 2);
}

void HUDDisplay::eventNoteReportAbuse(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
{
	// ptumik: not sure what to do with this yet... need design

	//r3dMouse::Hide();
	//writeNoteSavedSlotIDFrom = 0;
	//timeoutForNotes = r3dGetTime() + 1.0f;

	timeoutNoteReadAbuseReportedHideUI = r3dGetTime() + 0.5f;
}

void HUDDisplay::showYouAreDead(const char* killedBy)
{
	if(!Inited) return;
	//r3dMouse::Show();

	gfxHUD.SetVariable("_root.Main.PlayerDead.DeadMsg.Text2.text", killedBy);
	gfxHUD.Invoke("_root.Main.PlayerDead.gotoAndPlay", "in");
}

void HUDDisplay::showSafeZoneWarning(bool flag)
{
	if(!Inited) return;

	if(SafeZoneWarningVisible != flag)
	{
		SafeZoneWarningVisible = flag;
		gfxHUD.Invoke("_root.Main.Condition.gotoAndPlay", flag ? 0 : 8); //"in":"out"
	}
}

namespace 
{
	const char* getReputationIconString(int Reputation)
	{
		const char* algnmt = "";
		if(Reputation >= ReputationPoints::Paragon)
			algnmt = "paragon";
		else if(Reputation >= ReputationPoints::Vigilante)
			algnmt = "vigilante";
		else if(Reputation >= ReputationPoints::Guardian)
			algnmt = "guardian";
		else if(Reputation >= ReputationPoints::Lawman)
			algnmt = "lawmen";
		else if(Reputation >= ReputationPoints::Deputy)
			algnmt = "deputy";	
		else if(Reputation >= ReputationPoints::Constable)
			algnmt = "constable";
		else if(Reputation >= ReputationPoints::Civilian)
			algnmt = "civilian";
		else if(Reputation <= ReputationPoints::Villain)
			algnmt = "villain";
		else if(Reputation <= ReputationPoints::Assassin)
			algnmt = "assassin";
		else if(Reputation <= ReputationPoints::Hitman)
			algnmt = "hitman";
		else if(Reputation <= ReputationPoints::Bandit)
			algnmt = "bandit";
		else if(Reputation <= ReputationPoints::Outlaw)
			algnmt = "outlaw";
		else if(Reputation <= ReputationPoints::Thug)
			algnmt = "thug";

		return algnmt;
	}
}
//ViruZ Group
void HUDDisplay::JoinGroup(const char* gamertag, bool isLegend)
{
	Scaleform::GFx::Value var[2];
	var[0].SetString(gamertag);
	var[1].SetBoolean(isLegend);
	gfxHUD.Invoke("_root.api.addPlayerToGroup", var , 2);
}

void HUDDisplay::addgrouplist(const char* name)
{
	Scaleform::GFx::Value var[2];
	var[0].SetString(name);
	var[1].SetBoolean(false);
	gfxHUD.Invoke("_root.api.addPlayerToGroup", var , 2);
}

void HUDDisplay::removegrouplist(const char* name)
{
	Scaleform::GFx::Value var[2];
	var[0].SetString(name);
	gfxHUD.Invoke("_root.api.removePlayerFromGroup", var , 1);
}

void HUDDisplay::addCharTag1(const char* name, int Reputation, bool isSameClan, Scaleform::GFx::Value& result)
{
	if(!Inited) return;
	r3d_assert(result.IsUndefined());

	Scaleform::GFx::Value vars[3];
	vars[0].SetString(name);
	vars[1].SetBoolean(isSameClan);
	vars[2].SetString(getReputationIconString(Reputation));
	gfxHUD.Invoke("_root.api.addCharTag", &result, vars, 3);
}

void HUDDisplay::addCharTag(const char* name, int Reputation, bool isSameClan, Scaleform::GFx::Value& result)
{
	if(!Inited) return;
	r3d_assert(result.IsUndefined());

	Scaleform::GFx::Value var[3];
	var[0].SetString(name);
	var[1].SetBoolean(isSameClan);
	var[2].SetString(getReputationIconString(Reputation));
	gfxHUD.Invoke("_root.api.addCharTag", &result, var, 3);
}


void HUDDisplay::removeUserIcon(Scaleform::GFx::Value& icon)
{
	if(!Inited) return;
	r3d_assert(!icon.IsUndefined());

	Scaleform::GFx::Value var[1];
	var[0] = icon;
	gfxHUD.Invoke("_root.api.removeUserIcon", var, 1);

	icon.SetUndefined();
}

// optimized version
void HUDDisplay::moveUserIcon(Scaleform::GFx::Value& icon, const r3dPoint3D& pos, bool alwaysShow, bool force_invisible /* = false */, bool pos_in_screen_space/* =false */)
{
	if(!Inited)
		return;
	r3d_assert(!icon.IsUndefined());

	r3dPoint3D scrCoord;
	float x, y;
	int isVisible = 1;
	if(!pos_in_screen_space)
	{
		if(alwaysShow)
			isVisible = r3dProjectToScreenAlways(pos, &scrCoord, 20, 20);
		else
			isVisible = r3dProjectToScreen(pos, &scrCoord);
	}
	else
		scrCoord = pos;

	// convert screens into UI space
	float mulX = 1920.0f/r3dRenderer->ScreenW;
	float mulY = 1080.0f/r3dRenderer->ScreenH;
	x = scrCoord.x * mulX;
	y = scrCoord.y * mulY;

	Scaleform::GFx::Value::DisplayInfo displayInfo;
	icon.GetDisplayInfo(&displayInfo);
	displayInfo.SetVisible(isVisible && !force_invisible);
	displayInfo.SetX(x);
	displayInfo.SetY(y);
	icon.SetDisplayInfo(displayInfo);
}

/*void HUDDisplay::setCharTagTextVisible(Scaleform::GFx::Value& icon, bool isShowName, bool isSameGroup)
{
	if(!Inited) return;
	r3d_assert(!icon.IsUndefined());

	Scaleform::GFx::Value var[4]; //tag visivel
	var[0] = icon;
	var[1].SetBoolean(isShowName);
	var[2].SetBoolean(isSameGroup);
	gfxHUD.Invoke("_root.api.setCharTagTextVisible", var, 4); //tag visivel
}*/

void HUDDisplay::setCharTagTextVisible1(Scaleform::GFx::Value& icon, bool isShowName, bool isSameGroup)
{
	if(!Inited) return;
	r3d_assert(!icon.IsUndefined());

	Scaleform::GFx::Value vars[4];
	vars[0] = icon;
	vars[1].SetBoolean(isShowName);
	vars[2].SetBoolean(isSameGroup);
	vars[3].SetBoolean(false);
	gfxHUD.Invoke("_root.api.setCharTagTextVisible", vars, 4);
}

//Detecçao de zombie
void HUDDisplay::setThreatValue(int value)
{
    Scaleform::GFx::Value vars[1];
    vars[0].SetInt(value);
    gfxHUD.Invoke("_root.api.setThreatValue", vars, 1);
}

//cooldown novo
void HUDDisplay::setCooldown(int slot,int CoolSecond,int value)
{
   Scaleform::GFx::Value vars[3];
   vars[0].SetInt(slot);
   vars[1].SetInt(CoolSecond);
   vars[2].SetInt(value);
   gfxHUD.Invoke("_root.api.setSlotCooldown", vars, 3);
}