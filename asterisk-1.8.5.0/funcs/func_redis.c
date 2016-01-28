/*
 * Asterisk -- An open source telephony toolkit.
 *
 * Copyright (C) 2005-2006, Russell Bryant <russelb@clemson.edu> 
 *
 * func_db.c adapted from the old app_db.c, copyright by the following people 
 * Copyright (C) 2005, Mark Spencer <markster@digium.com>
 * Copyright (C) 2003, Jefferson Noxon <jeff@debian.org>
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

/*! \file
 *
 * \brief Functions for interaction with the Asterisk database
 *
 * \author Russell Bryant <russelb@clemson.edu>
 *
 * \ingroup functions
 */

#include "asterisk.h"

ASTERISK_FILE_VERSION(__FILE__, "$Revision: 191140 $")

#include <regex.h>

#include "asterisk/module.h"
#include "asterisk/channel.h"
#include "asterisk/pbx.h"
#include "asterisk/utils.h"
#include "asterisk/app.h"
#include "asterisk/astdb.h"
#include "asterisk/hiredis.h"
#include "asterisk/paths.h" /* use ast_config_AST_SYSTEM_NAME */
#include "asterisk/redis.h"

/*** DOCUMENTATION
    <function name="REDIS" language="en_US">
        <synopsis>
            Read from or write to the redis server.
        </synopsis>
        <syntax argsep="/">
            <parameter name="key" required="true" />
        </syntax>
        <description>
            <para>This function will read from or write a value to the redis server.  On a
            read, this function returns the corresponding value from the redis, or blank
            if it does not exist.</para>
        </description>
    </function>
        <function name="REDIS_CMD" language="en_US">
                <synopsis>
                        Execute command to redis server.
                </synopsis>
                <syntax argsep="/">
                        <parameter name="cmd" required="true" />
                </syntax>
                <description>
                        <para>This function will execute command to the redis server. </para>
                </description>
        </function>
 ***/
#define MAX_COMMAND_SIZE 1024
#define MAX_UUID_SIZE 128
static redisContext* conn;
static char g_server[256];
static int g_port;
static struct timeval g_timeout = { 3, 0 }; // 3 seconds
AST_MUTEX_DEFINE_STATIC(redis_lock);
static int script_loaded = 0;
static char script_lock_hash[256];
static char script_unlock_hash[256];
static char *script_lock = "return redis.call('SET', KEYS[1], ARGV[1], 'NX', 'PX', ARGV[2]) ";
static char *script_unlock = "if (redis.call('GET', KEYS[1]) == ARGV[1]) then return redis.call('DEL',KEYS[1]) else return 0 end";
static int uniqueint;

static int redis_connect(void){
    ast_mutex_lock(&redis_lock);
    if(!conn){
        conn = redisConnectWithTimeout(g_server, g_port, g_timeout);
        if ( conn->err){
            ast_mutex_unlock(&redis_lock);
            ast_log(LOG_ERROR, "Connect to redis %s:%d faild\n",g_server, g_port);
            return 0;
        }
        redisReply *reply = redisCommand(conn,"SELECT 4");
        freeReplyObject(reply);
    }
    ast_mutex_unlock(&redis_lock);
    return 1;
}

static void generate_uuid(char *uuid){
    if (!ast_strlen_zero(ast_config_AST_SYSTEM_NAME)) {
        snprintf(uuid, MAX_UUID_SIZE, "%s-%li.%d", ast_config_AST_SYSTEM_NAME, (long) time(NULL), ast_atomic_fetchadd_int(&uniqueint, 1));
    }else{
        snprintf(uuid, MAX_UUID_SIZE, "%li.%d", (long) time(NULL), ast_atomic_fetchadd_int(&uniqueint, 1));
    }
}

static void load_script(void){
    memset(script_lock_hash, 0, sizeof(script_lock_hash));
    memset(script_unlock_hash, 0, sizeof(script_unlock_hash));
    if(!redis_connect()){
        return;
    }
    ast_mutex_lock(&redis_lock);
    redisReply* r = (redisReply*)redisCommand(conn, "SCRIPT LOAD %s", script_lock);
    if( NULL == r)
    {
        ast_log(LOG_ERROR, "Execut command:%s failure\n", script_lock);
        redisFree(conn);
        conn = NULL;
        ast_mutex_unlock(&redis_lock);
        return;
    }
    if( !(r->type == REDIS_REPLY_STRING))
    {
        ast_log(LOG_WARNING, "Failed to execute command[%s]\n",script_lock);
        freeReplyObject(r);
        ast_mutex_unlock(&redis_lock);
        return;
    }
    ast_copy_string(script_lock_hash, r->str, sizeof(script_lock_hash));
    freeReplyObject(r);
    ast_log(LOG_DEBUG, "script_lock hash=%s\n", script_lock_hash);

    r = (redisReply*)redisCommand(conn, "SCRIPT LOAD %s", script_unlock);
    if( NULL == r)
    {
        ast_log(LOG_ERROR, "Execut command:%s failure\n", script_unlock);
        redisFree(conn);
        conn = NULL;
        ast_mutex_unlock(&redis_lock);
        return;
    }
    if( !(r->type == REDIS_REPLY_STRING))
    {
        ast_log(LOG_WARNING, "Failed to execute command[%s]\n",script_unlock);
        freeReplyObject(r);
        ast_mutex_unlock(&redis_lock);
        return;
    }
    ast_copy_string(script_unlock_hash, r->str, sizeof(script_unlock_hash));
    freeReplyObject(r);
    ast_log(LOG_DEBUG, "script_unlock hash=%s\n", script_unlock_hash);    

    ast_mutex_unlock(&redis_lock);
    script_loaded = 1;
    return;
}

int ast_redis_lock(const char* key, int timeout, char* uuid){
    if(!script_loaded){
        load_script();        
    }
    if(!redis_connect()){
        return -1;
    }
    generate_uuid(uuid);
    while(1){
        ast_mutex_lock(&redis_lock);
        redisReply* r = (redisReply*)redisCommand(conn, "EVALSHA %s 1 %s %s %d", script_lock_hash, key, uuid, timeout);
        if( NULL == r)
        {
            ast_log(LOG_ERROR, "Execut command:[EVALSHA %s 1 %s %s %d] failure\n", script_lock_hash, key, uuid, timeout);
            redisFree(conn);
            conn = NULL;
            ast_mutex_unlock(&redis_lock);
            return -1;
        }
        if( !(r->type == REDIS_REPLY_STATUS && strcasecmp(r->str,"OK")==0))
        {
            ast_log(LOG_WARNING, "Failed to execute command[EVALSHA %s 1 %s %s %d]\n", script_lock_hash, key, uuid, timeout);
            freeReplyObject(r);
        }else{
            ast_mutex_unlock(&redis_lock);
            break;
        }
        ast_mutex_unlock(&redis_lock);
        usleep(10*1000);
    }
    return 0;
}

void ast_redis_unlock(const char* key, const char* uuid){
    if(!script_loaded){
        load_script();
    }
    if(!redis_connect()){
        return;
    }
    ast_mutex_lock(&redis_lock);
    redisReply* r = (redisReply*)redisCommand(conn, "EVALSHA %s 1 %s %s", script_unlock_hash, key, uuid);
    if( NULL == r)
    {
        ast_log(LOG_ERROR, "Execut command:[EVALSHA %s 1 %s %s] failure\n", script_unlock_hash, key, uuid);
        redisFree(conn);
        conn = NULL;
        ast_mutex_unlock(&redis_lock);
        return;
    }
    freeReplyObject(r);
    ast_mutex_unlock(&redis_lock); 
    return;
}

int ast_redis_read(const char* key, char* value, int valuelen){
    if(!redis_connect()){
        return -1;
    }    
    ast_mutex_lock(&redis_lock);
    redisReply* r = (redisReply*)redisCommand(conn, "GET %s", key); 
    if( NULL == r)  
    {  
        ast_log(LOG_ERROR, "Execut command:[GET %s] failure\n", key);  
        redisFree(conn);
        conn = NULL;  
        ast_mutex_unlock(&redis_lock);
        return -1;  
    }  
    if ( r->type != REDIS_REPLY_STRING)  
    {  
        ast_log(LOG_WARNING, "Failed to execute command[GET %s]\n", key);  
        freeReplyObject(r);  
        ast_mutex_unlock(&redis_lock);
        return -1;  
    }  
    ast_copy_string(value, r->str, valuelen);
    freeReplyObject(r);  
    ast_mutex_unlock(&redis_lock);
    return 0;
}

int ast_redis_write(const char* key, const char* value){
    if(!redis_connect()){
        return -1;
    }
    ast_mutex_lock(&redis_lock);
    redisReply* r = (redisReply*)redisCommand(conn, "SET %s %s", key, value);  
    if( NULL == r)  
    {  
        ast_log(LOG_ERROR, "Execut command:[SET %s %s] failure\n", key, value);  
        redisFree(conn);
        conn = NULL;
        ast_mutex_unlock(&redis_lock);  
        return -1;  
    }  
    if( !(r->type == REDIS_REPLY_STATUS && strcasecmp(r->str,"OK")==0))  
    {  
        ast_log(LOG_WARNING, "Failed to execute command[SET %s %s]\n", key, value);  
        freeReplyObject(r); 
        ast_mutex_unlock(&redis_lock);
        return -1;  
    }  
    freeReplyObject(r);  
    ast_mutex_unlock(&redis_lock);
    return 0;
}

int ast_redis_cmd(const char* cmd, char* value, int valuelen){
    if(!redis_connect()){
        return -1;
    }    
    ast_mutex_lock(&redis_lock);
    redisReply* r = (redisReply*)redisCommand(conn, cmd); 
    if( NULL == r)  
    {  
        ast_log(LOG_ERROR, "Execut command:[%s] failure\n", cmd);  
        redisFree(conn);
    conn = NULL;  
        ast_mutex_unlock(&redis_lock);
        return -1;  
    } 
    if(value != NULL){
        if ( r->type == REDIS_REPLY_STRING || r->type == REDIS_REPLY_STATUS || r->type == REDIS_REPLY_ERROR)
        {  
            ast_copy_string(value, r->str, valuelen);
        } else if ( r->type == REDIS_REPLY_INTEGER )  {

            snprintf(value, valuelen, "%llu", r->integer);
        }
    }
    freeReplyObject(r);  
    ast_mutex_unlock(&redis_lock);
    return 0;
}

static int function_redis_read(struct ast_channel *chan, const char *cmd,
                char *parse, char *buf, size_t len)
{
    AST_DECLARE_APP_ARGS(args,
        AST_APP_ARG(key);
    );

    buf[0] = '\0';

    if (ast_strlen_zero(parse)) {
        ast_log(LOG_WARNING, "REDIS requires an argument, REDIS(<key>)\n");
        return -1;
    }        
        AST_STANDARD_APP_ARGS(args, parse);
    if (ast_redis_read(args.key, buf, len - 1)) {
        ast_debug(1, "REDIS: %s not found in redis.\n", args.key);
            pbx_builtin_setvar_helper(chan, "REDIS_RESULT", "fail");
    }else{
            pbx_builtin_setvar_helper(chan, "REDIS_RESULT", "ok");
        }

    return 0;
}

static int function_redis_write(struct ast_channel *chan, const char *cmd, char *parse,
                 const char *value)
{
    AST_DECLARE_APP_ARGS(args,
        AST_APP_ARG(key);
    );

    if (ast_strlen_zero(parse)) {
        ast_log(LOG_WARNING, "REDIS requires an argument, REDIS(<key>)=<value>\n");
        return -1;
    }
        AST_STANDARD_APP_ARGS(args, parse);
    if (ast_redis_write(args.key, value)) {
        ast_log(LOG_WARNING, "REDIS: Error writing value to redis.\n");
            pbx_builtin_setvar_helper(chan, "REDIS_RESULT", "fail");
    }else{
            pbx_builtin_setvar_helper(chan, "REDIS_RESULT", "ok");
        }

    return 0;
}

static int function_redis_cmd(struct ast_channel *chan, const char *cmd,
                            char *parse, char *buf, size_t len)
{
        AST_DECLARE_APP_ARGS(args,
                AST_APP_ARG(cmd);
        );

        buf[0] = '\0';

        if (ast_strlen_zero(parse)) {
            ast_log(LOG_WARNING, "REDIS_CMD requires an argument, REDIS_CMD(<cmd>)\n");
            return -1;
        }
        AST_STANDARD_APP_ARGS(args, parse);
        if (ast_redis_cmd(args.cmd, buf, len - 1)) {
            ast_debug(1, "REDIS: %s not found in redis.\n", args.cmd);
            pbx_builtin_setvar_helper(chan, "REDIS_RESULT", "fail");
        }else{
            pbx_builtin_setvar_helper(chan, "REDIS_RESULT", "ok");
        }

        return 0;
}
static int function_redis_cmd_write(struct ast_channel *chan, const char *cmd, char *parse,
                             const char *value)
{
        AST_DECLARE_APP_ARGS(args,
                AST_APP_ARG(cmd);
        );

        if (ast_strlen_zero(parse)) {
            ast_log(LOG_WARNING, "REDIS_CMD requires an argument, REDIS_CMD(<cmd>)\n");
            return -1;
        }
        AST_STANDARD_APP_ARGS(args, parse);
        if (ast_redis_cmd(args.cmd, NULL, 0)) {
            ast_debug(1, "REDIS: %s not found in redis.\n", args.cmd);
            pbx_builtin_setvar_helper(chan, "REDIS_RESULT", "fail");
        }else{
            pbx_builtin_setvar_helper(chan, "REDIS_RESULT", "ok");
        }

        return 0;
}
static struct ast_custom_function redis_function = {
    .name = "REDIS",
    .read = function_redis_read,
    .write = function_redis_write,
};
static struct ast_custom_function redis_cmd_function = {
    .name = "REDIS_CMD",
    .read = function_redis_cmd,
    .write = function_redis_cmd_write,
};
static int unload_module(void)
{
    int res = 0;
    res |= ast_custom_function_unregister(&redis_function);
    return res;
}

static int load_module(void)
{
    int res = 0;
    struct ast_config *cfg;
    const char *s;
        struct ast_flags flags = { 0 };

    if (!(cfg = ast_config_load("redis.conf", flags))) {
        return 0;
    } else if (cfg == CONFIG_STATUS_FILEINVALID) {
        ast_log(LOG_WARNING, "res_redis.conf could not be parsed!\n");
        return 0;
    }

    if (!(s = ast_variable_retrieve(cfg, "general", "server"))) {
        ast_log(LOG_WARNING,"No redis server config, use 127.0.0.1 by default.\n");
            strcpy(g_server, "127.0.0.1");
    } else {
        ast_copy_string(g_server, s, sizeof(g_server));
    }
    if (!(s = ast_variable_retrieve(cfg, "general", "port"))) {
        ast_log(LOG_WARNING, "No redis port config, use 6379 by default.\n");
            g_port=6379;
    } else {
        g_port = atoi(s);
    }
    if (!(s = ast_variable_retrieve(cfg, "general", "timeout"))) {
            ast_log(LOG_WARNING, "No redis timeout config, use 3000 by default.\n");
            g_timeout.tv_sec=3000;
    } else {
        g_timeout.tv_sec = atoi(s);
    }
    ast_config_destroy(cfg);
    res |= ast_custom_function_register(&redis_function);
    res |= ast_custom_function_register(&redis_cmd_function);
    return res;
}

AST_MODULE_INFO(ASTERISK_GPL_KEY, AST_MODFLAG_GLOBAL_SYMBOLS, "Redis related dialplan functions",
    .load = load_module,
    .unload = unload_module,
    );

