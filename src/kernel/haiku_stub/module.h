/*
 * module.h - Host-only stand-in for Haiku's <module.h>.
 */
#ifndef _MODULE_H
#define _MODULE_H

#include <SupportDefs.h>

#ifdef __cplusplus
extern "C" {
#endif

#define B_MODULE_INIT		1
#define B_MODULE_UNINIT		2

#define B_KEEP_LOADED		0x01
#define B_DONT_KEEP_LOADED	0x00

typedef struct module_info {
	const char	*name;
	uint32		flags;
	status_t	(*std_ops)(int32 op, ...);
} module_info;

status_t	get_module(const char *path, module_info **info);
status_t	put_module(const char *path);

#ifdef __cplusplus
}
#endif

#endif	/* _MODULE_H */
