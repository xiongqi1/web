#ifndef _LUAPARAMETER_H
#define _LUAPARAMETER_H

int li_param_meta_store(const char *name, const char *meta);
int li_param_meta_retrieve(const char *name, char **meta, size_t *len);
int li_param_meta_delete(const char *name);

int li_param_value_init(const char *name, const char *value);
int li_param_value_store(const char *name, const char *value);
int li_param_value_retrieve(const char *name, char **value, size_t *len);
int li_param_value_delete(const char *name);

int li_param_object_create(const char *name, int instanceId);
int li_param_object_delete(const char *name);

int li_param_delete();

#ifdef PLATFORM_PLATYPUS
int li_nvram_value_retrieve(const char *name, char **value, size_t *len);
#endif

#endif
