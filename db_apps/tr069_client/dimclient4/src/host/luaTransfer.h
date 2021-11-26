#ifndef _LUATRANSFER_H
#define _LUATRANSFER_H

int li_transfer_deleteAll();
int li_transfer_delete(char *name);
int li_transfer_add(char *name, char *value);
int li_transfer_getAll(int (*callback)(char *name, char *value));

int li_transfer_download_before(const char *name, const char *type);
int li_transfer_download_after(const char *name, const char *type);

int li_transfer_upload_before(const char *name, const char *type);
int li_transfer_upload_after(const char *name, const char *type);

#if defined(PLATFORM_PLATYPUS)
int li_transfer_get_configfilename(char **value, size_t *len);
#endif

#endif
