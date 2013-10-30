/* config/config.h.  Generated from config.h.in by configure.  */
/* config/config.h.in.  Generated from configure.ac by autoheader.  */

/* Define if building universal (internal helper macro) */
/* #undef AC_APPLE_UNIVERSAL_BUILD */

/* If we have byteswap.h */
#define HAVE_BYTESWAP_H 1

/* Define to 1 if you have the <dlfcn.h> header file. */
#define HAVE_DLFCN_H 1

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Define to 1 if you have the `gcs' library (-lgcs). */
/* #undef HAVE_LIBGCS */

/* Define if libmysql is available */
/* #undef HAVE_LIBMYSQLCLIENT */

/* Define to 1 if you have the `yogrt' library (-lyogrt). */
/* #undef HAVE_LIBYOGRT */

/* Define to 1 if you have the <memory.h> header file. */
#define HAVE_MEMORY_H 1

/* Define to 1 if you have a valid installation of MOAB */
/* #undef HAVE_MOAB */

/* Define to 1 if you have MPI libs and headers. */
#define HAVE_MPI 1

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 1

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* Define to the sub-directory in which libtool stores uninstalled libraries.
   */
#define LT_OBJDIR ".libs/"

/* Define the project alias string (name-ver or name-ver-rel). */
#define META_ALIAS "scr-1.1-8"

/* Define the project author. */
#define META_AUTHOR "Adam Moody <moody20@llnl.gov>"

/* Define the project release date. */
/* #undef META_DATE */

/* Define the libtool library 'age' version information. */
/* #undef META_LT_AGE */

/* Define the libtool library 'current' version information. */
/* #undef META_LT_CURRENT */

/* Define the libtool library 'revision' version information. */
/* #undef META_LT_REVISION */

/* Define the project name. */
#define META_NAME "scr"

/* Define the project release. */
#define META_RELEASE "8"

/* Define the project version. */
#define META_VERSION "1.1"

/* Define to 1 if your C compiler doesn't accept -c and -o together. */
/* #undef NO_MINUS_C_MINUS_O */

/* Name of package */
#define PACKAGE "scr"

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT ""

/* Define to the full name of this package. */
#define PACKAGE_NAME ""

/* Define to the full name and version of this package. */
#define PACKAGE_STRING ""

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME ""

/* Define to the version of this package. */
#define PACKAGE_VERSION ""

/* Specifies default base path for SCR cache directory */
#define SCR_CACHE_BASE "/tmp"

/* Specifies default base path for SCR control directory */
#define SCR_CNTL_BASE "/scr"

/* Specifies full path and file name to SCR config file */
#define SCR_CONFIG_FILE "/etc/scr/scr.conf"

/* Define for CRAY_XT machine type. */
#define SCR_CRAY_XT 1

/* Specify the type of machine. */
#define SCR_MACHINE_NAME TSUBAME2

/* Specify the type of machine. */
#define SCR_MACHINE_TYPE 5

/* Define for TLCC machine type. */
#define SCR_TLCC 0

/* Define for TSUBAME2.0 machine type. */
#define SCR_TSUBAME2 5

/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS 1

/* We should use flock for locking files */
#define USE_FLOCK 1

/* We should use fnctl for locking files */
/* #undef USE_FNCTL */

/* Version number of package */
#define VERSION "1.1-8"

/* Define WORDS_BIGENDIAN to 1 if your processor stores words with the most
   significant byte first (like Motorola and SPARC, unlike Intel). */
#if defined AC_APPLE_UNIVERSAL_BUILD
# if defined __BIG_ENDIAN__
#  define WORDS_BIGENDIAN 1
# endif
#else
# ifndef WORDS_BIGENDIAN
/* #  undef WORDS_BIGENDIAN */
# endif
#endif

/* Expansion of the "bindir" installation directory. */
#define X_BINDIR "/home/usr2/11D37048/usr/tools/scr-1.1-8/bin"

/* Expansion of the "datadir" installation directory. */
#define X_DATADIR "${prefix}/share"

/* Expansion of the "exec_prefix" installation directory. */
#define X_EXEC_PREFIX "/home/usr2/11D37048/usr/tools/scr-1.1-8"

/* Expansion of the "includedir" installation directory. */
#define X_INCLUDEDIR "/home/usr2/11D37048/usr/tools/scr-1.1-8/include"

/* Expansion of the "infodir" installation directory. */
#define X_INFODIR "${prefix}/share/info"

/* Expansion of the "libdir" installation directory. */
#define X_LIBDIR "/home/usr2/11D37048/usr/tools/scr-1.1-8/lib"

/* Expansion of the "libexecdir" installation directory. */
#define X_LIBEXECDIR "/home/usr2/11D37048/usr/tools/scr-1.1-8/libexec"

/* Expansion of the "localstatedir" installation directory. */
#define X_LOCALSTATEDIR "/home/usr2/11D37048/usr/tools/scr-1.1-8/var"

/* Expansion of the "mandir" installation directory. */
#define X_MANDIR "${prefix}/share/man"

/* Expansion of the "oldincludedir" installation directory. */
#define X_OLDINCLUDEDIR "/usr/include"

/* Expansion of the "prefix" installation directory. */
#define X_PREFIX "/home/usr2/11D37048/usr/tools/scr-1.1-8"

/* Expansion of the "sbindir" installation directory. */
#define X_SBINDIR "/home/usr2/11D37048/usr/tools/scr-1.1-8/sbin"

/* Expansion of the "sharedstatedir" installation directory. */
#define X_SHAREDSTATEDIR "/home/usr2/11D37048/usr/tools/scr-1.1-8/com"

/* Expansion of the "sysconfdir" installation directory. */
#define X_SYSCONFDIR "/home/usr2/11D37048/usr/tools/scr-1.1-8/etc"
