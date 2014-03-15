#include "r3dPCH.h"
#include "r3d.h"

#include "WOLoginHelper.h"
#include "WOBackendAPI.h"
#include "SteamHelper.h"

void CLoginHelper::DoLogin()
{
	SaveUserName();

	CWOBackendReq req("api_lobuginek.aspx");
	req.AddParam("username", username);
	req.AddParam("password", passwd);

	if(!req.Issue())
	{
		r3dOutToLog("Login FAILED, code: %d\n", req.resultCode_);
		loginAnswerCode = ANS_Error;
		return;
	}

	int n = sscanf(req.bodyStr_, "%d %d %d", 
		&CustomerID, 
		&SessionID,
		&AccountStatus);
	if(n != 3) {
		r3dError("Login: bad answer\n");
		return;
	}

	if(AccountStatus == 999) {
		loginAnswerCode = ANS_Deleted;
		return;
	}

	if(CustomerID == 0) {
		loginAnswerCode = ANS_BadPassword;
		return;
	}

	if(AccountStatus == 998) {
        loginAnswerCode = ANS_IPBANNED;
        return;
    }
	
	if(AccountStatus == 100)
		loginAnswerCode = ANS_Logged;
	else if(AccountStatus == 70)
		loginAnswerCode = ANS_GameActive;
	else if(AccountStatus == 200)
		loginAnswerCode = ANS_Banned;
	/*
	else if(AccountStatus == 400)
		loginAnswerCode = ANS_HardwareIDBanned;
	*/
	else if(AccountStatus == 201)
		loginAnswerCode = ANS_Frozen;
	else if(AccountStatus == 300)
		loginAnswerCode = ANS_TimeExpired;
	else
		loginAnswerCode = ANS_Unknown;
		
	return;
}

bool CLoginHelper::CheckSteamLogin()
{
	r3d_assert(gSteam.steamID);
	r3d_assert(gSteam.authToken.getSize() > 0);

	CkString ticket;
	gSteam.authToken.encode("hex", ticket);

	CWOBackendReq req("api_SteamLogin.aspx");
	req.AddParam("ticket", ticket.getAnsi());
	if(!req.Issue()) {
		r3dOutToLog("CheckSteamLogin: failed %d\n", req.resultCode_);
		return false;
	}

	int n = sscanf(req.bodyStr_, "%d %d %d", 
		&CustomerID, 
		&SessionID,
		&AccountStatus);
	if(n != 3) {
		r3dOutToLog("CheckSteamLogin: bad answer %s\n", req.bodyStr_);
		return false;
	}
	
	return true;
}

void CLoginHelper::SaveUserName()
{
	HKEY hKey;
	int hr;
	hr = RegCreateKeyEx(HKEY_CURRENT_USER, 
		"Software\\Arktos Entertainment Group\\WarZ", 
		0, 
		NULL,
		REG_OPTION_NON_VOLATILE, 
		KEY_ALL_ACCESS,
		NULL,
		&hKey,
		NULL);
	if(hr == ERROR_SUCCESS)
	{
		DWORD size = strlen(username) + 1;
		
		hr = RegSetValueEx(hKey, "username", NULL, REG_SZ, (BYTE*)username, size);
		RegCloseKey(hKey);
	}
}

bool CLoginHelper::LoadUserName()
{
	// query for game registry node
	HKEY hKey;
	int hr;
	hr = RegOpenKeyEx(HKEY_CURRENT_USER, 
		"Software\\Arktos Entertainment Group\\WarZ", 
		0, 
		KEY_ALL_ACCESS, 
		&hKey);
	if(hr != ERROR_SUCCESS)
		return true;

	DWORD size = sizeof(username);
	hr = RegQueryValueEx(hKey, "username", NULL, NULL, (BYTE*)username, &size);
	RegCloseKey(hKey);
	
	return true;
}

void CLoginHelper::CreateAuthToken(char* token) const
{
	char sessionInfo[512];
	sprintf(sessionInfo, "%d:%d:%d", CustomerID, SessionID, AccountStatus);

	for(size_t i=0; i<strlen(sessionInfo); ++i)
		sessionInfo[i] = sessionInfo[i] ^ 0x64;
        
	CkString encoded;
	encoded = sessionInfo;
	encoded.base64Encode("utf-8");
      
	strcpy(token, encoded.getAnsi());
	return;
}

void CLoginHelper::CreateLoginToken(char* token) const
{
	char crlogin[128];
	strcpy(crlogin, username);
	
	CkString s1;
	s1 = username;
	s1.base64Encode("utf-8");

	CkString s2;
	s2 = passwd;
	s2.base64Encode("utf-8");
      
	sprintf(token, "-login \"@%s\" -pwd \"@%s\"", s1.getAnsi(), s2.getAnsi());
	return;
}