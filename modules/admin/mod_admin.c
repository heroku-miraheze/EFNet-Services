/*
**   IRC services admin module -- mod_admin.c
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
#include "mod_admin.h"
#include "admindb.h"

RCSID("$Id: mod_admin.c,v 1.3 2004/03/30 22:23:44 cbehrens Exp $");

#define SIADMIN_DEFAULT_MINLEN	4


static int _conf_admindb(void *csc_cookie, char *args, void **cb_arg);
static int _conf_flags(void *csc_cookie, char *args, void **cb_arg);
static int _conf_minlen(void *csc_cookie, char *args, void **cb_arg);
static int _do_logout(SI_CLIENT *from, SI_CLIENT *to, char *args);
static int _do_login(SI_CLIENT *from, SI_CLIENT *to, char *args);
static int _do_passwd(SI_CLIENT *from, SI_CLIENT *to, char *args);
static int _simodadmin_client_auth(SI_CLIENT *cptr, char *account);
static int _simodadmin_client_unauth(SI_CLIENT *cptr);
static char *_simodadmin_crypt_passwd(char *passwd);
static void _conf_error_func(u_int severity, char *error, char *filename, u_int lineno);
static void _conf_deinit(void);
static int _simodule_init(void *modhandle, char *args);
static void _simodule_deinit(void);

typedef struct _si_admin_conf		SI_ADMIN_CONF;
typedef struct _si_admin_auth		SI_ADMIN_AUTH;

struct _si_admin_conf
{
	char		*ac_admindb;
	u_int		ac_flags;
	char		ac_passwd_minlen;
};

struct _si_admin_auth
{
	char		*aa_account;
	time_t		aa_time;
};

static CSC_COMMAND	Commands[] = {
	{	"admindb",		_conf_admindb,	0,		NULL },
	{	"flags",		_conf_flags,	0,		NULL },
	{	"flag",			_conf_flags,	0,		NULL },
	{	"passwd_minlen",_conf_minlen,	0,		NULL },
	{	NULL,			NULL,			0,		NULL }
};

static SI_MOD_COMMAND	_ACommands[] = {
    {   "admin",	"Login to services",	SI_MC_FLAGS_OPERONLY|SI_MC_FLAGS_NOLOG,	_do_login	},
    {   "login",	"Login to services",	SI_MC_FLAGS_OPERONLY|SI_MC_FLAGS_NOLOG,	_do_login	},
    {   "logout",	"Logout of services",	SI_MC_FLAGS_OPERONLY,	_do_logout	},
    {   "passwd",	"Change your password",	SI_MC_FLAGS_OPERONLY|SI_MC_FLAGS_NOLOG,	_do_passwd	},
    {   NULL,       NULL,                   0,  NULL        }
};

static SI_MOD_CLIENT    SIAClient = {
	"JUPES",    "services", "services.int", "EFNet Services",
		SI_CLIENT_TYPE_CLIENT, SI_CLIENT_UMODE_OPER, 0, _ACommands
};

static SI_MOD_CLIENT    SICFClient = {
	"CHANFIX",    "services", "services.int", "EFNet Services",
		SI_CLIENT_TYPE_CLIENT, SI_CLIENT_UMODE_OPER, 0, _ACommands
};

static void _client_removed(SI_CLIENT *cptr, char *comment);

static SI_MOD_CALLBACKS     SIACallbacks = {
	NULL,
	NULL,
	_client_removed,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL
};             

static SI_ADMIN_CONF		SIAConf;
static SI_MEMPOOL			SIAMemPool;
static SI_ADMINDB			*SIAdminDB = NULL;
static void					*SIModHandle = NULL;


SI_MODULE_DECL("Admin support module",
			&SIAMemPool,
			&SIACallbacks,
			_simodule_init,
			_simodule_deinit
);

static void _conf_error_func(u_int severity, char *error, char *filename, u_int lineno)
{
	int		level = SI_LOG_DEBUG;
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
	si_log(level, "mod_admin: %s:%d: %s%s%s%s",
		filename,
		lineno,
		string,
		*string ? ":" : string,
		*string ? " " : string,
		error);
}

static void _conf_deinit(void)
{
	if (SIAConf.ac_admindb)
		simempool_free(SIAConf.ac_admindb);
}

static int _simodule_init(void *modhandle, char *args)
{
	int				err;
	char			*confname = csconfig_next_arg(&args);
	struct timeval	tv;

	if (!modhandle || !confname || !*confname)
	{
		si_log(SI_LOG_ERROR, "mod_admin: not enough arguments passed to module")
;
		return EINVAL;
	}

	gettimeofday(&tv, NULL);

	srand(((u_int)srand / (getpid()+2)) * (tv.tv_sec/(tv.tv_usec+1)));

	SIModHandle = modhandle;

	memset(&SIAConf, 0, sizeof(SIAConf));

	err = csconfig_read(confname, Commands,
					&SIAConf, _conf_error_func);
	if (err)
	{
		si_log(SI_LOG_ERROR, "mod_admin: couldn't load \"%s\": %d",
			confname, err);
		if (err != ENOENT)
			return err;
	}

	if (!SIAConf.ac_passwd_minlen)
		SIAConf.ac_passwd_minlen = SIADMIN_DEFAULT_MINLEN;

	if (!SIAConf.ac_admindb)
	{
		si_log(SI_LOG_ERROR, "mod_admin: no admindb specified in \"%s\"",
			confname);
		_conf_deinit();
		return EINVAL;
	}

	err = siadmindb_init(SIAConf.ac_admindb, 0, &SIAdminDB);
	if (err)
	{
		_conf_deinit();
		si_log(SI_LOG_ERROR, "mod_admin: couldn't open \"%s\": %d",
			SIAConf.ac_admindb, err);
		return err;
	}

	err = simodule_add_client(SIModHandle, &SIAClient);
	if (err)                                            
	{       
		_conf_deinit();
		return err;
	}

	err = simodule_add_client(SIModHandle, &SICFClient);
	if (err)                                            
	{       
		simodule_del_client(SIModHandle, &SIAClient);
		_conf_deinit();
		return err;
	}

	return 0;
}


static void _simodule_deinit(void)
{
	simodule_del_client(SIModHandle, &SICFClient);
	simodule_del_client(SIModHandle, &SIAClient);
	siadmindb_deinit(SIAdminDB);
	_conf_deinit();
}

static int _conf_minlen(void *csc_cookie, char *args, void **cb_arg)
{
	SI_ADMIN_CONF	*ac = *((SI_ADMIN_CONF **)cb_arg);
	char			*arg = csconfig_next_arg(&args);
	int				ml;

    if (!arg || !*arg)
    {
		csconfig_error(csc_cookie, CSC_SEVERITY_ERROR, "no minlen specified");
		return EINVAL;
    }

	ml = atoi(arg);
	if (ml < 2)
	{
		csconfig_error(csc_cookie, CSC_SEVERITY_ERROR, "invalid minlen specified");
		return EINVAL;
	}

	if (ac->ac_passwd_minlen)
	{
		csconfig_error(csc_cookie, CSC_SEVERITY_ERROR, "minlen already specified");
		return EINVAL;
	}

	ac->ac_passwd_minlen = ml;

	return 0;
}

static int _conf_flags(void *csc_cookie, char *args, void **cb_arg)
{
#if 0
	SI_ADMIN_CONF	*ac = *((SI_ADMIN_CONF **)cb_arg);
#endif
	char			*arg = csconfig_next_arg(&args);

    if (!arg || !*arg)
    {
		csconfig_error(csc_cookie, CSC_SEVERITY_ERROR, "no flag specified");
		return EINVAL;
    }

    for(;;)
    {
#if 0
        if (!strcasecmp(arg, "foo"))
        {
			ac->ac_flags |= foo;
        }
        else
#endif
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

static int _conf_admindb(void *csc_cookie, char *args, void **cb_arg)
{
	SI_ADMIN_CONF	*ac = *((SI_ADMIN_CONF **)cb_arg);
	char			*arg = csconfig_next_arg(&args);

	if (!arg || !*arg)
	{
		csconfig_error(csc_cookie, CSC_SEVERITY_ERROR, "no admindb specified");
		return EINVAL;
	}

	if (ac->ac_admindb)
	{
		csconfig_error(csc_cookie, CSC_SEVERITY_ERROR, "admindb already specified");
		return EALREADY;
	}

	ac->ac_admindb = simempool_strdup(&SIAMemPool, arg);
	if (!ac->ac_admindb)
	{
		return ENOMEM;
	}

	return 0;
}

static int _do_logout(SI_CLIENT *from, SI_CLIENT *to, char *args)
{
	if (!simodadmin_client_is_authed(from))
	{
		siclient_send_privmsg(to, from, "You are not logged in.");
		si_log(SI_LOG_NOTICE, "mod_admin: \"logout %s\" failed from: %s!%s on %s [%s]: Not logged in",
            acct,
            from->c_name,
            from->c_userhost,
            from->c_servername,
            from->c_info);
		return EFAULT;
	}

	(void)_simodadmin_client_unauth(from);
	siclient_send_privmsg(to, from, "You've logged out.");

	return 0;
}

static int _do_login(SI_CLIENT *from, SI_CLIENT *to, char *args)
{
	char				*acct;
	char				*passwd;
	int					ret;
	SI_ADMIN_ACCOUNT	*arec;
	SI_ADMIN_USERHOST	*uhrec;
	char				salt[9];
	char				buf[80];
	char				*str;
	time_t				now;
	time_t				ll;

	assert(from && to && CliIsOper(from));

	acct = si_next_subarg_space(&args);
	passwd = si_next_subarg_space(&args);
	if (!passwd || !*passwd)
	{
		siclient_send_privmsg(to, from, "Usage: LOGIN <username> <password>");
		return 0;
	}

	if (args)
	{
		while(*args == ' ' || *args == '\t')
			args++;
		if (!*args)
			args = NULL;
	}

	ret = simodadmin_client_is_authed(from);
	if (ret)
	{
		if (!args)
		{
			siclient_send_privmsg(to, from, "You are already logged in.");
			return 0;
		}
		return simodule_process_message(from, to, args, 0);
	}

	siadmindb_lock(SIAdminDB);

	ret = siadmindb_account_find(SIAdminDB, acct, &arec);
	if (ret)
	{
		siadmindb_unlock(SIAdminDB);
		siclient_send_privmsg(to, from, "Invalid username/password.");
		si_log(SI_LOG_NOTICE, "mod_admin: \"login %s\" failed from: %s!%s on %s [%s]: Account doesn't exist",
            acct,
            from->c_name,
            from->c_userhost,
            from->c_servername,
            from->c_info);
		return EINVAL;
	}

	now = time(NULL);

	if (now - arec->aa_login_fail_time >= 60)
	{
		arec->aa_login_fail_time = now;
		arec->aa_login_fail_num = 0;
	}
	else if (arec->aa_login_fail_num >= 3)
	{
		if (arec->aa_login_fail_num++ == 3)
		{
			siadmindb_unlock(SIAdminDB);
			siclient_send_privmsg(to, from, "Not authorized...too many failures.  Try again later.");
		}
		else
			siadmindb_unlock(SIAdminDB);
		si_log(SI_LOG_NOTICE, "mod_admin: \"login %s\" failed from: %s!%s on %s [%s]: Too many failures",
            acct,
            from->c_name,
            from->c_userhost,
            from->c_servername,
            from->c_info);
		return EINVAL;
	}

	if (arec->aa_expires && now >= arec->aa_expires)
	{
		arec->aa_login_fail_num++;
		siadmindb_unlock(SIAdminDB);
		siclient_send_privmsg(to, from, "Account is expired or disabled.");
		si_log(SI_LOG_NOTICE, "mod_admin: \"login %s\" failed from: %s!%s on %s [%s]: Account has expired",
            acct,
            from->c_name,
            from->c_userhost,
            from->c_servername,
            from->c_info);
		return EINVAL;
	}

	ret = siadmindb_userhost_find(SIAdminDB, arec, from->c_userhost,
				from->c_servername, &uhrec);
	if (ret)
	{
		arec->aa_login_fail_num++;
		siadmindb_unlock(SIAdminDB);
		siclient_send_privmsg(to, from, "Invalid username/password.");
		si_log(SI_LOG_NOTICE, "mod_admin: \"login %s\" failed from: %s!%s on %s [%s]: Userhost not found for account",
            acct,
            from->c_name,
            from->c_userhost,
            from->c_servername,
            from->c_info);
		return EINVAL;
	}

	strncpy(salt, arec->aa_passwd, sizeof(salt));
	salt[sizeof(salt)-1] = '\0';

	str = crypt(passwd, salt);
	if (!str)
	{
		siadmindb_unlock(SIAdminDB);
		siclient_send_privmsg(to, from, "An error occurred.");
		si_log(SI_LOG_NOTICE, "mod_admin: \"login %s\" failed from: %s!%s on %s [%s]: crypt returned NULL",
			acct,
            from->c_name,
            from->c_userhost,
            from->c_servername,
            from->c_info);
		return EINVAL;
	}

	if (strcmp(str, arec->aa_passwd))
	{
		arec->aa_login_fail_num++;
		siadmindb_unlock(SIAdminDB);
		siclient_send_privmsg(to, from, "Invalid username/password.");
		si_log(SI_LOG_NOTICE, "mod_admin: \"login %s\" failed from: %s!%s on %s [%s]: Wrong password",
            acct,
            from->c_name,
            from->c_userhost,
            from->c_servername,
            from->c_info);
		return EINVAL;
	}

	ret = _simodadmin_client_auth(from, arec->aa_name);
	if (ret)
	{
		siadmindb_unlock(SIAdminDB);
		siclient_send_privmsg(to, from, "An error has occurred");
		si_log(SI_LOG_NOTICE, "mod_admin: \"login %s\" failed from: %s!%s on %s [%s]: client_auth() returned %d",
			acct,
            from->c_name,
            from->c_userhost,
            from->c_servername,
            from->c_info,
			ret);
		return EINVAL;
	}

	if (args)
	{
		siadmindb_unlock(SIAdminDB);

		if (!strcasecmp(args, "admin") ||
				!strncasecmp(args, "admin ", 6) ||
			!strcasecmp(args, "login") ||
				!strncasecmp(args, "login ", 6))
		{
			siclient_send_privmsg(to, from, "Can't recursively call \"login\".");
			si_log(SI_LOG_ERROR, "mod_admin: Recursive call to \"login\" tried from: %s!%s on %s [%s]",
				from->c_name, from->c_userhost,
				from->c_servername, from->c_info);
			ret = EFAULT;
		}
		else
			ret = simodule_process_message(from, to, args, 0);
		/* if doing a command, don't leave them authed */
		_simodadmin_client_unauth(from);
		return ret;
	}

	ll = arec->aa_last_login;
	arec->aa_last_login = now;
	siadmindb_unlock(SIAdminDB);

	siclient_send_privmsg(to, from, "You're now logged in with the following flags: ADMIN.");
	if (!ll)
		siclient_send_privmsg(to, from, "This is your first login.");
	else
		siclient_send_privmsg(to, from, "Your last login was at %s.",
				si_timestr(ll, buf, sizeof(buf)));

	si_log(SI_LOG_NOTICE, "\"login %s\" succeeded from: %s!%s on %s [%s]",
		acct, from->c_name, from->c_userhost,
		from->c_servername, from->c_info);

	return 0;
}

static int _do_passwd(SI_CLIENT *from, SI_CLIENT *to, char *args)
{
	SI_ADMIN_AUTH		*aa;
	SI_ADMIN_ACCOUNT	*arec;
	SI_ADMIN_USERHOST	*uhrec;
	char				*opasswd;
	char				*npasswd;
	int					ret;
	char				salt[9];
	char				*str;
	time_t				now;
	int					nlen;

	assert(from && to && CliIsOper(from));

	opasswd = si_next_subarg_space(&args);
	npasswd = si_next_subarg_space(&args);
	if (!npasswd || !*npasswd)
	{
		siclient_send_privmsg(to, from, "Usage: PASSWD <old_pass> <new_pass>");
		return 0;
	}

	nlen = strlen(npasswd);
	if (nlen < SIAConf.ac_passwd_minlen)
	{
		siclient_send_privmsg(to, from, "New password needs to be at least %d characters long.",
			SIAConf.ac_passwd_minlen);
		si_log(SI_LOG_NOTICE, "mod_admin: \"passwd\" failed from: %s!%s on %s [%s]: New password not long enough",
            from->c_name,
            from->c_userhost,
            from->c_servername,
            from->c_info);
		return EINVAL;
	}

	ret = simodule_data_get(SIModHandle, from, (void *)&aa);
	if (ret)
	{
		siclient_send_privmsg(to, from, "You are not authorized.");
		si_log(SI_LOG_NOTICE, "mod_admin: \"passwd\" failed from: %s!%s on %s [%s]: Not authorized",
            from->c_name,
            from->c_userhost,
            from->c_servername,
            from->c_info);
		return EACCES;
	}

	siadmindb_lock(SIAdminDB);

	ret = siadmindb_account_find(SIAdminDB, aa->aa_account, &arec);
	if (ret)
	{
		siadmindb_unlock(SIAdminDB);
		_simodadmin_client_unauth(from);
		siclient_send_privmsg(to, from, "Account has been removed.");
		si_log(SI_LOG_NOTICE, "mod_admin: \"passwd\" failed from: %s!%s on %s [%s]: Account doesn't exist",
            from->c_name,
            from->c_userhost,
            from->c_servername,
            from->c_info);
		return ENOENT;
	}

	now = time(NULL);

	if (now - arec->aa_passwd_fail_time >= 60)
	{
		arec->aa_passwd_fail_time = now;
		arec->aa_passwd_fail_num = 0;
	}
	else if (arec->aa_passwd_fail_num >= 3)
	{
		if (arec->aa_passwd_fail_num++ == 3)
		{
			siadmindb_unlock(SIAdminDB);
			_simodadmin_client_unauth(from);
			siclient_send_privmsg(to, from, "Too many failures.  Try again later.");
		}
		else
			siadmindb_unlock(SIAdminDB);
		si_log(SI_LOG_NOTICE, "mod_admin: \"passwd\" failed from: %s!%s on %s [%s]: Too many failures",
            from->c_name,
            from->c_userhost,
            from->c_servername,
            from->c_info);
		return EINVAL;
	}

	if (arec->aa_expires && now >= arec->aa_expires)
	{
		arec->aa_passwd_fail_num++;
		siadmindb_unlock(SIAdminDB);
		_simodadmin_client_unauth(from);
		siclient_send_privmsg(to, from, "Account is expired or disabled.");
		si_log(SI_LOG_NOTICE, "mod_admin: \"passwd\" failed from: %s!%s on %s [%s]: Account has expired",
            from->c_name,
            from->c_userhost,
            from->c_servername,
            from->c_info);
		return EINVAL;
	}

	ret = siadmindb_userhost_find(SIAdminDB, arec, from->c_userhost,
				from->c_servername, &uhrec);
	if (ret)
	{
		siadmindb_unlock(SIAdminDB);
		_simodadmin_client_unauth(from);
		siclient_send_privmsg(to, from, "Your user@host has been removed for this account.");
		si_log(SI_LOG_NOTICE, "mod_admin: \"passwd\" failed from: %s!%s on %s [%s]: Userhost not found for account",
            from->c_name,
            from->c_userhost,
            from->c_servername,
            from->c_info);
		return EINVAL;
	}

	strncpy(salt, arec->aa_passwd, sizeof(salt));
	salt[sizeof(salt)-1] = '\0';

	str = crypt(opasswd, salt);
	if (!str)
	{
		siadmindb_unlock(SIAdminDB);
		siclient_send_privmsg(to, from, "An error occurred.");
		si_log(SI_LOG_NOTICE, "mod_admin: \"passwd\" failed from: %s!%s on %s [%s]: crypt() returned NULL",
            from->c_name,
            from->c_userhost,
            from->c_servername,
            from->c_info);
		return EINVAL;
	}

	if (strcmp(str, arec->aa_passwd))
	{
		arec->aa_passwd_fail_num++;
		siadmindb_unlock(SIAdminDB);
		siclient_send_privmsg(to, from, "Old password is not valid.");
		si_log(SI_LOG_NOTICE, "mod_admin: \"passwd\" failed from: %s!%s on %s [%s]: Wrong password",
            from->c_name,
            from->c_userhost,
            from->c_servername,
            from->c_info);
		return EINVAL;
	}

	str = _simodadmin_crypt_passwd(npasswd);
	if (!str)
	{
		siadmindb_unlock(SIAdminDB);
		siclient_send_privmsg(to, from, "An error occurred.");
		si_log(SI_LOG_NOTICE, "mod_admin: \"passwd\" failed from: %s!%s on %s [%s]: crypt_passwd() returned NULL",
            from->c_name,
            from->c_userhost,
            from->c_servername,
            from->c_info);
		return EINVAL;
	}

	strncpy(arec->aa_passwd, str, sizeof(arec->aa_passwd));
	arec->aa_passwd[sizeof(arec->aa_passwd)-1] = '\0';

	siadmindb_unlock(SIAdminDB);

	siclient_send_privmsg(to, from, "Your password has been changed.");

	si_log(SI_LOG_NOTICE, "\"passwd\" succeeded from: %s!%s on %s [%s]",
            from->c_name,
            from->c_userhost,
            from->c_servername,
            from->c_info);

	return 0;
}

static int _simodadmin_client_auth(SI_CLIENT *cptr, char *account)
{
	SI_ADMIN_AUTH	*aa;
	int				err;

	if (!account || !*account)
		return EINVAL;

	aa = (SI_ADMIN_AUTH *)calloc(1,
					sizeof(SI_ADMIN_AUTH)+strlen(account)+1);
	if (!aa)
		return ENOMEM;

	aa->aa_time = time(NULL);
	aa->aa_account = (char *)aa + sizeof(SI_ADMIN_AUTH);
	strcpy(aa->aa_account, account);
	err = simodule_data_add(SIModHandle, cptr, aa);
	if (err)
	{
		free(aa);
	}
	return err;
}

static int _simodadmin_client_unauth(SI_CLIENT *cptr)
{
	SI_ADMIN_AUTH	*aa;
	int				err;

	err = simodule_data_get(SIModHandle, cptr, (void *)&aa);
	if (!err)
	{
		free(aa);
		simodule_data_del(SIModHandle, cptr);
	}
	else
		err = EALREADY;
	return err;
}

static char *_simodadmin_crypt_passwd(char *passwd)
{
	static char saltchars[] = "/.abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
	char		salt[9];
	int			i;

	for(i=0;i<8;i++)
		salt[i] = saltchars[(rand() & 0x3f)];
	salt[sizeof(salt)-1] = '\0';
	return crypt(passwd, salt);
}

static void _client_removed(SI_CLIENT *cptr, char *comment)
{
	(void)_simodadmin_client_unauth(cptr);
}

int simodadmin_client_is_authed(SI_CLIENT *cptr)
{
	return !simodule_data_get(SIModHandle, cptr, NULL);
}

int simodadmin_notice_authed_clients(SI_CLIENT *from, char *buffer)
{
	extern SI_CLIENT	*SIOpers;
	int					err;
	SI_CLIENT			*cptr;

    for(cptr=SIOpers;cptr;cptr=cptr->c_o_next)
    {
        if (!cptr->c_from)
            continue;
		if (!simodadmin_client_is_authed(cptr))
			continue;
        err =siconn_printf(cptr->c_from, ":%s PRIVMSG %s :%s",
            from?from->c_name:SIMyName, cptr->c_name, buffer);
		if (err)
			return err;
	}

	return 0;
}
