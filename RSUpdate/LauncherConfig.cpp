#include "r3dPCH.h"
#include "r3d.h"
#include "r3dNetwork.h"

#include "LauncherConfig.h"

CLauncherConfig gLauncherConfig;

CLauncherConfig::CLauncherConfig()
{
	const char* configFile = "Launcher.cfg";
	const char* group      = "Launcher";

	if(_access(configFile, 4) != 0) {
	r3dError("can't open config file %s", configFile);
	}

	serialBuyURL = r3dReadCFG_S(configFile, group, "serialBuyURL", "http://viruzmmo.com/buy?ref=WarL");
	serialExpiredBuyURL = r3dReadCFG_S(configFile, group, "serialExpiredBuyURL", "http://viruzmmo.com/buy?ref=WarE");

	accountForgotPasswordURL = r3dReadCFG_S(configFile, group, "accountForgotPasswordURL",
		"https://viruzmmo.com/account_check");

	myAccountURL = r3dReadCFG_S(configFile, group, "myAccountURL", "https://viruzmmo.com/account_check");
	forumsURL = r3dReadCFG_S(configFile, group, "forumsURL", "http://viruzmmo.com/forum");
	supportURL = r3dReadCFG_S(configFile, group, "supportURL", "http://viruzmmo.com/support");
	youtubeURL = r3dReadCFG_S(configFile, group, "youtubeURL", "http://www.youtube.com/viruzmmo");
	facebookURL = r3dReadCFG_S(configFile, group, "facebookURL", "http://www.facebook.com/viruzmmo");
	twitterURL = r3dReadCFG_S(configFile, group, "twitterURL", "http://twitter.com/viruzmmo");

	accountUnknownStatusMessage = r3dReadCFG_S(configFile, group, "accountUnknownStatusMessage",
		"Unknown account status, please contact support@viruzmmo.com");
	accountDeletedMessage = r3dReadCFG_S(configFile, group, "accountDeletedMessage",
		"Your account was deleted because your payment was refunded or cancelled\n\nPlease contact your payment provider");
	accountBannedMessage = r3dReadCFG_S(configFile, group, "accountBannedMessage",
		"Your account has been permanently banned");
	accountFrozenMessage = r3dReadCFG_S(configFile, group, "accountFrozenMessage",
		"Your account has been temporarily frozen because of violation of the Terms of Service ( Paragraph 2 )\n\nYou will be able to continue to use the service in %d hours");
  
	accountCreateFailedMessage = r3dReadCFG_S(configFile, group, "accountCreateFailedMessage",
		"Account creation failed, please try again later");
	accountCreateEmailTakenMessage = r3dReadCFG_S(configFile, group, "accountCreateEmailTakenMessage",
		"There is already registered account with that email!\nPlease note that you must use unique email per The War Z account");
	accountCreateInvalidSerialMessage = r3dReadCFG_S(configFile, group, "accountCreateInvalidSerialMessage",
		"Serial Key is not valid after Serial Key Check\ncontact support@viruzmmo.com");

	webAPIDomainIP = r3dReadCFG_S(configFile, group, "webAPIDomainIP", "198.50.173.42");
	webAPIDomainBaseURL = r3dReadCFG_S(configFile, group, "webAPIDomainBaseURL", "/WarZ/api/");
	webAPIDomainPort = r3dReadCFG_I(configFile, group, "webAPIDomainPort", 443);
	webAPIDomainUseSSL = r3dReadCFG_I(configFile, group, "webAPIDomainUseSSL", 1) ? true : false;

	ToSURL = r3dReadCFG_S(configFile, group, "tosURL", "http://198.50.173.42/EULA.rtf");
	EULAURL = r3dReadCFG_S(configFile, group, "eulaURL", "http://198.50.173.42/TOS.rtf");

	updateGameDataURL = r3dReadCFG_S(configFile, group, "updateGameDataURL",
		"https://198.50.173.42/wz/wz.xml");
	updateLauncherDataURL = r3dReadCFG_S(configFile, group, "updateLauncherDataURL",
		"https://198.50.173.42/wz/updater/woupd.xml");
	// updateLauncherDataHostURL Used by -generate cmdline arg to output a woupd.xml file.
	updateLauncherDataHostURL = r3dReadCFG_S(configFile, group, "updateLauncherDataHostURL",
		"https://198.50.173.42/wz/updater/");

	serverInfoURL = r3dReadCFG_S(configFile, group, "serverInfoURL",
		"http://198.50.173.42/api_getserverinfo.xml");
   
	#define CHECK_I(xx) if(xx == 0)  r3dError("missing %s value", #xx);
	#define CHECK_S(xx) if(xx == "") r3dError("missing %s value", #xx);
	CHECK_I(webAPIDomainPort);
	CHECK_S(webAPIDomainIP);
	CHECK_S(webAPIDomainBaseURL);

	CHECK_S(updateGameDataURL);
	CHECK_S(updateLauncherDataURL);
	CHECK_S(updateLauncherDataHostURL);

	CHECK_S(serverInfoURL);
	#undef CHECK_I
	#undef CHECK_S
 
	return;
}