#ifndef __EXTRACTFILES_H_
#define __EXTRACTFILES_H_

#include <cstdio>

void createdir(const char *fname);
bool extract_files(FILE *exefp, const char *out_dir = "data/doukutsu_data");

#endif