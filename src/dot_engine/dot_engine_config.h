#ifndef APPLICATION_CONFIG_H
#define APPLICATION_CONFIG_H

#define DOT_ENGINE_ASSET_PATH  "assets/"
#define DOT_RENDER_BACKEND_ONLY_VK
#define DOT_MAKE_VERSION(major, minor, patch) ((((u32)(major)) << 22U) | (((u32)(minor)) << 12U) | ((u32)(patch)))
enum { DOT_ENGINE_VERSION = DOT_MAKE_VERSION(0,0,0) };

#endif // !APPLICATION_CONFIG_H
