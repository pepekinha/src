#ifndef HUDDisplay_h
#define HUDDisplay_h

#include "r3d.h"
#include "APIScaleformGfx.h"
#include "../GameCode/UserProfile.h"
#include "../ObjectsCode/weapons/Weapon.h"

#define MAX_HUD_ACHIEVEMENT_QUEUE 8

class obj_Player;
class HUDDisplay
{
protected:
	bool Inited;
	bool isgroups; //ViruZ Groups
	bool chatVisible;
	bool chatInputActive;
	float chatVisibleUntilTime;
	float lastChatMessageSent;
	int	currentChatChannel;

	int playersListVisible;

	float bloodAlpha;

	int writeNoteSavedSlotIDFrom;
	float timeoutForNotes; // stupid UI design :(
	float timeoutNoteReadAbuseReportedHideUI;

	bool RangeFinderUIVisible;

	int weaponInfoVisible;
	bool SafeZoneWarningVisible;

public:
	r3dScaleformMovie gfxHUD;
	r3dScaleformMovie gfxBloodStreak;
	r3dScaleformMovie gfxRangeFinder;

	void	eventChatMessage(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);
	void	HUDDisplay::addplayertogroup(const char* gamertag,bool legend); //ViruZ Group
	void	HUDDisplay::removeplayerfromgroup(const char* gamertag,bool legend); //ViruZ Group
	void	eventNoteWritePost(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);
	void	eventNoteClosed(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);
	void	eventGraveNoteClosed(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);//Gravestone
	void	eventNoteReportAbuse(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);
	void	eventShowPlayerListContextMenu(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);
	void	eventPlayerListAction(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);

public:
	HUDDisplay();
	~HUDDisplay();

	bool	currentinvite; //ViruZ Group
	bool 	Init();
	bool 	Unload();

	int 	Update();
	int 	Draw();

	void	setBloodAlpha(float alpha);

	// HUD functions
	void	setVisibility(float percent); // [0,1]
	void	setHearing(float percent); // [0,1]
	void	setLifeParams(int food, int water, int health, int toxicity, int stamina);
	
	void	setWeaponInfo(int ammo, int clips, int firemode);
	void	showWeaponInfo(int state);
	void	setSlotInfo(int slotID, const char* name, int quantity, const char* icon);
	void	updateSlotInfo(int slotID, int quantity);
	void	showSlots(bool state);
	void	setActiveSlot(int slotID);
	void	setActivatedSlot(int slotID);
	void	showMessage(const wchar_t* text);

	void	showChat(bool showChat, bool force=false);
	void	showChatInput();
	void	addChatMessage(int tabIndex, const char* user, const char* text, uint32_t flags);
	void	JoinGroup(const char* gamertag, bool isLegend);
	bool	isChatInputActive() const { return chatInputActive || (r3dGetTime()-lastChatMessageSent)<0.25f || writeNoteSavedSlotIDFrom; }
	bool	isChatVisible() const { return chatVisible; }
	void	setChatTransparency(float alpha); //[0,1]
	void	setChatChannel(int index);
	void	enableClanChannel();
	void	enableGroupChannel(); //ViruZ Group
	
	//cooldown novo
	float Cooldown;
    int currentslot;

	// player list fn
	void	clearPlayersList();
	//void	addPlayerToList(int num, const char* name, int Reputation, bool isLegend, bool isDev, bool isPunisher, bool isInvitePending);
	void  addPlayerToList(int num, const char* name, int Reputation, bool isLegend, bool isDev, bool isPunisher, bool isInvitePending, bool isPremium);
	void	showPlayersList(int flag);
	int		isPlayersListVisible() const {return playersListVisible;}
	void	groupmenu(); // ViruZ Group
	// notes
	bool	canShowWriteNote() const { return r3dGetTime() > timeoutForNotes; }
	void	showWriteNote(int slotIDFrom);
	void	showReadNote(const char* msg);
	void  showGraveNote(const char* plr,const char* plr2);//Gravestone

	void	showRangeFinderUI(bool set) { RangeFinderUIVisible = set; }

	void	showYouAreDead(const char* killedBy);

	void	showSafeZoneWarning(bool flag);

	void	addCharTag(const char* name, int Reputation, bool isSameClan, Scaleform::GFx::Value& result);
	
	void	moveUserIcon(Scaleform::GFx::Value& icon, const r3dPoint3D& pos, bool alwaysShow, bool force_invisible = false, bool pos_in_screen_space=false); 
	// void	setCharTagTextVisible(Scaleform::GFx::Value& icon, bool isShowName, bool isSameGroup);
	void	removeUserIcon(Scaleform::GFx::Value& icon);
	void    setThreatValue(int value);	//Detecçao de zombie
	void	setCooldown(int slot,int CoolSecond,int value); // cooldown novo
	//ViruZ Group
	void	addgrouplist(const char* name);
	void	removegrouplist(const char* name);
	void	addCharTag1(const char* name, int Reputation, bool isSameClan, Scaleform::GFx::Value& result);
	void	setCharTagTextVisible1(Scaleform::GFx::Value& icon, bool isShowName, bool isSameGroup);
};

#endif