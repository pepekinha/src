#pragma once

#pragma pack(push)
#pragma pack(1)

#define SBNET_MASTER_PORT	34000	// default port for master server
#define GBNET_CLIENT_PORT	34001	// default port for game browser master server (client requests)
#define SBNET_MASTER_WATCH_PORT	34005	// watch port for master server
#define SBNET_SUPER_WATCH_PORT	34006	// watch port for supervisor server
#define SBNET_GAME_PORT		34010

enum EGBGameRegion
{
	GBNET_REGION_Unknown   = 0,
	GBNET_REGION_US_West   = 1,
	GBNET_REGION_US_East   = 2,
	GBNET_REGION_Europe    = 10,
	GBNET_REGION_Russia    = 20,
};

// MAKE SURE to increase GBGAMEINFO_VERSION after changing following structs
#define GBGAMEINFO_VERSION 0x06092013

struct GBGameInfo
{
enum EMapId
    {
      MAPID_Editor_Particles = 0,
      MAPID_ServerTest = 4,
      MAPID_WZ_Colorado = 2,
      MAPID_WZ_Cliffside = 3,
	  MAPID_WZ_Colorado_pve = 5,
	  MAPID_WZ_Atlanta = 6,
	  MAPID_WZ_ViruZ_pvp = 7,
	  MAPID_WZ_Valley = 8,
	  
      // NOTE: do *NOT* add maps inside current IDs, add ONLY at the end
      // otherwise current map statistics in DB will be broken
      MAPID_MAX_ID,
    };

	char	name[16];
	char    pwdchar[512];
	bool	ispass;
	bool	isfarm;
	bool	ispre;
	BYTE	mapId;
	BYTE	maxPlayers;
	BYTE	flags;		// some game flags
	DWORD	gameServerId;	// unique server ID in our DB
	BYTE	region;		// game region
	
	GBGameInfo()
	{
	  sprintf(name, "g%08X", this);
	  mapId = 0xFF;
	  maxPlayers = 0;
	  flags = 0;
	  gameServerId = 0;
	  region = GBNET_REGION_Unknown;
	}
	
	bool IsValid() const
	{
	  if(mapId == 0xFF) return false;
	  if(maxPlayers == 0) return false;
	  if(gameServerId == 0) return false;
	  return true;
	}
	
	bool FromString(const char* arg) 
	{
	  int v[14];
	  int args = sscanf(arg, "%d %d %d %d %d", 
	    &v[0], &v[1], &v[2], &v[3], &v[4]);
	  if(args != 5) return false;
	  
	  mapId         = (BYTE)v[0];
	  maxPlayers    = (BYTE)v[1];
	  flags         = (BYTE)v[2];
	  gameServerId  = (DWORD)v[3];
	  region        = (BYTE)v[4];
	  return true;
	}
	
	void ToString(char* arg) const
	{
	  sprintf(arg, "%d %d %d %d %d", 
	    mapId,
	    maxPlayers,
	    flags,
	    gameServerId,
	    region);
	}
};

#pragma pack(pop)