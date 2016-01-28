/*
 * Asterisk -- An open source telephony toolkit.
 *
 * Copyright (C) 1999 - 2005, Digium, Inc.
 *
 * Mark Spencer <markster@digium.com>
 *
 * See http://www.asterisk.org for more information about
 * the Asterisk project. Please do not directly contact
 * any of the maintainers of this project for assistance;
 * the project provides a web site, mailing lists and IRC
 * channels for your use.
 *
 * This program is free software, distributed under the terms of
 * the GNU General Public License Version 2. See the LICENSE file
 * at the top of the source tree.
 */

/*!
 * \file
 * \brief RADIUS CDR Support
 *
 * \author Philippe Sultan
 * The Radius Client Library
 * 	http://developer.berlios.de/projects/radiusclient-ng/
 *
 * \arg See also \ref AstCDR
 * \ingroup cdr_drivers
 */

/*! \li \ref cdr_radius.c uses the configuration file \ref cdr.conf
 * \addtogroup configuration_file Configuration Files
 */

/*** MODULEINFO
	<depend>radius</depend>
	<support_level>extended</support_level>
 ***/

#include "asterisk.h"

ASTERISK_FILE_VERSION(__FILE__, "$Revision$")

#ifdef FREERADIUS_CLIENT
#include <freeradius-client.h>
#else
#include <radiusclient-ng.h>
#endif

#include <signal.h>
#include <sys/signal.h>
#include <dirent.h>
#include "asterisk/lock.h"
#include "asterisk/app.h"
#include "asterisk/channel.h"
#include "asterisk/cdr.h"
#include "asterisk/module.h"
#include "asterisk/utils.h"

/*! ISO 8601 standard format */
#define DATE_FORMAT "%Y-%m-%d %T %z"

#define VENDOR_CODE           22736

enum {
	PW_AST_ACCT_CODE =    101,
	PW_AST_SRC =          102,
	PW_AST_DST =          103,
	PW_AST_DST_CTX =      104,
	PW_AST_CLID =         105,
	PW_AST_CHAN =         106,
	PW_AST_DST_CHAN =     107,
	PW_AST_LAST_APP =     108,
	PW_AST_LAST_DATA =    109,
	PW_AST_START_TIME =   110,
	PW_AST_ANSWER_TIME =  111,
	PW_AST_END_TIME =     112,
	PW_AST_DURATION =     113,
	PW_AST_BILL_SEC =     114,
	PW_AST_DISPOSITION =  115,
	PW_AST_AMA_FLAGS =    116,
	PW_AST_UNIQUE_ID =    117,
	PW_AST_USER_FIELD =   118,
	PW_AST_LINKED_ID  =   119
};

enum {
	/*! Log dates and times in UTC */
	RADIUS_FLAG_USEGMTIME = (1 << 0),
	/*! Log Unique ID */
	RADIUS_FLAG_LOGUNIQUEID = (1 << 1),
	/*! Log User Field */
	RADIUS_FLAG_LOGUSERFIELD = (1 << 2)
};

static const char desc[] = "RADIUS CDR Backend";
static const char name[] = "radius";
static const char cdr_config[] = "cdr.conf";

#ifdef FREERADIUS_CLIENT
static char radiuscfg[PATH_MAX] = "/etc/radiusclient/radiusclient.conf";
#else
static char radiuscfg[PATH_MAX] = "/usr/local/etc/radiusclient-ng/radiusclient.conf";
#endif

static const char cdr_radius_config[] = "cdr_radius.conf"; /* add by lidp for  add user define  radius attriutes */

static struct ast_flags global_flags = { RADIUS_FLAG_USEGMTIME | RADIUS_FLAG_LOGUNIQUEID | RADIUS_FLAG_LOGUSERFIELD };

static rc_handle *rh = NULL;

/*! Directory to store cdr avp file. add by liucl*/
static pthread_t monitor_thread = AST_PTHREADT_NULL;
AST_MUTEX_DEFINE_STATIC(monlock);
static char cdr_dir_tmp[PATH_MAX] = "/tmp";
static char cdr_directory[PATH_MAX] = "/var/lib/cdr";
/*
 *!\brief  struct for user defined radius attributes 
 * add by lidp  
 */
struct ast_radius_attr {
    AST_LIST_ENTRY(ast_radius_attr) list;
    int id;                                /* dictionary of this attr*/
    char name[0];                          /* attribute  name */
};

/* Read only list after load module, so no lock is necessary */
static AST_LIST_HEAD_NOLOCK_STATIC(radius_attr_list, ast_radius_attr); /*lidp add */


static int build_radius_record(VALUE_PAIR **tosend, struct ast_cdr *cdr)
{
	int recordtype = PW_STATUS_STOP;
	struct ast_tm tm;
	char timestr[128];
	char *tmp;
	struct ast_str *tmp_buf;
	tmp_buf = ast_str_thread_get(&ast_str_thread_global_buf, 16);	

	ast_cdr_serialize_variables(cdr->channel, &tmp_buf, '=', '\n');
	ast_log(LOG_ERROR, "CDR varable:\n%s\n", ast_str_buffer(tmp_buf));
	
    	/*lidp add*/
    	struct ast_radius_attr *cur;
	char buf[4096];

	if (!rc_avpair_add(rh, tosend, PW_ACCT_STATUS_TYPE, &recordtype, 0, 0)){
	ast_log(LOG_ERROR, "add failed\n");	
	return -1;
	}
	ast_log(LOG_ERROR, "add account \n");
	/* Account code */
	if (!rc_avpair_add(rh, tosend, PW_AST_ACCT_CODE, &cdr->accountcode, strlen(cdr->accountcode), VENDOR_CODE))
		return -1;
	ast_log(LOG_ERROR, "add vendor code src\n");
 	/* Source */
	if (!rc_avpair_add(rh, tosend, PW_AST_SRC, &cdr->src, strlen(cdr->src), VENDOR_CODE))
		return -1;
	ast_log(LOG_ERROR, "add vendor code dst\n");
 	/* Destination */
	if (!rc_avpair_add(rh, tosend, PW_AST_DST, &cdr->dst, strlen(cdr->dst), VENDOR_CODE))
		return -1;
ast_log(LOG_ERROR, "add vendor code context\n");
 	/* Destination context */
	if (!rc_avpair_add(rh, tosend, PW_AST_DST_CTX, &cdr->dcontext, strlen(cdr->dcontext), VENDOR_CODE))
		return -1;
ast_log(LOG_ERROR, "add vendor code callid\n");
	/* Caller ID */
	if (!rc_avpair_add(rh, tosend, PW_AST_CLID, &cdr->clid, strlen(cdr->clid), VENDOR_CODE))
		return -1;
ast_log(LOG_ERROR, "add vendor code channel\n");
	/* Channel */
	if (!rc_avpair_add(rh, tosend, PW_AST_CHAN, &cdr->channel, strlen(cdr->channel), VENDOR_CODE))
		return -1;
ast_log(LOG_ERROR, "add vendor code dst channel\n");
	/* Destination Channel */
	if (!rc_avpair_add(rh, tosend, PW_AST_DST_CHAN, &cdr->dstchannel, strlen(cdr->dstchannel), VENDOR_CODE))
		return -1;
ast_log(LOG_ERROR, "add vendor code app\n");
	/* Last Application */
	if (!rc_avpair_add(rh, tosend, PW_AST_LAST_APP, &cdr->lastapp, strlen(cdr->lastapp), VENDOR_CODE))
		return -1;
ast_log(LOG_ERROR, "add vendor code last data\n");
	/* Last Data */
	if (!rc_avpair_add(rh, tosend, PW_AST_LAST_DATA, &cdr->lastdata, strlen(cdr->lastdata), VENDOR_CODE))
		return -1;

ast_log(LOG_ERROR, "add vendor code start time\n");
	/* Start Time */
	ast_strftime(timestr, sizeof(timestr), DATE_FORMAT,
		ast_localtime(&cdr->start, &tm,
			ast_test_flag(&global_flags, RADIUS_FLAG_USEGMTIME) ? "GMT" : NULL));
	if (!rc_avpair_add(rh, tosend, PW_AST_START_TIME, timestr, strlen(timestr), VENDOR_CODE))
		return -1;
ast_log(LOG_ERROR, "add vendor code answer time\n");
	/* Answer Time */
	ast_strftime(timestr, sizeof(timestr), DATE_FORMAT,
		ast_localtime(&cdr->answer, &tm,
			ast_test_flag(&global_flags, RADIUS_FLAG_USEGMTIME) ? "GMT" : NULL));
	if (!rc_avpair_add(rh, tosend, PW_AST_ANSWER_TIME, timestr, strlen(timestr), VENDOR_CODE))
		return -1;
ast_log(LOG_ERROR, "add vendor code end time\n");
	/* End Time */
	ast_strftime(timestr, sizeof(timestr), DATE_FORMAT,
		ast_localtime(&cdr->end, &tm,
			ast_test_flag(&global_flags, RADIUS_FLAG_USEGMTIME) ? "GMT" : NULL));
	if (!rc_avpair_add(rh, tosend, PW_AST_END_TIME, timestr, strlen(timestr), VENDOR_CODE))
		return -1;
ast_log(LOG_ERROR, "add vendor code duration\n");
 	/* Duration */
	if (!rc_avpair_add(rh, tosend, PW_AST_DURATION, &cdr->duration, 0, VENDOR_CODE))
		return -1;
ast_log(LOG_ERROR, "add vendor code bill sec\n");
	/* Billable seconds */
	if (!rc_avpair_add(rh, tosend, PW_AST_BILL_SEC, &cdr->billsec, 0, VENDOR_CODE))
		return -1;
ast_log(LOG_ERROR, "add vendor code disposition\n");
	/* Disposition */
	tmp = ast_strdupa(ast_cdr_disp2str(cdr->disposition));
	if (!rc_avpair_add(rh, tosend, PW_AST_DISPOSITION, tmp, strlen(tmp), VENDOR_CODE))
		return -1;
ast_log(LOG_ERROR, "add vendor code amaflag\n");
	/* AMA Flags */
	tmp = ast_strdupa(ast_channel_amaflags2string(cdr->amaflags));
	if (!rc_avpair_add(rh, tosend, PW_AST_AMA_FLAGS, tmp, strlen(tmp), VENDOR_CODE))
		return -1;
ast_log(LOG_ERROR, "add vendor code uniqueid\n");
	if (ast_test_flag(&global_flags, RADIUS_FLAG_LOGUNIQUEID)) {
		/* Unique ID */
		if (!rc_avpair_add(rh, tosend, PW_AST_UNIQUE_ID, &cdr->uniqueid, strlen(cdr->uniqueid), VENDOR_CODE))
			return -1;
	}
ast_log(LOG_ERROR, "add vendor code userfield\n");
	if (ast_test_flag(&global_flags, RADIUS_FLAG_LOGUSERFIELD)) {
		/* append the user field */
		if (!rc_avpair_add(rh, tosend, PW_AST_USER_FIELD, &cdr->userfield, strlen(cdr->userfield), VENDOR_CODE))
			return -1;
	}
ast_log(LOG_ERROR, "add vendor code username\n");
	/* Setting Acct-Session-Id & User-Name attributes for proper generation
	 * of Acct-Unique-Session-Id on server side 
	 */
	/* Channel */
	if (!rc_avpair_add(rh, tosend, PW_USER_NAME, &cdr->channel, strlen(cdr->channel), 0))
		return -1;
ast_log(LOG_ERROR, "add vendor code session id\n");
	/* Unique ID */
	if (!rc_avpair_add(rh, tosend, PW_ACCT_SESSION_ID, &cdr->uniqueid, strlen(cdr->uniqueid), 0))
		return -1;
	ast_log(LOG_ERROR, "read self define attribute\n");
	
	/* retrive user define attrs , skip attr whose attr value is NULL */
	if(!AST_LIST_EMPTY(&radius_attr_list)) {
        AST_LIST_TRAVERSE(&radius_attr_list, cur, list) {
            if(cur) {
		ast_log(LOG_DEBUG, "cdr radius name=%s\n", cur->name);
                if(ast_cdr_getvar(cdr->channel, cur->name, buf, sizeof(buf)) == 0){
                    if(ast_strlen_zero(buf)){
                        continue;
		    }
			ast_log(LOG_DEBUG, "cdr has data value=%s\n", buf);
		    int left_len=strlen(buf);
                    char *left_value = buf;
                    int send_len;
                    while(left_len > 0){
                        if(left_len > 240){
                            send_len = 240;
                        }else{
                            send_len = left_len;
                        }
                        if(!rc_avpair_add(rh, tosend,cur->id, left_value,send_len,VENDOR_CODE )) {
                            ast_log(LOG_NOTICE,"cdr log failed user define name %s  value %s \n",cur->name,left_value);
                        }
                        left_len-=send_len;
                        left_value+=send_len;
                    }
		}            
	    }
    	}
        }

	return 0;
}

static int radius_log(struct ast_cdr *cdr)
{
	int result = ERROR_RC;
	VALUE_PAIR *tosend = NULL;
	int ret = 0;
	char cdr_full_path[256];

	if (build_radius_record(&tosend, cdr)) {
		ast_debug(1, "Unable to create RADIUS record. CDR not recorded!\n");
		goto return_cleanup;
	}

	result = rc_acct(rh, 0, tosend);
	if (result != OK_RC) {
		ast_log(LOG_ERROR, "Failed to record Radius CDR record!\n");
		goto return_cleanup;
	}

return_cleanup:
	if (tosend) {
		rc_avpair_free(tosend);
	}

	return result;
}

static int unload_module(void)
{
	/* avoid meory leak everitime reload module */
    struct ast_radius_attr *cur;
	
	while(!AST_LIST_EMPTY(&radius_attr_list)) {
        cur = AST_LIST_REMOVE_HEAD(&radius_attr_list, list);
        ast_free(cur);
    }

	if (ast_cdr_unregister(name)) {
		return -1;
	}

	if (rh) {
		rc_destroy(rh);
		rh = NULL;
	}
	return 0;
}

static int load_module(void)
{
	struct ast_config *cfg;
	struct ast_flags config_flags = { 0 };
	const char *tmp;

    /* lidp add */
    struct ast_config *radius_config;
	char *cat = NULL;
    struct ast_variable *v;
	struct ast_radius_attr *tmp_attr;
    /* lidp add end  */

	if ((cfg = ast_config_load(cdr_config, config_flags)) && cfg != CONFIG_STATUS_FILEINVALID) {
		ast_set2_flag(&global_flags, ast_true(ast_variable_retrieve(cfg, "radius", "usegmtime")), RADIUS_FLAG_USEGMTIME);
		ast_set2_flag(&global_flags, ast_true(ast_variable_retrieve(cfg, "radius", "loguniqueid")), RADIUS_FLAG_LOGUNIQUEID);
		ast_set2_flag(&global_flags, ast_true(ast_variable_retrieve(cfg, "radius", "loguserfield")), RADIUS_FLAG_LOGUSERFIELD);
		if ((tmp = ast_variable_retrieve(cfg, "radius", "radiuscfg")))
		ast_log(LOG_ERROR, "radiuscfg =%s\n", tmp);
			ast_copy_string(radiuscfg, tmp, sizeof(radiuscfg));
		ast_config_destroy(cfg);
	} else
		return AST_MODULE_LOAD_DECLINE;

	/* create dir /var/lib/cdr if it does not exist. add by liucl */    
	if (access(cdr_directory,F_OK) == -1){
		ast_log(LOG_DEBUG,"cdr_directory %s is not exist, I will create it.\n",cdr_directory);
		if(mkdir(cdr_directory, 0755) == -1) {
			ast_log(LOG_ERROR,"Failed to create %s\n", cdr_directory);
		}else{
			ast_log(LOG_DEBUG,"Create directory %s is OK\n",cdr_directory);
		}
	}
	/* liucl add end*/

    /* add  by lidp */
    if((radius_config = ast_config_load(cdr_radius_config, config_flags))) {
        while ((cat = ast_category_browse(radius_config, cat))) {
			if(!strcasecmp(cat, "attributes")) {
                for (v = ast_variable_browse(radius_config, cat); v; v = v->next) {
		           if(!(tmp_attr = ast_calloc(1, sizeof(*tmp_attr) + strlen(v->name) + 1 ))) {
                        ast_config_destroy(radius_config);
                        return AST_MODULE_LOAD_DECLINE;
		           }
                   strcpy(tmp_attr->name, v->name); /* safe  */
                   if(v->value) {
                       sscanf(v->value, "%30d", &tmp_attr->id ); /* numeric */
                   } else {
					   continue;                              /* no id for this attr ,skip it*/
                     }
                   if(AST_LIST_EMPTY(&radius_attr_list)) {
                       AST_LIST_INSERT_HEAD(&radius_attr_list, tmp_attr, list);
		           } else {
                         AST_LIST_INSERT_TAIL(&radius_attr_list, tmp_attr, list); 
                     }
            }
		}
            continue;  
	  }
	    ast_config_destroy(radius_config); 
	} else {
        return AST_MODULE_LOAD_DECLINE;
	}

	/*
	 * start logging
	 *
	 * NOTE: Yes this causes a slight memory leak if the module is
	 * unloaded.  However, it is better than a crash if cdr_radius
	 * and cel_radius are both loaded.
	 */
	tmp = ast_strdup("asterisk");
	if (tmp) {
		rc_openlog((char *) tmp);
	}

	/* read radiusclient-ng config file */
	if (!(rh = rc_read_config(radiuscfg))) {
		ast_log(LOG_NOTICE, "Cannot load radiusclient-ng configuration file %s.\n", radiuscfg);
		return AST_MODULE_LOAD_DECLINE;
	}

	/* read radiusclient-ng dictionaries */
	if (rc_read_dictionary(rh, rc_conf_str(rh, "dictionary"))) {
		ast_log(LOG_NOTICE, "Cannot load radiusclient-ng dictionary file.\n");
		rc_destroy(rh);
		rh = NULL;
		return AST_MODULE_LOAD_DECLINE;
	}

	if (ast_cdr_register(name, desc, radius_log)) {
		rc_destroy(rh);
		rh = NULL;
		return AST_MODULE_LOAD_DECLINE;
	} else {
		return AST_MODULE_LOAD_SUCCESS;
	}
}

AST_MODULE_INFO(ASTERISK_GPL_KEY, AST_MODFLAG_LOAD_ORDER, "RADIUS CDR Backend",
		.support_level = AST_MODULE_SUPPORT_EXTENDED,
		.load = load_module,
		.unload = unload_module,
		.load_pri = AST_MODPRI_CDR_DRIVER,
	);
