/* Copyright 2011 Nick Mathewson, George Kadianakis
   See LICENSE for other credits and copying information
*/

/**
 * \file obfs_main.c
 * \brief Real entry point of obfsproxy. Immediately calls
 * main.c:obfs_main().
 *
 * \details This all happens so that our unittests binary can link
 * against main.c.
 **/

int obfs_main(int argc, char *argv[]);

/**
   Simply calls obfs_main() from main.c;
   it allows our unittests binary to link against main.c.
*/
int
main(int argc, char *argv[])
{
  return obfs_main(argc, argv);
}

