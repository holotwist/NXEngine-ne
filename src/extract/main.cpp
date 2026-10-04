#include "extractfiles.h"
#include "extractpxt.h"
#include "extractstages.h"

#include <cstdio>
#include <cstdlib>

int main(int argc, char *argv[])
{
  const char *exename = "Doukutsu.exe";
  const char *outdir  = "data/doukutsu_data";

  if (argc > 1) exename = argv[1];
  if (argc > 2) outdir  = argv[2];

  FILE *fp = fopen(exename, "rb");
  if (!fp)
  {
    printf("Can't open %s!\n", exename);
    return 1;
  }

  if (extract_pxt(fp, outdir)) return 1;
  if (extract_files(fp, outdir)) return 1;
  if (extract_stages(fp, outdir)) return 1;

  fclose(fp);
  printf("Successfully extracted to %s.\n", outdir);
  return 0;
}
