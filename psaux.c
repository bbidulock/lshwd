/*
 * follwing unit was stolen from kudzu library, (c) RedHat inc.
 * some parts were removed and others changed to suit lshwd requirements.
 */

/* Copyright 1999-2003 Red Hat, Inc.
 *
 * This software may be freely redistributed under the terms of the GNU
 * public license.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */
/*
 mice = 
 {
 //(description)                                        (gpm protocol, X protocol, device, emulate3, shortname)
 'ALPS - GlidePoint (PS/2)':                            ('ps/2', 'GlidePointPS/2', 'psaux', 1, 'alpsps/2'),
 'ASCII - MieMouse (PS/2)':                             ('ps/2', 'NetMousePS/2', 'psaux', 1, 'asciips/2'),
 'ASCII - MieMouse (serial)':                           ('ms3', 'IntelliMouse', 'ttyS', 0, 'ascii'),
 'ATI - Bus Mouse':                                     ('Busmouse', 'BusMouse', 'atibm', 1, 'atibm'),
 'Generic - 2 Button Mouse (PS/2)':                     ('ps/2', 'PS/2', 'psaux', 1, 'genericps/2'),
 'Generic - 2 Button Mouse (serial)':                   ('Microsoft', 'Microsoft', 'ttyS', 1, 'generic'),
 'Generic - 2 Button Mouse (USB)':                      ('imps2', 'IMPS/2', 'input/mice', 1, 'genericusb'),
 'Generic - 3 Button Mouse (PS/2)':                     ('ps/2', 'PS/2', 'psaux', 0, 'generic3ps/2'),
 'Generic - 3 Button Mouse (serial)':                   ('Microsoft', 'Microsoft', 'ttyS', 0, 'generic3'),
 'Generic - 3 Button Mouse (USB)':                      ('imps2', 'IMPS/2', 'input/mice', 0, 'generic3usb'),
 'Generic - Wheel Mouse (PS/2)':                        ('imps2', 'IMPS/2', 'psaux', 0, 'genericwheelps/2'),
 'Generic - Wheel Mouse (USB)':                         ('imps2', 'IMPS/2', 'input/mice', 0, 'genericwheelusb'),
 'Genius - NetMouse Pro (PS/2)':                        ('netmouse', 'NetMousePS/2', 'psaux', 1, 'geniusprops/2'),
 'Genius - NetMouse (PS/2)':                            ('netmouse', 'NetMousePS/2', 'psaux', 1, 'geniusnmps/2'),
 'Genius - NetMouse (serial)':                          ('ms3', 'IntelliMouse', 'ttyS', 1, 'geniusnm'),
 'Genius - NetScroll+ (PS/2)':                          ('netmouse', 'NetMousePS/2', 'psaux', 0, 'geniusscrollps/2+'),
 'Genius - NetScroll (PS/2)':                           ('netmouse', 'NetScrollPS/2', 'psaux', 1, 'geniusscrollps/2'),
 'Kensington - Thinking Mouse (PS/2)':                  ('ps/2', 'ThinkingMousePS/2', 'psaux', 1, 'thinkingps/2'),
 'Kensington - Thinking Mouse (serial)':                ('Microsoft', 'ThinkingMouse', 'ttyS', 1, 'thinking'),
 'Logitech - Bus Mouse':                                ('Busmouse', 'BusMouse', 'logibm', 0, 'logibm'),
 'Logitech - C7 Mouse (serial, old C7 type)':           ('Logitech', 'Logitech', 'ttyS', 0, 'logitech'),
 'Logitech - CC Series (serial)':                       ('logim', 'MouseMan', 'ttyS', 0, 'logitechcc'),
 'Logitech - MouseMan+/FirstMouse+ (PS/2)':             ('ps/2', 'MouseManPlusPS/2', 'psaux', 0, 'logimman+ps/2'),
 'Logitech - MouseMan/FirstMouse (PS/2)':               ('ps/2', 'PS/2', 'psaux', 0, 'logimmanps/2'),
 'Logitech - MouseMan/FirstMouse (serial)':             ('MouseMan', 'MouseMan', 'ttyS', 0, 'logimman'),
 'Logitech - MouseMan+/FirstMouse+ (serial)':           ('pnp', 'IntelliMouse', 'ttyS', 0, 'logimman+'),
 'Logitech - MouseMan Wheel (USB)':                     ('ps/2', 'IMPS/2', 'input/mice', 0, 'logimmusb'),
 'Microsoft - Bus Mouse':                               ('Busmouse', 'BusMouse', 'inportbm', 1, 'msbm'),
 'Microsoft - Compatible Mouse (serial)':               ('Microsoft', 'Microsoft', 'ttyS', 1, 'microsoft'),
 'Microsoft - IntelliMouse (PS/2)':                     ('imps2', 'IMPS/2', 'psaux', 0, 'msintellips/2'),
 'Microsoft - IntelliMouse (serial)':                   ('ms3', 'IntelliMouse', 'ttyS', 0, 'msintelli'),
 'Microsoft - IntelliMouse (USB)':                      ('ps/2', 'IMPS/2', 'input/mice', 0, 'msintelliusb'),
 'Microsoft - Rev 2.1A or higher (serial)':             ('pnp', 'Auto', 'ttyS', 1, 'msnew')
 'MM - HitTablet (serial)':                             ('MMHitTab', 'MMHittab', 'ttyS', 1, 'mmhittab'),
 'MM - Series (serial)':                                ('MMSeries', 'MMSeries', 'ttyS', 1, 'mmseries'),
 'Mouse Systems - Mouse (serial)':                      ('MouseSystems', 'MouseSystems', 'ttyS', 1, 'mousesystems'),
 'No - mouse':                                          ('none', 'none', None, 0, 'none'),
 'Sun - Mouse':                                         ('sun', 'sun', 'sunmouse', 0, 'sun'),
 }

*/
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/select.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

#include "psaux.h"

#define MOUSE_CMD_RESET                  0xFF
#define MOUSE_CMD_RESEND                 0xFE
#define MOUSE_CMD_DEFAULTS               0xF6
#define MOUSE_CMD_DISABLE                0xF5
#define MOUSE_CMD_ENABLE                 0xF4
#define MOUSE_CMD_SET_SAMPLE_RATE        0xF3
#define MOUSE_CMD_GET_DEVICE_ID          0xF2
#define MOUSE_CMD_SET_REMOTE_MODE        0xF0
#define MOUSE_CMD_SET_WRAP_MODE          0xEE
#define MOUSE_CMD_RESET_WRAP_MODE        0xEC
#define MOUSE_CMD_READ_DATA              0xEB
#define MOUSE_CMD_SET_STREAM_MODE        0xEA
#define MOUSE_CMD_STATUS_REQUEST         0xE9
#define MOUSE_CMD_SET_RESOLUTION         0xE8
#define MOUSE_CMD_SET_SCALING_2          0xE7
#define MOUSE_CMD_SET_SCALING_1          0xE6

#define MOUSE_RESP_RESENT 0xFE
#define MOUSE_RESP_ERROR  0xFC
#define MOUSE_RESP_ACK    0xFA
#define MOUSE_RESP_TESTOK 0xAA

#ifdef DEBUG
#undef DEBUG
#define DEBUG(s...) fprintf(stderr, s)
#else
#define DEBUG(s...) ;
#endif

/*****************************************************************************
 *
 * TESTING_MOUSE part should be removed later on - only here for playing 
 * arround...
 *
 ****************************************************************************/
 
#ifdef TESTING_MOUSE

#define QP_DATA         0x310           /* Data Port I/O Address */
#define QP_STATUS       0x311           /* Status Port I/O Address */

#include <asm/io.h>

static int qp_data = QP_DATA;
static int qp_status = QP_STATUS;
/*
 * Function to read register in 82C710.
 */

inline unsigned char read_710(unsigned char index)
{
        outb(index, 0x390);                   /* Write index */
        return inb(0x391);                    /* Read the data */
}

/*
 * See if we can find a 82C710 device. Read mouse address.
 * lame as hell, but hey, thats testings...
 */

int probe_qp(void)
{
	/* Get access to the ports */
	if (ioperm(0x2fa, 3, 1)) {perror("ioperm"); return 0;}
	if (ioperm(0x3fa, 3, 1)) {perror("ioperm"); return 0;}
	if (ioperm(0x390, 3, 1)) {perror("ioperm"); return 0;}
	if (ioperm(0x391, 3, 1)) {perror("ioperm"); return 0;}
	outb(0x55, 0x2fa);                    /* Any value except 9, ff or 36*/
	outb(0xaa, 0x3fa);                    /* Inverse of 55 */
	outb(0x36, 0x3fa);                    /* Address the chip */
	outb(0xe4, 0x3fa);                    /* 390/4; 390 = config address*/
	outb(0x1b, 0x2fa);                    /* Inverse of e4 */
	if (read_710(0x0f) != 0xe4)             /* Config address found? */
		return 0;                             /* No: no 82C710 here */
	qp_data = read_710(0x0d)*4;             /* Get mouse I/O address */
	qp_status = qp_data+1;
	outb(0x0f, 0x390);
	outb(0x0f, 0x391);                    /* Close config mode */
	/* We don't need the ports anymore */
	if (ioperm(0x2fa, 3, 0)) {perror("ioperm"); return 0;}
	if (ioperm(0x3fa, 3, 0)) {perror("ioperm"); return 0;}
	if (ioperm(0x390, 3, 0)) {perror("ioperm"); return 0;}
	if (ioperm(0x391, 3, 0)) {perror("ioperm"); return 0;}
        return 1;
}

#endif /*TESTING_MOUSE*/

/*****************************************************************************
 *
 * end of TESTING_MOUSE part
 *
 ****************************************************************************/


static int
mouse_read(int fd)
{
	struct timeval tv;
	unsigned char ch;
	fd_set fds;
	int ret;

	FD_ZERO(&fds);
	FD_SET(fd, &fds);
	tv.tv_sec = 0;
	tv.tv_usec = 10000;//600000;
	ret = select(fd+1, &fds, NULL, NULL, &tv);
	if (-1 == ret)
	{
		DEBUG("select error: %s\n",strerror(errno));
		return -1;
	}

	if (0 == ret)
	{
		DEBUG("Timeout waiting for response\n");
		return -1;
	}

	ret = read(fd, &ch, 1);
	if (1 == ret)
	{
		DEBUG("mouse_read: %04x\n",ch);
		return ch;
	}

	DEBUG("error reading mouse data: %s\n",strerror(errno));
	return -1;
}

static int
mouse_cmd(int fd, unsigned char cmd)
{
	int ret;

	DEBUG("mouse_cmd: %02x\n",cmd);
	ret = write(fd, &cmd, 1);
	if (ret != 1)
	{
		return -1;
	}

	ret = mouse_read(fd);
	if (ret == MOUSE_RESP_ACK)
		return 0;
	else
		return -1;
}

const char *mouse_devices[] = {	"/dev/psaux", 
				"/dev/input/mice",
				"/dev/input/mouse",
				"/dev/input/mouse0",
				"/dev/mouse",
				};
int
psauxProbe(char* devclass, char *driver, char *desc)
{
/*
#ifdef TESTING_MOUSE
	int d = 0;
	d = probe_qp();
	printf("%d\n",d);
	return 0;
#endif
*/	
	int portfd, ret;
	
	for (ret=0; ret < sizeof(mouse_devices)/sizeof(mouse_devices[0]); ret++)
	{
		strcpy(devclass, mouse_devices[ret]);
		if ((portfd=open(devclass, O_RDWR | O_NONBLOCK)) > 0)
		{
			ret = 0;
			break;
		}
	}
	if (ret)
	{
		DEBUG("error opening mouse port.\n");
		return 0;
	}

	mouse_cmd(portfd, MOUSE_CMD_RESET);

	if ((ret = mouse_read(portfd)) != MOUSE_RESP_TESTOK)
		DEBUG("Mouse self-test failed.\n");

	if ((ret = mouse_read(portfd)) != 0x00)
		DEBUG("Mouse did not finish reset response.\n");

	mouse_cmd(portfd, MOUSE_CMD_ENABLE);

	if (mouse_cmd(portfd, MOUSE_CMD_GET_DEVICE_ID))
	{
		DEBUG("mouse device id command failed: no mouse\n");
		ret = 0;
		goto out;
	}

	ret = mouse_read(portfd);
	DEBUG("got mouse type %02x\n",ret);
	if (-1 == ret)
	{
		DEBUG("Failed to read initial mouse type\n");
		ret = 0;
		goto out;
	}

	/* attempt to enable IntelliMouse 3-button mode */
	DEBUG("Attempting to enable IntelliMouse 3-button mode\n");
	mouse_cmd(portfd, MOUSE_CMD_SET_SAMPLE_RATE);
	mouse_cmd(portfd, 0xc8);
	mouse_cmd(portfd, MOUSE_CMD_SET_SAMPLE_RATE);
	mouse_cmd(portfd, 0x64);
	mouse_cmd(portfd, MOUSE_CMD_SET_SAMPLE_RATE);
	mouse_cmd(portfd, 0x50);

	/* now issue get device id command */
	mouse_cmd(portfd, MOUSE_CMD_GET_DEVICE_ID);

	ret = mouse_read(portfd);
	DEBUG("Device ID after IntelliMouse initialization: %02x\n", ret);

	if (0x03 == ret)
	{
		/* attempt to enable IntelliMouse 5-button mode */
		DEBUG("Attempting to enable IntelliMouse 5-button mode\n");
		mouse_cmd(portfd, MOUSE_CMD_SET_SAMPLE_RATE);
		mouse_cmd(portfd, 0xc8);
		mouse_cmd(portfd, MOUSE_CMD_SET_SAMPLE_RATE);
		mouse_cmd(portfd, 0xc8);
		mouse_cmd(portfd, MOUSE_CMD_SET_SAMPLE_RATE);
		mouse_cmd(portfd, 0x50);

		/* now issue get device id command */
		mouse_cmd(portfd, MOUSE_CMD_GET_DEVICE_ID);
		ret = mouse_read(portfd);
		DEBUG("Device ID after IntelliMouse 5 button initialization: %02x\n", ret);
	}

	//ps2dev->device=strdup("psaux");
	//ps2dev->type=CLASS_MOUSE;
/*
###     generic        - 2-button serial
###     genericps/2    - 2-button ps/2
###     msintellips/2  - MS Intellimouse
###     generic3ps/2   - 3 button ps/2 
*/	
	switch (ret)
	{
	case 0x03:
	case 0x04:
	case 0x05:
		strcpy(driver,"msintellips/2");
		strcpy(desc,"Generic PS/2 Wheel Mouse");
		break;
	case 0x02:
		/* a ballpoint something or other */
	case 0x06:
		/* A4 Tech 4D Mouse ? */
	case 0x08:
		/* A4 Tech 4D+ Mouse ? */
	case 0x00:
	default:
		DEBUG("mouse type: %x\n",ret);

		strcpy(driver,"genericps/2");
		strcpy(desc,"Generic Mouse (PS/2)");
		break;
	}
	ret = 1;
out:
	DEBUG("resetting mouse\n");
	mouse_cmd(portfd, MOUSE_CMD_RESET);

	if (mouse_read(portfd) != MOUSE_RESP_TESTOK)
		DEBUG("Mouse self-test failed.\n");

	if (mouse_read(portfd) != 0x00)
		DEBUG("Mouse did not finish reset response.\n");

	if (mouse_cmd(portfd, MOUSE_CMD_ENABLE))
	{
		DEBUG("mouse enable failed: no mouse?\n");
	}

	if (mouse_cmd(portfd, MOUSE_CMD_GET_DEVICE_ID))
	{
		DEBUG("mouse type command failed: no mouse\n");
	}

	if (0x00 != mouse_read(portfd))
	{
		DEBUG("initial mouse type check strange: no mouse\n");
	}

	close(portfd);
	return ret;
}

