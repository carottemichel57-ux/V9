#include <taihen.h>
#include <stdint.h>
#include <string.h>
#include <psp2kern/kernel/sysmem.h>
#include <psp2kern/io/fcntl.h>
#include <psp2kern/io/stat.h>
#include <psp2kern/kernel/modulemgr.h>

/* securite : au cas ou le header ne definit pas ces constantes */
#ifndef SCE_KERNEL_START_SUCCESS
#define SCE_KERNEL_START_SUCCESS 0
#endif
#ifndef SCE_KERNEL_STOP_SUCCESS
#define SCE_KERNEL_STOP_SUCCESS 0
#endif

#define SAITY_MAX_PATH 256
#define SAITY_MAX_DATA 65536
#define SAITY_DIR "ux0:data/saity"

static char k_path[SAITY_MAX_PATH];
static uint8_t k_buf[SAITY_MAX_DATA];

/* The user plugin's legacy fallback names are intentionally mapped to this
   private, writable location. No user pointer is passed to ksceIo*. */
/* pas de libc dans un module kernel (-nostdlib) : on fait les copies a la main */
static unsigned int k_strlen(const char *s) { unsigned int n = 0; while (s[n]) n++; return n; }

static int make_path(const char *user_path) {
    const char *base = 0;
    for (const char *p = user_path; *p; p++) if (*p == '/') base = p + 1;
    if (!base || !base[0]) return -1;
    unsigned int len = k_strlen(base);
    if (len >= SAITY_MAX_PATH - sizeof(SAITY_DIR) - 2) return -1;
    for (unsigned int i = 0; i < len; i++) if (base[i] == ':') return -1;
    char tmp[SAITY_MAX_PATH];                       /* user_path peut etre k_path lui-meme : on copie le nom d'abord */
    for (unsigned int i = 0; i <= len; i++) tmp[i] = base[i];
    unsigned int d = sizeof(SAITY_DIR) - 1;
    for (unsigned int i = 0; i < d; i++) k_path[i] = SAITY_DIR[i];
    k_path[d] = '/';
    for (unsigned int i = 0; i <= len; i++) k_path[d + 1 + i] = tmp[i];
    return 0;
}

int saity_write(const char *path, const void *data, unsigned int size) {
    if (!path || !data || size > SAITY_MAX_DATA) return -1;
    if (ksceKernelStrncpyFromUser(k_path, path, SAITY_MAX_PATH) < 0) return -2;
    k_path[SAITY_MAX_PATH - 1] = 0;
    if (make_path(k_path) < 0) return -3;
    if (ksceKernelMemcpyUserToKernel(k_buf, data, size) < 0) return -4;
    (void)ksceIoMkdir("ux0:data", 0777);
    (void)ksceIoMkdir(SAITY_DIR, 0777);
    SceUID fd = ksceIoOpen(k_path, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0666);
    if (fd < 0) return (int)fd;
    int written = ksceIoWrite(fd, k_buf, size);
    int close_result = ksceIoClose(fd);
    if (written < 0) return written;
    if ((unsigned int)written != size) return -5;
    return close_result < 0 ? close_result : written;
}

int saity_read(const char *path, void *data, unsigned int max) {
    if (!path || !data || max > SAITY_MAX_DATA) return -1;
    if (ksceKernelStrncpyFromUser(k_path, path, SAITY_MAX_PATH) < 0) return -2;
    k_path[SAITY_MAX_PATH - 1] = 0;
    if (make_path(k_path) < 0) return -3;
    SceUID fd = ksceIoOpen(k_path, SCE_O_RDONLY, 0);
    if (fd < 0) return (int)fd;
    int n = ksceIoRead(fd, k_buf, max);
    int close_result = ksceIoClose(fd);
    if (n < 0) return n;
    if (close_result < 0) return close_result;
    if (ksceKernelMemcpyKernelToUser(data, k_buf, (unsigned int)n) < 0) return -4;
    return n;
}

int module_start(SceSize argc, const void *args) {
    int fd = ksceIoOpen("ux0:data/saity_kernel_test.txt", SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0666);
    if (fd >= 0) { static const char msg[] = "saity kernel ok\n"; ksceIoWrite(fd, msg, sizeof(msg)-1); ksceIoClose(fd); }
    return SCE_KERNEL_START_SUCCESS;
}
int module_stop(SceSize argc, const void *args) { return SCE_KERNEL_STOP_SUCCESS; }
