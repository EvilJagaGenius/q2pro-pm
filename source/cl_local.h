/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// client.h -- primary header for client

#include "com_local.h"
#include "q_list.h"
#include "files.h"
#include "sys_public.h"
#include "pmove.h"
#include "bsp.h"
#include "cmodel.h"
#include "protocol.h"
#include "q_msg.h"
#include "net_sock.h"
#include "net_chan.h"
#include "q_field.h"
#include "ref_public.h"
#include "key_public.h"
#include "snd_public.h"
#include "cl_public.h"
#include "ui_public.h"
#include "sv_public.h"
#include "io_sleep.h"
#if USE_ZLIB
#include <zlib.h>
#endif

//=============================================================================

typedef struct centity_s {
    entity_state_t    current;
    entity_state_t    prev;            // will always be valid, but might just be a copy of current

    vec3_t          mins, maxs;

    int             serverframe;        // if not current, this ent isn't in the frame

    int             trailcount;            // for diminishing grenade trails
    vec3_t          lerp_origin;        // for trails (variable hz)

    int             fly_stoptime;
} centity_t;

#define MAX_CLIENTWEAPONMODELS        20        // PGM -- upped from 16 to fit the chainfist vwep

typedef struct clientinfo_s {
    char name[MAX_QPATH];
    qhandle_t skin;
    qhandle_t icon;
    char model_name[MAX_QPATH];
    char skin_name[MAX_QPATH];
    qhandle_t model;
    qhandle_t weaponmodel[MAX_CLIENTWEAPONMODELS];
} clientinfo_t;

typedef struct {
    unsigned    sent;    // time sent, for calculating pings
    unsigned    rcvd;    // time rcvd, for calculating pings
    unsigned    cmdNumber;    // current cmdNumber for this frame
} client_history_t;

typedef struct {
    qboolean        valid;

    int             number;
    int             delta;

    byte            areabits[MAX_MAP_AREA_BYTES];
    int             areabytes;

    player_state_t  ps;
    int             clientNum;

    int             numEntities;
    int             firstEntity;
} server_frame_t;

typedef enum {
    LOAD_MAP,
    LOAD_MODELS,
    LOAD_IMAGES,
    LOAD_CLIENTS,
    LOAD_SOUNDS,
    LOAD_FINISH
} load_state_t;

//
// the client_state_t structure is wiped completely at every
// server map change
//
typedef struct client_state_s {
    int         timeoutcount;

    unsigned    lastTransmitTime;
    unsigned    lastTransmitCmdNumber;
    qboolean    sendPacketNow;

    usercmd_t    cmd;
    usercmd_t    cmds[CMD_BACKUP];    // each mesage will send several old cmds
    unsigned     cmdNumber;
    short        predicted_origins[CMD_BACKUP][3];    // for debug comparing against server
    client_history_t    history[CMD_BACKUP];
    int         initialSeq;

    float       predicted_step;                // for stair up smoothing
    unsigned    predicted_step_time;
    unsigned    predicted_step_frame;
    vec3_t      move;

    vec3_t      predicted_origin;    // generated by CL_PredictMovement
    vec3_t      predicted_angles;
    vec3_t      predicted_velocity;
    vec3_t      prediction_error;

    // rebuilt each valid frame
    centity_t       *solidEntities[MAX_PACKET_ENTITIES];
    int             numSolidEntities;

    entity_state_t  baselines[MAX_EDICTS];

    entity_state_t  entityStates[MAX_PARSE_ENTITIES];
    int             numEntityStates;

    msgEsFlags_t    esFlags;

    server_frame_t  frames[UPDATE_BACKUP];
    server_frame_t  frame;                // received from server
    server_frame_t  oldframe;
    int             servertime;
    int             serverdelta;
    frameflags_t    frameflags;

    int             lastframe;

    int             demoframe;
    int             demodelta;
    byte            dcs[CS_BITMAP_BYTES];

    // the client maintains its own idea of view angles, which are
    // sent to the server each frame.  It is cleared to 0 upon entering each level.
    // the server sends a delta each frame which is added to the locally
    // tracked view angles to account for standing on rotating objects,
    // and teleport direction changes
    vec3_t      viewangles;

#if USE_SMOOTH_DELTA_ANGLES
    short       delta_angles[3]; // interpolated
#endif

    int         time;            // this is the time value that the client
                                // is rendering at.  always <= cls.realtime
    float       lerpfrac;        // between oldframe and frame
    int         frametime;      // fixed server frame time
    float       framefrac;

    refdef_t    refdef;
    int         lightlevel;

    vec3_t      v_forward, v_right, v_up;    // set when refdef.angles is set

    qboolean    thirdPersonView;

    // predicted values, used for smooth player entity movement in thirdperson view
    vec3_t      playerEntityOrigin;
    vec3_t      playerEntityAngles;
    
    //
    // transient data from server
    //
    char        layout[MAX_STRING_CHARS];        // general 2D overlay
    int         inventory[MAX_ITEMS];
    qboolean    putaway;

    //
    // server state information
    //
    int         servercount;    // server identification for prespawns
    char        gamedir[MAX_QPATH];
    int         clientNum;            // never changed during gameplay, set by serverdata packet
    int         maxclients;
    pmoveParams_t pmp;

    char        configstrings[MAX_CONFIGSTRINGS][MAX_QPATH];
    char        mapname[MAX_QPATH]; // short format - q2dm1, etc

#if USE_AUTOREPLY
    unsigned    reply_time;
    unsigned    reply_delta;
#endif

    //
    // locally derived information from server state
    //
    bsp_t        *bsp;

    qhandle_t model_draw[MAX_MODELS];
    mmodel_t *model_clip[MAX_MODELS];

    qhandle_t sound_precache[MAX_SOUNDS];
    qhandle_t image_precache[MAX_IMAGES];

    clientinfo_t    clientinfo[MAX_CLIENTS];
    clientinfo_t    baseclientinfo;

    char    weaponModels[MAX_CLIENTWEAPONMODELS][MAX_QPATH];
    int     numWeaponModels;
} client_state_t;

extern    client_state_t    cl;

int CL_GetSoundInfo( vec3_t origin, vec3_t forward, vec3_t right, vec3_t up );

/*
==================================================================

the client_static_t structure is persistant through an arbitrary number
of server connections

==================================================================
*/

#define CONNECT_DELAY    3000

typedef enum {
    ca_uninitialized,
    ca_disconnected,    // not talking to a server
    ca_challenging,     // sending getchallenge packets to the server
    ca_connecting,      // sending connect packets to the server
    ca_connected,       // netchan_t established, waiting for svc_serverdata
    ca_loading,         // loading level data
    ca_precached,       // loaded level data, waiting for svc_frame
    ca_active           // game views should be displayed
} connstate_t;

typedef struct client_static_s {
    connstate_t state;
    keydest_t   key_dest;

    active_t    active;

    qboolean    ref_initialized;
    unsigned    disable_screen;
    qboolean    _unused1;

    int         userinfo_modified;
    cvar_t      *userinfo_updates[MAX_PACKET_USERINFOS];
// this is set each time a CVAR_USERINFO variable is changed
// so that the client knows to send it to the server

    int         framecount;
    unsigned    realtime;            // always increasing, no clamping, etc
    float       frametime;            // seconds since last frame

    unsigned    measureFramecount;
    unsigned    measureTime;
    int         fps, ping;

// connection information
    netadr_t    serverAddress;
    char        servername[MAX_OSPATH];    // name of server from original connect
    unsigned    connect_time;        // for connection retransmits
    int         connect_count;
#if USE_AC_CLIENT
    char        messageString[MAX_STRING_CHARS];
#endif
    qboolean    passive;

#if USE_ZLIB
    z_stream    z;
#endif

    int         quakePort;            // a 16 bit value that allows quake servers
                                    // to work around address translating routers
    netchan_t   *netchan;
    int         serverProtocol;        // in case we are doing some kind of version hack
    int         protocolVersion;    // minor version

    int         challenge;            // from the server to use for connecting

    struct {
        fileHandle_t    file;            // file transfer from server
        char            temp[MAX_QPATH+4]; // account 4 bytes for .tmp suffix
        char            name[MAX_QPATH];
        int             percent;
    } download;

// demo recording info must be here, so it isn't cleared on level change
    struct {
        fileHandle_t    playback;
        fileHandle_t    recording;
        unsigned        time_start;
        unsigned        time_frames;
        int             file_size;
        int             file_offset;
        int             file_percent;
        sizebuf_t       buffer;
        qboolean        paused;
    } demo;

#if USE_ICMP
    qboolean            errorReceived; // got an ICMP error from server
#endif
} client_static_t;

extern client_static_t    cls;

extern cmdbuf_t    cl_cmdbuf;
extern char        cl_cmdbuf_text[MAX_STRING_CHARS];

//=============================================================================

#define NOPART_GRENADE_EXPLOSION    1
#define NOPART_GRENADE_TRAIL        2
#define NOPART_ROCKET_EXPLOSION     4
#define NOPART_ROCKET_TRAIL         8
#define NOPART_BLOOD                16

#define NOEXP_GRENADE   1
#define NOEXP_ROCKET    2

//
// cvars
//
extern cvar_t    *cl_gun;
extern cvar_t    *cl_predict;
extern cvar_t    *cl_footsteps;
extern cvar_t    *cl_noskins;
extern cvar_t    *cl_kickangles;
extern cvar_t    *cl_rollhack;

#ifdef _DEBUG
#define SHOWNET(level,...) \
    if( cl_shownet->integer > level ) \
        Com_LPrintf( PRINT_DEVELOPER, __VA_ARGS__ )
#define SHOWCLAMP(level,...) \
    if( cl_showclamp->integer > level ) \
        Com_LPrintf( PRINT_DEVELOPER, __VA_ARGS__ )
#define SHOWMISS(...) \
    if( cl_showmiss->integer ) \
        Com_LPrintf( PRINT_DEVELOPER, __VA_ARGS__ )
extern cvar_t    *cl_shownet;
extern cvar_t    *cl_showmiss;
extern cvar_t    *cl_showclamp;
#else
#define SHOWNET(...)
#define SHOWCLAMP(...)
#define SHOWMISS(...)
#endif

extern cvar_t    *cl_vwep;

extern cvar_t    *cl_disable_particles;
extern cvar_t    *cl_disable_explosions;

extern cvar_t    *cl_chat_notify;
extern cvar_t    *cl_chat_sound;
extern cvar_t    *cl_chat_filter;

extern cvar_t    *cl_disconnectcmd;
extern cvar_t    *cl_changemapcmd;
extern cvar_t    *cl_beginmapcmd;

extern cvar_t    *cl_gibs;

extern cvar_t    *cl_thirdperson;
extern cvar_t    *cl_thirdperson_angle;
extern cvar_t    *cl_thirdperson_range;

extern cvar_t    *cl_async;

//
// userinfo
//
extern cvar_t    *info_password;
extern cvar_t    *info_spectator;
extern cvar_t    *info_name;
extern cvar_t    *info_skin;
extern cvar_t    *info_rate;
extern cvar_t    *info_fov;
extern cvar_t    *info_msg;
extern cvar_t    *info_hand;
extern cvar_t    *info_gender;
extern cvar_t    *info_uf;


typedef struct cdlight_s {
    int     key;                // so entities can reuse same entry
    vec3_t  color;
    vec3_t  origin;
    float   radius;
    float   die;                // stop lighting after this time
    float   decay;                // drop this each second
    float   minlight;            // don't add when contributing less
} cdlight_t;

extern centity_t    cl_entities[MAX_EDICTS];
extern cdlight_t    cl_dlights[MAX_DLIGHTS];


//=============================================================================


#ifdef _DEBUG
void CL_AddNetgraph (void);
#endif

//ROGUE
typedef struct cl_sustain_s {
    int     id;
    int     type;
    int     endtime;
    int     nextthink;
    int     thinkinterval;
    vec3_t  org;
    vec3_t  dir;
    int     color;
    int     count;
    int     magnitude;
    void    (*think)(struct cl_sustain_s *self);
} cl_sustain_t;

void CL_ParticleSteamEffect2(cl_sustain_t *self);

void CL_TeleporterParticles (vec3_t org);
void CL_ParticleEffect (vec3_t org, vec3_t dir, int color, int count);
void CL_ParticleEffect2 (vec3_t org, vec3_t dir, int color, int count);

// RAFAEL
void CL_ParticleEffect3 (vec3_t org, vec3_t dir, int color, int count);


//=================================================

// ========
// PGM
typedef struct cparticle_s{
    struct cparticle_s    *next;

    float   time;

    vec3_t  org;
    vec3_t  vel;
    vec3_t  accel;
    float   color;
    float   colorvel;
    float   alpha;
    float   alphavel;
    color_t rgb;
} cparticle_t;

cparticle_t *CL_AllocParticle( void );

#define PARTICLE_GRAVITY        40
#define BLASTER_PARTICLE_COLOR  0xe0
// PMM
#define INSTANT_PARTICLE    -10000.0
// PGM
// ========

void CL_InitEffects (void);
void CL_ClearEffects (void);
void CL_ClearTEnts (void);
void CL_BlasterTrail (vec3_t start, vec3_t end);
void CL_QuadTrail (vec3_t start, vec3_t end);
void CL_RailTrail (vec3_t start, vec3_t end);
void CL_BubbleTrail (vec3_t start, vec3_t end);
void CL_FlagTrail (vec3_t start, vec3_t end, float color);

// RAFAEL
void CL_IonripperTrail (vec3_t start, vec3_t end);

// ========
// PGM
void CL_BlasterParticles2 (vec3_t org, vec3_t dir, unsigned int color);
void CL_BlasterTrail2 (vec3_t start, vec3_t end);
void CL_DebugTrail (vec3_t start, vec3_t end);
void CL_SmokeTrail (vec3_t start, vec3_t end, int colorStart, int colorRun, int spacing);
void CL_Flashlight (int ent, vec3_t pos);
void CL_ForceWall (vec3_t start, vec3_t end, int color);
void CL_FlameEffects (centity_t *ent, vec3_t origin);
void CL_GenericParticleEffect (vec3_t org, vec3_t dir, int color, int count, int numcolors, int dirspread, float alphavel);
void CL_BubbleTrail2 (vec3_t start, vec3_t end, int dist);
void CL_Heatbeam (vec3_t start, vec3_t end);
void CL_ParticleSteamEffect (vec3_t org, vec3_t dir, int color, int count, int magnitude);
void CL_TrackerTrail (vec3_t start, vec3_t end, int particleColor);
void CL_Tracker_Explode(vec3_t origin);
void CL_TagTrail (vec3_t start, vec3_t end, float color);
void CL_ColorFlash (vec3_t pos, int ent, int intensity, float r, float g, float b);
void CL_Tracker_Shell(vec3_t origin);
void CL_MonsterPlasma_Shell(vec3_t origin);
void CL_ColorExplosionParticles (vec3_t org, int color, int run);
void CL_ParticleSmokeEffect (vec3_t org, vec3_t dir, int color, int count, int magnitude);
void CL_Widowbeamout (cl_sustain_t *self);
void CL_Nukeblast (cl_sustain_t *self);
void CL_WidowSplash (void);
// PGM
// ========


void CL_ParseMuzzleFlash (void);
void CL_ParseMuzzleFlash2 (void);
void SmokeAndFlash(vec3_t origin);

void CL_SetLightstyle( int index, const char *string, size_t length );

void CL_RunParticles (void);
void CL_RunDLights (void);
void CL_RunLightStyles (void);

void CL_AddEntities (void);
void CL_AddDLights (void);
void CL_AddTEnts (void);
void CL_AddLightStyles (void);

//=================================================

//
// cl_main
//

void CL_Init (void);
void CL_Quit_f (void);
void CL_Disconnect( error_type_t type, const char *text );
void CL_RequestNextDownload (void);
void CL_CheckForResend( void );
void CL_ClearState (void);
void CL_RestartFilesystem( qboolean total );
void CL_RestartRefresh( qboolean total );
void CL_ClientCommand( const char *string );
void CL_UpdateLocalFovSetting( void );
void CL_LoadState( load_state_t state );
void CL_SendRcon( const netadr_t *adr, const char *pass, const char *cmd );
const char *CL_Server_g( const char *partial, int argnum, int state );

// the sound code makes callbacks to the client for entitiy position
// information, so entities can be dynamically re-spatialized
void CL_GetEntitySoundOrigin( int ent, vec3_t org );


//
// cl_input
//
void IN_Init( void );
void IN_Shutdown( void );
void IN_Frame( void );
void IN_Activate( void );

void CL_RegisterInput( void );
void CL_UpdateCmd( int msec );
void CL_FinalizeCmd( void );
void CL_SendCmd( void );

//
// cl_parse.c
//

typedef struct {
    int type;
    vec3_t pos1;
    vec3_t pos2;
    vec3_t offset;
    vec3_t dir;
    int count;
    int color;
    int entity1;
    int entity2;
    int time;
} tent_params_t;

extern tent_params_t    te;

qboolean CL_CheckOrDownloadFile( const char *filename );
void CL_ParseServerMessage (void);
void CL_LoadClientinfo (clientinfo_t *ci, const char *s);
void CL_Download_f (void);
void CL_DeltaFrame( void );

//
// cl_view.c
//
extern    int       gun_frame;
extern    qhandle_t gun_model;

void V_Init( void );
void V_Shutdown( void );
void V_RenderView( void );
void V_AddEntity (entity_t *ent);
void V_AddParticle( particle_t *p );
void V_AddLight (vec3_t org, float intensity, float r, float g, float b);
void V_AddLightStyle (int style, vec4_t value);
void CL_PrepRefresh (void);

//
// cl_tent.c
//
void CL_RegisterTEntSounds (void);
void CL_RegisterTEntModels (void);
void CL_SmokeAndFlash(vec3_t origin);

#define LASER_FADE_NOT    1
#define LASER_FADE_ALPHA    2
#define LASER_FADE_RGBA        3

typedef struct laser_s {
    entity_t    ent;
    vec3_t      start;
    vec3_t      end;
    int         fadeType;
    qboolean    indexed;
    color_t     color;
    float       width;
    int         lifeTime;
    int         startTime;
} laser_t;

laser_t *CL_AllocLaser( void );

void CL_AddTEnt (void);


//
// cl_pred.c
//
void CL_PredictMovement (void);
void CL_CheckPredictionError (void);

//
// cl_fx.c
//
cdlight_t *CL_AllocDlight (int key);
void CL_BigTeleportParticles (vec3_t org);
void CL_RocketTrail (vec3_t start, vec3_t end, centity_t *old);
void CL_DiminishingTrail (vec3_t start, vec3_t end, centity_t *old, int flags);
void CL_FlyEffect (centity_t *ent, vec3_t origin);
void CL_BfgParticles (entity_t *ent);
void CL_AddParticles (void);
void CL_EntityEvent (int number);
// RAFAEL
void CL_TrapParticles (entity_t *ent);

//
// cl_demo.c
//
void CL_InitDemos( void );
void CL_DemoFrame( void );
void CL_EmitZeroFrame( void );
void CL_WriteDemoMessage( sizebuf_t *buf );
void CL_EmitDemoFrame( void ); 
void CL_Stop_f( void );
demoInfo_t *CL_GetDemoInfo( const char *path, demoInfo_t *info );


//
// cl_locs.c
//
void LOC_Init( void );
void LOC_LoadLocations( void );
void LOC_FreeLocations( void );
void LOC_UpdateCvars( void );
void LOC_AddLocationsToScene( void );


//
// cl_console.c
//
void Con_Init( void );
void Con_PostInit( void );
void Con_Shutdown( void );
void Con_DrawConsole( void );
void Con_RunConsole( void );
void Con_Print( const char *txt );
void Con_ClearNotify_f( void );
void Con_ToggleConsole_f (void);
void Con_ClearTyping( void );
void Con_Close( void );
void Con_SkipNotify( qboolean skip );
void Con_RegisterMedia( void );

void Key_Console( int key );
void Key_Message( int key );
void Char_Console( int key );
void Char_Message( int key );

//
// cl_ref.c
//
void    CL_InitRefresh( void );
void    CL_ShutdownRefresh( void );
void    CL_RunRefresh( void );

//
// cl_ui.c
//
void        CL_InitUI( void );
void        CL_ShutdownUI( void );

//
// cl_scrn.c
//
extern glconfig_t   scr_glconfig;
extern vrect_t      scr_vrect;        // position of render window

void    SCR_Init (void);
void    SCR_Shutdown( void );
void    SCR_UpdateScreen (void);
void    SCR_SizeUp( void );
void    SCR_SizeDown( void );
void    SCR_CenterPrint( const char *str );
void    SCR_BeginLoadingPlaque( void );
void    SCR_EndLoadingPlaque( void );
void    SCR_DebugGraph ( float value, int color );
void    SCR_TouchPics ( void );
void    SCR_RegisterMedia( void );
void    SCR_ModeChanged( void );
void    SCR_LagSample( void );
void    SCR_LagClear( void );

float   SCR_FadeAlpha( unsigned startTime, unsigned visTime, unsigned fadeTime );
int     SCR_DrawStringEx( int x, int y, int flags, size_t maxlen, const char *s, qhandle_t font );
void    SCR_DrawStringMulti( int x, int y, int flags, size_t maxlen, const char *s, qhandle_t font );


//
// cl_keys.c
//
void    Key_Init( void );
void    Key_Event( unsigned key, qboolean down, unsigned time );
void    Key_CharEvent( int key );
void    Key_WriteBindings( fileHandle_t f );


//
// cl_aastat.c
//
void CL_InitAscii( void ); 

