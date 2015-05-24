/*
 * lshwd - lists hardware devices and their approp modules. 
 * see the README for additional information.
 *
 * by z4ziggy at bliss-solutions dot org
 *
 */

#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/syscall.h>
#include <pci/pci.h>
#include <usb.h>
#include "usb_names.h"
#include "psaux.h"
#include "pcmcia.h"
#include <syslog.h>
/*
 * only 1 thing should usually be tampered with - the updated_modules. behind that, 
 * its your program :-)
 */

/*
 * generic modules to be changed. format = "generic module name", "kernel module name"
 * change to suit whatever changes are needed in the table files (pci/usb/pcmcia)
 */

#define lshwd_version "1.1.3"

const char *
updated_modules[][2] =
{
	//{ "Mouse:USB|Wheel", "usbhid"},
	{ "nv", "nvidia"},
	{ "via82cxxx", "snd_via82xx"},
	{ "usb-uhci", "uhci_hcd"},						// usb-uhci=2.4 uhci-hcd=2.6 
	{ "usb-ohci", "ohci_hcd"}						// usb-uhci=2.4 uhci-hcd=2.6
};

/* the defines, we cant live without... ;) */

#define LSHWD_VERSION           "lshwd " lshwd_version ", list hardware devices with approp modules\n" \
                                "Detects all types of PCI/PCMCIA/USB/FireWire cards/chips (ethernet,vga,sound,etc.).\n" \
                                "coded by <z4ziggy at bliss-solutions dot org> for Arch linux, Nov. 2004\n\n"

#define SHARED_PATH 		"/usr/share/hwdata"		/* shared path which contains \
								   pcitable/usbtable/pcmciatable*/
#define USBIDS_FILE 		"/usr/share/usb.ids"		/* file used for usb naming  	*/
#define MODPROBE_PGM		"/sbin/modprobe"		/* the secret of life  		*/

#ifdef DEBUG
#undef DEBUG
#define DEBUG(s...) fprintf(stderr, s)
#else
#define DEBUG(s...) ;
#endif
	
/*
 *
 * below here nothing should be changed
 *
 */

/* some global vars */
char *lookup_block;			/* allocated buffer for modules list (table files)	*/
uint lookup_block_len;			/* length of allocated buffer (table files) 		*/
static struct pci_dev *first_dev; 	/* first device in pci device list from pci-lib 	*/
static struct usb_device *first_usb_dev;
int showids, autoload, disdefdesc,machinemode,plainmode,showlist,removeduplicates,outputxinfo;
int usbmousefound;

// ANSI COLORS (from Arch scheme)
#define COLOR_NORMAL	"\033[0;39m" 
// RED: Warning message 
#define COLOR_RED	"\033[1;31m" 
// GREEN: Success message 
#define COLOR_GREEN	"\033[1;32m" 
// YELLOW: Attention message 
#define COLOR_YELLOW	"\033[1;33m" 
// BLUE: System message 
#define COLOR_BLUE	"\033[1;34m" 
// BOLD WHITE: Found devices and modules 
#define COLOR_WHITE	"\033[1;37m"

char *title_color = COLOR_BLUE;
char *modules_color = COLOR_WHITE;
char *normal_color = COLOR_NORMAL;

/*
char *green = "\033[0;40;32m";
char *title_color = "\033[1;34m";
char *modules_color = "\033[1;36m";	// cyan
char *normal_color = "\033[0m";
*/
/*
char *green = "\033[0;40;32m"; 
char *title_color = "\033[0;34m"; //--- device category 
char *modules_color = "\033[1;36m"; //--- modules and devices 
char *normal_color = "\033[0m";  //--- description 
*/

/*
 *
 * let the real work begin
 *
 */

int
usage(char *option)
{
	printf(LSHWD_VERSION);
	if (option) fprintf(stderr,"lshwd: invalid option '%s'\n",option);
	printf( "usage: lshwd [-a] [-c] [-cc] [-d] [-id] [-m] [-n] [-ox]\n"
		"\t-a   auto-modprobe\n"
		"\t-c   categorized output\n"
		"\t-cc  colorized & categorized output\n"
		"\t-d   display default description\n"
		"\t-id  display hardware id\n"
		"\t-m   machine readable format\n"
		"\t-n   no duplicates\n"
		"\t-ox  output X info to /tmp/xinfo (gfx card section only)\n"
	);
	return option? 1 : 0;
}

/* qsort procedure for sorting pci devices per class type */
int
compare_pci_class(const void *A, const void *B)
{
	struct pci_dev *a = (*(struct pci_dev **)A);
	struct pci_dev *b = (*(struct pci_dev **)B);
	unsigned int ca,cb;
	ca = pci_read_word(a, PCI_CLASS_DEVICE);	/* Read config register directly */
	cb = pci_read_word(b, PCI_CLASS_DEVICE);	/* Read config register directly */
	if (ca > cb) return 1;
	if (ca < cb) return -1;
	return 0;
}

/* qsort procedure for sorting pci devices per device bus/dev/func id */
int
compare_pci_id(const void *A, const void *B)
{
	struct pci_dev *a = (*(struct pci_dev **)A);
	struct pci_dev *b = (*(struct pci_dev **)B);
	if (a->bus < b->bus) return -1;
	if (a->bus > b->bus) return 1;
	if (a->dev < b->dev) return -1;
	if (a->dev > b->dev) return 1;
	if (a->func < b->func) return -1;
	if (a->func > b->func) return 1;
	return 0;
}

/* sorting the pci devices  */
void
sort_pci_list(void)
{
	struct pci_dev **index, **h, **last_dev;
	int cnt;
	struct pci_dev *d;
	
	/* counting number of total devices */
	for(cnt=0,d=first_dev; d; d=d->next)
	{	cnt++;
		pci_fill_info(d, PCI_FILL_IDENT | PCI_FILL_BASES);	/* Fill in header info we need 	*/
        }
	/* allocating memory for all pci devices */
	h = index = alloca(sizeof(struct pci_dev *) * cnt);
	for(d=first_dev; d; d=d->next)
		*h++ = d;
	if (showlist)
		qsort(index, cnt, sizeof(struct pci_dev *), compare_pci_id);
	else
		qsort(index, cnt, sizeof(struct pci_dev *), compare_pci_class);
	/* arranging result of qsort starting from first_dev */
	last_dev = &first_dev;
	h = index;
	/* loop on all devices and copy contents from array to pci device list */
	while (cnt--)
	{
		*last_dev = *h;
		/* checking for duplicates only if removeduplicates is on */
		/* we compare the vendor_id and device_id of next item */
		if (!removeduplicates || ((!cnt) || //((!(*(h+1))) ||
			(!( ((*(h+1))->vendor_id == (*last_dev)->vendor_id) &&
			    ((*(h+1))->device_id == (*last_dev)->device_id) ))))
		{
			last_dev = &(*last_dev)->next;
		}
		h++;
	}
	*last_dev = NULL;
}

/* qsort procedure for sorting usb devices per bInterfaceClass */
int
compare_usb_class(const void *A, const void *B)
{
	struct usb_device *a = (*(struct usb_device **)A);
	struct usb_device *b = (*(struct usb_device **)B);
	unsigned int ca,cb;
	ca = a->config[0].interface[0].altsetting[0].bInterfaceClass;	/* get USB class */
	cb = b->config[0].interface[0].altsetting[0].bInterfaceClass;	/* get USB class */
	if (ca > cb) return 1;
	if (ca < cb) return -1;
	return 0;
}

/* sorting usb devices */
void
sort_usb_list(void)
{
	struct usb_bus *bus; 
	struct usb_device *dev; 
	struct usb_device **index, **h, **last_dev;
	int cnt;

	cnt = 0;
	/* counting number of total devices */
	for (bus = usb_busses; bus; bus = bus->next) 
		for (dev = bus->devices; dev; dev = dev->next) 
			cnt++;
	/* allocating memory for all usb devices */
	h = index = alloca(sizeof(struct usb_device *) * cnt);
	
	/* arranging all usb devices in 1 array */
	for (bus = usb_busses; bus; bus = bus->next) 
		for (dev = bus->devices; dev; dev = dev->next) 
			*h++ = dev;
	
	/* only need to sort if not in showlist mode - since its sorted already */
	if (!showlist)
		qsort(index, cnt, sizeof(struct usb_device *), compare_usb_class);
	/* arranging result of qsort starting from first_dev */
	last_dev = &first_usb_dev;
	h = index;
	/* loop on all devices and copy contents from array to usb device list */
	while (cnt--)
	{
		*last_dev = *h;
		if (!removeduplicates || (!(cnt) ||
			!(((*(h+1))->descriptor.idVendor == (*last_dev)->descriptor.idVendor) &&
			((*(h+1))->descriptor.idProduct == (*last_dev)->descriptor.idProduct) &&
			((*(h+1))->descriptor.bcdUSB == (*last_dev)->descriptor.bcdUSB))))
		{
			last_dev = &(*last_dev)->next;
		}
		h++;
	}
	*last_dev = NULL;
}

/* quick compare strings procedure */
int
compare(char *string1,char *string2,int len)
{
	while(len)
	{
		if(*string1 != *string2)
			break;
		string1++;
		string2++;
		len--;
	}
	return(!len);
}

/* search in updated_modules for module name, and replace with updated one if found */
void
updated_module_name(char* module)
{
	uint i = 0;
	for (i=sizeof(updated_modules)/sizeof(updated_modules[0]); i--;)
	{
		if (compare((char*)updated_modules[i][0], module, strlen(updated_modules[i][0])))
		{
			sprintf(module, updated_modules[i][1]);
		}
	}
}

/* search for module name and description according to vendorid and deviceid. */
/* the search uses buffer (lookup_block) allocated by init_lookup_block which must to be called prior */
int
lookup_module(int vendorid, int deviceid, char* module, int sizeofbuf, char* description, int sizeofdesc)
{
	char cmpbuf[16];
	/* formatting the string to search for */
	sprintf(cmpbuf, "0x%04x\t0x%04x", vendorid, deviceid);
	//memset(module, 0, sizeofbuf);
	/* default module name... */
	sprintf(module,"unknown");
	uint i = 0;
	char *buf = lookup_block;
	while (i<lookup_block_len)
	{
		if (*buf == cmpbuf[0])
		{
			if (compare(buf, cmpbuf, 13))
			{
				for (; *buf++ != '\"'; );
				/*lets grab the pcitable module name*/
				for (i=0; (i < sizeofbuf) && (*buf != '\n') && (*buf != '\"') ; module[i++] = *buf++);
				module[i] = '\0';
				/* check if module name should be replaced according to updated_modules */
				updated_module_name(module);
				(void) *buf++;
				for (; *buf++ != '\"'; );
				for (i=0; (i < sizeofdesc) && (*buf != '\n') && (*buf != '\"') ; description[i++] = *buf++);
				description[i] = '\0';
				return 1;
			}
		}
		for (; (i++ < lookup_block_len) && (*buf++ != '\n') ; );
	}
	return 0;
}

/* free lookup_block after calling init_lookup_block */
void
cleanup_lookup_block(void)
{
	free(lookup_block);
	lookup_block = NULL;
}

/* allocating buffer (lookup_block) and reading table file to memory for quick searching */
void
init_lookup_block(char *filename)
{
	char sharedfile[1024];
	sprintf(sharedfile, "%s/%s", SHARED_PATH, filename);
	lookup_block_len = 0;
	FILE *f;
	int num;
	if ((f=fopen(filename, "r")) || (f=fopen(sharedfile,"r")))
	{
		fseek(f, 0L, SEEK_END);
		lookup_block_len = ftell(f);
		rewind(f);
		lookup_block = (char*)realloc(lookup_block, lookup_block_len);
		num = fread(lookup_block, sizeof(char), lookup_block_len, f);
		(void) num;
		fclose(f);
	}
}

#define USE_FORK_COMMAND
#ifdef USE_FORK_COMMAND
/* executing command to /dev/null */
static int
execCommand(char **argv)
{
	int status, pid;

	if (!(pid = fork()))
	{
		close(0);
		close(1);
		close(2);
		open("/dev/null", 2);
		open("/dev/null", 1);
		open("/dev/null", 0);
		execv(argv[0], argv);
		exit(-1);
	}
	waitpid(pid,&status, 0); //WCONTINUED);
	//wait(&status);
	if (WIFEXITED(status))
		return WEXITSTATUS(status);
	return -1;
}
#else
static int
execCommand(char **argv)
{
	char cmd[1024]={0};
	int n;
	for (n=0;argv[n];n++)
	{
		strcat(cmd, argv[n]);
		strcat(cmd, " ");
	}
	//strcat(cmd,"> /dev/null 2>&1");
	//strcat(cmd,"&>/dev/null");
	strcat(cmd,">&/dev/null");
	return system(cmd);
}
#endif

#define QM_INFO 5
struct module_info
{
	unsigned long addr;
	unsigned long size;
	unsigned long flags;
	long usecount;
};

//#define USE_SYSCALL_MODULE_CHECK
#ifdef USE_SYSCALL_MODULE_CHECK
int
isLoaded(char *module)
{
	struct module_info mi;
	int tmp;
	return !syscall(SYS_query_module,module,QM_INFO,&mi,sizeof(mi),&tmp);
}
#else
/*
 * using rmmod technique to search for mdoule, since syscall(SYS_query_module... 
 * doesnt work...
 */
/* If we can check usage without entering kernel, do so. */
int 
isLoaded(const char *module)
{
	FILE *module_list;
	char line[10240], name[64];
	unsigned long size, refs;
	int scanned;
	
	module_list = fopen("/proc/modules", "r");
	if (!module_list) 
	{
		if (errno == ENOENT) /* /proc may not be mounted. */
			return 0;
		DEBUG("can't open /proc/modules: %s\n", strerror(errno));
	}
	while (fgets(line, sizeof(line)-1, module_list) != NULL) 
	{
		if (strchr(line, '\n') == NULL) 
		{
			DEBUG("V. v. long line broke rmmod.\n");
			refs = 0;
			goto out;
		}

		scanned = sscanf(line, "%s %lu %lu", name, &size, &refs);
		(void) scanned;
		DEBUG("%s %s\n",name, module);
		if (strcmp(name, module) == 0)
		{
			refs = 1;
			goto out;
		}
	}
	DEBUG("Module %s does not exist in /proc/modules\n", module);
	refs = 0;
out:
	fclose(module_list);
	return refs;
}
#endif

/* loading module using modprobe */
int
loadModule(char *module)
{
	char *args[] = { MODPROBE_PGM, NULL, "-q", NULL };
	if (isLoaded(module))
	{
		return -1;
	}
	else
	{
		args[1] = module;
		DEBUG("> modprobe %s\n", module);
		return execCommand(args);
	}
}

/* removing module using modprobe */
int
removeModule(char *module)
{
	char *args[] = { MODPROBE_PGM, "-r", NULL, NULL };
	if (isLoaded(module))
	{
		args[2] = module;
		DEBUG("> modprobe -r %s\n", module);
		return execCommand(args);
	}
	else
	{
		return -1;
	}
}

/* checking if module has attached ethernet device */
char *
find_ethernet_devices(char *module)
{
	DIR *dirp;
	struct dirent *direntry;
	static char result[127];
	char dirbuf[127], *res = result;

	memset(dirbuf, 0, sizeof(dirbuf));
	memset(result, 0, sizeof(result));
	sprintf(dirbuf, "/proc/net/%s", module);
	/* check if directory with module name exists */
	if ((dirp = opendir(dirbuf)) != NULL)
	{
		/* we found a matching module dir, lets enum */
		while ((direntry = readdir(dirp)) != NULL)
		{
			if (direntry->d_type == DT_REG)
			{
				memset(dirbuf, 0, sizeof(dirbuf));
				if (machinemode)
					sprintf(dirbuf,"\" \"%s" ,direntry->d_name);
				else
					sprintf(dirbuf," %s[%s]%s" ,modules_color,direntry->d_name,normal_color);
				strcat(result, dirbuf);
			}
		}
		closedir(dirp);
	}
	return res;
}

/* copied getxinfo from hwsetup, by  Klaus Knopper <knopper@knopper.net> */
#define CARDSDB "/usr/share/hwdata/Cards"
#define XPATH "/usr/bin/"
#define XMODPATH "/usr/lib/xorg/modules/drivers/"

int exists(char *filename)
{
	struct stat s;
	return !stat(filename,&s);
}

struct xinfo {
	char xserver[16];
	char xmodule[16];
	char xdesc[128];
	char xopts[128];
};

/* searching in /usr/share/kudzu/Cards for x module name and parameters */
/* if outputxinfo is on, /tmp/xinfo will be created containing gfx card section for X config */
struct xinfo *
getxinfo ( char* devdesc, char* devdriver )
{
	const char *xfree4 = "Xorg", *xvesa4 = "vesa";
	const char *xpath = XPATH;
	static struct xinfo xi;
	int rescanned = 0;
	char *xinfo_opt = malloc(0);
	int xinfo_opt_len = 0;
	memset ( &xi, 0, sizeof ( struct xinfo ) );
	if ( devdesc )
		strncpy ( xi.xdesc, devdesc, sizeof ( xi.xdesc ) );
	if ( devdriver )
	{
		const char * driver[] =
		{ 
			"3DLabs", "Mach64", "Mach32", "Mach8", "AGX",
			"P9000", "S3 ViRGE", "S3V", "S3", "W32",
			"8514", "I128", "SVGA", xfree4, NULL
		};
		const char *server[] =
		{
			driver[ 0 ], driver[ 1 ], driver[ 2 ], driver[ 3 ],
			driver[ 4 ],
			driver[ 5 ], "S3V", driver[ 7 ], driver[ 8 ], driver[ 9 ],
			driver[ 10 ], driver[ 11 ], driver[ 12 ], xfree4, NULL
		};
		if ( !strncasecmp ( devdriver, "Card:", 5 ) ) 	/* RedHat Cards-DB */
		{	/* Kudzu "Cards" format */
			FILE * cardsdb;
			char xfree3server[ 128 ];
			memset ( xfree3server, 0, sizeof ( xfree3server ) );
			if ( ( cardsdb = fopen ( CARDSDB, "r" ) ) != NULL )
			{	/* Try to find Server and Module in /usr/share/kudzu/Cards */
				char buffer[ 1024 ];
				char searchfor[ 128 ];
				int found = 0;
				memset ( searchfor, 0, sizeof ( searchfor ) );
				sscanf ( &devdriver[ 5 ], "%127[^\r\n]", searchfor );
				while ( !found && !feof ( cardsdb ) && fgets ( buffer, 1024, cardsdb ) )
				{
					char sfound[ 128 ];
					memset ( sfound, 0, sizeof ( sfound ) );
					if ( strncasecmp ( buffer, "NAME ", 5 )
						|| ( sscanf( &buffer[ 5 ], "%127[^\r\n]", sfound ) != 1 )
						|| strcasecmp ( sfound, searchfor ) )
						continue;
					while ( !feof ( cardsdb ) && fgets ( buffer, 1024, cardsdb ) )
					{
						if ( buffer[ 0 ] < 32 )
							break;	/* End-of-line */
						if ( !strncasecmp( buffer, "SERVER ", 7 ) )
						{
							char x[ 20 ] = "";
							if ( sscanf( &buffer[ 7 ], "%19s", x ) == 1 )
							{
								char xserver[ 32 ];
								char fullpath[ 128 ];
								char *xf[ 2 ] =  { "", "XF86_" };
								int i;
								for ( i = 0; i < 2; i++ )
								{
									sprintf ( xserver, "%s%.24s", xf[ i ], x );
									sprintf ( fullpath, "%.90s%.32s", xpath, xserver );
									if ( exists ( fullpath ) )
									{
										strncpy ( xfree3server, xserver, sizeof ( xfree3server ) );
										break;	/* for */
									}
								}
							}
						}
						else if ( !strncasecmp( buffer, "DRIVER ", 7 ) )
						{
							char xmodule[ 32 ];
							char fullpath[ 128 ];
							sscanf ( &buffer[ 7 ], "%31s", xmodule );
							sprintf ( fullpath, XMODPATH "%.31s_drv.so", xmodule );
							if ( exists ( fullpath ) )
							{
								strncpy ( xi.xmodule, xmodule, sizeof( xi.xmodule ) );
							}
						}
						else if ( !strncasecmp( buffer, "SEE ", 4 ) && rescanned < 10 )
						{	/* rescan Cards-DB for other server */
							fseek ( cardsdb, 0L, SEEK_SET );
							++rescanned;
							memset ( searchfor, 0, sizeof( searchfor ) );
							sscanf ( &buffer[ 4 ], "%127[^\r\n]", searchfor );
							break;	/* Continue with outer while() */
						}
						else if ( !strncasecmp( buffer, "LINE ", 5 ) && outputxinfo )
						{
							char xinfo_opt_line[128];
							sscanf ( &buffer[ 5 ], "%127[^\r\n]", xinfo_opt_line );
							xinfo_opt_len += strlen(xinfo_opt_line) + 1;
							xinfo_opt = (char*)realloc(xinfo_opt, xinfo_opt_len );
							strcat(xinfo_opt, xinfo_opt_line);
							strcat(xinfo_opt,"\n");
						}
						
					}
				}
				fclose ( cardsdb );
			}
			if ( *xi.xmodule || *xi.xserver || *xfree3server ) 	/* (Partial) Success */
			{
				if ( !*xi.xserver )
				{
					if ( *xfree3server && !*xi.xmodule )
						strncpy ( xi.xserver, xfree3server, sizeof ( xi.xserver ) );
					else
						strncpy ( xi.xserver, xfree4, sizeof ( xi.xserver ) );
				}
				if ( !*xi.xmodule )
					strcpy ( xi.xmodule, xvesa4 );
				goto bailout;
			}
		}
		/* Card not found in Cards database -> Try to guess from description */
		{
			int i;
			for ( i = 0; driver[ i ] != NULL; i++ )
			{
				if ( strstr ( devdriver, driver[ i ] ) )
				{
					char * xpos;
					if ( ( xpos = strstr ( devdriver, xfree4 ) ) != NULL ) 	/* Check for XFree 4 */
					{
						char xm[ 32 ] = "";
						strcpy ( xi.xserver, xfree4 );
						if ( sscanf ( xpos, "XFree86(%30[^)])", xm ) == 1 )
							strcpy ( xi.xmodule, xm );
						else
							strcpy ( xi.xmodule, xvesa4 );
					} else
					{
						char xserver[ 32 ];
						char fullpath[ 128 ];
						char *xf[ 2 ] = { "", "XF86_" };
						int j;
						for ( j = 0; j < 2; j++ )
						{
							sprintf ( xserver, "%s%.24s", xf[ j ], server[ i ] );
							sprintf ( fullpath, "%.90s%.32s", xpath, xserver );
							if ( exists ( fullpath ) )
							{
								strncpy ( xi.xserver, xserver, sizeof( xi.xserver ) );
								break;	/* for */
							}
						}
					}
				}
			}
		}
	}
	/* TODO: include xopts as Option in X config instead of startx parameters */
	/* Special options required? */
	if ( devdesc )
	{
		strncpy ( xi.xdesc, devdesc, sizeof ( xi.xdesc ) - 1 );
		/* Handle special cards that require special options */
		if ( strstr ( devdesc, "Trident" ) || strstr ( devdesc, "TGUI" )
		        || strstr ( devdesc, "Cirrus" ) || strstr ( devdesc, "clgd" ) )
		{
			if ( !strcmp ( xi.xserver, xfree4 ) )
				strncpy ( xi.xopts, "-depth 16", sizeof ( xi.xopts ) - 1 );
			else
				strncpy ( xi.xopts, "-bpp 16", sizeof ( xi.xopts ) - 1 );
		}
		else if ( strstr ( devdesc, "Savage 4" ) ) 	/* S3 Inc.|Savage 4 */
		{
			if ( !strcmp ( xi.xserver, xfree4 ) )
				strncpy ( xi.xopts, "-depth 32", sizeof ( xi.xopts ) - 1 );
			else
				strncpy ( xi.xopts, "-bpp 32", sizeof ( xi.xopts ) - 1 );
		}
	}
	/* Fallback values */
	if ( !*xi.xserver )
		strcpy ( xi.xserver, xfree4 );
	if ( !*xi.xmodule )
		strcpy ( xi.xmodule, xvesa4 );

bailout:
	if (outputxinfo)
	{
		/* dump gfx X config section to /tmp/xinfo */
		FILE *f;
		if ((f=fopen("/tmp/xinfo", "w")) != NULL)
		{
			*xinfo_opt='\0';	/* marking end of string */
			fprintf(f,"Section \"Device\"\n");
			fprintf(f,"\tIdentifier  \"Card0\"\n");
			fprintf(f,"\tDriver      \"%s\"\n",xi.xmodule);
			fprintf(f,"\tVendorName  \"All\"\n");
			fprintf(f,"\tBoardName   \"All\"\n");
			fprintf(f,"%s", xinfo_opt);
			fprintf(f,"EndSection\n");
			fclose(f);
			free(xinfo_opt);
		}
	}
	
	return &xi;
}

int
check_module(char *module)
{
	struct pci_dev *d;
	char descbuf[128], modulebuf[128];
	
	for(d=first_dev; d; d=d->next)
	{
		lookup_module(d->vendor_id, d->device_id,
				modulebuf, sizeof(modulebuf),
				descbuf, sizeof(descbuf));
		if (!strcmp(modulebuf, module))
		{
			return !isLoaded(modulebuf);
		}
	}
	return 0;
}

/* listing pci devices using pci busses and devices initialized using pcilib function */
void
list_pci(void)
{
	unsigned int c;
	struct pci_access *pacc;
	struct pci_dev *dev;
	char lastclassbuf[128]={0};
	char classbuf[128], descbuf[128], modulebuf[128];
	char idstring[20]={0};

	init_lookup_block("pcitable");
//pci_begin:	
	pacc = pci_alloc();					/* Get the pci_access structure 	*/
	/* Set all options you want -- here we stick with the defaults */
	pci_init(pacc);						/* Initialize the PCI library 		*/
	pci_scan_bus(pacc);					/* We want to get the list of devices 	*/
	first_dev = pacc->devices;
	
	/* 
	 * checking for CardBus devices, which requires loading yenta_socket module, and rescanning 
	 * pci bus in autoload moduled mode	
	 *
	 */
	if (autoload) 
	{
		if (check_module("yenta_socket"))		/* check for yenta_socket module	*/
		{
			//printf("Probing FireWire devices...\r");
			//fflush(stdout);					
			loadModule("yenta_socket");
			//system("modprobe yenta_socket > /dev/null 2>&1");
			//while (!isLoaded("yenta_socket"));
			sleep(1);
			pci_cleanup(pacc);			/* Close everything 			*/
			pacc = NULL;
			//goto pci_begin;
			pacc = pci_alloc();			/* Get the pci_access structure 	*/
			pci_init(pacc);				/* Initialize the PCI library 		*/
			pci_scan_bus(pacc);			/* We want to get the list of devices 	*/
			first_dev = pacc->devices;
			//printf("\r");
			//fflush(stdout);
		}
	}
	sort_pci_list();					/* sorting according to display format	*/
	for(dev=first_dev; dev; dev=dev->next)			/* Iterate over all devices 		*/
	{
		/* look for pci information : name, module, description  */
		c = pci_read_word(dev, PCI_CLASS_DEVICE);	/* Read config register directly 	*/
		char *pclassbuf =
			pci_lookup_name(pacc, classbuf, sizeof(classbuf),
					PCI_LOOKUP_CLASS,
					c, 0, 0, 0);
		int foundmodule =
			lookup_module(dev->vendor_id, dev->device_id,
					modulebuf, sizeof(modulebuf),
					descbuf, sizeof(descbuf));
		/* check if its a gfx card to get X module and info for */
		 if ((c >> 8) == PCI_BASE_CLASS_DISPLAY)
		{
			/* getxinfo will retrieve x info and create /tmp/xinfo if needed */
			struct xinfo *x = getxinfo(descbuf, modulebuf);
			strcpy(modulebuf, x->xmodule);
			DEBUG("%s\n%s\n%s\n%s\n", x->xserver, x->xmodule, x->xdesc, x->xopts);
		}
		
		if (foundmodule && autoload)
		{
			loadModule(modulebuf);
		}
		
		if (disdefdesc)
		{
			pci_lookup_name(pacc, descbuf, sizeof(descbuf),
					PCI_LOOKUP_VENDOR | PCI_LOOKUP_DEVICE,
					dev->vendor_id, dev->device_id, 0, 0);
		}

		/* if this is a ethernet controller, lets find the device(s) its attached to */
		if (((c >> 8) == PCI_BASE_CLASS_NETWORK) && foundmodule)
		{
			char *ethdev=find_ethernet_devices(modulebuf);
			if (*ethdev)
				strcat(machinemode ? modulebuf : descbuf, ethdev);
		} 

		/* now lets display results... */
		if (machinemode)
		{
			//sprintf(idstring, "\"%04x:%04x\" ",dev->vendor_id, dev->device_id);
			printf("\"%s\" \"%s\" \"%s\"\n",
				pclassbuf, descbuf, modulebuf );
		}
		else if (showlist)
		{
/*			printf("%02x:%02x.%d Class %04x: %04x:%04x %s (%s)\n",
				dev->bus, dev->dev, dev->func, c,
				dev->vendor_id, dev->device_id, descbuf, modulebuf);*/
			sprintf(idstring, "%04x:%04x ",dev->vendor_id, dev->device_id);
			printf("%02x:%02x.%d %s%s: %s (%s)\n",
				dev->bus, dev->dev, dev->func,
				showids ? idstring : "", pclassbuf, descbuf, modulebuf);
		}
		else
		{
			if (strcmp(lastclassbuf, pclassbuf)) printf("%s%s%s\n",title_color, pclassbuf, normal_color);
			strcpy(lastclassbuf,pclassbuf);
			printf("  %s%-16s:%s %s\n", modules_color,modulebuf, normal_color, descbuf);
		}
	}
	pci_cleanup(pacc);					/* Close everything 			*/
	cleanup_lookup_block();					/* free modules memory... 		*/
}

/* search for usb class name using pci.ids file.  */
/* the search uses allocated memory by names_init which must to be called prior */
static int 
get_usb_class_string(char *buf, size_t size, u_int8_t class)
{
	const char *cp;
	
	if (size < 1)
		return 0;
	*buf = 0;
	if (!(cp = names_class(class)))
		return 0;
	return snprintf(buf, size, "%s", cp);
}

/* listing usb devices using busses and devices initialized using usblib functions */
/* thanks to the sorting procedure all devices are organized in 1 list */
void
list_usb(void)
{
	int ret; 
	usb_dev_handle *udev; 
	struct usb_device *dev; 
	struct usb_interface_descriptor desc;
	char lastclassbuf[128]={0};
	char string[128],classbuf[128], descbuf[128], modulebuf[128];
	char idstring[20]={0};

	/* TODO: the approp modules are loaded anyway if usb controllers were found, so 
	 *       there is no need to load those modules again. instead, need to find a
	 *       better way to make sure usb modules are loaded to continue autoload.
	 */
	 
	if (autoload) 
	{
		//if ((!isLoaded("uhci_hcd")) && (!isLoaded("ohci_hcd")) && (!isLoaded("ehci_hcd")))
		{
			loadModule("ohci_hcd");
			loadModule("ehci_hcd");
			loadModule("uhci_hcd");
		}
	}

        /* 1st we load usb.ids for usb naming */
	if ((ret = names_init("./usb.ids")) != 0)
	{
		if ((ret = names_init(USBIDS_FILE)))
		{
			DEBUG("Error, cannot open USBIDS File \"%s\", %s\n", USBIDS_FILE, strerror(ret));
			return;
		}
	}
	/* load usbtable file for module name and extra description */
 	init_lookup_block("usbtable");

	/* executing mount command just to make sure we have /proc/bus/usb mounted.
	 * if its not mounted, devices cant be browsed...
	 * and if usb module wasnt loaded on system-startup, mount doesnt exists yet -
	 * at least not on my computer...
	 */
	char *mountargs[] = { "/bin/mount", "-t", "usbfs", "none", "/proc/bus/usb", NULL };
	execCommand(mountargs);
	
	/* now we init and load usblib functions */	
	usb_init(); 
	usb_find_busses(); 
	if (!usb_find_devices()) 
	{
		DEBUG("No USB devices. maybe no usb module is loaded, or /proc/bus/usb is not mounted\n");
		goto out_list_usb;
	}
	first_usb_dev = usb_busses->devices;
	sort_usb_list();

	/* lets enum busses and devices ... */
	for(dev=first_usb_dev; dev; dev=dev->next)			/* Iterate over all devices 		*/
	{
		udev = usb_open(dev); 
		if (!udev) continue;
		string[0] = classbuf[0] = modulebuf[0] = descbuf[0] = 0;
		desc = dev->config[0].interface[0].altsetting[0];
		get_usb_class_string(classbuf, sizeof(classbuf), desc.bInterfaceClass);
		lookup_module(dev->descriptor.idVendor, dev->descriptor.idProduct,
					modulebuf, sizeof(modulebuf),
					descbuf, sizeof(descbuf));
		/* if device already has a module attached (ie, loaded), show it instead */
		if (usb_get_driver_np(udev, desc.bInterfaceNumber, string, sizeof(string)) == 0)
		{
			strcpy(modulebuf, string);
			/* no need to check for autoload, since module is already loaded */
		}
		else 
		{
			if (autoload)
			{
				loadModule(modulebuf);
			}
		}
		
		if (disdefdesc || !*descbuf)
			usb_get_string_simple(udev, dev->descriptor.iProduct, descbuf, sizeof(descbuf)); 
		
		usb_close (udev); 
		
		 /* check if mouse device, and if so add [input/mice] */
		if ((desc.bInterfaceClass == USB_CLASS_HID) && (desc.bInterfaceProtocol == 2))
		{
			usbmousefound = 1;
			sprintf(modulebuf, "usbhid");
			if (autoload)
			{
				loadModule(modulebuf);
			}
			if (machinemode)
			{
				strcat(modulebuf, "\" \"/dev/input/mice");
			}
			else
			{
				char mouse_string[128]={0};
				sprintf(mouse_string, " %s[/dev/input/mice]%s",modules_color, normal_color);
				strcat(descbuf, mouse_string);
			}
		}
		/* check if this is a communication device, and attach device name if so */
		else if (desc.bInterfaceClass == USB_CLASS_COMM)
		{
			char *ethdev=find_ethernet_devices(modulebuf);
			if (*ethdev)
				strcat(machinemode ? modulebuf : descbuf, ethdev);
		}
				
		if (machinemode)
		{
			printf("\"USB %s\" \"%s\" \"%s\"\n",
				classbuf, descbuf, modulebuf );
		}
		else if (showlist)
		{
			sprintf(idstring, "%04x:%04x ",dev->descriptor.idVendor, dev->descriptor.idProduct);
			printf("%s:%s %sUSB %s: %s (%s)\n",
				dev->bus->dirname, dev->filename,
				showids ? idstring : "", classbuf, descbuf, modulebuf);
		}
		else
		{
			if (strcmp(lastclassbuf, classbuf)) printf("%sUSB %s%s\n",title_color, classbuf, normal_color);
			strcpy(lastclassbuf,classbuf);
			printf("  %s%-16s:%s %s\n", modules_color, modulebuf, normal_color, descbuf);
		}
	}
out_list_usb:
	cleanup_lookup_block();					/* free modules memory... 		*/
}

/* testing mouse to see if one exists and list it. only 1 mouse is actually needed, so if usb mouse was found 
   it will report only as ps/2 port */
void
list_mouse(void)
{
	char devicebuf[128], descbuf[128], modulebuf[128];
	char idstring[20]={0};
	
	if (psauxProbe( devicebuf, modulebuf, descbuf))
	{
		if (usbmousefound)
		{
			sprintf(descbuf, "PS/2 Mouse port");
		}
		
		if (machinemode)
		{
			printf("\"Mouse\" \"%s\" \"%s\" \"%s\"\n", 
				descbuf, modulebuf, devicebuf);
		}
		else if (showlist)
		{
			sprintf(idstring, "----:---- ");
			printf("---:--- %sMouse: %s [%s] (%s)\n", 
				showids ? idstring : "", descbuf, devicebuf, modulebuf);
		}
		else
		{
			printf("%sMouse%s\n",title_color, normal_color);
			printf("  %s%-16s:%s %s %s[%s]%s\n", 
				modules_color,modulebuf, normal_color, descbuf, modules_color, devicebuf, normal_color);
		}
	}
}

#define MAX_SOCKS 8

/* listing pcmcia devices according to /proc/devices */
void
list_pcmcia(void)
{
	if (!init_pcmcia()) return;
	
	int ns;
	ds_ioctl_arg_t arg;
	/* setting pointers to point to the approp variables */
	cistpl_vers_1_t *vers = &arg.tuple_parse.parse.version_1;
	cistpl_manfid_t *manfid = &arg.tuple_parse.parse.manfid;
	cistpl_funcid_t *funcid = &arg.tuple_parse.parse.funcid;
	config_info_t config;
	int fd[MAX_SOCKS];
	char lastclassbuf[128]={0};
	char string[128],classbuf[128], descbuf[128], modulebuf[128];
	char idstring[20]={0};
	static char *pcmcia_fn[] = 
	{
		"Multifunction", "Memory", "Serial", "Parallel",
		"Fixed disk", "Video", "Network", "AIMS", "SCSI"
	};
	
 	init_lookup_block("pcmciatable");

	for (ns = 0; ns < MAX_SOCKS; ns++) 
	{
		fd[ns] = open_sock(ns);
		if (fd[ns] < 0) break;

		vers->ns = 0;
		get_tuple(fd[ns], CISTPL_VERS_1, &arg);
		
		/* check if socket is empty */
		if (vers->ns <= 0) continue;

		string[0] = classbuf[0] = modulebuf[0] = descbuf[0] = 0;
		(void) string;
	
		*manfid = (cistpl_manfid_t) { 0, 0 };
		get_tuple(fd[ns], CISTPL_MANFID, &arg);
		
		int foundmodule =
			lookup_module(manfid->manf, manfid->card,
					modulebuf, sizeof(modulebuf),
					descbuf, sizeof(descbuf));

		if (disdefdesc || !*descbuf)
		{
			sprintf(descbuf,"%s|%s", 
				vers->str+vers->ofs[0],
				vers->str+vers->ofs[1]);
		}
		
		*funcid = (cistpl_funcid_t) { 0xff, 0xff };
		get_tuple(fd[ns], CISTPL_FUNCID, &arg);

		config.Function = config.ConfigBase = 0;
	
		/* if this is a ethernet controller, lets find the device(s) its attached to */
		if (funcid->func == 6 && foundmodule)
		{
			char *ethdev=find_ethernet_devices(modulebuf);
			if (*ethdev)
				strcat(machinemode ? modulebuf : descbuf, ethdev);
		}

		if (machinemode)
		{
			printf("\"PCMCIA %s card\" \"%s\" \"%s\"\n",
				pcmcia_fn[funcid->func], descbuf, modulebuf );
		}
		else if (showlist)
		{
			sprintf(idstring, "%04x:%04x ",manfid->manf, manfid->card);
			printf("--:%02x.%x %sPCMCIA %s card: %s (%s)\n",
				ns, funcid->func,
				showids ? idstring : "", pcmcia_fn[funcid->func], descbuf, modulebuf);
		}
		else
		{
			if (strcmp(lastclassbuf, pcmcia_fn[funcid->func])) 
				printf("%sPCMCIA %s card%s\n",title_color, pcmcia_fn[funcid->func], normal_color);
			strcpy(lastclassbuf,pcmcia_fn[funcid->func]);
			printf("  %s%-16s:%s %s\n", modules_color, modulebuf, normal_color,descbuf);
		}
		
		
	}
	cleanup_lookup_block();					/* free modules memory... 		*/
}

/* listing firewire deivces according to /proc/bus/ieee1394/devices */
/* lame as hell, but keep the users happy... */
void
list_firewire(void)
{
	//int loaded_driver = 0;
	unsigned long specid, version;
	char lastclassbuf[128]={0};
	char node[16],classbuf[128], descbuf[128], modulebuf[128];
	char idstring[20]={0};
	
	char *next, *buf = NULL, *tmp;
	char tmpbuf[4096];
	int fd, bytes =0;
		
/* Do NOT attempt to load firewire module (may crash), rather check for existing one. -KK */
#if 0
	if (!loadModule("ohci1394"))
		loaded_driver = 1;
#endif
	fd = open("/proc/bus/ieee1394/devices", O_RDONLY);
	if (fd < 0)
		goto out;
	memset(tmpbuf,'\0',4096);
	while (read(fd,tmpbuf,4096) > 0) 
	{
		buf = realloc(buf,bytes+4096);
		memcpy(buf+bytes,tmpbuf,4096);
		bytes += 4096;
		memset(tmpbuf,'\0',4096);
	}
	close(fd);
	
	if (!buf) goto out;
	
	while (buf && *buf) 
	{
		specid = version = 0;
		node[0] = classbuf[0] = modulebuf[0] = descbuf[0] = 0;
		next = strstr(buf+1,"Node[");
		if (next) 
		{
			*(next-1) = '\0';
			char *end;
			end = strstr(next,"]");
			if (end) *end = '\0';
			strcpy(node, next);
		}
/*			
		if (tmp = strstr(buf,"Vendor ID: ")) 
		{
			char *end;
			end = strstr(tmp,"\'");
			if (end) *end = '\0';
			strcpy(classbuf, tmp+12);
		}
*/			
		if ((tmp=strstr(buf,"Software Specifier ID: "))) {
			int vendor_id = 0, product_id = 0;
			specid = strtoul(tmp+23,NULL,16);
			tmp = strstr(buf,"Software Version: ");
			if (tmp)
				version = strtoul(tmp+18,NULL,16);
			if (version == 0x010483 && specid == 0x00609e) 
			{
				strcpy(modulebuf, "sbp2");
				tmp = strstr(buf,"Vendor ID:");
				if (tmp) 
				{
					char *end;
					end = strstr(tmp,"\n");
					int pos = end - tmp - 9;
					char vendor_id_str[10];
					strncpy(vendor_id_str, tmp+pos, 8);
					vendor_id = strtoul(vendor_id_str,NULL,16);
				}
				
				tmp = strstr(buf,"Vendor/Model ID:");
				if (tmp) 
				{
					char *end;
					end = strstr(tmp,"\n");
					if (end) *end = '\0';
					strcpy(descbuf, tmp+17);
					
					int pos = end - tmp - 7;
					char product_id_str[10];
					strncpy(product_id_str, tmp+pos, 6);
					product_id = strtoul(product_id_str,NULL,16);
				} 
				else 
				{
					strcpy(descbuf, "Generic Firewire Storage Controller");
				}
				
				if (machinemode)
				{
					printf("\"FireWire device\" \"%s\" \"%s\"\n",
						descbuf, modulebuf );
				}
				else if (showlist)
				{
					sprintf(idstring, "%04x:%04x ",vendor_id, product_id);
					printf("%s %sFireWire device: %s (%s)\n", 
						node, showids ? idstring : "", descbuf, modulebuf);
				}
				else
				{
					if (strcmp(lastclassbuf, "FireWire")) 
						printf("%sFireWire device(s)%s\n",title_color, normal_color);
					strcpy(lastclassbuf,"FireWire");
					printf("  %s%-16s:%s %s\n", modules_color, modulebuf, normal_color,descbuf);
				}
			}
		}
		buf = next;
	}
	
out:
	free(buf);
/* Do NOT attempt to unload firewire module (may crash), rather check for existing one. -KK */
#if 0
	if (loaded_driver == 1)
		removeModule("ohci1394");
#endif
}

int
main(int argc, char **argv)
{
	//struct device *devlst;
	//firewireProbe(CLASS_SCSI, 0, devlst);
	//return 0;
	
	unsigned int c;

	/* default settings */
	autoload = disdefdesc = removeduplicates = machinemode = outputxinfo = usbmousefound = showids = 0;
	showlist = plainmode  =1;
	
	/* checking for command line parameters*/
	for(c=1;c<argc;c++)
	{
		if(!strcasecmp(argv[c],"-a"))      autoload = 1;
		else if(!strcasecmp(argv[c],"-d")) disdefdesc = 1;
		else if(!strcasecmp(argv[c],"-c")) showlist = 0;
		else if(!strcasecmp(argv[c],"-m")) machinemode = 1;
		else if(!strcasecmp(argv[c],"-n")) removeduplicates = 1;
		else if(!strcasecmp(argv[c],"-ox")) outputxinfo = 1;
		else if(!strcasecmp(argv[c],"-id")) showids = 1;
		else if(!strcasecmp(argv[c],"-cc")) plainmode = showlist = 0;
		else return usage(argv[c]);
	}

	if (plainmode || showlist || machinemode)
		normal_color = modules_color = title_color = "";
	else
	{
		printf("\e[H\e[J");				/* print screen with ansi chars */
		//printf("%sListing hardware devices and modules...%s",modules_color,normal_color);
	}
/*
	if ((showlist == 0) && (machinemode == 0))
	{
		printf("%s%s%s%-13s%s: DESCRIPTION\n",title_color,"TYPE/",modules_color,"MODULE",normal_color);
	}
*/
	list_pci();
//	if (!showlist) putchar('\n');
	list_usb();
//	if (!showlist) putchar('\n');
	list_pcmcia();
//	if (!showlist) putchar('\n');
	list_firewire();
//	if (!showlist) putchar('\n');
	list_mouse();
	if (!showlist) putchar('\n');
	
	return 0;
}
