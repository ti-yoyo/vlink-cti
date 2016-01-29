#ifndef _FUNC_REDIS_H
#define _FUNC_REDIS_H
int ast_redis_lock(const char* key, int timeout, char* uuid);
void ast_redis_unlock(const char* key, const char* uuid);
int ast_redis_read(const char* key, char* value, int valuelen);
int ast_redis_write(const char* key, const char* value);
int ast_redis_cmd(const char* key, char* value, int valuelen);
#endif
