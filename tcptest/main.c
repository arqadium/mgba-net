/* coding: utf-8 */

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef volatile u8 vu8;
typedef volatile u16 vu16;

enum
{
	SIODATA8 = 0x400012A
};

static u8 sample[0x10] = {
	0x00, 0xFF, 0x00, 0xFF,
	0xDA, 0xDB, 0xDC, 0xDD,
	0xFE, 0xFE, 0xFA, 0xFA,
	0x99, 0x88, 0x66, 0x11
};

struct rcpt_f
{
	u32 crc;
	u16 len;
} __attribute__((packed));

struct rcpt_b
{
	u8 b[6];
};

struct rcpt
{
	union
	{
		struct rcpt_b b;
		struct rcpt_f f;
	};
};

static struct rcpt rcpt;
static u8 buf[0x10] = {0};

static u8 sample_chk[4] = { 0xE9, 0xC2, 0x68, 0x18 };

int main( void )
{
	int i;
	u8 byte;

	/* magic words! */
	*(vu16*)SIODATA8 = 0x89;

	/* connect */
	*(vu16*)SIODATA8 = 0x05;

	/* IP */
	*(vu16*)SIODATA8 = 0x01;
	*(vu16*)SIODATA8 = 0x00;
	*(vu16*)SIODATA8 = 0x00;
	*(vu16*)SIODATA8 = 0x7F;

	/* port */
	*(vu16*)SIODATA8 = 3301 & 0xFF;
	*(vu16*)SIODATA8 = (3301 >> 8);

	*(vu16*)SIODATA8 = 0xFE;

	for(;;)
	{
		byte = (u8)(*(vu16*)SIODATA8);

		if(byte != 0)
		{
			break;
		}
	}

	if(byte == 2)
	{
		for(;;);
	}

	/* magic words! */
	*(vu16*)SIODATA8 = 0x89;

	/* send data */
	*(vu16*)SIODATA8 = 0x02;

	/* checksum */
	*(vu16*)SIODATA8 = sample_chk[3];
	*(vu16*)SIODATA8 = sample_chk[2];
	*(vu16*)SIODATA8 = sample_chk[1];
	*(vu16*)SIODATA8 = sample_chk[0];

	/* length */
	*(vu16*)SIODATA8 = 0x10;
	*(vu16*)SIODATA8 = 0x00;

	/* finish it */
	*(vu16*)SIODATA8 = 0xFE;

	for(i = 0; i < 0x10; ++i)
	{
		*(vu16*)SIODATA8 = sample[i];
	}

	/* magic words! */
	*(vu16*)SIODATA8 = 0x89;

	/* recv data */
	*(vu16*)SIODATA8 = 0x03;

	for(i = 0; i < 6; ++i)
	{
		rcpt.b[i] = *(vu8*)SIODATA8;
	}

	for(;;);

	return 0;
}
