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
 * \extref The Radius Client Library - http://developer.berlios.de/projects/radiusclient-ng/
 *
 * \arg See also \ref AstCDR
 * \ingroup cdr_drivers
 */

/*** MODULEINFO
	<depend>radius</depend>
 ***/

#include "asterisk.h"

ASTERISK_FILE_VERSION(__FILE__, "$Revision: 321926 $")

#include <signal.h>
#include <sys/signal.h>
#include <radiusclient-ng.h>
#include <dirent.h>
#include "asterisk/channel.h"
#include "asterisk/cdr.h"
#include "asterisk/module.h"
#include "asterisk/utils.h"
#include "asterisk/lock.h"

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

static char radiuscfg[PATH_MAX] = "/etc/radiusclient-ng/radiusclient.conf";

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
    /*lidp add*/
    struct ast_radius_attr *cur;
	char *value = NULL;
	char buf[4096];

	if (!rc_avpair_add(rh, tosend, PW_ACCT_STATUS_TYPE, &recordtype, 0, 0))
		return -1;

	/* Account code */
	if (!rc_avpair_add(rh, tosend, PW_AST_ACCT_CODE, &cdr->accountcode, strlen(cdr->accountcode), VENDOR_CODE))
		return -1;

 	/* Source */
	if (!rc_avpair_add(rh, tosend, PW_AST_SRC, &cdr->src, strlen(cdr->src), VENDOR_CODE))
		return -1;

 	/* Destination */
	if (!rc_avpair_add(rh, tosend, PW_AST_DST, &cdr->dst, strlen(cdr->dst), VENDOR_CODE))
		return -1;

 	/* Destination context */
	if (!rc_avpair_add(rh, tosend, PW_AST_DST_CTX, &cdr->dcontext, strlen(cdr->dcontext), VENDOR_CODE))
		return -1;

	/* Caller ID */
	if (!rc_avpair_add(rh, tosend, PW_AST_CLID, &cdr->clid, strlen(cdr->clid), VENDOR_CODE))
		return -1;

	/* Channel */
	if (!rc_avpair_add(rh, tosend, PW_AST_CHAN, &cdr->channel, strlen(cdr->channel), VENDOR_CODE))
		return -1;

	/* Destination Channel */
	if (!rc_avpair_add(rh, tosend, PW_AST_DST_CHAN, &cdr->dstchannel, strlen(cdr->dstchannel), VENDOR_CODE))
		return -1;

	/* Last Application */
	if (!rc_avpair_add(rh, tosend, PW_AST_LAST_APP, &cdr->lastapp, strlen(cdr->lastapp), VENDOR_CODE))
		return -1;

	/* Last Data */
	if (!rc_avpair_add(rh, tosend, PW_AST_LAST_DATA, &cdr->lastdata, strlen(cdr->lastdata), VENDOR_CODE))
		return -1;


	/* Start Time */
	ast_strftime(timestr, sizeof(timestr), DATE_FORMAT,
		ast_localtime(&cdr->start, &tm,
			ast_test_flag(&global_flags, RADIUS_FLAG_USEGMTIME) ? "GMT" : NULL));
	if (!rc_avpair_add(rh, tosend, PW_AST_START_TIME, timestr, strlen(timestr), VENDOR_CODE))
		return -1;

	/* Answer Time */
	ast_strftime(timestr, sizeof(timestr), DATE_FORMAT,
		ast_localtime(&cdr->answer, &tm,
			ast_test_flag(&global_flags, RADIUS_FLAG_USEGMTIME) ? "GMT" : NULL));
	if (!rc_avpair_add(rh, tosend, PW_AST_ANSWER_TIME, timestr, strlen(timestr), VENDOR_CODE))
		return -1;

	/* End Time */
	ast_strftime(timestr, sizeof(timestr), DATE_FORMAT,
		ast_localtime(&cdr->end, &tm,
			ast_test_flag(&global_flags, RADIUS_FLAG_USEGMTIME) ? "GMT" : NULL));
	if (!rc_avpair_add(rh, tosend, PW_AST_END_TIME, timestr, strlen(timestr), VENDOR_CODE))
		return -1;

 	/* Duration */
	if (!rc_avpair_add(rh, tosend, PW_AST_DURATION, &cdr->duration, 0, VENDOR_CODE))
		return -1;

	/* Billable seconds */
	if (!rc_avpair_add(rh, tosend, PW_AST_BILL_SEC, &cdr->billsec, 0, VENDOR_CODE))
		return -1;

	/* Disposition */
	tmp = ast_cdr_disp2str(cdr->disposition);
	if (!rc_avpair_add(rh, tosend, PW_AST_DISPOSITION, tmp, strlen(tmp), VENDOR_CODE))
		return -1;

	/* AMA Flags */
	tmp = ast_cdr_flags2str(cdr->amaflags);
	if (!rc_avpair_add(rh, tosend, PW_AST_AMA_FLAGS, tmp, strlen(tmp), VENDOR_CODE))
		return -1;

	if (ast_test_flag(&global_flags, RADIUS_FLAG_LOGUNIQUEID)) {
		/* Unique ID */
		if (!rc_avpair_add(rh, tosend, PW_AST_UNIQUE_ID, &cdr->uniqueid, strlen(cdr->uniqueid), VENDOR_CODE))
			return -1;
	}

	if (ast_test_flag(&global_flags, RADIUS_FLAG_LOGUSERFIELD)) {
		/* append the user field */
		if (!rc_avpair_add(rh, tosend, PW_AST_USER_FIELD, &cdr->userfield, strlen(cdr->userfield), VENDOR_CODE))
			return -1;
	}

	/* Setting Acct-Session-Id & User-Name attributes for proper generation
	   of Acct-Unique-Session-Id on server side */
	/* Channel */
	if (!rc_avpair_add(rh, tosend, PW_USER_NAME, &cdr->channel, strlen(cdr->channel), 0))
		return -1;

	/* Unique ID */
	if (!rc_avpair_add(rh, tosend, PW_ACCT_SESSION_ID, &cdr->uniqueid, strlen(cdr->uniqueid), 0))
		return -1;

	/* add linked_id */
	if(!rc_avpair_add(rh, tosend, PW_AST_LINKED_ID, &cdr->linkedid, strlen(cdr->linkedid), VENDOR_CODE))
               return -1;
    /* retrive user define attrs , skip attr whose attr value is NULL */
	if(!AST_LIST_EMPTY(&radius_attr_list)) {
        AST_LIST_TRAVERSE(&radius_attr_list, cur, list) {
            if(cur) {
                ast_cdr_getvar(cdr, cur->name, &value, buf, sizeof(buf), 0, 0);
                if(!value)
                    continue;
				int left_len=strlen(value);
                char *left_value = value;
                int send_len;
                while(left_len > 0){
                    if(left_len > 240){
                        send_len = 240;
                    }else{
                        send_len = left_len;
                    }
                    if(!rc_avpair_add(rh, tosend,cur->id, left_value,send_len,VENDOR_CODE )) {
                        ast_log(LOG_NOTICE,"cdr log failed user define name %s  value %s \n",cur->name,value);
                    }
                    left_len-=send_len;
                    left_value+=send_len;
                }
			}            
		}
    }
	return 0;
}

/*!
 * Get cdr avp file and assumble in VALUE_PAIR
 * add by liucl
 */
static VALUE_PAIR *get_avp(const char *file)
{
	FILE *in;
	char tmp[256];
	VALUE_PAIR *avp = NULL;
	VALUE_PAIR *avp_head = NULL;
	VALUE_PAIR *avp_tmp = NULL;
	int len = 0;

	if((in=fopen(file,"r")) != NULL) {
		while(!feof(in)){
			memset(tmp,0,sizeof(tmp));
			if(!fgets(tmp,sizeof(tmp),in))
				break;
			avp = (VALUE_PAIR *)ast_malloc(sizeof(VALUE_PAIR));
			if(avp == NULL){
				return NULL;
			}
			if(avp_head == NULL){
				avp_head = avp;
			}
			ast_copy_string(avp->name,tmp,strlen(tmp));
			memset(tmp,0,sizeof(tmp));
			fgets(tmp,sizeof(tmp),in);
			avp->attribute = atoi(tmp);
			memset(tmp,0,sizeof(tmp));
			fgets(tmp,sizeof(tmp),in);
			avp->type = atoi(tmp);
			memset(tmp,0,sizeof(tmp));
			fgets(tmp,sizeof(tmp),in);
			if(avp->type == 0) {
				len = strlen(tmp)-1;
				tmp[len] = '\0';
				memcpy(avp->strvalue,tmp,strlen(tmp));
				avp->lvalue = strlen(tmp);
			}else{
				avp->lvalue = atoi(tmp);
			}
			avp->next = NULL;
			if(avp_tmp != NULL){
				avp_tmp->next = avp;
			}
			avp_tmp = avp;
		}
		fclose(in);
	}

	return avp_head;
}

/*! save avp to file if failed sending to radius server 
 * add by liucl
 */
static int save_avp_to_file(const char *avp_file, const char *file_name, VALUE_PAIR *avp)
{
	int result = 0;
	int len = 0;
	FILE *wfile;
	char tmp1[256],tmp2[256],tmp3[256];
    char full_file_name[256];
    char cmd_string[256];
    int ret = 1; // return value of system()
	
	if((wfile=fopen(avp_file, "a+")) != NULL){
		while(avp != NULL) {
			memset(tmp1,0,sizeof(tmp1));
			memset(tmp2,0,sizeof(tmp2));
			memset(tmp3,0,sizeof(tmp3));
			if(!fwrite(avp->name,1,strlen(avp->name),wfile))
				return result;
			fwrite("\n",1,1,wfile);
			len = sprintf(tmp1,"%d",avp->attribute);
			fwrite(tmp1,1,len,wfile);
			fwrite("\n",1,1,wfile);
			len = sprintf(tmp2,"%d",avp->type);
			fwrite(tmp2,1,len,wfile);
			fwrite("\n",1,1,wfile);
			if(avp->type != 0) { // 1 | 2 | 3
				sprintf(tmp3,"%u",avp->lvalue);
				fwrite(tmp3,1,strlen(tmp3),wfile);
				fwrite("\n",1,1,wfile);
			}else { // type = 0
				fwrite(avp->strvalue,1,strlen(avp->strvalue),wfile);
				fwrite("\n",1,1,wfile);
			}
			avp = avp->next;
		}
		fclose(wfile);
		result = 1;
	}
    memset(full_file_name, 0, sizeof(full_file_name));
    memset(cmd_string, 0, sizeof(cmd_string));
    snprintf(full_file_name,sizeof(full_file_name),"%s/%s", cdr_directory, file_name);
    snprintf(cmd_string,sizeof(cmd_string),"mv -T %s %s", avp_file, full_file_name);
    ret = ast_safe_system(cmd_string);
    ast_log(LOG_DEBUG, "ret:%d <> avp_file:%s <> full_file_name:%s\n",ret,avp_file,full_file_name);
    if(ret != 0) {
        ast_log(LOG_ERROR, "Failed to move %s\n", avp_file);
    }

	return result;
}

/*!frees all value_pairs in the list
 */
static void ast_avpair_free(VALUE_PAIR *pair)
{
    VALUE_PAIR *next;
    while (pair != NULL){
        next = pair->next;
        ast_free(pair);
        pair = next;
    }
}

static void *do_monitor(void *data)
{
    int result = ERROR_RC;
    char cdr_full_path[256];
    DIR *cdr_dir;
	VALUE_PAIR *resend = NULL;
	struct dirent *dir_s = NULL;
	int count = 0; // times of resend
	int wait_time = 0;
	
	for(;;){
        sleep(1); /* sleep 1 minute */
		/* Open dir */
		if((cdr_dir=opendir(cdr_directory)) == NULL){
			ast_log(LOG_ERROR,"Failed to open %s\n",cdr_directory);
		} else {
		/* open */
			while((dir_s=readdir(cdr_dir))!=NULL) {
				if(!strcmp(dir_s->d_name,".") || !strcmp(dir_s->d_name,".."))
					continue;
				memset(cdr_full_path,0,sizeof(cdr_full_path));
				snprintf(cdr_full_path,sizeof(cdr_full_path),"%s/%s",cdr_directory,dir_s->d_name);
				resend = get_avp(cdr_full_path);
				result = rc_acct(rh,0,resend);
				if(result != OK_RC){
					ast_log(LOG_ERROR, "Failed to re-record Radius CDR record!\n");
					if(resend){
						ast_avpair_free(resend);
						resend = NULL;
					}
					count++;
					wait_time = count * 60; // reterval to resend cdr record: 60s, 120s,180s,240s......
					ast_log(LOG_DEBUG,"----------wait_time:%d-----------\n", wait_time);
					break;
				}else{
				ast_log(LOG_DEBUG,"----------cdr_full_path:%s send OK!-----------\n", cdr_full_path);
				wait_time = 0;
				count = 0;
				if(remove(cdr_full_path) != 0)
					ast_log(LOG_WARNING,"Failed to delete %s\n",cdr_full_path);
			}
			if(resend){
				ast_avpair_free(resend);
				resend = NULL;
			}
		}
		closedir(cdr_dir);
		}
		
		if (wait_time > 0) {
			ast_log(LOG_DEBUG,"if-wait_time>0----------wait_time:%d-----------\n", wait_time);
			sleep(wait_time);
		}
	}
	return NULL;
}

static int start_monitor(void)
{
	/* If we're supposed to be stopped -- stay stopped */
	if (monitor_thread == AST_PTHREADT_STOP)
		return 0;
	ast_mutex_lock(&monlock);
	if (monitor_thread == pthread_self()) {
		ast_mutex_unlock(&monlock);
		ast_log(LOG_WARNING, "Cannot kill myself\n");
		return -1;
	}
	if (monitor_thread != AST_PTHREADT_NULL) {
		/* Wake up the thread */
		pthread_kill(monitor_thread, SIGURG);
	} else {
		/* Start a new monitor */
		if (ast_pthread_create_background(&monitor_thread, NULL, do_monitor, NULL) < 0) {
			ast_mutex_unlock(&monlock);
			ast_log(LOG_ERROR, "Unable to start monitor thread.\n");
			return -1;
		}
	}
	ast_mutex_unlock(&monlock);

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
        /* file name */
        if(cdr->uniqueid && (!ast_strlen_zero(cdr->uniqueid))){
            snprintf(cdr_full_path,sizeof(cdr_full_path),"%s/%s",cdr_dir_tmp,cdr->uniqueid);
			/* write cdr to file if rc_acct failed */
			ret = save_avp_to_file(cdr_full_path,cdr->uniqueid,tosend);
			if(!ret)
				ast_log(LOG_ERROR,"Failed to write cdr to file!\n");
        }
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

	ast_cdr_unregister(name);
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
		/*
		* Create a independent thread to monitoring /var/lib/cdr. 
		* If there is file in the directory, then send it to radius.
		* add by liucl
		*/
		start_monitor();
		return AST_MODULE_LOAD_SUCCESS;
	}
}

AST_MODULE_INFO(ASTERISK_GPL_KEY, AST_MODFLAG_LOAD_ORDER, "RADIUS CDR Backend",
		.load = load_module,
		.unload = unload_module,
		.load_pri = AST_MODPRI_CDR_DRIVER,
	);
