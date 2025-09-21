#pragma once

#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <sys/stat.h>
#include <time.h>

// builds a permission string like "-rwxr-xr-x"
static void status_permstring(const mode_t mode, char out[11]) {
  const char flags[] = {'r', 'w', 'x'};
  // file type
  if (S_ISDIR(mode))
    out[0] = 'd';
  else if (S_ISCHR(mode))
    out[0] = 'c';
  else if (S_ISBLK(mode))
    out[0] = 'b';
  else if (S_ISLNK(mode))
    out[0] = 'l';
  else if (S_ISFIFO(mode))
    out[0] = 'p';
  else if (S_ISSOCK(mode))
    out[0] = 's';
  else
    out[0] = '-';

  // user, group, other
  for (int who = 0; who < 3; who++) {
    for (int bit = 0; bit < 3; bit++) {
      mode_t m = (mode_t)1 << (8 - (who * 3 + bit));
      out[1 + who * 3 + bit] = (mode & m) ? flags[bit] : '-';
    }
  }

  // special bits: setuid, setgid, sticky
  if (mode & S_ISUID) {
    out[3] = (out[3] == 'x') ? 's' : 'S';
  }
  if (mode & S_ISGID) {
    out[6] = (out[6] == 'x') ? 's' : 'S';
  }
  if (mode & S_ISVTX) {
    out[9] = (out[9] == 'x') ? 't' : 'T';
  }

  out[10] = '\0';
}

// builds a human readible size string like "128.9K"
static void status_human_size(const off_t size, char out[8]) {
  const char *units[] = {"B", "K", "M", "G", "T", "P"};
  double s = (double)size;
  int ui = 0;
  while (s >= 1024.0 && ui < (int)(sizeof(units) / sizeof(*units)) - 1) {
    s /= 1024.0;
    ui++;
  }
  // "%.1f%s", but trim ".0"
  if (s - (int)s < 0.05) // e.g. 5.0 -> 5
    snprintf(out, 8, "%d%s", (int)s, units[ui]);
  else
    snprintf(out, 8, "%.1f%s", s, units[ui]);
}

char *status_normal_string(const struct stat *st) {
  static char status_string[200];
  char perms[11];
  status_permstring(st->st_mode, perms);

  struct passwd *pw = getpwuid(st->st_uid);
  const char *uname = pw ? pw->pw_name : "UNKNOWN";

  struct group *gr = getgrgid(st->st_gid);
  const char *gname = gr ? gr->gr_name : "UNKNOWN";

  char hsize[8];
  status_human_size(st->st_size, hsize);

  char timestr[64];
  struct tm tm;
  localtime_r(&st->st_mtime, &tm);
  // e.g. "Fri Aug  1 23:01:46 2025"
  strftime(timestr, sizeof(timestr), "%a %b %e %H:%M:%S %Y", &tm);

  sprintf(status_string, "%s %s %s %s %s", perms, uname, gname, hsize, timestr);
  return status_string;
}
