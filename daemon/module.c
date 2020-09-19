/*
**   IRC services -- module.c
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
#include <dlfcn.h>

RCSID("$Id: module.c,v 1.3 2004/03/30 22:23:43 cbehrens Exp $");

#define SI_MOD_DATA_HASH_SIZE	((256*1024)-1)

typedef struct si_moddata				SI_MODDATA;
typedef struct si_mod_client_int		SI_MOD_CLIENT_INT;
typedef struct si_mod_cmdlist			SI_MOD_CMDLIST;
typedef struct si_mod_clientlist		SI_MOD_CLIENTLIST;
typedef struct si_module				SI_MODULE;

struct si_moddata
{
	char				*md_addr;
	u_int				md_modid;
	char				*md_data;

	SI_MODDATA			*md_a_prev;
	SI_MODDATA			*md_a_next;
	SI_MODDATA			*md_h_prev;
	SI_MODDATA			*md_h_next;
};

struct si_mod_client_int
{
	SI_MOD_CLIENT		*mcli_mcl;

	SI_CLIENT			*mcli_cptr;
	SI_MOD_CMDLIST		*mcli_cmdlist;

	SI_MOD_CLIENT_INT	*mcli_prev;
	SI_MOD_CLIENT_INT	*mcli_next;
};

struct si_mod_cmdlist
{
	SI_MOD_CLIENT		*cl_mcl;
	SI_MODULE			*cl_module;

	SI_MOD_CMDLIST		*cl_next;
	SI_MOD_CMDLIST		*cl_prev;
};

struct si_mod_clientlist
{
	SI_MOD_CLIENT_INT	*cll_mcli;
	SI_MOD_CMDLIST		*cll_cmdlist;

	SI_MOD_CLIENTLIST	*cll_prev;
	SI_MOD_CLIENTLIST	*cll_next;
};

struct si_module
{
	char					*mod_name;
	void					*mod_handle;

	u_int					mod_id;
	int						mod_debug;
	int						mod_debug_fd;

#define SI_MOD_FLAGS_REMOVED		0x001
	u_int					mod_flags;

	struct si_module_decl	*mod_decl;

	SI_MOD_CLIENTLIST		*mod_modclients;
	SI_MOD_CALLBACKS		mod_callbacks;

	SI_MODULE				*mod_prev;
	SI_MODULE				*mod_next;
};


static SI_MODDATA			*SIModDataHash[SI_MOD_DATA_HASH_SIZE];
static SI_MODDATA			*SIModData = NULL;
static SI_MODULE			*SIModules = NULL;
static SI_MOD_CLIENT_INT	*SIModClients = NULL;
static u_int				SIModId = 0;

static SI_MODULE *_simodule_find(char *name);
static SI_MODDATA *simoddata_find(SI_MODULE *mod, void *addr);
static SI_MOD_CLIENT_INT *simodule_client_find(char *name);
static int _simodule_unload(SI_MODULE *mod, const char **errstr);

static SI_MODULE *_simodule_find(char *name)
{
	SI_MODULE	*mod;

	for(mod=SIModules;mod;mod=mod->mod_next)
		if (!strcmp(mod->mod_name, name))
			break;
	return mod;
}

static SI_MODDATA *simoddata_find(SI_MODULE *mod, void *addr)
{
	SI_MODDATA	*md;

	assert(mod && addr);

	md = SIModDataHash[(u_int)(addr + mod->mod_id) % SI_MOD_DATA_HASH_SIZE];
	for(;md;md=md->md_h_next)
	{
		if ((md->md_addr == addr) &&
				(md->md_modid == mod->mod_id))
			return md;
	}
	return NULL;
}

static SI_MOD_CLIENT_INT *simodule_client_find(char *name)
{
	SI_MOD_CLIENT_INT	*mcli;

	for(mcli=SIModClients;mcli;mcli=mcli->mcli_next)
	{
		if (!strcmp(name, mcli->mcli_mcl->mcl_name))
			return mcli;
	}
	return mcli;
}

static int _simodule_unload(SI_MODULE *mod, const char **errstr)
{
	SI_MOD_CLIENTLIST	*cll;
	SI_MOD_CMDLIST		*cl;

	assert(mod != NULL);

	/* clean up with their deinit didn't clean up */
	while((cll = mod->mod_modclients) != NULL)
	{
		assert(cll->cll_mcli != NULL);

#ifdef DEBUG
		printf("Removing module \"%s\" from client \"%s\"\n",
			mod->mod_name, cll->cll_mcli->mcli_mcl->mcl_name);
#endif
		cl = cll->cll_cmdlist;
		si_ll_del_from_list(cll->cll_mcli->mcli_cmdlist, cl, cl);
		si_ll_del_from_list(mod->mod_modclients, cll, cll);
		if (!cll->cll_mcli->mcli_cmdlist)
		{
#ifdef DEBUG
			printf("Removing module client \"%s\"\n",
				cll->cll_mcli->mcli_mcl->mcl_name);
#endif
			if (cll->cll_mcli->mcli_cptr)
			{
				CliSetDelJupe(cll->cll_mcli->mcli_cptr);
				siclient_free(cll->cll_mcli->mcli_cptr, "Unloading module");
			}
			si_ll_del_from_list(SIModClients, cll->cll_mcli, mcli);
			simempool_free(cll->cll_mcli);
		}
		else
		{
			if (cl->cl_mcl == cll->cll_mcli->mcli_mcl)
			{
				cll->cll_mcli->mcli_mcl = cll->cll_mcli->mcli_cmdlist->cl_mcl;
			}
		}
		simempool_free(cl);
		simempool_free(cll);
	}

	if (mod->mod_decl)
	{
		if (mod->mod_decl->mod_mempool->mp_num_entries)
		{
			si_log(SI_LOG_NOTICE, "cleaning up %d byte%s (%d entr%s) of memory from module %s module",
				mod->mod_decl->mod_mempool->mp_tot_used,
				mod->mod_decl->mod_mempool->mp_tot_used==1?"":"s",
				mod->mod_decl->mod_mempool->mp_num_entries,
				mod->mod_decl->mod_mempool->mp_num_entries==1?"y":"ies",
				mod->mod_name);
		}

		simempool_deinit(mod->mod_decl->mod_mempool);
	}

#ifndef NDEBUG
	{
		SI_MOD_CLIENT_INT	*mcli;

		for(mcli=SIModClients;mcli;mcli=mcli->mcli_next)
			for(cl=mcli->mcli_cmdlist;cl;cl=cl->cl_next)
				assert(cl->cl_module != mod);
	}
#endif

	if (dlclose(mod->mod_handle) != 0)
	{
		if (errstr)
			*errstr = dlerror();
		return -1;
	}

#if 0
	si_ll_del_from_list(SIModules, mod, mod);

	simempool_free(mod->mod_name);
	simempool_free(mod);
#else
	
	mod->mod_flags |= SI_MOD_FLAGS_REMOVED;
	mod->mod_decl = NULL;
	mod->mod_handle = NULL;
	memset(&(mod->mod_callbacks), 0, sizeof(mod->mod_callbacks));

#endif

	return 0;
}

int simodule_init(void)
{
	memset(SIModDataHash, 0, sizeof(SIModDataHash));

	return 0;
}

void simodule_deinit(void)
{
	SI_MODDATA	*md;

	while(SIModData)
	{
		md = SIModData->md_a_next;
		simempool_free(SIModData);
		SIModData = md;
	}
}

int simodule_load(char *name, char *arg, int debug, const char **errstr)
{
	SI_MODULE				*mod;
	void					*handle;
	struct si_module_decl	*sym;
	static char				errbuf[64];
	int						err;
	u_int					their_size;
	u_int					cr = 0;

	mod = _simodule_find(name);
	if (mod && !(mod->mod_flags & SI_MOD_FLAGS_REMOVED))
	{
		if (errstr)
			*errstr = "Module already loaded";
		return EALREADY;
	}

	handle = dlopen(name, RTLD_LAZY|RTLD_GLOBAL);
	if (!handle)
	{
		if (errstr)
			*errstr = dlerror();
		return -1;
	}

	sym = (struct si_module_decl *)dlsym(handle, "simodule_decl");
	if (sym)
	{
		if (sym->mod_version < SIMODULE_MIN_VERSION)
		{
			if (errstr)
				*errstr = "Version too old";
			dlclose(handle);
			return -1;
		}

		if (sym->mod_compat_version != SIMODULE_COMPAT_VERSION)
		{
			if (errstr)
				*errstr = "Version is not compatable";
			dlclose(handle);
			return -1;
		}

		if (sym->mod_init == NULL)
		{
			if (errstr)
				*errstr = "No init function found in simodule_decl";
			dlclose(handle);
			return -1;
		}

		if (sym->mod_deinit == NULL)
		{
			if (errstr)
				*errstr = "No deinit function found in simodule_decl";
			dlclose(handle);
			return -1;
		}

		if (sym->mod_mempool == NULL)
		{
			if (errstr)
				*errstr = "No memory pool found in simodule_decl";
			dlclose(handle);
			return -1;
		}
		err = simempool_init(sym->mod_mempool, "Module's Pool", SIModulePool.mp_flags);
		if (err)
		{
			if (errstr)
			{
				sprintf(errbuf, "simempool_init() for module failed: %d", err);
				*errstr = errbuf;
			}
			dlclose(handle);
			return err;
		}
	}

	if (!mod)
	{
		mod = (SI_MODULE *)simempool_calloc(&SIModulePool, 1, sizeof(SI_MODULE));
		if (!mod)
		{
			if (errstr)
				*errstr = "Out of memory";
			dlclose(handle);
			return ENOMEM;
		}
		mod->mod_name = simempool_strdup(&SIModulePool, name);
		if (!mod->mod_name)
		{
			dlclose(handle);
			simempool_free(mod);
			if (errstr)
				*errstr = "Out of memory";
			return ENOMEM;
		}
		cr = 1;
	}

	mod->mod_debug = debug;
	mod->mod_handle = handle;
	mod->mod_decl = sym;

	if (sym)
	{
		/*
		** if we end up adding more things to SI_MOD_CALLBACKS, we'll
		**  need to check the version of the module to see what size
		**  theirs is...and only copy what they have...
		*/

		if (sym->mod_version == 1)
			their_size = sizeof(SI_MOD_CALLBACKS);
		/* else */

		assert(their_size <= sizeof(SI_MOD_CALLBACKS));

		if (sym->mod_callbacks != NULL)
			memcpy(&(mod->mod_callbacks), sym->mod_callbacks, their_size);
	}

	if (cr)
	{
		mod->mod_id = ++SIModId;
		si_ll_add_to_list(SIModules, mod, mod);
	}

	mod->mod_flags &= ~SI_MOD_FLAGS_REMOVED;

	if (sym)
	{
		err = sym->mod_init((void *)mod, arg);
		if (err)
		{
			if (errstr)
			{
				sprintf(errbuf, "module's init() failed: %d", err);
				*errstr = errbuf;
			}
			(void)_simodule_unload(mod, NULL);
			return -1;
		}
	}

	return 0;
}

int simodule_unload(char *name, const char **errstr)
{
	SI_MODULE	*mod;

	if (!name)
		return EINVAL;

	mod = _simodule_find(name);
	if (!mod || (mod->mod_flags & SI_MOD_FLAGS_REMOVED))
	{
		if (errstr)
			*errstr = "Module does not exist";
		return ENOENT;
	}

	if (mod->mod_decl)
		mod->mod_decl->mod_deinit();

	return _simodule_unload(mod, errstr);
}

int simodule_data_add(void *modhandle, void *addr, void *data)
{
	SI_MODDATA	*md;

	if (!modhandle || !addr)
		return EINVAL;
	md = simoddata_find((SI_MODULE *)modhandle, addr);
	if (md)
		return EEXIST;
	md = (SI_MODDATA *)simempool_alloc(&SIModulePool, sizeof(SI_MODDATA));
	if (!md)
		return ENOMEM;
	md->md_addr = addr;
	md->md_modid = ((SI_MODULE *)modhandle)->mod_id;
	md->md_data = data;
	si_ll_add_to_list(SIModData, md, md_a);
	si_ll_add_to_list(SIModDataHash[(u_int)(addr + md->md_modid) % SI_MOD_DATA_HASH_SIZE], md, md_h);
	return 0;
}

int simodule_data_get(void *modhandle, void *addr, void **dataptr)
{
	SI_MODDATA	*md;

	if (!modhandle || !addr)
		return EINVAL;
	md = simoddata_find((SI_MODULE *)modhandle, addr);
	if (!md)
		return ENOENT;
	if (dataptr)
		*dataptr = md->md_data;
	return 0;
}

int simodule_data_del(void *modhandle, void *addr)
{
	SI_MODDATA *md;

	if (!modhandle || !addr)
		return EINVAL;
	md = simoddata_find((SI_MODULE *)modhandle, addr);
	if (!md)
		return ENOENT;
	si_ll_del_from_list(SIModData, md, md_a);
	si_ll_del_from_list(SIModDataHash[(u_int)(addr + md->md_modid) % SI_MOD_DATA_HASH_SIZE], md, md_h);
	simempool_free(md);
	return 0;
}

int simodule_add_client(void *modhandle, SI_MOD_CLIENT *cptr)
{
	SI_MODULE			*mod = (SI_MODULE *)modhandle;
	SI_MOD_CLIENT_INT	*mcli;
	SI_MOD_CMDLIST		*cl = NULL;
	SI_MOD_COMMAND		*com1;
	SI_MOD_COMMAND		*com2;
	SI_MOD_CLIENTLIST	*cll;
	SI_CLIENT			*cliptr = NULL;

	if (!mod || !cptr)
		return EINVAL;

	if (!cptr->mcl_name)
		return EINVAL;

	switch(cptr->mcl_type)
	{
		case SI_CLIENT_TYPE_CLIENT:
			if (!cptr->mcl_username || !cptr->mcl_hostname)
				return EINVAL;
		case SI_CLIENT_TYPE_SERVER:
			break;
		default:
			return EINVAL;
	}

	mcli = simodule_client_find(cptr->mcl_name);
	if (mcli)
	{
		if (!cptr->mcl_commands)
			cl = NULL;
		else
		for(cl=mcli->mcli_cmdlist;cl;cl=cl->cl_next)
		{	
			if (!(com1=cl->cl_mcl->mcl_commands))
				continue;
			for(;com1->mc_name;com1++)
			{
				for(com2=cptr->mcl_commands;com2->mc_name;com2++)
				{
					if (com1->mc_name == (char *)-1 || com2->mc_name == (char *)-1)
					{
						if (com1->mc_name == com2->mc_name)
							break;
					}
					else if (!strcasecmp(com1->mc_name, com2->mc_name))
						break;
				}
				if (com2->mc_name)
					break;
			}
			if (com1->mc_name)
				break;
		}
		if (cl)
		{
			si_log(SI_LOG_ERROR,
				"can't add client \"%s\" for module \"%s\": command \"%s\" already exists via module \"%s\"",
				cptr->mcl_name==(char *)-1?"<*>":cptr->mcl_name,
				mod->mod_name,
				com2->mc_name==(char *)-1?"<*>":com2->mc_name,
				cl->cl_module->mod_name);
			return EEXIST;
		}
	}
	else
	{
		cliptr = (SI_CLIENT *)simempool_calloc(&SIModulePool, 1, sizeof(SI_CLIENT));
		if (!cliptr)
			return ENOMEM;

		cliptr->c_name = simempool_strdup(&SIModulePool, cptr->mcl_name);
		if (!cliptr->c_name)
		{
			simempool_free(cliptr);
			return ENOMEM;
		}

		cliptr->c_type = cptr->mcl_type;

		if (CliIsClient(cliptr))
		{
			int		ulen;
			int		hlen;

			cliptr->c_username = simempool_strdup(&SIModulePool, cptr->mcl_username);
			if (!cliptr->c_username)
			{
				simempool_free(cliptr->c_name);
				simempool_free(cliptr);
				return ENOMEM;
			}

			ulen = strlen(cptr->mcl_username);
			hlen = strlen(cptr->mcl_hostname);

			cliptr->c_userhost = simempool_alloc(&SIModulePool,
									ulen + hlen + 2);
			if (!cliptr->c_userhost)
			{
				simempool_free(cliptr->c_username);
				simempool_free(cliptr->c_name);
				simempool_free(cliptr);
				return ENOMEM;
			}
			strcpy(cliptr->c_userhost, cptr->mcl_username);
			cliptr->c_userhost[ulen] = '@';
			cliptr->c_hostname = cliptr->c_userhost + ulen + 1;
			strcpy(cliptr->c_hostname, cptr->mcl_hostname);
		}

		cliptr->c_servername = simempool_strdup(&SIModulePool, SIMyName);
		if (!cliptr->c_servername)
		{
			if (cliptr->c_userhost)
				simempool_free(cliptr->c_userhost);
			if (cliptr->c_username)
				simempool_free(cliptr->c_username);
			simempool_free(cliptr->c_name);
			simempool_free(cliptr);
			return ENOMEM;
		}

		cliptr->c_info = simempool_strdup(&SIModulePool, cptr->mcl_info);
		if (!cliptr->c_info)
		{
			simempool_free(cliptr->c_servername);
			if (cliptr->c_userhost)
				simempool_free(cliptr->c_userhost);
			if (cliptr->c_username)
				simempool_free(cliptr->c_username);
			simempool_free(cliptr->c_name);
			simempool_free(cliptr);
			return ENOMEM;
		}

		mcli = (SI_MOD_CLIENT_INT *)simempool_calloc(&SIModulePool, 1, sizeof(SI_MOD_CLIENT_INT));
		if (!mcli)
		{
			simempool_free(cliptr->c_info);
			simempool_free(cliptr->c_servername);
			if (cliptr->c_userhost)
				simempool_free(cliptr->c_userhost);
			if (cliptr->c_username)
				simempool_free(cliptr->c_username);
			simempool_free(cliptr->c_name);
			simempool_free(cliptr);
			return ENOMEM;
		}

		mcli->mcli_cptr = cliptr;
		mcli->mcli_mcl = cptr;
		si_ll_add_to_list(SIModClients, mcli, mcli);
	}

	cll = (SI_MOD_CLIENTLIST *)simempool_calloc(&SIModulePool, 1, sizeof(SI_MOD_CLIENTLIST));
	if (!cll)
	{
		if (cliptr)
		{
			si_ll_del_from_list(SIModClients, mcli, mcli);
			simempool_free(mcli);
			simempool_free(cliptr->c_info);
			simempool_free(cliptr->c_servername);
			if (cliptr->c_userhost)
				simempool_free(cliptr->c_userhost);
			if (cliptr->c_username)
				simempool_free(cliptr->c_username);
			simempool_free(cliptr->c_name);
			simempool_free(cliptr);
		}
		return ENOMEM;
	}
	cl = (SI_MOD_CMDLIST *)simempool_calloc(&SIModulePool, 1, sizeof(SI_MOD_CMDLIST));
	if (!cl)
	{
		simempool_free(cll);
		if (cliptr)
		{
			si_ll_del_from_list(SIModClients, mcli, mcli);
			simempool_free(mcli);
			simempool_free(cliptr->c_info);
			simempool_free(cliptr->c_servername);
			if (cliptr->c_userhost)
				simempool_free(cliptr->c_userhost);
			if (cliptr->c_username)
				simempool_free(cliptr->c_username);
			simempool_free(cliptr->c_name);
			simempool_free(cliptr);
		}
		return ENOMEM;
	}
	cl->cl_mcl = cptr;
	cl->cl_module = mod;
	si_ll_add_to_list(mcli->mcli_cmdlist, cl, cl);
	cll->cll_mcli = mcli;
	cll->cll_cmdlist = cl;
	si_ll_add_to_list(mod->mod_modclients, cll, cll);

	if (cliptr)
	{
		CliSetJupe(cliptr);
		cliptr->c_from = NULL;
		cliptr->c_conn = NULL;
		cliptr->c_creation = Now;
		cliptr->c_umodes = cptr->mcl_umodes;
		siclient_add(NULL, cliptr);
	}

#ifdef DEBUG
	if (cliptr)
		printf("Module client \"%s\" created\n", mcli->mcli_mcl->mcl_name);
	printf("Added module \"%s\" to client \"%s\"\n", mod->mod_name,
				mcli->mcli_mcl->mcl_name);
#endif

	return 0;
}

void simodule_del_client(void *modhandle, SI_MOD_CLIENT *cptr)
{
	SI_MODULE			*mod = (SI_MODULE *)modhandle;
	SI_MOD_CLIENTLIST	*cll;
	SI_MOD_CMDLIST		*cl;

	if (!mod || !cptr)
		return;

	cll = mod->mod_modclients;
	while(cll != NULL)
	{
		assert(cll->cll_mcli != NULL);

		if (ircd_strcmp(cll->cll_mcli->mcli_mcl->mcl_name, cptr->mcl_name))
		{
			cll = cll->cll_next;
			continue;
		}

#ifdef DEBUG
		printf("Removing module \"%s\" from client \"%s\"\n",
			mod->mod_name, cll->cll_mcli->mcli_mcl->mcl_name);
#endif
		cl = cll->cll_cmdlist;
		si_ll_del_from_list(cll->cll_mcli->mcli_cmdlist, cl, cl);
		si_ll_del_from_list(mod->mod_modclients, cll, cll);
		if (!cll->cll_mcli->mcli_cmdlist)
		{
#ifdef DEBUG
			printf("Removing module client \"%s\"\n",
				cll->cll_mcli->mcli_mcl->mcl_name);
#endif
			if (cll->cll_mcli->mcli_cptr)
			{
				CliSetDelJupe(cll->cll_mcli->mcli_cptr);
				siclient_free(cll->cll_mcli->mcli_cptr, "Unloading module");
			}
			si_ll_del_from_list(SIModClients, cll->cll_mcli, mcli);
			simempool_free(cll->cll_mcli);
		}
		else
		{
			if (cl->cl_mcl == cll->cll_mcli->mcli_mcl)
			{
				cll->cll_mcli->mcli_mcl = cll->cll_mcli->mcli_cmdlist->cl_mcl;
			}
		}
		simempool_free(cl);
		simempool_free(cll);
		break;
	}
}

#define SIMODULE_CALLBACK(__a, __b, __c)				\
		do {											\
			for(mod=SIModules;mod;mod=mod->mod_next)	\
			{											\
				if (mod->mod_flags & SI_MOD_FLAGS_REMOVED) 	\
					continue;								\
				if (mod->mod_callbacks.__b != NULL)		\
				{										\
					mod->mod_callbacks.__b __c;			\
				}										\
			}											\
		} while(0)

void simodule_client_added(SI_CLIENT *cptr)
{
	SI_MODULE	*mod;

	if (CliIsClient(cptr))
		SIMODULE_CALLBACK(mod, cb_client_added, (cptr));
	else
		SIMODULE_CALLBACK(mod, cb_server_added, (cptr));
}

void simodule_client_removed(SI_CLIENT *cptr, char *comment)
{
	SI_MODULE	*mod;

	if (CliIsClient(cptr))
		SIMODULE_CALLBACK(mod, cb_client_removed, (cptr, comment));
	else
		SIMODULE_CALLBACK(mod, cb_server_removed, (cptr, comment));
}

void simodule_channel_added(SI_CHANNEL *chptr)
{
	SI_MODULE	*mod;

	SIMODULE_CALLBACK(mod, cb_channel_added, (chptr));
}

void simodule_channel_removed(SI_CHANNEL *chptr)
{
	SI_MODULE	*mod;

	SIMODULE_CALLBACK(mod, cb_channel_removed, (chptr));
}

void simodule_nick_changed(SI_CLIENT *cptr, char *old_nick)
{
	SI_MODULE	*mod;

	SIMODULE_CALLBACK(mod, cb_nick_changed, (cptr, old_nick));
}

void simodule_umode_changed(SI_CLIENT *cptr, u_int old_modes)
{
	SI_MODULE	*mod;

	SIMODULE_CALLBACK(mod, cb_umode_changed, (cptr, old_modes));
}

void simodule_chmode_changed(SI_CHANNEL *chptr, u_int old_modes, char *old_key, int old_limit)
{
	SI_MODULE	*mod;

	SIMODULE_CALLBACK(mod, cb_chmode_changed, (chptr, old_modes, old_key, old_limit));
}

void simodule_topic_changed(SI_CHANNEL *chptr, char *old_topic, char *old_topic_who, time_t old_topic_time)
{
	SI_MODULE	*mod;

	SIMODULE_CALLBACK(mod, cb_topic_changed, (chptr, old_topic, old_topic_who, old_topic_time));
}

void simodule_channel_client_added(SI_CHANNEL *chptr, SI_CLIENT *cptr, SI_CLIENTLIST *clptr)
{
	SI_MODULE	*mod;

	SIMODULE_CALLBACK(mod, cb_channel_client_added, (chptr, cptr, clptr));
}

void simodule_channel_client_removed(SI_CHANNEL *chptr, SI_CLIENT *cptr, SI_CLIENTLIST *clptr)
{
	SI_MODULE	*mod;

	SIMODULE_CALLBACK(mod, cb_channel_client_removed, (chptr, cptr, clptr));
}

void simodule_channel_clmode_changed(SI_CHANNEL *chptr, SI_CLIENTLIST *clptr, u_int old_modes)
{
	SI_MODULE	*mod;

	SIMODULE_CALLBACK(mod, cb_channel_clmode_changed, (chptr, clptr, old_modes));
}

void simodule_channel_ban_added(SI_CHANNEL *chptr, SI_CLIENT *from, SI_BAN *ban)
{
	SI_MODULE	*mod;

	SIMODULE_CALLBACK(mod, cb_channel_ban_added, (chptr, from, ban));
}

void simodule_channel_ban_removed(SI_CHANNEL *chptr, SI_CLIENT *from, SI_BAN *ban)
{
	SI_MODULE	*mod;

	SIMODULE_CALLBACK(mod, cb_channel_ban_removed, (chptr, from, ban));
}

void simodule_sjoin_done(SI_CHANNEL *chptr)
{
	SI_MODULE	*mod;

	SIMODULE_CALLBACK(mod, cb_sjoin_done, (chptr));
}

int simodule_process_message(SI_CLIENT *from, SI_CLIENT *to, char *args, int not)
{
	char				*command = si_next_subarg_space(&args);
	SI_MOD_CLIENT_INT	*mcli;
	SI_MOD_CMDLIST		*cl;
	SI_MOD_COMMAND		*com;
	int					num = 0;
	int					lusers_commands = 0;
	int					help_commands = 0;
	int					err;
	char				argssave[512];

	if (!CliIsClient(from))
		return 0;
	if (!command || !*command)
		return 0;

	mcli = simodule_client_find(to->c_name);
	if (!mcli)
		return 0;

	if (not)	/* ignore notices */
		return 0;

	for(cl=mcli->mcli_cmdlist;cl;cl=cl->cl_next)
	{
		for(com=cl->cl_mcl->mcl_commands;com&&com->mc_name;com++)
		{
			num++;
			if (!(com->mc_flags & SI_MC_FLAGS_NOHELP))
				help_commands++;
			if (!(com->mc_flags & SI_MC_FLAGS_OPERONLY))
				lusers_commands++;
			if ((com->mc_name != (char *)-1) &&
					!strcasecmp(com->mc_name, command))
			{
				if (!CliIsOper(from) &&
					(com->mc_flags & SI_MC_FLAGS_OPERONLY))
				{
					si_log(SI_LOG_NOTICE, "\"%s%s%s\" tried from non-oper: %s!%s on %s [%s]",
						command,
						args ? " " : "",
						args ? args : "",
						from->c_name,
						from->c_userhost,
						from->c_servername,
						from->c_info);
					return 0;
				}

				if (args &&
					!(com->mc_flags & SI_MC_FLAGS_NOLOG))
				{
					strncpy(argssave, args, sizeof(argssave)-1);
					argssave[sizeof(argssave)-1] = '\0';
				}

				err = com->mc_func(from, to, args);
				if (!err)
				{
					if (!(com->mc_flags & SI_MC_FLAGS_NOLOG)) /* if this line is removed, don't forget to fix the one above, too */
					{
						si_log(SI_LOG_NOTICE, "\"%s%s%s\" succeeded from: %s%s!%s on %s [%s]",
							command,
							args ? " " : "",
							args ? argssave : "",
							CliIsOper(from) ? "*" : "",
							from->c_name,
							from->c_userhost,
							from->c_servername,
							from->c_info);
					}
				}
				return 0;
			}
		}
	}

	for(cl=mcli->mcli_cmdlist;cl;cl=cl->cl_next)
	{
		for(com=cl->cl_mcl->mcl_commands;com&&com->mc_name;com++)
		{
			if (com->mc_name == (char *)-1)
			{
				if (!CliIsOper(from) &&
					(com->mc_flags & SI_MC_FLAGS_OPERONLY))
				{
					si_log(SI_LOG_NOTICE, "\"%s%s%s\" tried from non-oper: %s!%s on %s [%s]",
						command,
						args ? " " : "",
						args ? args : "",
						from->c_name,
						from->c_userhost,
						from->c_servername,
						from->c_info);
					return 0;
				}
				if (args &&
					!(com->mc_flags & SI_MC_FLAGS_NOLOG))
				{
					strncpy(argssave, args, sizeof(argssave)-1);
					argssave[sizeof(argssave)-1] = '\0';
				}
				err = com->mc_func(from, to, args);
				if (!err)
				{
					if (!(com->mc_flags & SI_MC_FLAGS_NOLOG)) /* if this line is removed, don't forget to fix the one above, too */
					{
						si_log(SI_LOG_NOTICE, "\"%s%s%s\" succeeded from: %s%s!%s on %s [%s]",
							command,
							args ? " " : "",
							args ? argssave : "",
							CliIsOper(from) ? "*" : "",
							from->c_name,
							from->c_userhost,
							from->c_servername,
							from->c_info);
					}
				}
				return 0;
			}
		}
	}

	if (!help_commands || (!lusers_commands && !CliIsOper(from)))
		return 0;

	if (num)
	{
		if (!strcasecmp(command, "help"))
		{
			siclient_send_privmsg(to, from, "Following are the commands that I recognize:");
			for(cl=mcli->mcli_cmdlist;cl;cl=cl->cl_next)
			{
				for(com=cl->cl_mcl->mcl_commands;com&&com->mc_name;com++)
				{
					if (!(com->mc_flags & SI_MC_FLAGS_NOHELP))
					{
						siclient_send_privmsg(to, from, "   %s%s%s",
							com->mc_name,
							com->mc_desc?" -- ":"",
							com->mc_desc?com->mc_desc:"");
					}
				}
			}
			siclient_send_privmsg(to, from, "End of list");
		}
		else
		{
			siclient_send_privmsg(to, from, "Unknown command.");
		}
	}

	return 0;
}

void simodule_debug_printf(void *modhandle, int level, char *format, ...)
{
	va_list			ap;
	SI_MODULE		*mod = (SI_MODULE *)modhandle;
	char			nfbuf[256];
	char			datebuf[30];
	char			buffer[128];
	char			*nf = nfbuf;
	char			*ptr;
	int				err;
	time_t			now = Now;
	int				dlen;
	int				mlen;
	int				flen;

	if (!mod || (level < 1) || (level > 10) || !format)
		return;

	if (mod->mod_debug < level)
		return;

	ctime_r(&now, datebuf);

	/* log: time module_name: message */

	dlen = strlen(datebuf) - 1;
	mlen = strlen(mod->mod_name);
	flen = strlen(format);
	err = dlen + mlen + flen + 5;

	if (err > sizeof(nfbuf))
	{
		nf = (char *)malloc(err);
		if (!nf)
			return;
	}

	strcpy(nf, datebuf);
	ptr = nf + dlen;
	*ptr++ = ' ';
	strcpy(ptr, mod->mod_name);
	ptr += mlen;
	*ptr++ = ':';
	*ptr++ = ' ';

	va_start(ap, format);
	strcpy(ptr, format);
	va_end(ap);

	ptr[flen] = '\n';
	ptr[flen+1] = '\0';

	err = vsnprintf(buffer, sizeof(buffer), nf, ap);
	if (err >= sizeof(buffer))
	{
		char *buf = (char *)malloc(err+1);
		if (buf)
		{
			err = vsnprintf(buf, err+1, nf, ap);
			if (err > 0)
			{
				if (mod->mod_debug_fd < 0)
				{
					fwrite(buf, strlen(buf), 1, stderr);
				}
				else
				{
					write(mod->mod_debug_fd, buf, strlen(buf));
				}
			}
			free(buf);
		}
	}
	else if (err > 0)
	{
		if (mod->mod_debug_fd < 0)
		{
			fwrite(buffer, strlen(buffer), 1, stderr);
		}
		else
		{
			write(mod->mod_debug_fd, buffer, strlen(buffer));
		}
	}
	if (nf != nfbuf)
	{
		free(nf);
	}
}

