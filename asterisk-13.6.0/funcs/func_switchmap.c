/*
 * Asterisk -- An open source telephony toolkit.
 *
 * Copyright (C) 2005 - 2006, Digium, Inc.
 * Copyright (C) 2005, Claude Patry
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
 * \brief switch map 
 * 
 * \ingroup functions
 */

#include "asterisk.h"

ASTERISK_FILE_VERSION(__FILE__, "$Revision: 20140723 $")

#include "asterisk/module.h"
#include "asterisk/pbx.h"	/* function register/unregister */
#include "asterisk/utils.h"
#include "asterisk/strings.h"
#include "asterisk/cJSON.h"
#include "asterisk/app.h"
#include <float.h>
#include <math.h>

/*** DOCUMENTATION
	<function name="SWITCH_MAP" language="en_US">
		<synopsis>
			Map certain string from json object.
		</synopsis>
		<syntax>
			<parameter name="string" required="true">
				<para>Input string</para>
			</parameter>
			<parameter name="jsonstring" required="true">
                <para>Input string. "jsonstring" should be JSON object, and it's format</para>
				<para>like {"sex":1,"name":"Jack","weight":65.50}, and cannot include sub-object or sub-array.</para>
            </parameter>
		</syntax>
		<description>
			<para>Returns the value which key matched the string.</para>
			<para>Example:</para>
			<para>Set(item=name);</para>
			<para>Set(json_str={"item":"student","sex":1,"name":"Jack","weight":65.50,"5":123});</para>
			<para>NoOp(${SWITCH_MAP(item,json_str)}); //output: student</para>
			<para>NoOp(${SWITCH_MAP(${item},json_str)}); //output: Jack</para>
			<para>NoOp(${SWITCH_MAP(sex,json_str)}); //output: 1</para>
			<para>NoOp(${SWITCH_MAP(5,json_str)}); //output: 123</para>
		</description>
		<see-also>
		</see-also>
	</function>
 ***/

static int switch_map_exec(struct ast_channel *chan, const char *cmd, char *data,
			 char *buf, ssize_t len)
{
	char *parse;
	char strbuf[1024];
	char varbuf[1024];
	AST_DECLARE_APP_ARGS(args,
			AST_APP_ARG(parm);
			AST_APP_ARG(jsonstring);
	);
	const char *varvalue;
        cJSON *doc;
        
        memset(strbuf, 0, sizeof(strbuf));
        memset(varbuf, 0, sizeof(varbuf));

	parse = ast_strdupa(data);

	AST_STANDARD_APP_ARGS(args, parse);
	if (ast_strlen_zero(args.jsonstring) || ast_strlen_zero(args.parm)) {
		ast_log(LOG_WARNING, "Syntax: %s(parm,jsonstring) - missing argument!\n", cmd);
		return -1;
	}

	if (args.parm[0] == '$' && strlen(args.parm)>3){ // ${cdr_status} -> 2
		snprintf(varbuf,(int)strlen(args.parm)-2,"%s", args.parm+2);
		varvalue=pbx_builtin_getvar_helper(chan, varbuf);
		if(varvalue){
			snprintf(varbuf,(int)strlen(varvalue)+1,"%s",varvalue);
		}
	} else { // cdr_status -> cdr_status
		snprintf(varbuf,(int)strlen(args.parm)+1,"%s",args.parm);
	}
	ast_log(LOG_NOTICE, "~~~~~~args.parm:%s ~~~ varbuf:%s\n", args.parm,varbuf);
	
	doc = cJSON_Parse(pbx_builtin_getvar_helper(chan, args.jsonstring));
	if (!doc) {
		ast_log(LOG_WARNING, "%s is not a valid JSON string!\n", args.jsonstring);
		return -1;
	} else { // valid JSON string
		if (doc->type == cJSON_Object) { // format like {"name":"Jack"} or {"name":"Jack","age":10} and so on
			cJSON *c = doc->child;
			while(c) {
				if(!strcmp(c->string, varbuf)){
					if(c->type == cJSON_Number) { // number, include int/double/float
						double d = c->valuedouble;
						if (fabs(((double)c->valueint)-d)<=DBL_EPSILON && d<=INT_MAX && d>=INT_MIN){
							snprintf(strbuf,sizeof(strbuf),"%d",c->valueint);
						} else {
							if (fabs(floor(d)-d)<=DBL_EPSILON)
								snprintf(strbuf,sizeof(strbuf),"%.0f",d);
							else if (fabs(d)<1.0e-6 || fabs(d)>1.0e9)
								snprintf(strbuf,sizeof(strbuf),"%e",d);
							else
								snprintf(strbuf,sizeof(strbuf),"%f",d);
						}
					} else if (c->type == cJSON_String) { // string
						snprintf(strbuf,sizeof(strbuf),"%s", c->valuestring);
					} else if(c->type == cJSON_True) { // true
						snprintf(strbuf, sizeof(strbuf), "%d", 1);
					} else if (c->type == cJSON_False) { // false
						snprintf(strbuf, sizeof(strbuf), "%d", 0);
					} else if (c->type == cJSON_NULL) { // null
						snprintf(strbuf, sizeof(strbuf), "%s", "null");
					}
					ast_copy_string(buf, strbuf, len);
				}
				c = c->next;
			}
		} else { // Only JSON Object 
			ast_log(LOG_WARNING, "Only allow JSON Object!\n");
			return -1;
		}
	}

	return 0;
}

static int switch_map_helper(struct ast_channel *chan, const char *cmd, char *data,
			 char *buf, size_t len)
{
	return switch_map_exec(chan, cmd, data, buf, len);
}

static struct ast_custom_function switch_map_function = {
	.name = "SWITCH_MAP",
	.read = switch_map_helper,
};

static int unload_module(void)
{
	return ast_custom_function_unregister(&switch_map_function);
}

static int load_module(void)
{
	return ast_custom_function_register(&switch_map_function);
}

AST_MODULE_INFO_STANDARD(ASTERISK_GPL_KEY, "switch map dialplan functions");
