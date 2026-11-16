#ifndef TEOPCE_APP_PATHS_H
#define TEOPCE_APP_PATHS_H

/* Shared compile-time roots for the historical layout and the 0.7.1 folder. */
#ifdef TEO_APP_ROOT
#ifdef TEO_METADATA_ROOT
#undef TEO_METADATA_ROOT
#endif
#define TEO_METADATA_ROOT TEO_APP_ROOT "/data"
#ifdef TEO_RESOURCE_ROOT
#undef TEO_RESOURCE_ROOT
#endif
#define TEO_RESOURCE_ROOT TEO_APP_ROOT "/data"
#ifdef TEO_USER_PARENT
#undef TEO_USER_PARENT
#endif
#define TEO_USER_PARENT TEO_APP_ROOT
#ifdef TEO_USER_ROOT
#undef TEO_USER_ROOT
#endif
#define TEO_USER_ROOT TEO_APP_ROOT "/user"
#ifdef TEO_CHEAT_ROOT
#undef TEO_CHEAT_ROOT
#endif
#define TEO_CHEAT_ROOT TEO_APP_ROOT "/cheats"
#ifdef TEO_LEGACY_CHEAT_ROOT
#undef TEO_LEGACY_CHEAT_ROOT
#endif
#define TEO_LEGACY_CHEAT_ROOT TEO_APP_ROOT "/cheats"
#else
#ifndef TEO_METADATA_ROOT
#define TEO_METADATA_ROOT "teo-menu3"
#endif
#ifndef TEO_RESOURCE_ROOT
#define TEO_RESOURCE_ROOT "teo-menu2"
#endif
#ifndef TEO_USER_PARENT
#define TEO_USER_PARENT "teo-menu2"
#endif
#ifndef TEO_USER_ROOT
#define TEO_USER_ROOT TEO_USER_PARENT "/user"
#endif
#ifndef TEO_CHEAT_ROOT
#define TEO_CHEAT_ROOT "teo-cheats1"
#endif
#ifndef TEO_LEGACY_CHEAT_ROOT
#define TEO_LEGACY_CHEAT_ROOT TEO_RESOURCE_ROOT "/cheats"
#endif
#endif

#define TEO_CHEAT_PROFILE_ROOT TEO_USER_ROOT "/cc"

#endif
