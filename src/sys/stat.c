#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "fs.h"
#include "stat.h"
#include "dirent.h"

extern int8_t fs_follow_path(DIR *restrict dir,
                             const char *restrict path,
                             uint8_t flags);

/*!
 * @brief Get file attributes by name
 * @param dir A base dir for a relative filename
 * @param flags Flags
 * @param path Pathname to file
 * @param stat Struct to store attributes
 * @return 0 on success
 */
static int8_t __stat_at(DIR *restrict dir, int flags,
                        const char *restrict path,
                        struct stat *restrict stat) {
    int8_t err;

    if (!dir) {
        dir = malloc(sizeof(*dir));
        if (dir)
            // ENOMEM
            return -1;
        memset(dir, 0, sizeof(*dir));
    }

    err = fs_follow_path(dir, path, flags);

    return -1;
}

/*!
 * @brief Get file attributes from \p path and put them to \p buf
 * @param path
 * @param buf
 * @return 0 on success; negative on error (errno)
 */
int8_t stat(const char *restrict path, struct stat *restrict buf) {
    int8_t err;

    err = __stat_at(NULL, 0, path, buf);
    if (err)
        return err;

    return 0;
}
