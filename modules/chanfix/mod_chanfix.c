/*
**   IRC services chanfix module -- mod_chanfix.c
**   Copyright (C) 2001-2004 Chris Behrens
**
**   This program is free software; you can redistribute it and/or modify
**   it under the terms of the GNU General Public License as published by
**   the Free Software Foundation; either version 1, or (at your option)
**   any later version.
**
**   This program is distributed in the hope that it will be useful,
**   but WITHOUT ANY WARRANTY; without even the implied warranty of
**   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**   GNU General Public License for more details.
**
**   You should have received a copy of the GNU General Public License
**   along with this program; if not, write to the Free Software
**   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include "services.h"
#include "chanfixdb.h"
#include "mod_admin.h"

RCSID("$Id: mod_chanfix.c,v 1.5 2004/08/26 17:59:10 cbehrens Exp $");

#define CHANFIX_CHECK_FREQ      (5*60)
#define CHANFIX_ROLL_FREQ		(86400/CHANFIX_CHECK_FREQ) /* 1 day */
#define CHANFIX_CHECKS_NEEDED   (86400*3/CHANFIX_CHECK_FREQ) /* 3 days*/
#define CHANFIX_CLIENTS_NEEDED  4
#define CHANFIX_SERVERS_NEEDED  875                     /* 87.5% */
#define CHANFIX_NICKS_TO_OP		5
#define CHANFIX_WAIT_TIME_MIN	(30*60)
#define CHANFIX_WAIT_TIME_MAX	(60*60)
#define CHANFIX_SCORES_NUM		10
#define CHANFIX_TIME_BETWEEN_FIXES	(60*30)		/* 30 min */

#define CHANFIX_DEBUG(__level, ...)		\
			simodule_debug_printf(SIModHandle, __level, __VA_ARGS__)
#ifdef SI_CHANFIX_EXTRA_DEBUG
#define CHANFIX_EDEBUG(__level, ...)	\
			simodule_debug_printf(SIModHandle, __level, __VA_ARGS__)
#else
#define CHANFIX_EDEBUG(__level, ...)		
#endif

#define CHANFIX_ENABLED		\
	(((SIStats.st_numgservers - SIStats.st_serversbursting) * 1000 / SIStats.st_maxgservers) >= SICFConf.cfc_servers_needed)

typedef struct _si_chanfix_ignore		SI_CHANFIX_IGNORE;
typedef struct _si_chanfix_conf			SI_CHANFIX_CONF;
typedef struct _si_chanfix_scoreinfo	SI_CHANFIX_SCOREINFO;

struct _si_chanfix_ignore
{
	SI_CHANFIX_IGNORE		*ig_next;
#define SI_IG_FLAGS_MANUAL		0x001
#define SI_IG_FLAGS_AUTO		0x002
	int						ig_flags;
	char					ig_string[1];
};

struct _si_chanfix_conf
{
#define SI_CFC_FLAGS_REOP_DISABLE			0x001
#define SI_CFC_FLAGS_CHANFIX_DISABLE		0x002
#define SI_CFC_FLAGS_CHANFIX_MODERATE		0x004
#define SI_CFC_FLAGS_MANUAL_OVERRIDE		0x008
#define SI_CFC_FLAGS_AUTO_OVERRIDE			0x010
	u_int				cfc_flags;
	u_int				cfc_checkfreq;
	u_int				cfc_rollfreq;
	u_int				cfc_checks_needed;
	u_int				cfc_clients_needed;
	u_int				cfc_servers_needed;
	u_int				cfc_nicks_to_op;
	u_int				cfc_reop_wait_time_min;
	u_int				cfc_reop_wait_time_max;
	u_int				cfc_time_between_fixes;
	SI_CHANFIX_IGNORE	*cfc_uhignores;
	SI_CHANFIX_IGNORE	*cfc_chanignores;
	char				*cfc_chanfixdb;
};

struct _si_chanfix_scoreinfo
{
	u_short			score;
	void			*vptr;
};

static int _do_oplist(SI_CLIENT *from, SI_CLIENT *to, char *args);
static int _do_chanfix(SI_CLIENT *from, SI_CLIENT *to, char *args);
static int _do_score(SI_CLIENT *from, SI_CLIENT *to, char *args);

static SI_MOD_COMMAND   _CFCommands[] = {
    {   "oplist",	NULL,			 SI_MC_FLAGS_NOHELP|SI_MC_FLAGS_OPERONLY,  _do_oplist  },
    {   "chanfix",	"Restore ops to the people with the top 5 scores in a channel",	SI_MC_FLAGS_OPERONLY,  _do_chanfix },
    {   "score",	"List the score of a user for a channel or the top 5 scores for a channel",	SI_MC_FLAGS_OPERONLY,  _do_score },
    {   NULL,       NULL,                   0,  NULL        }
};

static SI_MOD_CLIENT    SIJClient = {
	"JUPES",  "services", "services.int", "EFNet Services",
			SI_CLIENT_TYPE_CLIENT, SI_CLIENT_UMODE_OPER, 0, _CFCommands
};

static SI_MOD_CLIENT    SICFClient = {
	"CHANFIX",    "services", "services.int", "EFNet Services",
			SI_CLIENT_TYPE_CLIENT, SI_CLIENT_UMODE_OPER, 0, _CFCommands
};


static u_int			SITimedCbId = 0;
static SI_MEMPOOL		SICFMemPool;
static void				*SIModHandle = NULL;
static SI_CHANFIXDB		*SIChanFixDB = NULL;
static SI_CHANFIX_CONF	SICFConf;

static int _chanfix_valid_userhost(char *userhost);
static void _chanfix_check(void);
static int _chanfix_ignore(SI_CHANFIX_IGNORE *ignores, char *string, int flags);
static u_int _chanfix_get_scores(SI_CHANNEL *chptr, SI_CHANFIX_CHAN *chan, SI_CHANFIX_SCOREINFO *scores, u_int scores_max, u_int score_min, u_int flags);
static int _chanfix_channel_reop(SI_CHANNEL *chptr, SI_CHANFIX_CHAN *chan, SI_CLIENT *from, SI_CLIENT *to, int do_deop);
static int _chanfix_automatic_reop(SI_CHANNEL *chptr, SI_CHANFIX_CHAN *chan);
static int _chanfix_manual_reop(SI_CHANNEL *chptr, SI_CLIENT *from, SI_CLIENT *to, int override);
static void _channel_added(SI_CHANNEL *chptr);
static void _channel_removed(SI_CHANNEL *chptr);
static void _channel_client_added(SI_CHANNEL *chptr, SI_CLIENT *cptr, SI_CLIENTLIST *clptr);
static void _channel_client_removed(SI_CHANNEL *chptr, SI_CLIENT *cptr, SI_CLIENTLIST *clptr);
static void _channel_clmode_changed(SI_CHANNEL *chptr, SI_CLIENTLIST *clptr, u_int old_modes);
static void _sjoin_done(SI_CHANNEL *chptr);
static void _conf_deinit(void);
static void _conf_error_func(u_int severity, char *error, char *filename, u_int lineno);
static int _simodule_init(void *modhandle, char *args);
static void _simodule_deinit(void);
static int _conf_flags(void *csc_cookie, char *args, void **cb_arg);
static int _conf_checkfreq(void *csc_cookie, char *args, void **cb_arg);
static int _conf_rollfreq(void *csc_cookie, char *args, void **cb_arg);
static int _conf_checks_needed(void *csc_cookie, char *args, void **cb_arg);
static int _conf_clients_needed(void *csc_cookie, char *args, void **cb_arg);
static int _conf_servers_needed(void *csc_cookie, char *args, void **cb_arg);
static int _conf_nicks_to_op(void *csc_cookie, char *args, void **cb_arg);
static int _conf_wait_min_time(void *csc_cookie, char *args, void **cb_arg);
static int _conf_wait_max_time(void *csc_cookie, char *args, void **cb_arg);
static int _conf_time_between_fixes(void *csc_cookie, char *args, void **cb_arg);
static int _conf_uhignore(void *csc_cookie, char *args, void **cb_arg);
static int _conf_chanignore(void *csc_cookie, char *args, void **cb_arg);
static int _conf_chanfixdb(void *csc_cookie, char *args, void **cb_arg);


static SI_MOD_CALLBACKS     SICFCallbacks = {
	NULL,
	NULL,
	NULL,
	NULL,
    _channel_added,
    _channel_removed,
	NULL,
	NULL,
	NULL,
	NULL,
    _channel_client_added,
    _channel_client_removed,
    _channel_clmode_changed,
	NULL,
	NULL,
    _sjoin_done
};

static CSC_COMMAND  Commands[] = {
	{   "flag",			_conf_flags,		0,		NULL },
	{   "flags",		_conf_flags,		0,		NULL },
	{	"checkfreq",	_conf_checkfreq,	0,		NULL },
	{	"rollfreq",		_conf_rollfreq,		0,		NULL },
	{	"checks_needed",_conf_checks_needed,0,		NULL },
	{	"clients_needed",_conf_clients_needed,0,		NULL },
	{	"servers_needed",_conf_servers_needed,0,		NULL },
	{	"nicks_to_op",	_conf_nicks_to_op,	0,		NULL },
	{	"wait_min_time",_conf_wait_min_time,	0,		NULL },
	{	"wait_max_time",_conf_wait_max_time,	0,		NULL },
	{	"time_between_fixes", _conf_time_between_fixes,	0,	NULL },
	{	"uhignore",		_conf_uhignore,		0,		NULL },
	{	"chanignore",	_conf_chanignore,	0,		NULL },
	{	"chanfixdb",	_conf_chanfixdb,	0,		NULL },
	{   NULL,       NULL,           0,      NULL } 
};                                                

static int _simodule_init(void *modhandle, char *args);
static void _simodule_deinit(void);

SI_MODULE_DECL("Chanfix module",
                &SICFMemPool,
				&SICFCallbacks,
                _simodule_init,
                _simodule_deinit
            );

extern SI_CHANNEL	*SIChannels;


static int _chanfix_valid_userhost(char *userhost)
{
	if ((*userhost == '~') ||
			_chanfix_ignore(SICFConf.cfc_uhignores, userhost, 0))
		return 0;
	return 1;
}

static void _chanfix_check(void)
{
	SI_CHANNEL			*chptr;
	SI_CHANFIX_CHAN		*chan;
	SI_CLIENTLIST		*users;
	SI_CHANFIX_CHANUSER	*chanuser;
	int					err;
	int					en;
	u_int				when;
	u_int				modes;
	int					i;
	int					isave = -1;
	SI_CHANFIX_CHANMODE	mode;
	time_t				now;

	CHANFIX_DEBUG(5, "_chanfix_check called");

	when = SICFConf.cfc_checkfreq - (time(NULL) % SICFConf.cfc_checkfreq);
	if (when < 2)
		when = SICFConf.cfc_checkfreq;
	err = sitimedcb_add(when,
			0,
			(void(*)(void *))_chanfix_check, NULL, &SITimedCbId);
	if (err)
	{
		SITimedCbId = 0;
		CHANFIX_DEBUG(1, "chanfix_check(): couldn't add timer: %d (%s)",
								err, strerror(err));
		si_log(SI_LOG_ERROR, "chanfix: couldn't add timer: %d", err);
	}

	en = CHANFIX_ENABLED;
	if (!en)
	{
		CHANFIX_DEBUG(1, "chanfix_check(): chanfix is not enabled.");
		return;
	}

	SIChanFixDB->cf_info->cfi_checks++;
	now = time(NULL);

	if (!(SIChanFixDB->cf_info->cfi_checks % SICFConf.cfc_rollfreq))
	{
		CHANFIX_DEBUG(1, "chanfix_check(): rolling the database.");
		chanfixdb_roll(SIChanFixDB);
	}

	for(chptr=SIChannels;chptr;chptr=chptr->ch_a_next)
	{
		if (chptr->ch_numclients < SICFConf.cfc_clients_needed)
		{
			continue;
		}

		err = chanfixdb_channel_find(SIChanFixDB, chptr->ch_name, &chan);
		if (err)
		{
			if (!chptr->ch_ops)
				continue;
			err = chanfixdb_channel_create(SIChanFixDB, chptr->ch_name, &chan);
			if (err)
			{
				CHANFIX_DEBUG(1, "_chanfix_check: couldn't create channel \"%s\": %d (%s)",
						chptr->ch_name, err, strerror(err));
				si_log(SI_LOG_ERROR, "chanfix: couldn't create channel \"%s\": %d",
						chptr->ch_name, err);
				continue;
			}
		}

		chan->cfc_last_seen = now;

		if (en && (!chptr->ch_ops ||
				(chan->cfc_flags & SI_CHANFIX_CHAN_FLAGS_FIXING)))
		{
			CHANFIX_DEBUG(1, "chanfix_check(): calling auto_reop().");
			(void)_chanfix_automatic_reop(chptr, chan);
		}

		chan->cfc_checks++;

		modes = chptr->ch_modes;
		if (chptr->ch_limit)
			modes |= SI_CHANFIXDB_CHAN_MODE_LIMIT;
		if (chptr->ch_key)
			modes |= SI_CHANFIXDB_CHAN_MODE_KEY;

		/* check modes */
		for(i=0;
			i < (sizeof(chan->cfc_modes)/sizeof(chan->cfc_modes[0]));
			i++)
		{
			if (modes == chan->cfc_modes[i].cfcm_modes)
				break;
			if ((isave == -1) || (chan->cfc_modes[i].cfcm_tot_score <
						chan->cfc_modes[isave].cfcm_tot_score))
			{
				isave = i;
			}
		}
		if (i == sizeof(chan->cfc_modes)/sizeof(chan->cfc_modes[0]))
		{
			assert(isave != -1);	/* can't happen -- sizeof(chan->cfc_modes) is always > 0, so isave will get set */

			i = isave;
			chan->cfc_modes[i].cfcm_modes = modes;
			memset(chan->cfc_modes[i].cfcm_scores, 0,
				sizeof(chan->cfc_modes[i].cfcm_scores));
			chan->cfc_modes[i].cfcm_tot_score = 0;
		}

		chan->cfc_modes[i].cfcm_last_seen = now;
		chan->cfc_modes[i].cfcm_scores[0]++;
		chan->cfc_modes[i].cfcm_tot_score++;

		if (i != 0)
		{
			mode = chan->cfc_modes[0];
			chan->cfc_modes[0] = chan->cfc_modes[i];
			chan->cfc_modes[i] = mode;
		}


		for(users=chptr->ch_ops;users;users=users->cl_next)
		{
			if (!_chanfix_valid_userhost(users->cl_cptr->c_userhost))
			{
				CHANFIX_EDEBUG(10, "_chanfix_check: ignoring chanuser \"%s\" on channel \"%s\"",
					users->cl_cptr->c_userhost, chptr->ch_name);
				continue;
			}

			err = chanfixdb_chanuser_find(SIChanFixDB, chan,
						users->cl_cptr->c_userhost, &chanuser);
			if (err)
			{
				err = chanfixdb_chanuser_create(SIChanFixDB, &chan,
							users->cl_cptr->c_userhost, &chanuser);
				if (err)
				{
					CHANFIX_DEBUG(1, "_chanfix_check: couldn't create chanuser \"%s\" for channel \"%s\": %d (%s).",
						users->cl_cptr->c_userhost,
						chptr->ch_name, err, strerror(err));
					si_log(SI_LOG_ERROR, "chanfix: couldn't create chanuser \"%s\": %d",
						users->cl_cptr->c_userhost, err);
					continue;
				}
			}

			if (chanuser->cfcu_last_seen != now)
			{
				chanuser->cfcu_last_seen = now;
				chanuser->cfcu_tot_score++;
				chanuser->cfcu_scores[0]++;
			}

			chanfixdb_chanuser_sort(SIChanFixDB, chanuser);
		}
	}
}

static int _chanfix_ignore(SI_CHANFIX_IGNORE *ignores, char *string, int flags)
{
	for(;ignores;ignores=ignores->ig_next)
	{
		if (flags && !(flags & ignores->ig_flags))
			continue;
		if (!ircd_match(ignores->ig_string, string))
			return 1;
	}
	return 0;
}

static u_int _chanfix_get_scores(SI_CHANNEL *chptr, SI_CHANFIX_CHAN *chan, SI_CHANFIX_SCOREINFO *scores, u_int scores_max, u_int score_min, u_int flags)
{
	SI_CLIENTLIST			*clients;
	SI_CHANFIX_CHANUSER		*cfcu;
	int						err;
	u_int					scores_num = 0;
	u_int					which = 0;

	assert(chptr && chan && scores && scores_max);

	if (flags & 0x001)
		which++;

	while(which < 2)
	{
		for(clients=which ? chptr->ch_ops : chptr->ch_non_ops;
				clients;
				clients=clients->cl_next)
		{
			if (!_chanfix_valid_userhost(clients->cl_cptr->c_userhost))
				continue;

			err = chanfixdb_chanuser_find(SIChanFixDB, chan,
						clients->cl_cptr->c_userhost, &cfcu);
			if (err)
			{
				continue;
			}

			if (cfcu->cfcu_tot_score < score_min)
				continue;

			for(err=0;err<scores_num;err++)
			{
				if (cfcu->cfcu_tot_score > scores[err].score)
					break;
			}

			if (err == scores_max)
			{
				/* too low, too bad */
				continue;
			}

			if (scores_num < scores_max)
				scores_num++;

			memmove(scores+err+1, scores+err,
				sizeof(SI_CHANFIX_SCOREINFO) * (scores_max - err - 1));
			scores[err].score = cfcu->cfcu_tot_score;
			scores[err].vptr = (void *)clients;
		}

		if (flags & 0x002)
		{
			which++;
			continue;
		}
		break;
	}
	return scores_num;
}

static int _chanfix_channel_reop(SI_CHANNEL *chptr, SI_CHANFIX_CHAN *chan, SI_CLIENT *from, SI_CLIENT *to, int do_deop)
{
	u_int					scores_max = SICFConf.cfc_nicks_to_op;
	u_int					scores_num;
	u_int					users_num;
	u_int					users_per_pass;
	u_int					score_needed;
	SI_CLIENTLIST			*clptr;
	SI_CHANFIX_SCOREINFO	scoresbuf[5];
	SI_CHANFIX_SCOREINFO	*scores = scoresbuf;
	SI_CLIENTLIST			*clients;
	SI_CLIENTLIST			*nextcl;
	u_int					i;
	int						doit = !(SICFConf.cfc_flags & SI_CFC_FLAGS_REOP_DISABLE);
	int						do_mode = 1;
	u_int					nmodes = 0;
	char					modebuf1[10];
	char					*modebufptr1;
	char					buffer[1024];
	u_int					len_needed;
	char					*bufptr;
	char					*endptr;
	time_t					ts_to_send;
	int						deopped_num = -1;
	int						last;
	int						restarting = 0;
	int						manual = 0;
	register int			modes_changed = 0;

	assert(chptr && chan);

	ts_to_send = chptr->ch_creation;

	if (do_deop)
	{
		if (ts_to_send > 1)
		{
			ts_to_send--;
			do_mode = 0;
		}
	}

	if (!(chan->cfc_flags & SI_CHANFIX_CHAN_FLAGS_FIXING))
	{
		CHANFIX_DEBUG(1, "Channel \"%s\" being fixed, 1st pass.", chptr->ch_name);
		chan->cfc_flags |= SI_CHANFIX_CHAN_FLAGS_FIXING;
		if (from)
		{
			chan->cfc_flags |= SI_CHANFIX_CHAN_FLAGS_FIXING_MANUAL;
			manual = 1;
		}
		else
			chan->cfc_flags &= ~SI_CHANFIX_CHAN_FLAGS_FIXING_MANUAL;
		chan->cfc_fixed_time = Now;
		chan->cfc_fixed_passes = 0;
	}
	else if (from)
	{
		chan->cfc_fixed_time = Now;
		chan->cfc_fixed_passes = 0;
		chan->cfc_flags |= SI_CHANFIX_CHAN_FLAGS_FIXING_MANUAL;
		restarting = 1;
		manual = 1;
	}
	else
	{
		manual = chan->cfc_flags & SI_CHANFIX_CHAN_FLAGS_FIXING_MANUAL;
	}

	if ((Now - chan->cfc_fixed_time)
					<= SICFConf.cfc_reop_wait_time_min)
	{
		users_per_pass = chan->cfc_users_num * SICFConf.cfc_checkfreq /
					SICFConf.cfc_reop_wait_time_min;
		if (users_per_pass > scores_max)
			users_per_pass = scores_max;
	}
	else
	{
		users_per_pass = chan->cfc_users_num * SICFConf.cfc_checkfreq /
					SICFConf.cfc_reop_wait_time_max;
	}

	if (!users_per_pass)
		users_per_pass++;

	users_num = users_per_pass * (++chan->cfc_fixed_passes);

	CHANFIX_DEBUG(2, "Channel \"%s\" (%d users in db) being fixed (for %d second%s now), pass#=%d, users_per_pass=%d.",
				chptr->ch_name, chan->cfc_users_num,
				Now - chan->cfc_fixed_time,
				Now - chan->cfc_fixed_time == 1 ? "" : "s",
				chan->cfc_fixed_passes,
				users_per_pass);

	/* find the score needed */

	if (users_num >= chan->cfc_users_num)
	{
		score_needed = 0;	/* any score */
	}
	else
	{
		u_int					pos;
		u_int					which_way;
		u_int					num;
		SI_CHANFIX_CHANUSER		*curec;

		if ((num = (1 + chan->cfc_users_num - users_num)) < users_num)
		{
			pos = chan->cfc_users_sorted_first;
			which_way = 1;
		}
		else
		{
			pos = chan->cfc_users_sorted_last;
			which_way = 0;
			num = users_num;
		}

		while(num--)
		{
			if (pos == (u_int)-1)	/* shouldn't happen */
				break;
			curec = SI_CHANFIX_CHANUSER_PTR(SIChanFixDB, pos);
			pos = which_way ? curec->cfcu_s_next : curec->cfcu_s_prev;
		}

		score_needed = curec->cfcu_tot_score;
	}

	CHANFIX_DEBUG(2, "A score of %d is needed to find ops for channel \"%s\".",
				score_needed, chptr->ch_name);

	if (!do_deop)
	{
		u_int	num_ops = 0;

		/*
		** we only want to continue opping until
		**  'scores_max' == # of ops.
		*/

		for(clptr=chptr->ch_ops;clptr;clptr=clptr->cl_next)
		{
			num_ops++;
		}

		if (num_ops >= scores_max)
		{
			CHANFIX_DEBUG(2, "Channel \"%s\" already has enough ops (%d >= %d).",
				chptr->ch_name, num_ops, scores_max);
			/* already have enough ops */
			si_log(SI_LOG_NOTICE, "mod_chanfix: Channel \"%s\" is done being fixed, has enough ops after %d sec.",
				chptr->ch_name,
				Now - chan->cfc_fixed_time);
			if (from)
			{
				siclient_send_privmsg(to, from, "Sorry, channel \"%s\" already has enough ops.", chptr->ch_name);
			}
			chan->cfc_flags &= ~SI_CHANFIX_CHAN_FLAGS_FIXING;
			return ENOSPC;
		}

		scores_max -= num_ops;

		CHANFIX_DEBUG(2, "Channel \"%s\" needs %d more op%s.",
			chptr->ch_name, scores_max, scores_max == 1 ? "" : "s");
	}

	if (scores_max > (sizeof(scoresbuf)/sizeof(scoresbuf[0])))
	{
		scores = (SI_CHANFIX_SCOREINFO *)simempool_alloc(&SICFMemPool,
							sizeof(SI_CHANFIX_SCOREINFO) *
								scores_max);
		if (!scores)
		{
			if (from)
			{
				siclient_send_privmsg(to, from, "Out of memory.");
			}
			return ENOMEM;
		}
	}

	scores_num = _chanfix_get_scores(chptr, chan,
						scores, scores_max,
						score_needed,
						do_deop ? 2 : 0);
	last = (!score_needed || (scores_num == scores_max));

	CHANFIX_DEBUG(2, "Found %d people to op for channel \"%s\", last pass=%d",
			scores_num,
			chptr->ch_name,
			last);

	if (doit && (!last || !scores_num))
	{
		if (chptr->ch_modes & SI_CHANNEL_MODE_INVITEONLY)
		{
			modes_changed++;
			chptr->ch_modes &= ~SI_CHANNEL_MODE_INVITEONLY;
		}
		if (chptr->ch_key)
		{
			simempool_free(chptr->ch_key);
			chptr->ch_key = NULL;
			modes_changed++;
		}
		if (chptr->ch_limit)
		{
			modes_changed++;
			chptr->ch_limit = 0;
		}
		if (SICFConf.cfc_flags & SI_CFC_FLAGS_CHANFIX_MODERATE)
		{
			if (!(chptr->ch_modes & SI_CHANNEL_MODE_MODERATED))
			{
				modes_changed++;
				chptr->ch_modes |= SI_CHANNEL_MODE_MODERATED;
			}
		}
		else
		{
			if (chptr->ch_modes & SI_CHANNEL_MODE_MODERATED)
			{
				modes_changed++;
				chptr->ch_modes &= ~SI_CHANNEL_MODE_MODERATED;
			}
		}
	}

	if (doit && (modes_changed || scores_num ||
					(do_deop && chptr->ch_ops)))
	{
		char	modebuf[10];
		char	limit[40];
		char	*modeptr = modebuf;
		SI_BAN	*ban;

		if (chptr->ch_modes & SI_CHANNEL_MODE_SECRET)
			*modeptr++ = 's';
		if (chptr->ch_modes & SI_CHANNEL_MODE_TOPICLIMIT)
			*modeptr++ = 't';
		if (chptr->ch_modes & SI_CHANNEL_MODE_INVITEONLY)
			*modeptr++ = 'i';
		if (chptr->ch_modes & SI_CHANNEL_MODE_NOPRIVMSGS)
			*modeptr++ = 'n';
		if (chptr->ch_modes & SI_CHANNEL_MODE_PRIVATE)
			*modeptr++ = 'p';
		if (chptr->ch_modes & SI_CHANNEL_MODE_MODERATED)
			*modeptr++ = 'm';
		if (chptr->ch_limit)
		{
			*modeptr++ = 'l';
			sprintf(limit, "%i", chptr->ch_limit);
		}
		if (chptr->ch_key)
			*modeptr++ = 'k';

		*modeptr = '\0';

		siuplink_printf(":%s SJOIN %lu %s +%s%s%s%s%s :@%s",
			SIMyName,
			ts_to_send, chptr->ch_name,
			modebuf,
			chptr->ch_limit ? " " : "",
			chptr->ch_limit ? limit : "",
			chptr->ch_key ? " " : "",
			chptr->ch_key ? chptr->ch_key : "",
			SICFClient.mcl_name);

		chptr->ch_creation = ts_to_send;

		if ((chan->cfc_fixed_passes == 1) && (!last || !scores_num))
		{
			/* delete bans */

			modebufptr1 = modebuf1;
			bufptr = buffer;
			endptr = buffer + sizeof(buffer) - 1;
			nmodes = 0;

			CHANFIX_DEBUG(3, "Removing bans from channel \"%s\".",
					chptr->ch_name);

			for(ban=chptr->ch_bans;ban;ban=ban->b_next)
			{
				len_needed = strlen(ban->b_string);

				if ((!(nmodes % 4) && (bufptr != buffer)) || (endptr-bufptr) < (2+len_needed))
				{
					*bufptr = '\0';
					*modebufptr1 = '\0';
					siuplink_printf(":%s MODE %s -%s %s",
						SICFClient.mcl_name, chptr->ch_name,
						modebuf1, buffer);
						bufptr = buffer;
						modebufptr1 = modebuf1;
						nmodes = 0;
				}

				*modebufptr1++ = 'b';

				strcpy(bufptr, ban->b_string);
				bufptr += len_needed;
				*bufptr++ = ' ';
				nmodes++;
			}

			if (bufptr != buffer)
			{
				*bufptr = '\0';
				*modebufptr1 = '\0';
				siuplink_printf(":%s MODE %s -%s %s",
					SICFClient.mcl_name, chptr->ch_name,
					modebuf1, buffer);
			}

			sichannel_del_ban(chptr, NULL, NULL); /* del bans from mem */
		}

		if (modes_changed && do_mode)
		{
			if (chptr->ch_modes & SI_CHANNEL_MODE_MODERATED)
			{
				/*
				** we could do +m again, but it was done
				**  above with the SJOIN
				*/
				siuplink_printf(":%s MODE %s -ilk *",
					SICFClient.mcl_name, chptr->ch_name);
			}
			else
			{
				siuplink_printf(":%s MODE %s -milk *",
					SICFClient.mcl_name, chptr->ch_name);
			}
		}
	}

	if (do_deop)
	{
		modebufptr1 = modebuf1;
		bufptr = buffer;
		endptr = buffer + sizeof(buffer) - 1;
		nmodes = 0;

		deopped_num = 0;
		for(clients=chptr->ch_ops;clients;clients=nextcl)
		{
			nextcl = clients->cl_next;

			deopped_num++;
			if (doit)
			{
				clients->cl_modes &= ~SI_CHANNEL_MODE_CHANOP;
				si_ll_del_from_list(chptr->ch_ops, clients, cl);
				si_ll_add_to_list(chptr->ch_non_ops, clients, cl);
				si_log(SI_LOG_NOTICE, "mod_chanfix: deopping %s!%s from %s on %s",
					clients->cl_cptr->c_name,
					clients->cl_cptr->c_userhost,
					clients->cl_cptr->c_servername,
					chptr->ch_name);

				CHANFIX_EDEBUG(8, "Deopping %s!%s in channel \"%s\".",
					clients->cl_cptr->c_name,
					clients->cl_cptr->c_userhost,
					chptr->ch_name);
					
				if (do_mode)
				{
					len_needed = strlen(clients->cl_cptr->c_name);

					if ((!(nmodes % 4) && (bufptr != buffer)) || (endptr-bufptr) < (2+len_needed))
					{
						*bufptr = '\0';
						*modebufptr1 = '\0';
						siuplink_printf(":%s MODE %s -%s %s",
							SICFClient.mcl_name, chptr->ch_name,
							modebuf1, buffer);
						bufptr = buffer;
						modebufptr1 = modebuf1;
						nmodes = 0;
					}

					*modebufptr1++ = 'o';

					strcpy(bufptr, clients->cl_cptr->c_name);
					bufptr += len_needed;
					*bufptr++ = ' ';
					nmodes++;
				}
			}
			else
			{
				si_log(SI_LOG_NOTICE, "mod_chanfix: would have deopped %s!%s from %s on %s",
					clients->cl_cptr->c_name,
					clients->cl_cptr->c_userhost,
					clients->cl_cptr->c_servername,
					chptr->ch_name);
				CHANFIX_EDEBUG(8, "Would have Deopped %s!%s in channel \"%s\".",
					clients->cl_cptr->c_name,
					clients->cl_cptr->c_userhost,
					chptr->ch_name);
			}
		}

		if (bufptr != buffer)
		{
			*bufptr = '\0';
			*modebufptr1 = '\0';
			siuplink_printf(":%s MODE %s -%s %s",
				SICFClient.mcl_name, chptr->ch_name,
				modebuf1, buffer);
		}

		if (doit)
		{
			CHANFIX_DEBUG(3, "Deopped %d %s in channel \"%s\".",
				deopped_num, deopped_num == 1 ? "person" : "people",
					chptr->ch_name);
		}
		else
		{
			CHANFIX_DEBUG(3, "Would have deopped %d %s in channel \"%s\".",
				deopped_num, deopped_num == 1 ? "person" : "people",
					chptr->ch_name);
		}
	}

	modebufptr1 = modebuf1;
	bufptr = buffer;
	endptr = buffer + sizeof(buffer) - 1;
	nmodes = 0;

	for(i=0;i<scores_num;i++)
	{
		if (doit)
		{
			si_log(SI_LOG_NOTICE, "mod_chanfix: opping %s!%s on %s for %s fix of channel %s with a score of %u\n",
					((SI_CLIENTLIST *)scores[i].vptr)->cl_cptr->c_name,
					((SI_CLIENTLIST *)scores[i].vptr)->cl_cptr->c_userhost,
					((SI_CLIENTLIST *)scores[i].vptr)->cl_cptr->c_servername,
					manual ? "manual" : "automatic",
					chptr->ch_name,
					scores[i].score);

			CHANFIX_EDEBUG(8, "Opping %s!%s [%d] in channel \"%s\".",
					((SI_CLIENTLIST *)scores[i].vptr)->cl_cptr->c_name,
					((SI_CLIENTLIST *)scores[i].vptr)->cl_cptr->c_userhost,
					scores[i].score,
					chptr->ch_name);

			((SI_CLIENTLIST *)scores[i].vptr)->cl_modes |= SI_CHANNEL_MODE_CHANOP;
			si_ll_del_from_list(chptr->ch_non_ops,
								(SI_CLIENTLIST *)scores[i].vptr, cl);
			si_ll_add_to_list(chptr->ch_ops,
								(SI_CLIENTLIST *)scores[i].vptr, cl);
			
			len_needed = strlen(((SI_CLIENTLIST *)scores[i].vptr)->cl_cptr->c_name);

			if ((!(nmodes % 4) && (bufptr != buffer)) || (endptr-bufptr) < (2+len_needed))
			{
				*bufptr = '\0';
				*modebufptr1 = '\0';
				siuplink_printf(":%s MODE %s +%s %s",
					SICFClient.mcl_name, chptr->ch_name,
					modebuf1, buffer);
				bufptr = buffer;
				modebufptr1 = modebuf1;
				nmodes = 0;
			}

			*modebufptr1++ = 'o';

			strcpy(bufptr, ((SI_CLIENTLIST *)scores[i].vptr)->cl_cptr->c_name);
			bufptr += len_needed;
			*bufptr++ = ' ';
			nmodes++;
		}
		else
		{
			si_log(SI_LOG_NOTICE, "mod_chanfix: would have opped %s!%s on %s for channel %s with a score of %u\n",
					((SI_CLIENTLIST *)scores[i].vptr)->cl_cptr->c_name,
					((SI_CLIENTLIST *)scores[i].vptr)->cl_cptr->c_userhost,
					((SI_CLIENTLIST *)scores[i].vptr)->cl_cptr->c_servername,
					chptr->ch_name,
					scores[i].score);
			CHANFIX_EDEBUG(8, "Would have opped %s!%s [%d] in channel \"%s\".",
					((SI_CLIENTLIST *)scores[i].vptr)->cl_cptr->c_name,
					((SI_CLIENTLIST *)scores[i].vptr)->cl_cptr->c_userhost,
					scores[i].score,
					chptr->ch_name);
		}
	}

	if (bufptr != buffer)
	{
		*bufptr = '\0';
		*modebufptr1 = '\0';
		siuplink_printf(":%s MODE %s +%s %s",
			SICFClient.mcl_name, chptr->ch_name,
			modebuf1, buffer);
	}

	if (doit && (modes_changed || scores_num || (deopped_num > 0)))
	{
		if (!scores_num && (deopped_num <= 0))
		{
			siuplink_printf(":%s PRIVMSG %s :I only joined to clear modes -- bye!",
				SICFClient.mcl_name, chptr->ch_name);
		}
		else
		{
			if (deopped_num > 0)
			{
				siuplink_printf(":%s PRIVMSG %s :%d client%s should have been deopped.",
					SICFClient.mcl_name, chptr->ch_name, deopped_num,
					deopped_num == 1 ? "" : "s");
			}
			if (scores_num > 0)
				siuplink_printf(":%s PRIVMSG %s :%d client%s should have been opped.",
					SICFClient.mcl_name, chptr->ch_name, scores_num,
					scores_num == 1 ? "" : "s");
		}

		siuplink_printf(":%s PART %s",
			SICFClient.mcl_name, chptr->ch_name);
	}

	if (scores != scoresbuf)
	{
		simempool_free(scores);
	}

	if (last)
	{
		if (!scores_num)
		{
			si_log(SI_LOG_NOTICE, "mod_chanfix: No one found to op for %s fix of channel \"%s\" in %d sec. [%d user%s in db]",
				manual ? "manual" : "automatic",
				chptr->ch_name, Now - chan->cfc_fixed_time,
				chan->cfc_users_num,
				chan->cfc_users_num == 1 ? "" : "s");
			if (from)
			{
				siclient_send_privmsg(to, from, "Channel \"%s\" is done being fixed, but there was no one found to op.", chptr->ch_name);
			}
		}
		else
		{
			si_log(SI_LOG_NOTICE, "mod_chanfix: Channel \"%s\" fixed %s, taking %d sec.  %d opped, %d deopped. [%d user%s in db]",
				chptr->ch_name,
				manual ? "manually" : "automatically",
				Now - chan->cfc_fixed_time,
				scores_num, deopped_num,
				chan->cfc_users_num,
				chan->cfc_users_num == 1 ? "" : "s");
			if (from)
			{
				if (do_deop)
				{
					siclient_send_privmsg(to, from, "Channel \"%s\" has been fixed.  %d %s %s opped and %d %s %s deopped.", 
						chptr->ch_name,
						scores_num,
						scores_num == 1 ? "person" : "people",
						scores_num == 1 ? "was" : "were",
						deopped_num,
						deopped_num == 1 ? "person" : "people",
						deopped_num == 1 ? "was" : "were");
				}
				else
				{
					siclient_send_privmsg(to, from, "Channel \"%s\" has been fixed.  %d %s %s opped.",
						chptr->ch_name,
						scores_num,
						scores_num == 1 ? "person" : "people",
						scores_num == 1 ? "was" : "were");
				}
			}
		}
		CHANFIX_DEBUG(1, "Channel %s done being fixed.", chptr->ch_name);
		chan->cfc_flags &= ~SI_CHANFIX_CHAN_FLAGS_FIXING;
		chan->cfc_fixed_time = Now;
		return 0;
	}

	CHANFIX_DEBUG(1, "Channel \"%s\" not done being fixed.", chptr->ch_name);

	if (chan->cfc_fixed_passes == 1)
	{
		if (restarting)
		si_log(SI_LOG_NOTICE, "mod_chanfix: Fix for channel \"%s\" restarting manually.  %d opped, %d deopped",
				chptr->ch_name,
				scores_num,
				deopped_num);
		else
		si_log(SI_LOG_NOTICE, "mod_chanfix: Channel \"%s\" now being fixed %s.  %d opped, %d deopped",
				chptr->ch_name,
				manual ? "manually" : "automatically",
				scores_num,
				deopped_num);
	}
	else if (scores_num || (deopped_num > 0))
	{
		si_log(SI_LOG_NOTICE, "mod_chanfix: Channel \"%s\" still being fixed %s after %d sec.  %d opped, %d deopped",
				chptr->ch_name,
				manual ? "manually" : "automatically",
				Now - chan->cfc_fixed_time,
				scores_num,
				deopped_num);
	}

	if (from)
	{
		if (restarting)
		siclient_send_privmsg(to, from, "Fixing has started over for channel \"%s\".", chptr->ch_name);
		else
		siclient_send_privmsg(to, from, "Channel \"%s\" is now in the process of being fixed.", chptr->ch_name);
	}

	return EAGAIN;
}

static int _chanfix_automatic_reop(SI_CHANNEL *chptr, SI_CHANFIX_CHAN *chan)
{
	assert(chptr != NULL);
	assert(chptr->ch_numclients >= SICFConf.cfc_clients_needed);


	if (_chanfix_ignore(SICFConf.cfc_chanignores, chptr->ch_name, SI_IG_FLAGS_AUTO))
	{
		CHANFIX_DEBUG(1, "auto_reop(): ignoring channel \"%s\"", chptr->ch_name);
		return 0;
	}

	if (!chan)
	{
		int		err = chanfixdb_channel_find(SIChanFixDB,
								chptr->ch_name, &chan);
		if (err)
		{
			CHANFIX_DEBUG(1, "auto_reop(): Can't find channel \"%s\" in database", chptr->ch_name);
			return err;
		}
	}

	if (chan->cfc_checks < SICFConf.cfc_checks_needed)
	{
		CHANFIX_DEBUG(1, "auto_reop(): Number of checks too low for channel \"%s\" (%d < %d)", chptr->ch_name, chan->cfc_checks, SICFConf.cfc_checks_needed);
		return 0;
	}

	if (!(chan->cfc_flags & SI_CHANFIX_CHAN_FLAGS_FIXING) &&
			((Now - chan->cfc_fixed_time)
				<= SICFConf.cfc_time_between_fixes))
	{
		CHANFIX_DEBUG(1, "auto_reop(): Not enough time since last fix for channel \"%s\" (%d < %d)", chptr->ch_name, Now - chan->cfc_fixed_time,
			SICFConf.cfc_time_between_fixes);
		return 0;
	}

	return _chanfix_channel_reop(chptr, chan, NULL, NULL, 0);
}

static int _chanfix_manual_reop(SI_CHANNEL *chptr, SI_CLIENT *from, SI_CLIENT *to, int override)
{
	SI_CHANFIX_CHAN		*chan;
	u_int				st_numservers = SIStats.st_numgservers -
								SIStats.st_serversbursting;
	u_int				percent;
	int					err;

	assert(chptr != NULL);
	assert(from != NULL);
	assert(to != NULL);

	percent = st_numservers * 1000 / SIStats.st_maxgservers;
	if (percent < SICFConf.cfc_servers_needed)
	{
		siclient_send_privmsg(to, from, "Sorry, only %d.%d of servers exist, but %d.%d is required.",
			percent / 10,
			percent % 10,
			SIStats.st_maxgservers / 10,
			SIStats.st_maxgservers % 10);
		return EPERM;
	}

	err = chanfixdb_channel_find(SIChanFixDB, chptr->ch_name, &chan);
	if (err)
	{
		CHANFIX_DEBUG(1, "manual_reop(): Can't find channel \"%s\" in database", chptr->ch_name);
		siclient_send_privmsg(to, from, "Couldn't find channel \"%s\" in the database.",
				chptr->ch_name);
		return ENOENT;
	}

	if (_chanfix_ignore(SICFConf.cfc_chanignores, chptr->ch_name, SI_IG_FLAGS_MANUAL))
	{
		CHANFIX_DEBUG(1, "manual_reop(): ignoring channel \"%s\"", chptr->ch_name);
		si_log(SI_LOG_NOTICE, "mod_chanfix: \"chanfix %s\" tried on ignored channel from: %s!%s on %s [%s]",
			chptr->ch_name,
            from->c_name, from->c_userhost,
            from->c_servername, from->c_info);
		siclient_send_privmsg(to, from, "Sorry, that channel is not available for a chanfix.");
		return 0;
	}

	if (chan->cfc_flags & SI_CHANFIX_CHAN_FLAGS_FIXING)
	{
		int		man = chan->cfc_flags & SI_CHANFIX_CHAN_FLAGS_FIXING_MANUAL;
		int		mo = SICFConf.cfc_flags & SI_CFC_FLAGS_MANUAL_OVERRIDE;
		int		ao = SICFConf.cfc_flags & SI_CFC_FLAGS_AUTO_OVERRIDE;

		if (man)
		{
			if (!mo)
			{
				if (!ao)
					siclient_send_privmsg(to, from, "Sorry, channel \"%s\" is already being fixed.", chptr->ch_name);
				else
					siclient_send_privmsg(to, from, "Sorry, channel \"%s\" is already being manually fixed.", chptr->ch_name);
				return EALREADY;
			}
			if (!override)
			{
				siclient_send_privmsg(to, from, "Sorry, channel \"%s\" is already being manually fixed.  Append 'override' flag to override.", chptr->ch_name);
				return EALREADY;
			}
		}
		else
		{
			if (!ao)
			{
				if (!mo)
					siclient_send_privmsg(to, from, "Sorry, channel \"%s\" is already being fixed.", chptr->ch_name);
				else
					siclient_send_privmsg(to, from, "Sorry, channel \"%s\" is being automatically fixed.", chptr->ch_name);
				return EALREADY;
			}
			if (!override)
			{
				siclient_send_privmsg(to, from, "Sorry, channel \"%s\" is being automatically fixed.  Append 'override' flag to override.", chptr->ch_name);
				return EALREADY;
			}
		}
		siclient_send_privmsg(to, from, "Attempting to start fix of channel \"%s\" over again...", chptr->ch_name);
	}

	return _chanfix_channel_reop(chptr, chan, from, to, 1);
}

static void _conf_deinit(void)
{
	SI_CHANFIX_IGNORE	*ig;

	while(SICFConf.cfc_uhignores)
	{
		ig = SICFConf.cfc_uhignores->ig_next;
		simempool_free(SICFConf.cfc_uhignores);
		SICFConf.cfc_uhignores = ig;
	}
	while(SICFConf.cfc_chanignores)
	{
		ig = SICFConf.cfc_chanignores->ig_next;
		simempool_free(SICFConf.cfc_chanignores);
		SICFConf.cfc_chanignores = ig;
	}
	if (SICFConf.cfc_chanfixdb)
		simempool_free(SICFConf.cfc_chanfixdb);
}

static void _conf_error_func(u_int severity, char *error, char *filename, u_int lineno)
{
	int	 level = SI_LOG_DEBUG;
	char	*string = "";

	if (severity == CSC_SEVERITY_WARNING)
	{
		level = SI_LOG_WARNING;
		string = "Warning";
	}
	else
	{
		level = SI_LOG_ERROR;
		string = "Error";
	}
	si_log(level, "mod_chanfix: %s:%d: %s%s%s%s",
		filename,
		lineno,
		string,
		*string ? ":" : string,
		*string ? " " : string,
		error);
}

static int _simodule_init(void *modhandle, char *args)
{
	int		err;
	u_int	when;
    char	*confname = csconfig_next_arg(&args);

	if (!modhandle || !confname || !*confname)
	{
		si_log(SI_LOG_ERROR, "mod_chanfix: not enough arguments passed to module");
		return EINVAL;
	}

	SIModHandle = modhandle;

	memset(&SICFConf, 0, sizeof(SICFConf));
	SICFConf.cfc_nicks_to_op = CHANFIX_NICKS_TO_OP;
	SICFConf.cfc_reop_wait_time_min = CHANFIX_WAIT_TIME_MIN;
	SICFConf.cfc_reop_wait_time_max = CHANFIX_WAIT_TIME_MAX;
	SICFConf.cfc_time_between_fixes = CHANFIX_TIME_BETWEEN_FIXES;
	SICFConf.cfc_checkfreq = CHANFIX_CHECK_FREQ;
	SICFConf.cfc_rollfreq = CHANFIX_ROLL_FREQ;
	SICFConf.cfc_checks_needed = CHANFIX_CHECKS_NEEDED;
	SICFConf.cfc_servers_needed = CHANFIX_SERVERS_NEEDED;
	SICFConf.cfc_clients_needed = CHANFIX_CLIENTS_NEEDED;

	err = csconfig_read(confname, Commands, &SICFConf, _conf_error_func);
	if (err)
	{
		si_log(SI_LOG_ERROR, "mod_chanfix: couldn't load \"%s\": %d",
			confname, err);
		_conf_deinit();
		return err;
	}

	if (!SICFConf.cfc_chanfixdb)
	{
		si_log(SI_LOG_ERROR, "mod_chanfix: no chanfixdb specified in \"%s\"",
			confname);
		_conf_deinit();
		return EINVAL;
	}

	err = chanfixdb_init(SICFConf.cfc_chanfixdb, 0, &SIChanFixDB);
	if (err)
	{
		_conf_deinit();
		si_log(SI_LOG_ERROR, "mod_chanfix: couldn't open \"%s\": %d",
				SICFConf.cfc_chanfixdb, err);
		return err;
	}

	when = SICFConf.cfc_checkfreq - (time(NULL) % SICFConf.cfc_checkfreq);

	err = simodule_add_client(SIModHandle, &SICFClient);
	if (err)
	{
		si_log(SI_LOG_ERROR, "mod_chanfix: couldn't add CFClient: %d",
				err);
		chanfixdb_deinit(SIChanFixDB);
		_conf_deinit();
		_simodule_deinit();
		return err;
	}

	err = simodule_add_client(SIModHandle, &SIJClient);
	if (err)
	{
		si_log(SI_LOG_ERROR, "mod_chanfix: couldn't add JClient: %d",
				err);
		simodule_del_client(SIModHandle, &SICFClient);
		chanfixdb_deinit(SIChanFixDB);
		_conf_deinit();
		_simodule_deinit();
		return err;
	}

	err = sitimedcb_add(when,
				0,
				(void(*)(void *))_chanfix_check, NULL, &SITimedCbId);
	if (err)
	{
		simodule_del_client(SIModHandle, &SIJClient);
		simodule_del_client(SIModHandle, &SICFClient);
		chanfixdb_deinit(SIChanFixDB);
		_conf_deinit();
		_simodule_deinit();
		return err;
	}

	return err;
}

static void _simodule_deinit(void)
{
	if (SITimedCbId != 0)
	{
		sitimedcb_del(SITimedCbId);
		SITimedCbId = 0;
	}
	simodule_del_client(SIModHandle, &SIJClient);
	simodule_del_client(SIModHandle, &SICFClient);
	chanfixdb_deinit(SIChanFixDB);
	_conf_deinit();
}

static void _channel_added(SI_CHANNEL *chptr)
{
}

static void _channel_removed(SI_CHANNEL *chptr)
{
}

static void _channel_client_added(SI_CHANNEL *chptr, SI_CLIENT *cptr, SI_CLIENTLIST *clptr)
{
}

static void _channel_client_removed(SI_CHANNEL *chptr, SI_CLIENT *cptr, SI_CLIENTLIST *clptr)
{
	if (!chptr->ch_ops && chptr->ch_numclients &&
		!CliIsFromSplit(cptr) &&
		(clptr->cl_modes & SI_CHANNEL_MODE_CHANOP))
	{
		/* channel just lost ops not from a split */

		CHANFIX_DEBUG(1, "cli_rem(): ops was just lost in channel \"%s\", client \"%s\" left irc",
					chptr->ch_name, clptr->cl_cptr->c_name);

		if (chptr->ch_numclients < SICFConf.cfc_clients_needed)
		{
			CHANFIX_DEBUG(1, "cli_rem(): not enough clients on channel \"%s\" (%d < %d)", chptr->ch_name, chptr->ch_numclients, SICFConf.cfc_clients_needed);
		}
		else
		{
			(void)_chanfix_automatic_reop(chptr, NULL);
		}
	}
}

static void _channel_clmode_changed(SI_CHANNEL *chptr, SI_CLIENTLIST *clptr, u_int old_modes)
{
	if (!chptr->ch_ops &&
			!(clptr->cl_modes & SI_CHANNEL_MODE_CHANOP) &&
			(old_modes & SI_CHANNEL_MODE_CHANOP))
	{
		/* channel just lost ops */

		CHANFIX_DEBUG(1, "clmode_changed(): ops was just lost in channel \"%s\", client \"%s\" was deopped.",
					chptr->ch_name, clptr->cl_cptr->c_name);

		if (chptr->ch_numclients < SICFConf.cfc_clients_needed)
		{
			CHANFIX_DEBUG(1, "clmode_changed(): not enough clients on channel \"%s\" (%d < %d)", chptr->ch_name, chptr->ch_numclients, SICFConf.cfc_clients_needed);
		}
		else
		{
			(void)_chanfix_automatic_reop(chptr, NULL);
		}
	}
}

static void _sjoin_done(SI_CHANNEL *chptr)
{
}


static int _conf_flags(void *csc_cookie, char *args, void **cb_arg)
{
	SI_CHANFIX_CONF	*cfc = *((SI_CHANFIX_CONF **)cb_arg);
	char			*arg = csconfig_next_arg(&args);

	if (!arg || !*arg)
	{
		csconfig_error(csc_cookie, CSC_SEVERITY_ERROR, "no flag specified");
		return EINVAL;
	}

	for(;;)
	{
		if (!strcasecmp(arg, "reop_disable"))
		{
			cfc->cfc_flags |= SI_CFC_FLAGS_REOP_DISABLE;
		}
		else if (!strcasecmp(arg, "chanfix_disable"))
		{
			cfc->cfc_flags |= SI_CFC_FLAGS_CHANFIX_DISABLE;
		}
		else if (!strcasecmp(arg, "chanfix_moderate"))
		{
			cfc->cfc_flags |= SI_CFC_FLAGS_CHANFIX_MODERATE;
		}
		else if (!strcasecmp(arg, "manual_override"))
		{
			cfc->cfc_flags |= SI_CFC_FLAGS_MANUAL_OVERRIDE;
		}
		else if (!strcasecmp(arg, "auto_override"))
		{
			cfc->cfc_flags |= SI_CFC_FLAGS_AUTO_OVERRIDE;
		}
		else if (!strcasecmp(arg, "both_override"))
		{
			cfc->cfc_flags |= SI_CFC_FLAGS_AUTO_OVERRIDE|
								SI_CFC_FLAGS_MANUAL_OVERRIDE;
		}
		else
		{
			csconfig_error(csc_cookie, CSC_SEVERITY_ERROR, "unknown flag");
			return EINVAL;
		}
		arg = csconfig_next_arg(&args);
		if (!arg || !*arg)
			break;
	}

	return 0;
}

static int _conf_uhignore(void *csc_cookie, char *args, void **cb_arg)
{
	SI_CHANFIX_CONF		*cfc = *((SI_CHANFIX_CONF **)cb_arg);
	char				*arg = csconfig_next_arg(&args);
#if 0
	char				*flag = csconfig_next_arg(&args);
#endif
	u_int				len;
	SI_CHANFIX_IGNORE	*ig;

	if (!arg || !*arg)
	{
		csconfig_error(csc_cookie, CSC_SEVERITY_ERROR, "no userhost specified");
		return EINVAL;
	}

	len = strlen(arg);

	ig = (SI_CHANFIX_IGNORE *)simempool_alloc(&SICFMemPool,
							sizeof(SI_CHANFIX_IGNORE) + len);
	if (!ig)
		return ENOMEM;

	strcpy(ig->ig_string, arg);
	ig->ig_flags = 0;
	ig->ig_next = cfc->cfc_uhignores;
	cfc->cfc_uhignores = ig;

	

	return 0;
}

static int _conf_chanignore(void *csc_cookie, char *args, void **cb_arg)
{
	SI_CHANFIX_CONF		*cfc = *((SI_CHANFIX_CONF **)cb_arg);
	char				*arg = csconfig_next_arg(&args);
	char				*flag = csconfig_next_arg(&args);
	u_int				len;
	SI_CHANFIX_IGNORE	*ig;

	if (!arg || !*arg)
	{
		csconfig_error(csc_cookie, CSC_SEVERITY_ERROR, "no channel specified");
		return EINVAL;
	}

	len = strlen(arg);

	ig = (SI_CHANFIX_IGNORE *)simempool_alloc(&SICFMemPool,
							sizeof(SI_CHANFIX_IGNORE) + len);
	if (!ig)
		return ENOMEM;

	strcpy(ig->ig_string, arg);
	ig->ig_next = cfc->cfc_chanignores;
	cfc->cfc_chanignores = ig;
	ig->ig_flags = 0;

	while(flag && *flag)
	{
		if (!strcasecmp(flag, "manual"))
			ig->ig_flags |= SI_IG_FLAGS_MANUAL;
		else if (!strcasecmp(flag, "auto"))
			ig->ig_flags |= SI_IG_FLAGS_AUTO;
		else if (!strcasecmp(flag, "both") || !strcasecmp(flag, "uber"))
			ig->ig_flags |= SI_IG_FLAGS_MANUAL|SI_IG_FLAGS_AUTO;
		else
			csconfig_error(csc_cookie, CSC_SEVERITY_WARNING, "unknown flag");
		flag = csconfig_next_arg(&args);
	}

	if (!ig->ig_flags)
		ig->ig_flags = SI_IG_FLAGS_AUTO;

	return 0;
}

static int _conf_chanfixdb(void *csc_cookie, char *args, void **cb_arg)
{
	SI_CHANFIX_CONF		*cfc = *((SI_CHANFIX_CONF **)cb_arg);
	char				*arg = csconfig_next_arg(&args);

	if (!arg || !*arg)
	{
		csconfig_error(csc_cookie, CSC_SEVERITY_ERROR, "no chanfixdb specified");
		return EINVAL;
	}

	if (cfc->cfc_chanfixdb)
	{
		csconfig_error(csc_cookie, CSC_SEVERITY_ERROR, "chanfixdb already specified");
		return EALREADY;
	}

	cfc->cfc_chanfixdb = simempool_strdup(&SICFMemPool, arg);
	if (!cfc->cfc_chanfixdb)
	{
		return ENOMEM;
	}

	return 0;
}

static int _conf_checkfreq(void *csc_cookie, char *args, void **cb_arg)
{
	SI_CHANFIX_CONF	*cfc = *((SI_CHANFIX_CONF **)cb_arg);
	char			*arg = csconfig_next_arg(&args);
	int				num;

	if (!arg || !*arg)
	{
		csconfig_error(csc_cookie, CSC_SEVERITY_ERROR, "no frequency specified");
		return EINVAL;
	}

	num = atoi(arg);
	if (num <= 0)
	{
		csconfig_error(csc_cookie, CSC_SEVERITY_ERROR, "invalid check frequency");
		return EINVAL;
	}

	cfc->cfc_checkfreq = num;

	return 0;
}

static int _conf_rollfreq(void *csc_cookie, char *args, void **cb_arg)
{
	SI_CHANFIX_CONF	*cfc = *((SI_CHANFIX_CONF **)cb_arg);
	char			*arg = csconfig_next_arg(&args);
	int				num;

	if (!arg || !*arg)
	{
		csconfig_error(csc_cookie, CSC_SEVERITY_ERROR, "no frequency specified");
		return EINVAL;
	}

	num = atoi(arg);
	if (num <= 0)
	{
		csconfig_error(csc_cookie, CSC_SEVERITY_ERROR, "invalid roll frequency");
		return EINVAL;
	}
	cfc->cfc_rollfreq = num;
	return 0;
}

static int _conf_checks_needed(void *csc_cookie, char *args, void **cb_arg)
{
	SI_CHANFIX_CONF	*cfc = *((SI_CHANFIX_CONF **)cb_arg);
	char			*arg = csconfig_next_arg(&args);
	int				num;

	if (!arg || !*arg)
	{
		csconfig_error(csc_cookie, CSC_SEVERITY_ERROR, "no argument specified");
		return EINVAL;
	}

	num = atoi(arg);
	if (num < 1)
	{
		csconfig_error(csc_cookie, CSC_SEVERITY_ERROR, "invalid argument");
		return EINVAL;
	}
	cfc->cfc_checks_needed = num;
	return 0;
}

static int _conf_clients_needed(void *csc_cookie, char *args, void **cb_arg)
{
	SI_CHANFIX_CONF	*cfc = *((SI_CHANFIX_CONF **)cb_arg);
	char			*arg = csconfig_next_arg(&args);
	int				num;

	if (!arg || !*arg)
	{
		csconfig_error(csc_cookie, CSC_SEVERITY_ERROR, "no argument specified");
		return EINVAL;
	}

	num = atoi(arg);
	if (num < 1)
	{
		csconfig_error(csc_cookie, CSC_SEVERITY_ERROR, "invalid argument");
		return EINVAL;
	}
	cfc->cfc_clients_needed = num;
	return 0;
}

static int _conf_servers_needed(void *csc_cookie, char *args, void **cb_arg)
{
	SI_CHANFIX_CONF	*cfc = *((SI_CHANFIX_CONF **)cb_arg);
	char			*arg = csconfig_next_arg(&args);
	int				num;

	if (!arg || !*arg)
	{
		csconfig_error(csc_cookie, CSC_SEVERITY_ERROR, "no argument specified");
		return EINVAL;
	}

	num = atoi(arg);
	if (num < 1)
	{
		csconfig_error(csc_cookie, CSC_SEVERITY_ERROR, "invalid argument");
		return EINVAL;
	}
	cfc->cfc_servers_needed = num;
	return 0;
}

static int _conf_nicks_to_op(void *csc_cookie, char *args, void **cb_arg)
{
	SI_CHANFIX_CONF	*cfc = *((SI_CHANFIX_CONF **)cb_arg);
	char			*arg = csconfig_next_arg(&args);
	int				num;

	if (!arg || !*arg)
	{
		csconfig_error(csc_cookie, CSC_SEVERITY_ERROR, "no argument specified");
		return EINVAL;
	}

	num = atoi(arg);
	if (num < 1)
	{
		csconfig_error(csc_cookie, CSC_SEVERITY_ERROR, "invalid argument");
		return EINVAL;
	}

	cfc->cfc_nicks_to_op = num;

	return 0;
}

static int _conf_wait_min_time(void *csc_cookie, char *args, void **cb_arg)
{
	SI_CHANFIX_CONF	*cfc = *((SI_CHANFIX_CONF **)cb_arg);
	char			*arg = csconfig_next_arg(&args);
	int				num;

	if (!arg || !*arg)
	{
		csconfig_error(csc_cookie, CSC_SEVERITY_ERROR, "no argument specified");
		return EINVAL;
	}

	num = atoi(arg);
	if ((num <= 0) || (num > cfc->cfc_reop_wait_time_max))
	{
		csconfig_error(csc_cookie, CSC_SEVERITY_ERROR, "invalid argument");
		return EINVAL;
	}

	cfc->cfc_reop_wait_time_min = num;

	return 0;
}

static int _conf_wait_max_time(void *csc_cookie, char *args, void **cb_arg)
{
	SI_CHANFIX_CONF	*cfc = *((SI_CHANFIX_CONF **)cb_arg);
	char			*arg = csconfig_next_arg(&args);
	int				num;

	if (!arg || !*arg)
	{
		csconfig_error(csc_cookie, CSC_SEVERITY_ERROR, "no argument specified");
		return EINVAL;
	}

	num = atoi(arg);
	if ((num <= 0) || (num < cfc->cfc_reop_wait_time_min))
	{
		csconfig_error(csc_cookie, CSC_SEVERITY_ERROR, "invalid argument");
		return EINVAL;
	}

	cfc->cfc_reop_wait_time_max = num;

	return 0;
}

static int _conf_time_between_fixes(void *csc_cookie, char *args, void **cb_arg)
{
	SI_CHANFIX_CONF	*cfc = *((SI_CHANFIX_CONF **)cb_arg);
	char			*arg = csconfig_next_arg(&args);
	int				num;

	if (!arg || !*arg)
	{
		csconfig_error(csc_cookie, CSC_SEVERITY_ERROR, "no argument specified");
		return EINVAL;
	}

	num = atoi(arg);
	if (num < 0)
	{
		csconfig_error(csc_cookie, CSC_SEVERITY_ERROR, "invalid argument");
		return EINVAL;
	}

	cfc->cfc_time_between_fixes = num;

	return 0;
}

static int _do_oplist(SI_CLIENT *from, SI_CLIENT *to, char *args)
{
	SI_CHANFIX_CHAN			*cfc;
	SI_CHANFIX_CHANUSER		*curec;
	SI_CHANFIX_SCOREINFO	*scores;
	u_int					scores_num = 0;
	u_int					scores_max = 10;
	char					*chname;
	u_int					pos;
	u_int					i;
	u_int					ii;
	int						err;
	char					buf[80];
	char					noticebuf[475];

	assert(from && to && CliIsOper(from));

	if (!simodadmin_client_is_authed(from))
	{
		si_log(SI_LOG_NOTICE, "mod_chanfix: \"oplist%s%s\" tried from non-admin: %s!%s on %s [%s]",
            args?" ":"",args?args:"",
            from->c_name, from->c_userhost,
            from->c_servername, from->c_info);
		siclient_send_privmsg(to, from, "Unknown command.");
        return 0;
	}

	chname = si_next_subarg_space(&args);
	if (!chname || !*chname)
	{
		siclient_send_privmsg(to, from, "Usage: OPLIST #channel");
		return 0;
	}

	if (strlen(chname) > 250)
		chname[250] = '\0';

	snprintf(noticebuf, sizeof(noticebuf), "OPLIST %s requested by %s!%s from %s",
		chname, from->c_name, from->c_userhost,
		from->c_servername);

	simodadmin_notice_authed_clients(to, noticebuf);

	err = chanfixdb_channel_find(SIChanFixDB, chname, &cfc);
	if (err)
	{
		siclient_send_privmsg(to, from, "Channel \"%s\" does not exist in my database", chname);
		si_log(SI_LOG_NOTICE, "mod_chanfix: \"oplist %s\" tried but not found in database from: %s!%s on %s [%s]",
			chname,
            from->c_name, from->c_userhost,
            from->c_servername, from->c_info);
		return 0;
	}

	scores = (SI_CHANFIX_SCOREINFO *)simempool_alloc(&SICFMemPool,
						sizeof(SI_CHANFIX_SCOREINFO) * scores_max);
	if (!scores)
	{
		siclient_send_privmsg(to, from, "Out of memory");
		return ENOMEM;
	}

    for(ii=0;ii<SI_CHANFIX_CHANUSER_HASHSIZE;ii++)
    for(pos=cfc->cfc_users_offset[ii];pos!=-1;pos=curec->cfcu_h_next)
    {
        curec = SI_CHANFIX_CHANUSER_PTR(SIChanFixDB, pos);

		for(i=0;i<scores_num;i++)
		{
			if (curec->cfcu_tot_score > scores[i].score)
				break;
		}

		if (i == scores_max)
		{
			continue;
		}

		if (scores_num < scores_max)
			scores_num++;

		memmove(scores+i+1, scores+i,
			sizeof(SI_CHANFIX_SCOREINFO) * (scores_max - i - 1));
		scores[i].score = curec->cfcu_tot_score;
		scores[i].vptr = (void *)curec;
	}

	if (!scores_num)
	{
		simempool_free(scores);
		siclient_send_privmsg(to, from, "Couldn't find any userhosts for %s", chname);
		si_log(SI_LOG_NOTICE, "mod_chanfix: \"oplist %s\" tried, but no users found from: %s!%s on %s [%s]",
			chname,
            from->c_name, from->c_userhost,
            from->c_servername, from->c_info);
		return 0;
	}

	siclient_send_privmsg(to, from, "OPLIST for %s:", chname);

	for(i=0;i<scores_num;i++)
	{
		curec = (SI_CHANFIX_CHANUSER *)scores[i].vptr;
		siclient_send_privmsg(to, from, "%5d - %s -- %s",
			curec->cfcu_tot_score,
			curec->cfcu_userhost,
			si_timestr(curec->cfcu_last_seen, buf, sizeof(buf)));
	}

	siclient_send_privmsg(to, from, "End of list");

	si_log(SI_LOG_NOTICE, "mod_chanfix: \"oplist %s\" from: %s!%s on %s [%s]",
			chname,
            from->c_name, from->c_userhost,
            from->c_servername, from->c_info);

	simempool_free(scores);

	return 0;

}

static int _do_chanfix(SI_CLIENT *from, SI_CLIENT *to, char *args)
{
	SI_CHANNEL		*chptr;
	char			*chname;
	char			*flag;
	char			noticebuf[475];

	assert(from && to && CliIsOper(from));

	chname = si_next_subarg_space(&args);
	if (!chname || !*chname)
	{
		if (SICFConf.cfc_flags & (SI_CFC_FLAGS_AUTO_OVERRIDE|
									SI_CFC_FLAGS_MANUAL_OVERRIDE))
			siclient_send_privmsg(to, from, "Usage: CHANFIX #channel [override]");
		else
			siclient_send_privmsg(to, from, "Usage: CHANFIX #channel");
		return 0;
	}

	if (!simodadmin_client_is_authed(from))
	{
		si_log(SI_LOG_NOTICE, "\"chanfix %s%s%s\" tried from non-admin: %s!%s on %s [%s]",
            chname, args?" ":"",args?args:"",
            from->c_name, from->c_userhost,
            from->c_servername, from->c_info);
		siclient_send_privmsg(to, from, "You need to be an admin to use this command.");
        return 0;
	}

	if (SICFConf.cfc_flags & SI_CFC_FLAGS_CHANFIX_DISABLE)
	{
		si_log(SI_LOG_NOTICE, "\"chanfix %s%s%s\" tried, but disabled from: %s!%s on %s [%s]",
            chname, args?" ":"",args?args:"",
            from->c_name, from->c_userhost,
            from->c_servername, from->c_info);
		siclient_send_privmsg(to, from, "Sorry, chanfix is currently disabled.");
		return 0;
	}

	if (strlen(chname) > 250)
		chname[250] = '\0';

	snprintf(noticebuf, sizeof(noticebuf), "CHANFIX %s%s%s requested by %s!%s from %s",
		chname, args?" " :"",args?args:"",from->c_name, from->c_userhost,
		from->c_servername);

	simodadmin_notice_authed_clients(to, noticebuf);

	chptr = sichannel_find(chname);
	if (!chptr)
	{
		siclient_send_privmsg(to, from, "Channel \"%s\" does not exist.",
				chname);
		si_log(SI_LOG_NOTICE, "\"chanfix %s%s%s\" tried but not found from: %s!%s on %s [%s]",
			chname,
            args?" ":"",args?args:"",
            from->c_name, from->c_userhost,
            from->c_servername, from->c_info);
		return 0;
	}

	flag = si_next_subarg_space(&args);

	(void)_chanfix_manual_reop(chptr, from, to, flag && !strcmp(flag, "override"));	

	return 0;
}

static int _do_score(SI_CLIENT *from, SI_CLIENT *to, char *args)
{
	SI_CHANFIX_CHAN		*chan;
	SI_CHANFIX_CHANUSER	*chanuser;
	char				*chname;
	int					err;
	char				*uh;
	int					max_scores;

	assert(from && to && CliIsOper(from));

	chname = si_next_subarg_space(&args);
	if (!chname || !*chname)
	{
		siclient_send_privmsg(to, from, "Usage: SCORE #channel [nick | user@host]");
		return 0;
	}

	uh = si_next_subarg_space(&args);
	if (uh && !*uh)
		uh = NULL;

#if 0
	if (!simodadmin_client_is_authed(from))
	{
		si_log(SI_LOG_NOTICE, "\"score %s%s%s\" tried from non-admin: %s!%s on %s [%s]",
			chname, uh ? " " : "", uh ? uh : "",
            from->c_name, from->c_userhost,
            from->c_servername, from->c_info);
		siclient_send_privmsg(to, from, "You need to be an admin to use this command.");
        return 0;
	}
#endif

	if (strlen(chname) > 250)
		chname[250] = '\0';

	err = chanfixdb_channel_find(SIChanFixDB, chname, &chan);
	if (err)
	{
		siclient_send_privmsg(to, from,
			"Couldn't find channel \"%s\" in the database.", chname);
		return 0;
	}

	if (!uh)
	{
		char					buf[255];
		char					*curptr = buf;
		char					*ending = buf + sizeof(buf);
		SI_CHANFIX_CHANUSER		*curec;
		SI_CHANNEL				*chptr;
		u_int					pos = chan->cfc_users_sorted_last;
		SI_CHANFIX_SCOREINFO	*scores;
		int						i;

		if (pos == (u_int)-1)
		{
			chanfixdb_channel_release(SIChanFixDB, chan);
			siclient_send_privmsg(to, from,
				"No users found in the database for channel \"%s\".",
					chname);
			return 0;
		}

		max_scores = CHANFIX_SCORES_NUM;
		siclient_send_privmsg(to, from,
				"Top %d scores for channel \"%s\" in the database:",
				max_scores, chname);
		for(;(pos != (u_int)-1) && max_scores--;pos=curec->cfcu_s_prev)
		{
	        curec = SI_CHANFIX_CHANUSER_PTR(SIChanFixDB, pos);

			for(;;)
			{
				err = snprintf(curptr, ending-curptr,
						"%u, ", curec->cfcu_tot_score);
				if (err >= (ending - curptr))
				{
					if (curptr == buf) /* don't see this happening */
						break;
					*(curptr-2) = '\0';	/* chop off ,*space* */
					siclient_send_privmsg(to, from, "%s", buf);
					curptr = buf;
					continue;
				}
				else if (err < 0)
				{
					siclient_send_privmsg(to, from,
						"%d", curec->cfcu_tot_score);
				}
				curptr += err;
				break;
			}
		}
		if (curptr != buf)
		{
			*(curptr-2) = '\0';	/* chop off ,*space* */
			siclient_send_privmsg(to, from, "%s", buf);
		}
		chanfixdb_channel_release(SIChanFixDB, chan);

		chptr = sichannel_find(chname);
		if (!chptr)
			return 0;

		max_scores = CHANFIX_SCORES_NUM;
		scores = (SI_CHANFIX_SCOREINFO *)simempool_alloc(
					&SICFMemPool,
					sizeof(SI_CHANFIX_SCOREINFO) * max_scores);
		memset(scores, 0, sizeof(SI_CHANFIX_SCOREINFO) * max_scores);

		siclient_send_privmsg(to, from,
				"Top %d scores for current ops in channel \"%s\":",
				max_scores, chname);
		max_scores = _chanfix_get_scores(chptr, chan, scores, max_scores, 0, 0x001);
		curptr = buf;
		for(i=0;i<max_scores;i++)
		{
			for(;;)
			{
				err = snprintf(curptr, ending-curptr,
						"%u, ", scores[i].score);
				if (err >= (ending - curptr))
				{
					if (curptr == buf) /* don't see this happening */
						break;
					*(curptr-2) = '\0';	/* chop off ,*space* */
					siclient_send_privmsg(to, from, "%s", buf);
					curptr = buf;
					continue;
				}
				else if (err < 0)
				{
					siclient_send_privmsg(to, from,
						"%d", curec->cfcu_tot_score);
				}
				curptr += err;
				break;
			}
		}

		if (curptr != buf)
		{
			*(curptr-2) = '\0';	/* chop off ,*space* */
			siclient_send_privmsg(to, from, "%s", buf);
		}

		memset(scores, 0, sizeof(SI_CHANFIX_SCOREINFO) * max_scores);
		max_scores = CHANFIX_SCORES_NUM;

		siclient_send_privmsg(to, from,
				"Top %d scores for current non-ops in channel \"%s\":",
				max_scores, chname);
		max_scores = _chanfix_get_scores(chptr, chan, scores, max_scores, 0, 0);
		curptr = buf;
		for(i=0;i<max_scores;i++)
		{
			for(;;)
			{
				err = snprintf(curptr, ending-curptr,
						"%u, ", scores[i].score);
				if (err >= (ending - curptr))
				{
					if (curptr == buf) /* don't see this happening */
						break;
					*(curptr-2) = '\0';	/* chop off ,*space* */
					siclient_send_privmsg(to, from, "%s", buf);
					curptr = buf;
					continue;
				}
				else if (err < 0)
				{
					siclient_send_privmsg(to, from,
						"%d", curec->cfcu_tot_score);
				}
				curptr += err;
				break;
			}
		}

		if (curptr != buf)
		{
			*(curptr-2) = '\0';	/* chop off ,*space* */
			siclient_send_privmsg(to, from, "%s", buf);
		}
		
		simempool_free(scores);

		return 0;
	}

	if (strchr(uh, '@') == NULL)
	{
		SI_CLIENT *cli = siclient_find(uh);

		if (!cli || !CliIsClient(cli))
		{
			chanfixdb_channel_release(SIChanFixDB, chan);
			siclient_send_privmsg(to, from,
					"Client \"%s\" doesn't exist.", uh);
			return 0;
		}
		uh = cli->c_userhost;
	}

	err = chanfixdb_chanuser_find(SIChanFixDB, chan, uh, &chanuser);
	if (err)
	{
		chanfixdb_channel_release(SIChanFixDB, chan);
		siclient_send_privmsg(to, from,
			"Couldn't find user \"%s\" in the database for channel \"%s\".", uh, chname);
		return 0;
	}

	siclient_send_privmsg(to, from,
		"User \"%s\"'s score in channel \"%s\": %u",
			uh, chname, chanuser->cfcu_tot_score);

	chanfixdb_chanuser_release(SIChanFixDB, chanuser);
	chanfixdb_channel_release(SIChanFixDB, chan);

	return 0;
}

