#include "../qcommon/qcommon.h"

void NET_Init (void) {}
void NET_Shutdown (void) {}
void NET_Config (qboolean multiplayer) {}
qboolean NET_GetPacket (netsrc_t sock, netadr_t *net_from, sizebuf_t *net_message) { return false; }
void NET_SendPacket (netsrc_t sock, int length, void *data, netadr_t to) {}
qboolean NET_CompareAdr (netadr_t a, netadr_t b) { return false; }
qboolean NET_CompareBaseAdr (netadr_t a, netadr_t b) { return false; }
qboolean NET_IsLocalAddress (netadr_t adr) { return false; }
char *NET_AdrToString (netadr_t a) { return "null"; }
qboolean NET_StringToAdr (char *s, netadr_t *a) { return false; }
void NET_Sleep(int msec) {}