/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/gba/sio.h>

#include <mgba/internal/gba/gba.h>
#include <mgba/internal/gba/io.h>

#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

mLOG_DEFINE_CATEGORY(GBA_SIO, "GBA Serial I/O", "gba.sio");

const int GBASIOCyclesPerTransfer[4][MAX_GBAS] = {
	{ 38326, 73003, 107680, 142356 },
	{ 9582, 18251, 26920, 35589 },
	{ 6388, 12167, 17947, 23726 },
	{ 3194, 6075, 8973, 11863 }
};

static struct GBASIODriver* _lookupDriver(struct GBASIO* sio, enum GBASIOMode mode) {
	switch (mode) {
	case SIO_NORMAL_8:
	case SIO_NORMAL_32:
		return sio->drivers.normal;
	case SIO_MULTI:
		return sio->drivers.multiplayer;
	case SIO_JOYBUS:
		return sio->drivers.joybus;
	default:
		return 0;
	}
}

static void _switchMode(struct GBASIO* sio) {
	unsigned mode = ((sio->rcnt & 0xC000) | (sio->siocnt & 0x3000)) >> 12;
	enum GBASIOMode newMode;
	if (mode < 8) {
		newMode = (enum GBASIOMode) (mode & 0x3);
	} else {
		newMode = (enum GBASIOMode) (mode & 0xC);
	}
	if (newMode != sio->mode) {
		if (sio->activeDriver && sio->activeDriver->unload) {
			sio->activeDriver->unload(sio->activeDriver);
		}
		sio->mode = newMode;
		sio->activeDriver = _lookupDriver(sio, sio->mode);
		if (sio->activeDriver && sio->activeDriver->load) {
			sio->activeDriver->load(sio->activeDriver);
		}
	}
}

void GBASIOInit(struct GBASIO* sio) {
	sio->drivers.normal = 0;
	sio->drivers.multiplayer = 0;
	sio->drivers.joybus = 0;
	sio->activeDriver = 0;
	memset( sio->net.cmd, 0, 255 );
	sio->net.cmd_i = 0;
	sio->net.in_i = 0;
	sio->net.out_i = 0;
	sio->net.mode = NETMODE_IDLE;
	sio->net.ip = 0;
	sio->net.port = 0;
	sio->net.crc = 0;
	sio->net.len = 0;
	sio->net.sock_fd = 0;
	GBASIOReset(sio);
}

void GBASIODeinit(struct GBASIO* sio) {
	if (sio->activeDriver && sio->activeDriver->unload) {
		sio->activeDriver->unload(sio->activeDriver);
	}
	if (sio->drivers.multiplayer && sio->drivers.multiplayer->deinit) {
		sio->drivers.multiplayer->deinit(sio->drivers.multiplayer);
	}
	if (sio->drivers.joybus && sio->drivers.joybus->deinit) {
		sio->drivers.joybus->deinit(sio->drivers.joybus);
	}
	if (sio->drivers.normal && sio->drivers.normal->deinit) {
		sio->drivers.normal->deinit(sio->drivers.normal);
	}
}

void GBASIOReset(struct GBASIO* sio) {
	if (sio->activeDriver && sio->activeDriver->unload) {
		sio->activeDriver->unload(sio->activeDriver);
	}
	sio->rcnt = RCNT_INITIAL;
	sio->siocnt = 0;
	sio->mode = -1;
	sio->activeDriver = NULL;
	_switchMode(sio);
}

void GBASIOSetDriverSet(struct GBASIO* sio, struct GBASIODriverSet* drivers) {
	GBASIOSetDriver(sio, drivers->normal, SIO_NORMAL_8);
	GBASIOSetDriver(sio, drivers->multiplayer, SIO_MULTI);
	GBASIOSetDriver(sio, drivers->joybus, SIO_JOYBUS);
}

void GBASIOSetDriver(struct GBASIO* sio, struct GBASIODriver* driver, enum GBASIOMode mode) {
	struct GBASIODriver** driverLoc;
	switch (mode) {
	case SIO_NORMAL_8:
	case SIO_NORMAL_32:
		driverLoc = &sio->drivers.normal;
		break;
	case SIO_MULTI:
		driverLoc = &sio->drivers.multiplayer;
		break;
	case SIO_JOYBUS:
		driverLoc = &sio->drivers.joybus;
		break;
	default:
		mLOG(GBA_SIO, ERROR, "Setting an unsupported SIO driver: %x", mode);
		return;
	}
	if (*driverLoc) {
		if ((*driverLoc)->unload) {
			(*driverLoc)->unload(*driverLoc);
		}
		if ((*driverLoc)->deinit) {
			(*driverLoc)->deinit(*driverLoc);
		}
	}
	if (driver) {
		driver->p = sio;

		if (driver->init) {
			if (!driver->init(driver)) {
				driver->deinit(driver);
				mLOG(GBA_SIO, ERROR, "Could not initialize SIO driver");
				return;
			}
		}
	}
	if (sio->activeDriver == *driverLoc) {
		sio->activeDriver = driver;
		if (driver && driver->load) {
			driver->load(driver);
		}
	}
	*driverLoc = driver;
}

void GBASIOWriteRCNT(struct GBASIO* sio, uint16_t value) {
	sio->rcnt &= 0xF;
	sio->rcnt |= value & ~0xF;
	_switchMode(sio);
	if (sio->activeDriver && sio->activeDriver->writeRegister) {
		sio->activeDriver->writeRegister(sio->activeDriver, REG_RCNT, value);
	}
}

void GBASIOWriteSIOCNT(struct GBASIO* sio, uint16_t value) {
	if ((value ^ sio->siocnt) & 0x3000) {
		sio->siocnt = value & 0x3000;
		_switchMode(sio);
	}
	if (sio->activeDriver && sio->activeDriver->writeRegister) {
		value = sio->activeDriver->writeRegister(sio->activeDriver, REG_SIOCNT, value);
	} else {
		// Dummy drivers
		switch (sio->mode) {
		case SIO_NORMAL_8:
		case SIO_NORMAL_32:
			value |= 0x0004;
			if ((value & 0x0081) == 0x0081) {
				if (value & 0x4000) {
					// TODO: Test this on hardware to see if this is correct
					GBARaiseIRQ(sio->p, IRQ_SIO, 0);
				}
				value &= ~0x0080;
			}
			break;
		case SIO_MULTI:
			value &= 0xFF83;
			value |= 0xC;
			break;
		default:
			// TODO
			break;
		}
	}
	sio->siocnt = value;
}

uint16_t GBASIOWriteRegister(struct GBASIO* sio, uint32_t address, uint16_t value) {
	if (sio->activeDriver && sio->activeDriver->writeRegister) {
		return sio->activeDriver->writeRegister(sio->activeDriver, address, value);
	}
	return value;
}

static int _connect_socket( struct GBASIO* sio )
{
	sio->net.sock_fd = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );
	
	struct sockaddr_in addr = {0};
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr( "127.0.0.1" );
	addr.sin_port = ((3301 & 0xFF) << 8) | (3301 >> 8);
	int r = connect( sio->net.sock_fd, (struct sockaddr*)(&addr), sizeof(addr) );

	if( r )
	{
		printf( "connect() failed\n" );
	}
	else
	{
		printf( "Socket connected!\n" );
	}

	sio->net.mode = NETMODE_IDLE;
	return sio->net.sock_fd != 0;
}

static void _disconnect_socket( struct GBASIO* sio )
{
	shutdown( sio->net.sock_fd, 2 );
	printf( "Socket disconnected.\n" );
	sio->net.mode = NETMODE_IDLE;
	sio->net.sock_fd = 0;
}

static void _siodata8_write_idle( struct GBASIO* sio, uint8_t v )
{
	if(v == 0x89) {
		printf( "Going into parse mode.\n" );
		sio->net.mode = NETMODE_PARSE;
		sio->net.cmd_i = 0;
	}
}

static void _siodata8_write_parse( struct GBASIO* sio, uint8_t v )
{
	if(sio->net.cmd_i == 0)
	{
		if(v == 0x00 || v == 0x01 || v >= 0xFE)
		{
			printf( "Canceled parse. Idling...\n" );
			sio->net.mode = NETMODE_IDLE;
		}
		else if(v <= 0x07)
		{
			sio->net.cmd[sio->net.cmd_i++] = v;
		}
	}
	else if(v == 0xFE)
	{
		int i;
		printf( "Command buffer completed.\nContains: " );
		for(i = 0; i < sio->net.cmd_i; ++i)
		{
			if(sio->net.cmd[i] < 0x10)
			{
				printf( "0%X ", sio->net.cmd[i] );
			}
			else
			{
				printf( "%X ", sio->net.cmd[i] );
			}
		}
		printf( "\ncmd_i := %iu\n", sio->net.cmd_i );
		/* done receiving data. time to parse */
		if(sio->net.cmd[0] == 0x02 && sio->net.cmd_i >= 7 && sio->net.sock_fd)
		{
			printf( "SEND\n" );
			sio->net.len = (uint16_t)(sio->net.cmd[5]) |
				((uint16_t)(sio->net.cmd[6]) << 8);
			sio->net.crc = (uint32_t)(sio->net.cmd[1]) |
				((uint32_t)(sio->net.cmd[2]) << 8) |
				((uint32_t)(sio->net.cmd[3]) << 16) |
				((uint32_t)(sio->net.cmd[4]) << 24);
			sio->net.mode = NETMODE_SEND;
			sio->net.out_i = 0;
			write( sio->net.sock_fd, &(sio->net.cmd[2]), 1 );
			write( sio->net.sock_fd, &(sio->net.cmd[1]), 1 );
			write( sio->net.sock_fd, &(sio->net.cmd[6]), 1 );
			write( sio->net.sock_fd, &(sio->net.cmd[5]), 1 );
			write( sio->net.sock_fd, &(sio->net.cmd[4]), 1 );
			write( sio->net.sock_fd, &(sio->net.cmd[3]), 1 );
		}
		else if(sio->net.cmd[0] == 0x03 && sio->net.sock_fd)
		{
			printf( "RECV\n" );
			sio->net.mode = NETMODE_RECV;
			sio->net.in_i = 0;
		}
		else if(sio->net.cmd[0] == 0x04 && sio->net.cmd_i >= 7
		&& sio->net.sock_fd)
		{
			/* XXX: full duplex comms not implemented yet */
		}
		else if(sio->net.cmd[0] == 0x05 && sio->net.cmd_i >= 7
		&& !sio->net.sock_fd)
		{
			sio->net.port = (uint16_t)(sio->net.cmd[5]) |
				((uint16_t)(sio->net.cmd[6]) << 8);
			sio->net.ip = (uint32_t)(sio->net.cmd[1]) |
				((uint32_t)(sio->net.cmd[2]) << 8) |
				((uint32_t)(sio->net.cmd[3]) << 16) |
				((uint32_t)(sio->net.cmd[4]) << 24);
			printf( "Connecting to socket on %i.%i.%i.%i:%i.\n",
				sio->net.cmd[4], sio->net.cmd[3], sio->net.cmd[2],
				sio->net.cmd[1], sio->net.port );
			sio->net.mode = NETMODE_ESTAB;
			_connect_socket( sio );
		}
		else if(sio->net.cmd[0] == 0x06 && sio->net.sock_fd)
		{
			_disconnect_socket( sio );
		}
		else if(sio->net.cmd[0] == 0x07)
		{
			/* TODO: DNS lookup, async */
		}
	}
	else if(sio->net.cmd_i < 255)
	{
		sio->net.cmd[sio->net.cmd_i++] = v;
	}
	else
	{
		/* reset on overflow of command buffer */
		sio->net.cmd_i = 0;
		memset( sio->net.cmd, 0, 255 );
		sio->net.mode = NETMODE_IDLE;
	}
}

static void _siodata8_write_send( struct GBASIO* sio, uint8_t v )
{
	write( sio->net.sock_fd, &v, 1 );
	sio->net.out_i++;

	if((sio->net.mode == NETMODE_SEND && sio->net.out_i >= sio->net.len)
	|| (sio->net.out_i >= sio->net.len_out && sio->net.in_i >= sio->net.len))
	{
		sio->net.mode = NETMODE_IDLE;
	}
}

static uint8_t _siodata8_read_recv( struct GBASIO* sio )
{
	uint8_t v;

	read( sio->net.sock_fd, &v, 1 );
	sio->net.in_i++;

	if((sio->net.mode == NETMODE_RECV && sio->net.in_i >= sio->net.len)
	|| (sio->net.out_i >= sio->net.len_out && sio->net.in_i >= sio->net.len))
	{
		sio->net.mode = NETMODE_IDLE;
	}

	return v;
}

static void _siodata8_write_nop( struct GBASIO* sio, uint8_t v ) {}
static uint8_t _siodata8_read_nop( struct GBASIO* sio ) { return 0xDA; }
static uint8_t _siodata8_read_zero( struct GBASIO* sio) { return 0; }

typedef void (*fn_siodata8_write)(struct GBASIO*, uint8_t);
typedef uint8_t (*fn_siodata8_read)(struct GBASIO*);

static fn_siodata8_write _siodata8_write[MAX_NETMODE] = {
	_siodata8_write_idle,
	_siodata8_write_parse,
	_siodata8_write_send,
	_siodata8_write_nop,
	_siodata8_write_send,
	_siodata8_write_nop,
	_siodata8_write_nop
};

static fn_siodata8_read _siodata8_read[MAX_NETMODE] = {
	_siodata8_read_nop,
	_siodata8_read_nop,
	_siodata8_read_nop,
	_siodata8_read_recv,
	_siodata8_read_recv,
	_siodata8_read_zero,
	_siodata8_read_nop
};

void GBASIOWriteSIODATA8( struct GBASIO* sio, uint16_t value )
{
	printf( "Writing value 0x%x.\n", (uint8_t)value );
	_siodata8_write[sio->net.mode]( sio, (uint8_t)(value & 0xFF) );
}

uint16_t GBASIOReadSIODATA8( struct GBASIO* sio )
{
	return (uint16_t)(_siodata8_read[sio->net.mode]( sio ));
}