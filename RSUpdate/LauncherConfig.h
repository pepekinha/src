#pragma once

class CLauncherConfig
{
public:
	CLauncherConfig();

	std::string serialBuyURL;
	std::string serialExpiredBuyURL;

	std::string accountForgotPasswordURL;

	std::string myAccountURL;
	std::string forumsURL;
	std::string supportURL;
	std::string youtubeURL;
	std::string facebookURL;
	std::string twitterURL;

	std::string accountUnknownStatusMessage;
	std::string accountDeletedMessage;
	std::string accountBannedMessage;
	std::string accountFrozenMessage;

	std::string accountCreateFailedMessage;
	std::string accountCreateEmailTakenMessage;
	std::string accountCreateInvalidSerialMessage;

	std::string ToSURL;
	std::string EULAURL;

	std::string updateGameDataURL;
	std::string updateLauncherDataURL;
	std::string updateLauncherDataHostURL;

	std::string serverInfoURL;

	std::string webAPIDomainIP;
	std::string webAPIDomainBaseURL;
	int webAPIDomainPort;
	bool webAPIDomainUseSSL;
};

extern CLauncherConfig gLauncherConfig;