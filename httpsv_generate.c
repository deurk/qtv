/*
   httpsv_generate.c
 */

#include "qtv.h"
#include "stdlib.h"

#define CRLF "\r\n"
#define HTMLFILESPATH "qtv"
#define NOTFOUNDLEVELSHOT "levelshots/_notfound.jpg"
#define HTMLPRINT(str) Net_ProxySend(cluster, dest, str, strlen(str))

// Join button display
cvar_t	allow_join 			= {"allow_join", 		"0"};

void Http_Init(void)
{
	Cvar_Register (&allow_join);
}

qbool IsPlayer(const playerinfo_t *p)
{
	char buffer[64];

	if (!p->userinfo[0])
		return false;

	return !atoi(Info_ValueForKey(p->userinfo, "*spectator", buffer, sizeof(buffer)));
}

qbool IsSpectator(const playerinfo_t* p)
{
	char buffer[64];

	if (!p->userinfo[0])
		return false;

	if (!*Info_ValueForKey(p->userinfo, "*spectator", buffer, sizeof(buffer)))
		return false;

	return atoi(buffer);
}

void HTTPSV_GenerateNotFoundError(cluster_t *cluster, oproxy_t *dest)
{
	HTTPSV_SendHTTPHeader(cluster, dest, "404", "", false);
}

void HTTPSV_GenerateCSSFile(cluster_t *cluster, oproxy_t *dest)
{
	int s;

	if (dest->buffer_file)
		Sys_Error("HTTPSV_GenerateCSSFile: dest->buffer_file");

	dest->buffer_file = FS_OpenFile(HTMLFILESPATH, "style.css", &s);
	if (!dest->buffer_file) {
		HTTPSV_GenerateNotFoundError(cluster, dest);
		return;
	}

	HTTPSV_SendHTTPHeader(cluster, dest, "200", "text/css", false);
}

void HTTPSV_GenerateJSFile(cluster_t *cluster, oproxy_t *dest)
{
	int s;

	if (dest->buffer_file)
		Sys_Error("HTTPSV_GenerateJSFile: dest->buffer_file");

	dest->buffer_file = FS_OpenFile(HTMLFILESPATH, "script.js", &s);
	if (!dest->buffer_file) {
		HTTPSV_GenerateNotFoundError(cluster, dest);
		return;
	}

	HTTPSV_SendHTTPHeader(cluster, dest, "200", "text/javascript", false);
}

/***** SCOREBOARD *****/

typedef struct  {

	int		frags;
	char	name[MAX_INFO_KEY];

} scoreboard_teaminfo;

typedef struct {

	int		frags;
	char	name[MAX_INFO_KEY];
	char	team[MAX_INFO_KEY];

} scoreboard_playerinfo;

typedef struct {

	int						players_count;
	int						teams_count;

	scoreboard_playerinfo	players[MAX_CLIENTS];
	scoreboard_teaminfo		teams[MAX_CLIENTS];

} scoreboard;

void ScoreBoard_Init(scoreboard *b)
{
	b->players_count = 0;
	b->teams_count   = 0;
}

void ScoreBoard_AddPlayer(scoreboard *b, playerinfo_t* p)
{
	int i;
	qbool teamfound = false;

	if (b->players_count < 0 || b->players_count >= MAX_CLIENTS)
		return;

	// add player data
	b->players[b->players_count].frags = p->frags;
	Info_ValueForKey(p->userinfo, "name", b->players[b->players_count].name, sizeof(b->players[0].name));
	Info_ValueForKey(p->userinfo, "team", b->players[b->players_count].team, sizeof(b->players[0].team));

	// add frags to team
	for (i = 0; i < b->teams_count; i++)
	{
		if (!strcmp(b->teams[i].name, b->players[b->players_count].team))
		{
			b->teams[i].frags += p->frags;
			teamfound = true;
			break;
		}
	}

	if (!teamfound && i < MAX_CLIENTS)
	{ // i == b->teams_count, new team found
		strlcpy(b->teams[i].name, b->players[b->players_count].team, sizeof(b->players[0].team));
		b->teams[i].frags = p->frags;
		b->teams_count++;
	}

	b->players_count++;
}

#define cmpresult(x,y) ((x) < (y) ? -1 : ((x) > (y) ? 1 : 0))

static int ScoreBoard_CompareTeams(const void *t1, const void *t2)
{
	int f1 = ((scoreboard_teaminfo *) t1)->frags;
	int f2 = ((scoreboard_teaminfo *) t2)->frags;
	return -cmpresult(f1, f2);
}

static int ScoreBoard_CompareTeamsTN(const void *t1, const void *t2)
{
	char* f1 = ((scoreboard_teaminfo *) t1)->name;
	char* f2 = ((scoreboard_teaminfo *) t2)->name;
	return strcmp(f1,f2);
}

static int ScoreBoard_ComparePlayers(const void *pv1, const void *pv2)
{
	int f1 = ((scoreboard_playerinfo *) pv1)->frags;
	int f2 = ((scoreboard_playerinfo *) pv2)->frags;
	return -cmpresult(f1, f2);
}

void ScoreBoard_FSort(scoreboard *b)
{ // frag-wise sorting
	qsort(b->teams,   b->teams_count,   sizeof(scoreboard_teaminfo),   ScoreBoard_CompareTeams);
	qsort(b->players, b->players_count, sizeof(scoreboard_playerinfo), ScoreBoard_ComparePlayers);
}

void ScoreBoard_TSort(scoreboard *b)
{ // teamname-wise sorting
	qsort(b->teams,   b->teams_count,   sizeof(scoreboard_teaminfo),   ScoreBoard_CompareTeamsTN);
	qsort(b->players, b->players_count, sizeof(scoreboard_playerinfo), ScoreBoard_ComparePlayers);
}

/**********/

// NOTE: team argument passed as NULL when we need ignore it, for example in duel mode
void HTTPSV_GenerateTableForTeam(cluster_t *cluster, oproxy_t *dest, scoreboard *b, char* team)
{
	int i, p;
	char buffer[128];

	HTMLPRINT("    <table class='scores' cellspacing='0'>\n      <tr><th>Frags</th><th>Players</th></tr>\n");

	for (i = 0, p = 0; i < b->players_count; i++)
	{
		if (team && strcmp(b->players[i].team, team))
			continue; // not on the same team?

		// row start
		if (p++ % 2)
			HTMLPRINT("      <tr class='scodd'>\n");
		else
			HTMLPRINT("      <tr>\n");

		// frags
		snprintf(buffer, sizeof(buffer), "        <td class='frags'>%i</td>\n", b->players[i].frags);
		Net_ProxySend(cluster, dest, buffer, strlen(buffer));

		// nick
		HTMLPRINT("        <td class='nick'>");
		HTMLprintf(buffer, sizeof(buffer), true, "%s", b->players[i].name);
		Net_ProxySend(cluster, dest, buffer, strlen(buffer));
		HTMLPRINT("</td>\n");
		// row end
		HTMLPRINT("      </tr>\n");
	}

	HTMLPRINT("    </table>\n");
}

void HTTPSV_GenerateScoreBoard(cluster_t *cluster, oproxy_t *dest, scoreboard *b, qbool teams)
{
	int i, t;
	char buffer[MAX_INFO_KEY];

	if (teams)
	{
		HTMLPRINT("    <table class='overallscores'>\n      <tr class='teaminfo'>\n");
		for (i = 0; i < b->teams_count; i++)
		{
			HTMLPRINT("        <td><span>Team: </span><span class='teamname'>");
			HTMLprintf(buffer, sizeof(buffer), true, "%s", b->teams[i].name);
			Net_ProxySend(cluster, dest, buffer, strlen(buffer));
			HTMLPRINT("</span><span class='frags'> ");
			HTMLprintf(buffer, sizeof(buffer), true, "[%i]", b->teams[i].frags);
			Net_ProxySend(cluster, dest, buffer, strlen(buffer));
			HTMLPRINT("</span></td>\n");
		}
		HTMLPRINT("</tr>\n      <tr>\n");
	}

	if (teams)
	{
		for (t = 0; t < b->teams_count; t++)
		{
			HTMLPRINT("        <td>");
			HTTPSV_GenerateTableForTeam(cluster, dest, b, b->teams[t].name);
			HTMLPRINT("</td>\n");
		}
	}
	else
	{
		HTTPSV_GenerateTableForTeam(cluster, dest, b, NULL); // NOTE: team argement sent as NULL
	}

	if (teams)
	{
		HTMLPRINT("      </tr>\n    </table>\n");
	}
}

void HTTPSV_GenerateNowPlaying(cluster_t *cluster, oproxy_t *dest)
{
	int player, spectators;
	char *s, *server;
	char buffer[1024];
	sv_t *streams;
	int oddrow = 0;
	qbool sv_empty, teamplay;
	char mapname[MAX_QPATH];
	scoreboard sboard;

	HTTPSV_SendHTTPHeader(cluster, dest, "200", "text/html", true);
	HTTPSV_SendHTMLHeader(cluster, dest, "QuakeTV: Now Playing");

	if (!strcmp(hostname.string, DEFAULT_HOSTNAME))
		snprintf(buffer, sizeof(buffer), "    <h1>QuakeTV: Now Playing</h1>\n");	//don't show the hostname if its set to the default
	else
		snprintf(buffer, sizeof(buffer), "    <h1>QuakeTV: Now Playing on %s</h1>\n", hostname.string);
	Net_ProxySend(cluster, dest, buffer, strlen(buffer));
	snprintf(buffer, sizeof(buffer), "    <h2>%s</h2>\n\n", hosttitle.string);
	Net_ProxySend(cluster, dest, buffer, strlen(buffer));

	HTMLPRINT("    <table id='nowplaying' cellspacing='0'>\n");
	for (streams = cluster->servers; streams; streams = streams->next)
	{
		spectators=0;

		// skip "tcp:" prefix if any
		server = (strncmp(streams->server, "tcp:", sizeof("tcp:") - 1) ? streams->server : streams->server + sizeof("tcp:") - 1);

		// get the name of the map
		// FIXME: is "maps/notready.bsp" is ok to show when we are not ready?
		strlcpy(mapname, streams->modellist[1].name[0] ? streams->modellist[1].name : "maps/notready.bsp", sizeof(mapname));
		FS_StripPathAndExtension(mapname);

		// is the server empty? (except spectators)
		sv_empty = 1;
		ScoreBoard_Init(&sboard);

		for (player = 0; player < MAX_CLIENTS; player++)
		{
			if (IsPlayer(streams->players + player))
			{
				sv_empty = 0;
				ScoreBoard_AddPlayer(&sboard, streams->players + player);
			} else if ((IsSpectator(streams->players + player)) && //count all spectators except serveme
					(strcmp("lqwc",Info_ValueForKey((streams->players+player)->userinfo, "team", buffer, sizeof(buffer)))))
				spectators++;
		}

		teamplay = false;
		if (sboard.players_count > 2)
			teamplay = atoi(Info_ValueForKey(streams->serverinfo, "teamplay", buffer, sizeof(buffer)));

		if (!sv_empty)
		{
			if (teamplay)
				ScoreBoard_TSort(&sboard);
			else
				ScoreBoard_FSort(&sboard);
		}

		// table row
		snprintf(buffer, sizeof(buffer), "      <tr class='%s%s'>\n", (oddrow ? "odd" : ""), (sv_empty ? "" : " notempty netop"));
		Net_ProxySend(cluster, dest, buffer, strlen(buffer));
		oddrow = !oddrow;


		// 1st cell: watch now button
		if (!allow_join.integer) {
			snprintf(buffer, sizeof(buffer), "        <td class='wn'><span class=\"qtvfile\"><a href=\"qw://%i@%s:28000/qtvplay\">Watch&nbsp;now!</a></span></td>\n", streams->streamid, hostname.string);
			Net_ProxySend(cluster, dest, buffer, strlen(buffer));
		} else {
			snprintf(buffer, sizeof(buffer), "        <td class='wn'><span class=\"qtvfile\"><a href=\"qw://%i@n%s:28000/qtvplay\">Watch&nbsp;now!</a></span></td>\n", streams->streamid, hostname.string);
			Net_ProxySend(cluster, dest, buffer, strlen(buffer));
		}

		char hostname[1024];

		// 2nd cell: server adress
		HTMLPRINT("        <td class='adr'><p class='hostname' style='display:none'>");
		HTMLPRINT(Info_ValueForKey(streams->serverinfo, "hostname", hostname, sizeof(hostname)));
		HTMLPRINT("</p>");

		HTMLprintf(buffer, sizeof(buffer), true, "%s", server);
		Net_ProxySend(cluster, dest, buffer, strlen(buffer));
		HTMLPRINT("</td>\n");

		// 3rd cell: map name
		if (streams->qstate < qs_active)
		{
			HTMLPRINT("        <td class='mn'>NOT CONNECTED</td>\n");
		}
		else if (!strcmp(streams->gamedir, "qw"))
		{
			HTMLPRINT("        <td class='mn'>");
			Net_ProxySend(cluster, dest, "<span>", sizeof("<span>")-1);
			HTMLprintf(buffer, sizeof(buffer), true, "%s", streams->mapname);
			Net_ProxySend(cluster, dest, buffer, strlen(buffer));			
			HTMLPRINT("</span>");
			HTMLprintf(buffer, sizeof(buffer), true, " (%s)", mapname);
			Net_ProxySend(cluster, dest, buffer, strlen(buffer));
			HTMLPRINT("</td>\n");
		}
		else
		{
			HTMLPRINT("        <td class='mn'>");
			HTMLPRINT("<span>");
			HTMLprintf(buffer, sizeof(buffer), true, "%s", streams->mapname);
			Net_ProxySend(cluster, dest, buffer, strlen(buffer));			
			HTMLPRINT("</span>");
			HTMLprintf(buffer, sizeof(buffer), true, "(%s: ", streams->gamedir);
			Net_ProxySend(cluster, dest, buffer, strlen(buffer));
			HTMLprintf(buffer, sizeof(buffer), true, "%s)", mapname);
			Net_ProxySend(cluster, dest, buffer, strlen(buffer));
			HTMLPRINT("</td>\n");
		}

		HTMLPRINT("      </tr>\n");
		// end of row

		// details if server not empty
		if (!sv_empty) 
		{
			char buf[32], matchtag[32];
			HTMLPRINT("      <tr class='notempty nebottom'>\n");

			// map preview
			snprintf(buffer, sizeof(buffer), "        <td class='mappic'><img src='/levelshots/%s.jpg' width='144' height='108' alt='%s' title='%s' /></td>\n", mapname, mapname, mapname);
			Net_ProxySend(cluster, dest, buffer, strlen(buffer));

			// scores table
			HTMLPRINT("        <td class='svstatus' colspan='2'>\n");			
			HTTPSV_GenerateScoreBoard(cluster, dest, &sboard, teamplay);

			// (match) tag and status
			Info_ValueForKey(streams->serverinfo, "matchtag", matchtag, sizeof(matchtag));
			Info_ValueForKey(streams->serverinfo, "status", buf, sizeof(buf));
			if (matchtag[0] || buf[0])
			{
				snprintf(buffer,sizeof(buffer), "          <p class='status'>%s%s%s</p>\n",
						matchtag, (matchtag[0] && buf[0]) ? ": " : "", buf);
				Net_ProxySend(cluster, dest, buffer, strlen(buffer));
			}

			HTMLPRINT("<table class='meta' cols=2><tr class='metatr'><td class='metatd'>");

			//number of spectators	
			snprintf(buffer,sizeof(buffer), "<p class='spectators'>Spectators: <span>%u</span></p>\n", spectators);
			Net_ProxySend(cluster, dest, buffer, strlen(buffer));

			HTMLPRINT("</td><td class='metatd'>");

			// number of observers
			snprintf(buffer,sizeof(buffer), "<p class='observers'>Observers: <span>%u</span></p>\n", Proxy_UsersCount(streams));
			Net_ProxySend(cluster, dest, buffer, strlen(buffer));

			HTMLPRINT("</td></tr></table>");

			HTMLPRINT("</td>\n</tr>\n");
		}
	}
	HTMLPRINT("    </table>\n");

	if (!cluster->servers)
	{
		s = "    <p>No streams are currently being played</p>\n";
		Net_ProxySend(cluster, dest, s, strlen(s));
	}

	HTTPSV_SendHTMLFooter(cluster, dest);
}

void HTTPSV_GenerateQTVStub(cluster_t *cluster, oproxy_t *dest, char *streamtype, char *streamid)
{
	char hostname[64];
	char buffer[1024];
	char unescaped_streamid[512];

	HTTPSV_UnescapeURL(streamid, unescaped_streamid, sizeof(unescaped_streamid));

	// Get the hostname from the header.
	if (!HTTPSV_GetHostname(cluster, dest, hostname, sizeof(hostname)))
	{
		return;
	}

	HTTPSV_SendHTTPHeader(cluster, dest, "200", "application/octet-stream", false);

	snprintf(buffer, sizeof(buffer), "[QTV]\r\n"
			"Stream: %s%s@%s\r\n"
			"", 
			streamtype, unescaped_streamid, hostname);

	Net_ProxySend(cluster, dest, buffer, strlen(buffer));
}

void HTTPSV_GenerateQTVJoinStub(cluster_t *cluster, oproxy_t *dest, char *streamid)
{
	qbool streamfound = false;
	char *server;
	char buffer[1024];
	sv_t *streams;
	char unescaped_streamid[512];

	HTTPSV_UnescapeURL(streamid, unescaped_streamid, sizeof(unescaped_streamid));

	server = NULL;
	// get server address -- deurk: any better way to access it?
	for (streams = cluster->servers; streams; streams = streams->next)
	{
		server = (strncmp(streams->server, "tcp:", sizeof("tcp:") - 1) ? streams->server : streams->server + sizeof("tcp:") - 1);
		if ((unsigned int) atoi(streamid) == streams->streamid) {
			streamfound = true;
			break;
		}
	}

	if (!streamfound)
		return;

	HTTPSV_SendHTTPHeader(cluster, dest, "200", "application/octet-stream", false);

	snprintf(buffer, sizeof(buffer), "[QTV]\r\n"
			"Join: %s\r\n"
			"", 
			server);

	Net_ProxySend(cluster, dest, buffer, strlen(buffer));
}

void HTTPSV_GenerateAdmin(cluster_t *cluster, oproxy_t *dest, int streamid, char *postbody)
{
	char pwd[64]      = {0};
	char cmd[256]     = {0};
	char result[8192] = {0};
	char *s = "";
	char *o = "";
	int passwordokay = false;

	if (!admin_password.string[0])
	{
		HTTPSV_SendHTTPHeader(cluster, dest, "403", "text/html", true);
		HTTPSV_SendHTMLHeader(cluster, dest, "QuakeTV: Admin Error");

		s = "    <p>The admin password is disabled. You may not log in remotely.</p>\n  </body>\n\n</html>\n";
		Net_ProxySend(cluster, dest, s, strlen(s));
		return;
	}

	pwd[0] = 0;
	cmd[0] = 0;

	if (postbody)
		while (*postbody)
		{
			if (!strncmp(postbody, "pwd=", 4))
			{
				postbody = HTTPSV_ParsePOST(postbody+4, pwd, sizeof(pwd));
			}
			else if (!strncmp(postbody, "cmd=", 4))
			{
				postbody = HTTPSV_ParsePOST(postbody+4, cmd, sizeof(cmd));
			}
			else
			{
				while(*postbody && *postbody != '&')
				{
					postbody++;
				}
				if (*postbody == '&')
					postbody++;
			}
		}

	if (!*pwd)
	{
		if (postbody)
			o = "No Password";
		else
			o = "";
	}
	else if (!strcmp(pwd, admin_password.string))
	{
		passwordokay = true;
		//small hack (as http connections are considered non-connected proxies)
		cluster->numproxies--;
		if (*cmd)
			o = Cmd_RconCommand(cmd, result, sizeof(result));
		else
			o = "";
		cluster->numproxies++;
	}
	else
	{
		o = "Bad Password";
	}

	if (o != result)
	{
		strlcpy(result, o, sizeof(result));
		o = result;
	}

	HTTPSV_SendHTTPHeader(cluster, dest, "200", "text/html", true);
	HTTPSV_SendHTMLHeader(cluster, dest, "QuakeTV: Admin");

	s = "    <h1>QuakeTV Admin: ";
	Net_ProxySend(cluster, dest, s, strlen(s));
	s = hostname.string;
	Net_ProxySend(cluster, dest, s, strlen(s));
	s = "</h1>\n\n";
	Net_ProxySend(cluster, dest, s, strlen(s));


	s =	"    <form action=\"admin.html\" method=\"post\" name='f'>\n"
		"      <center>Password <input name='pwd' type='password' value=\"";

	Net_ProxySend(cluster, dest, s, strlen(s));
	/*
	   if (passwordokay)
	   Net_ProxySend(cluster, dest, pwd, strlen(pwd));
	 */

	s =	"\" />"
		"<br />"
		"Command <input name='cmd' maxlength='255' size='40' value=\"\" />"
		"<input type='submit' value=\"Submit\" name='btn' />"
		"</center>\n"
		"    </form>\n";
	Net_ProxySend(cluster, dest, s, strlen(s));

	if (passwordokay)
		HTMLPRINT("    <script type='text/javascript'>document.forms[0].elements[1].focus();</script>\n");
	else
		HTMLPRINT("    <script type='text/javascript'>document.forms[0].elements[0].focus();</script>\n");

	while(*o)
	{
		s = strchr(o, '\n');
		if (s)
			*s = 0;
		HTMLprintf(cmd, sizeof(cmd), true, "%s", o);
		Net_ProxySend(cluster, dest, cmd, strlen(cmd));
		Net_ProxySend(cluster, dest, "<br />", 6);
		if (!s)
			break;
		o = s+1;
	}

	HTTPSV_SendHTMLFooter(cluster, dest);
}

void HTTPSV_GenerateDemoListing(cluster_t *cluster, oproxy_t *dest)
{
	int i;
	char link[1024],
	name[sizeof(cluster->availdemos[0].name) * 5],
	href[sizeof(cluster->availdemos[0].name) * 5];
	char *s;

	dest->_bufferautoadjustmaxsize_ = 1024 * 1024; // NOTE: this allow 1MB buffer...

	HTTPSV_SendHTTPHeader(cluster, dest, "200", "text/html", true);
	HTTPSV_SendHTMLHeader(cluster, dest, "QuakeTV: Demos");

	s = "    <h1>QuakeTV: Demo Listing</h1>\n\n";
	Net_ProxySend(cluster, dest, s, strlen(s));

	s = "    <table id='demos' cellspacing='0'>\n      <thead>\n        <tr>\n"
		"          <th class='stream'>stream</th>\n"
		"          <th class='save'>Download</th>\n"
		"          <th class='name'>Demoname</th>\n"
		"          <th class='size'>Size</th>\n        </tr>\n      </thead>\n      <tbody>\n";
	Net_ProxySend(cluster, dest, s, strlen(s));

	Cluster_BuildAvailableDemoList(cluster);
	for (i = 0; i < cluster->availdemoscount; i++)
	{
		// URL escaping (inside <a href='...'>)
		HTTPSV_EscapeURL(cluster->availdemos[i].name, href, sizeof(href));
		// HTML escaping (in HTML text)
		HTMLprintf(name, sizeof(name), false, "%s", cluster->availdemos[i].name);

		snprintf(link, sizeof(link), "        <tr class='%s'>\n          <td class='stream'>", ((i % 2) ? "even" : "odd"));
		Net_ProxySend(cluster, dest, link, strlen(link));

		if (stricmp(name + strlen(name) - 4, ".mvd") == 0) {
			snprintf(link, sizeof(link),
					"<a href='/watch.qtv?demo=%s'><img src='/stream.png' width='14' height='15' /></a>",
					href);
			Net_ProxySend(cluster, dest, link, strlen(link));
		}

		snprintf(link, sizeof(link), "</td>\n"
				"          <td class='save'><a href='/dl/demos/%s'><img src='/save.png' width='16' height='16' /></a></td>\n"
				"          <td class='name'>%s</td><td class='size'>%i kB</td>\n"
				"        </tr>\n",
				href, name, cluster->availdemos[i].size/1024);
		Net_ProxySend(cluster, dest, link, strlen(link));
	}
	s = "      </tbody>\n    </table>\n";
	Net_ProxySend(cluster, dest, s, strlen(s));

	snprintf(link, sizeof(link), "\n    <p>Total: %i demos</p>\n", cluster->availdemoscount);
	Net_ProxySend(cluster, dest, link, strlen(link));

	HTTPSV_SendHTMLFooter(cluster, dest);
}

void HTTPSV_GenerateImage(cluster_t *cluster, oproxy_t *dest, char *imgfilename)
{
	int s;

	if (dest->buffer_file)
		Sys_Error("HTTPSV_GenerateImage: dest->buffer_file");

	dest->buffer_file = FS_OpenFile(HTMLFILESPATH, imgfilename, &s);
	if (!dest->buffer_file) 
	{
		HTTPSV_GenerateNotFoundError(cluster, dest);
		return;
	}

	HTTPSV_SendHTTPHeader(cluster, dest, "200", "image/png", false);
}

//========================================================================
// LEVELSHOTS
//========================================================================

static void MediaPathName(char *buf, size_t bufsize, char *name, char *mediadir) 
{
	char pathname[256];

	buf[0] = 0;

	// parse requested file, its string before first space
	COM_ParseToken(name, pathname, sizeof(pathname), " ");

	snprintf(buf, bufsize, "%s/%s", mediadir, pathname);
}

void HTTPSV_GenerateLevelshot(cluster_t *cluster, oproxy_t *dest, char *name)
{
	char pathname[256];
	int s;

	if (dest->buffer_file)
		Sys_Error("HTTPSV_GenerateLevelshot: dest->buffer_file");

	MediaPathName(pathname, sizeof(pathname), name, "levelshots");

	dest->buffer_file = FS_OpenFile(HTMLFILESPATH, pathname, &s);
	if (!dest->buffer_file) 
	{
		dest->buffer_file = FS_OpenFile(HTMLFILESPATH, NOTFOUNDLEVELSHOT, &s);
		if (!dest->buffer_file)
		{
			HTTPSV_GenerateNotFoundError(cluster, dest);
			return;
		}
	}

	HTTPSV_SendHTTPHeader(cluster, dest, "200", "image/jpeg", false);
}

//========================================================================
// DEMO DOWNLOAD
//========================================================================

void HTTPSV_GenerateDemoDownload(cluster_t *cluster, oproxy_t *dest, char *name)
{
	char pathname[256];
	char unescaped_name[1024];
	qbool valid;
	int ext, size;

	HTTPSV_UnescapeURL(name, unescaped_name, sizeof(unescaped_name));

	if (dest->buffer_file)
		Sys_Error("HTTPSV_GenerateDemoDownload: dest->buffer_file");

	MediaPathName(pathname, sizeof(pathname), unescaped_name, DEMO_DIR);

	valid = false;
	for (ext = 0; ext < demos_allowed_ext_count; ext++)
	{
		if (stricmp(demos_allowed_ext[ext], FS_FileExtension(pathname)) == 0)
		{
			valid = true;
		}
	}

	if (!valid)
	{
		HTTPSV_GenerateNotFoundError(cluster, dest);
		return;
	}

	dest->buffer_file = FS_OpenFile(NULL, pathname, &size);
	if (!dest->buffer_file) 
	{
		HTTPSV_GenerateNotFoundError(cluster, dest);
		return;
	}

	HTTPSV_SendHTTPHeader(cluster, dest, "200", "application/octet-stream", false);
}

//========================================================================
// RSS
//========================================================================

void HTTPSV_GenerateRSS(cluster_t *cluster, oproxy_t *dest, char *str)
{
	char *header_fmt = NULL;
	char *footer_fmt = NULL;
	char *item_fmt = NULL;
	char *server = NULL;
	char *link_fmt = NULL;
	char link[1024];
	char *s = NULL;
	sv_t *streams;
	char playerlist[1024];
	char playername[32];
	size_t item_len = 0;
	char hostname[1024];
	char tmp[2048];
	char tmp2[2048];
	char mapname[MAX_QPATH];
	int player;

	// Get the hostname from the header.
	if (!HTTPSV_GetHostname(cluster, dest, hostname, sizeof(hostname)))
	{
		return;
	}

	HTTPSV_SendHTTPHeader(cluster, dest, "200", "application/rss+xml", false);

	header_fmt = 
		"<?xml version=\"1.0\"?>" CRLF
		"<rss version=\"2.0\">" CRLF
		"<channel>" CRLF
		"<title>QTV RSS</title>" CRLF
		"<link></link>" CRLF // TODO: Set channel link for RSS.
		"<description>%s - QTV Current active feeds</description>" CRLF
		"<language>en-us</language>" CRLF
		"<pubDate></pubDate>" CRLF; // TODO: Set date when stream started?

	footer_fmt =
		"</channel>" CRLF
		"</rss>" CRLF;

	link_fmt = "http://%s/watch.qtv?sid=%i";

	// Estimate the size of the item buffer.
	item_len = strlen(link_fmt) + strlen(hostname) + 2048;
	s = Sys_malloc(item_len);

	// Send RSS header.
	snprintf(link, sizeof(link), header_fmt, hostname);
	Net_ProxySend(cluster, dest, link, strlen(link));

	// Send RSS items.
	for (streams = cluster->servers; streams; streams = streams->next)
	{
		HTMLPRINT("<item>");

		playerlist[0] = 0;

		// Output the playerlist (this will be shown in the description field of the RSS item).
		for (player = 0; player < MAX_CLIENTS; player++)
		{
			qbool isSpectator = false;

			if (IsPlayer(&streams->players[player])) {
				HTMLPRINT("<player>");
			}
			else if ((isSpectator = IsSpectator(&streams->players[player]))) {
				HTMLPRINT("<spectator>");
			}
			else {
				continue;
			}

			Info_ValueForKey(streams->players[player].userinfo, "name", tmp, sizeof(tmp));
			HTMLprintf(playername, sizeof(playername), true, "%s", tmp);

			// Save a player list for the description also.
			if (!isSpectator) {
				strlcat(playerlist, playername, sizeof(playerlist));
				strlcat(playerlist, CRLF, sizeof(playerlist));
			}

			HTMLPRINT("<name>");
			HTMLPRINT(playername);
			HTMLPRINT("</name>" CRLF);

			if (!isSpectator) {
				HTMLPRINT("<team>");
				Info_ValueForKey(streams->players[player].userinfo, "team", tmp, sizeof(tmp));
				HTMLprintf(tmp2, sizeof(tmp2), true, "%s", tmp);
				HTMLPRINT(tmp2);
				HTMLPRINT("</team>" CRLF);

				HTMLPRINT("<frags>");
				HTMLprintf(tmp, sizeof(tmp), true, "%i", streams->players[player].frags);
				HTMLPRINT(tmp);
				HTMLPRINT("</frags>" CRLF);
			}

			HTMLPRINT("<ping>");
			HTMLprintf(tmp, sizeof(tmp), true, "%i", streams->players[player].ping);
			HTMLPRINT(tmp);
			HTMLPRINT("</ping>" CRLF);

			HTMLPRINT("<pl>");
			HTMLprintf(tmp, sizeof(tmp), true, "%i", streams->players[player].packetloss);
			HTMLPRINT(tmp);
			HTMLPRINT("</pl>" CRLF);

			if (!isSpectator) {
				HTMLPRINT("<topcolor>");
				Info_ValueForKey(streams->players[player].userinfo, "topcolor", tmp, sizeof(tmp));
				HTMLprintf(tmp2, sizeof(tmp2), true, "%s", tmp);
				HTMLPRINT(tmp2);
				HTMLPRINT("</topcolor>" CRLF);

				HTMLPRINT("<bottomcolor>");
				Info_ValueForKey(streams->players[player].userinfo, "bottomcolor", tmp, sizeof(tmp));
				HTMLprintf(tmp2, sizeof(tmp2), true, "%s", tmp);
				HTMLPRINT(tmp2);
				HTMLPRINT("</bottomcolor>" CRLF);
			}

			if (!isSpectator) {
				HTMLPRINT("</player>" CRLF);
			}
			else {
				HTMLPRINT("</spectator>" CRLF);
			}
		}

		// Skip "tcp:" prefix if any.
		server = (strncmp(streams->server, "tcp:", sizeof("tcp:") - 1) ? streams->server : streams->server + sizeof("tcp:") - 1);

		// Set the url to the stream.
		snprintf(link, sizeof(link), link_fmt, hostname, streams->streamid);

		strlcpy(mapname, streams->modellist[1].name[0] ? streams->modellist[1].name : "maps/notready.bsp", sizeof(mapname));
		FS_StripPathAndExtension(mapname);

		item_fmt = CRLF
			"<type>text/plain</type>" CRLF
			"<title>%s</title>" CRLF 
			"<link>%s</link>" CRLF
			"<description>%s</description>" CRLF
			"<pubDate>%s</pubDate>" CRLF
			"<guid>%s</guid>" CRLF
			"<hostname>%s</hostname>" CRLF
			"<port>%i</port>" CRLF
			"<map>%s</map>" CRLF
			"<observercount>%u</observercount>" CRLF
			"<status>%s</status>" CRLF;

		{
			// Separate the hostname and port.
			char *t = NULL;
			int port = 27500;
			char statusbuf[MAX_INFO_KEY];

			snprintf(tmp, sizeof(tmp), "%s", server);
			t = tmp;
			while (*t && (*t != ':')) ++t;
			*t = 0;
			port = atoi(++t);
			port = (port == 0) ? 27500 : port;
			Info_ValueForKey(streams->serverinfo, "status", statusbuf, sizeof(statusbuf));

			snprintf(s, item_len, item_fmt, server, link, playerlist, "", "", tmp, port, mapname, Proxy_UsersCount(streams), statusbuf);
			HTMLPRINT(s);
		}

		{
			oproxy_t *p;

			HTMLPRINT("<observers>" CRLF);
			for (p = streams->proxies; p; p = p->next) {
				if (p->drop)
					continue;

				tmp[0] = '\0';
				Info_Get(&p->ctx, "name", tmp, sizeof(tmp));
				if (tmp[0]) {
					HTMLprintf(playername, sizeof(playername), true, "%s", tmp);

					HTMLPRINT("<o>");
					HTMLPRINT(playername);
					HTMLPRINT("</o>" CRLF);
				}
			}
			HTMLPRINT(CRLF "</observers>" CRLF);
		}

		Info_ValueForKey(streams->players[player].userinfo, "name", tmp, sizeof(tmp));
		HTMLprintf(playername, sizeof(playername), true, "%s", tmp);


		HTMLPRINT("</item>" CRLF);
	}

	Sys_free(s);

	// Send RSS footer.
	Net_ProxySend(cluster, dest, footer_fmt, strlen(footer_fmt));
}

//========================================================================
// STATUS
//========================================================================

void HTTPSV_GenerateQTVStatus(cluster_t *cluster, oproxy_t *dest, char *str)
{
	unsigned int d, h, m;
	char *net;

	HTTPSV_SendHTTPHeader(cluster, dest, "200", "text/html", true);
	HTTPSV_SendHTMLHeader(cluster, dest, "QuakeTV: Status");

	Net_ProxyPrintf(dest, "%s", "<h1>QuakeTV: Status</h1>");

	Net_ProxyPrintf(dest, "%s", "<PRE>");

	Net_ProxyPrintf(dest, "servers: %i/%i\n", g_cluster.NumServers, maxservers.integer);
	Net_ProxyPrintf(dest, "clients: %i/%i\n", g_cluster.numproxies, get_maxclients());
	Net_ProxyPrintf(dest, "net: in/out %llu/%llu\n", g_cluster.socket_stat.r, g_cluster.socket_stat.w);

	// parse something like http://localhost?net=xxx
	if ((net = strstr(str, "?net=")) || (net = strstr(str, "&net=")))
	{
		net += sizeof("*net=")-1;

		if (atoi(net))
		{
			if (g_cluster.servers)
			{
				sv_t *qtv;

				for (qtv = g_cluster.servers; qtv; qtv = qtv->next)
					Net_ProxyPrintf(dest, "net: %s in/out %llu/%llu clients: in/out %llu/%llu\n", 
							qtv->server, qtv->socket_stat.r, qtv->socket_stat.w, qtv->proxies_socket_stat.r, qtv->proxies_socket_stat.w);
			}
		}
	}

	Get_Uptime(g_cluster.curtime / 1000, &d, &h, &m);

	Net_ProxyPrintf(dest, "uptime: %u days %02u:%02u\n", d, h, m);

	Net_ProxyPrintf(dest, "%s", "</PRE>");

	HTTPSV_SendHTMLFooter(cluster, dest);
}
