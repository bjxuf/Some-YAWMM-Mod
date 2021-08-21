#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <ogcsys.h>
#include <ogc/pad.h>

#include "sys.h"
#include "title.h"
#include "utils.h"
#include "video.h"
#include "wad.h"
#include "wpad.h"

// Turn upper and lower into a full title ID
#define TITLE_ID(x,y)		(((u64)(x) << 32) | (y))
// Get upper or lower half of a title ID
#define TITLE_UPPER(x)		((u32)((x) >> 32))
// Turn upper and lower into a full title ID
#define TITLE_LOWER(x)		((u32)(x))

typedef struct {
	int version;
	int region;

} SMRegion;

SMRegion regionlist[] = {
	{33, 'X'},
	{128, 'J'}, {97, 'E'}, {130, 'P'},
	{162, 'P'},
	{192, 'J'}, {193, 'E'}, {194, 'P'},
	{224, 'J'}, {225, 'E'}, {226, 'P'},
	{256, 'J'}, {257, 'E'}, {258, 'P'},
	{288, 'J'}, {289, 'E'}, {290, 'P'},
	{352, 'J'}, {353, 'E'}, {354, 'P'}, {326, 'K'},
	{384, 'J'}, {385, 'E'}, {386, 'P'},
	{390, 'K'},
	{416, 'J'}, {417, 'E'}, {418, 'P'},
	{448, 'J'}, {449, 'E'}, {450, 'P'}, {454, 'K'},
	{480, 'J'}, {481, 'E'}, {482, 'P'}, {486, 'K'},
	{512, 'E'}, {513, 'E'}, {514, 'P'}, {518, 'K'},
};

#define NB_SM		(sizeof(regionlist) / sizeof(SMRegion))

u32 WaitButtons(void);

u32 be32(const u8 *p)
{
	return (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
}

u64 be64(const u8 *p)
{
	return ((u64)be32(p) << 32) | be32(p + 4);
}

u64 get_title_ios(u64 title) {
	s32 ret, fd;
	static char filepath[256] ATTRIBUTE_ALIGN(32);	
	
	// Check to see if title exists
	if (ES_GetDataDir(title, filepath) >= 0 ) {
		u32 tmd_size;
		static u8 tmd_buf[MAX_SIGNED_TMD_SIZE] ATTRIBUTE_ALIGN(32);
	
		ret = ES_GetStoredTMDSize(title, &tmd_size);
		if (ret < 0){
			// If we fail to use the ES function, try reading manually
			// This is a workaround added since some IOS (like 21) don't like our
			// call to ES_GetStoredTMDSize
			
			//printf("Error! ES_GetStoredTMDSize: %d\n", ret);
					
			sprintf(filepath, "/title/%08lx/%08lx/content/title.tmd", TITLE_UPPER(title), TITLE_LOWER(title));
			
			ret = ISFS_Open(filepath, ISFS_OPEN_READ);
			if (ret <= 0)
			{
				//printf("Error! ISFS_Open (ret = %d)\n", ret);
				return 0;
			}
			
			fd = ret;
			
			ret = ISFS_Seek(fd, 0x184, 0);
			if (ret < 0)
			{
				//printf("Error! ISFS_Seek (ret = %d)\n", ret);
				return 0;
			}
			
			ret = ISFS_Read(fd,tmd_buf,8);
			if (ret < 0)
			{
				//printf("Error! ISFS_Read (ret = %d)\n", ret);
				return 0;
			}
			
			ret = ISFS_Close(fd);
			if (ret < 0)
			{
				//printf("Error! ISFS_Close (ret = %d)\n", ret);
				return 0;
			}
			
			return be64(tmd_buf);
			
		} else {
			// Normal versions of IOS won't have a problem, so we do things the "right" way.
			
			// Some of this code adapted from bushing's title_lister.c
			signed_blob *s_tmd = (signed_blob *)tmd_buf;
			ret = ES_GetStoredTMD(title, s_tmd, tmd_size);
			if (ret < 0){
				//printf("Error! ES_GetStoredTMD: %d\n", ret);
				return -1;
			}
			tmd *t = SIGNATURE_PAYLOAD(s_tmd);
			return t->sys_version;
		}
		
		
	} 
	return 0;
}

int get_sm_region_basic()
{
	u32 tmd_size;
		
	u64 title = TITLE_ID(1, 2);
	static u8 tmd_buf[MAX_SIGNED_TMD_SIZE] ATTRIBUTE_ALIGN(32);
	
	int ret = ES_GetStoredTMDSize(title, &tmd_size);
		
	// Some of this code adapted from bushing's title_lister.c
	signed_blob *s_tmd = (signed_blob *)tmd_buf;
	ret = ES_GetStoredTMD(title, s_tmd, tmd_size);
	if (ret < 0){
		//printf("Error! ES_GetStoredTMD: %d\n", ret);
		return -1;
	}
	tmd *t = SIGNATURE_PAYLOAD(s_tmd);
	ret = t->title_version;
	int i = 0;
	while( i <= NB_SM)
	{
		if(	regionlist[i].version == ret) return regionlist[i].region;
		i++;
	}
	return 0;
}

/* 'WAD Header' structure */
typedef struct {
	/* Header length */
	u32 header_len;

	/* WAD type */
	u16 type;

	u16 padding;

	/* Data length */
	u32 certs_len;
	u32 crl_len;
	u32 tik_len;
	u32 tmd_len;
	u32 data_len;
	u32 footer_len;
} ATTRIBUTE_PACKED wadHeader;

/* Variables */
static u8 wadBuffer[BLOCK_SIZE] ATTRIBUTE_ALIGN(32);


s32 __Wad_ReadFile(FILE *fp, void *outbuf, u32 offset, u32 len)
{
	s32 ret;

	/* Seek to offset */
	fseek(fp, offset, SEEK_SET);

	/* Read data */
	ret = fread(outbuf, len, 1, fp);
	if (ret < 0)
		return ret;

	return 0;
}

s32 __Wad_ReadAlloc(FILE *fp, void **outbuf, u32 offset, u32 len)
{
	void *buffer = NULL;
	s32   ret;

	/* Allocate memory */
	buffer = memalign(32, len);
	if (!buffer)
		return -1;

	/* Read file */
	ret = __Wad_ReadFile(fp, buffer, offset, len);
	if (ret < 0) {
		free(buffer);
		return ret;
	}

	/* Set pointer */
	*outbuf = buffer;

	return 0;
}

s32 __Wad_GetTitleID(FILE *fp, wadHeader *header, u64 *tid)
{
	signed_blob *p_tik    = NULL;
	tik         *tik_data = NULL;

	u32 offset = 0;
	s32 ret;

	/* Ticket offset */
	offset += round_up(header->header_len, 64);
	offset += round_up(header->certs_len,  64);
	offset += round_up(header->crl_len,    64);

	/* Read ticket */
	ret = __Wad_ReadAlloc(fp, (void *)&p_tik, offset, header->tik_len);
	if (ret < 0)
		goto out;

	/* Ticket data */
	tik_data = (tik *)SIGNATURE_PAYLOAD(p_tik);

	/* Copy title ID */
	*tid = tik_data->titleid;

out:
	/* Free memory */
	if (p_tik)
		free(p_tik);

	return ret;
}

void __Wad_FixTicket(signed_blob *p_tik)
{
	u8 *data = (u8 *)p_tik;
	u8 *ckey = data + 0x1F1;

	/* Check common key */
	if (*ckey > 1)
		*ckey = 0;
		
	 /* Fakesign ticket */
	 Title_FakesignTik(p_tik);
}

s32 Wad_Install(FILE *fp)
{
	wadHeader   *header  = NULL;
	signed_blob *p_certs = NULL, *p_crl = NULL, *p_tik = NULL, *p_tmd = NULL;

	tmd *tmd_data  = NULL;

	u32 cnt, offset = 0;
	int ret;
	u64 tid;

	printf("\t\t>> Leyendo datos de WAD...");
	fflush(stdout);
	
	ret = __Wad_ReadAlloc(fp, (void *)&header, offset, sizeof(wadHeader));
	if (ret >= 0)
		offset += round_up(header->header_len, 64);
	else
	goto err;
	
	//Don't try to install boot2
	__Wad_GetTitleID(fp, header, &tid);
	
	if (tid == TITLE_ID(1, 1))
	{
		printf("\n    Accion no permitida\n");
		ret = -999;
		goto out;
	}
	
 /* WAD certificates */
	ret = __Wad_ReadAlloc(fp, (void *)&p_certs, offset, header->certs_len);
	if (ret >= 0)
		offset += round_up(header->certs_len, 64);
	else
	goto err;
	
	/* WAD crl */
	if (header->crl_len) {
		ret = __Wad_ReadAlloc(fp, (void *)&p_crl, offset, header->crl_len);
		if (ret < 0)
			goto err;
		else
			offset += round_up(header->crl_len, 64);
	}

	/* WAD ticket */
	ret = __Wad_ReadAlloc(fp, (void *)&p_tik, offset, header->tik_len);
	if (ret < 0)
		goto err;
	else
		offset += round_up(header->tik_len, 64);

	/* WAD TMD */
	ret = __Wad_ReadAlloc(fp, (void *)&p_tmd, offset, header->tmd_len);
	if (ret < 0)
		goto err;
	else
		offset += round_up(header->tmd_len, 64);

	Con_ClearLine();
	
	/* Get TMD info */
	
	tmd_data = (tmd *)SIGNATURE_PAYLOAD(p_tmd);
	
	if(TITLE_LOWER(tmd_data->sys_version) != 0 && isIOSstub(TITLE_LOWER(tmd_data->sys_version)))
	{
		printf("\n    Este titulo necesita el IOS%li pero la version\n    es un stub.\n", TITLE_LOWER(tmd_data->sys_version));
		ret = -999;
		goto err;
	}
	
	if(get_title_ios(TITLE_ID(1, 2)) == tid)
	{
		if ( ( tmd_data->num_contents == 3) && (tmd_data->contents[0].type == 1 && tmd_data->contents[1].type == 0x8001 && tmd_data->contents[2].type == 0x8001) )
		{
			printf("\n    Instalar un IOS stub para menu de sistema no permitido\n");
			ret = -999;
			goto err;
		}
	}
	
	if(tid  == get_title_ios(TITLE_ID(0x10008, 0x48414B00 | 'E')) || tid  == get_title_ios(TITLE_ID(0x10008, 0x48414B00 | 'P')) || tid  == get_title_ios(TITLE_ID(0x10008, 0x48414B00 | 'J')) || tid  == get_title_ios(TITLE_ID(0x10008, 0x48414B00 | 'K')))
	{
		if ( ( tmd_data->num_contents == 3) && (tmd_data->contents[0].type == 1 && tmd_data->contents[1].type == 0x8001 && tmd_data->contents[2].type == 0x8001) )
		{
			printf("\n    Instalar un EULA stub no permitido\n");
			ret = -999;
			goto err;
		}
	}
	
	if(tid  == get_title_ios(TITLE_ID(0x10008, 0x48414C00 | 'E')) || tid  == get_title_ios(TITLE_ID(0x10008, 0x48414C00 | 'P')) || tid  == get_title_ios(TITLE_ID(0x10008, 0x48414C00 | 'J')) || tid  == get_title_ios(TITLE_ID(0x10008, 0x48414C00 | 'K')))
	{
		if ( ( tmd_data->num_contents == 3) && (tmd_data->contents[0].type == 1 && tmd_data->contents[1].type == 0x8001 && tmd_data->contents[2].type == 0x8001) )
		{
			printf("\n    Instalar rgsel stub no permitido\n");
			ret = -999;
			goto err;
		}
	}
	if (tid == get_title_ios(TITLE_ID(0x10001, 0x48415858)) || tid == get_title_ios(TITLE_ID(0x10001, 0x4A4F4449)))
	{
		if ( ( tmd_data->num_contents == 3) && (tmd_data->contents[0].type == 1 && tmd_data->contents[1].type == 0x8001 && tmd_data->contents[2].type == 0x8001) )
		{
			printf("\n    ¿Seguro que quieres instalar un stub en el IOS del HBC?\n");
			printf("\n    Presiona A para continuar.\n");
			printf("    Presiona B para saltar.");
		
			u32 buttons = WaitButtons();
		
			if (!(buttons & WPAD_BUTTON_A))
			{
				ret = -998;
				goto err;
			}
		}
	}
	
	if (tid == TITLE_ID(1, 2))
	{
		if(get_sm_region_basic() == 0)
		{
			printf("\n    No se pudo obtener region del sistema\n    Por favor, revisa el sitio para actualizaciones\n");
			ret = -999;
			goto err;
		}
		int i, ret = -1;
		for(i = 0; i <= NB_SM; i++)
		{
			if(	regionlist[i].version == tmd_data->title_version)
			{
				ret = 1;
				break;
			}
		}
		if(ret -1)
		{
			printf("\n    No se pudo obtener region del sistema\n    Por favor, revisa el sitio para actualizaciones\n");
			ret = -999;
			goto err;
		}
		if( get_sm_region_basic() != regionlist[i].region)
		{
			printf("\n    No se instalara version del menu de sistema incorrecta\n");
			ret = -999;
			goto err;
		}
		if(tmd_data->title_version < 416)
		{
			if(boot2version == 4)
			{
				printf("\n    Esta version del menu de sistema\n    no es compatible con tu Wii\n");
				ret = -999;
				goto err;
			}
		}
	}
	
	/* Fix ticket */
	__Wad_FixTicket(p_tik);

	printf("\t\t>> Instalando ticket...");
	fflush(stdout);

	/* Install ticket */
	ret = ES_AddTicket(p_tik, header->tik_len, p_certs, header->certs_len, p_crl, header->crl_len);
	if (ret < 0)
		goto err;

	Con_ClearLine();

	printf("\r\t\t>> Instalando titulo...");
	fflush(stdout);

	/* Install title */
	ret = ES_AddTitleStart(p_tmd, header->tmd_len, p_certs, header->certs_len, p_crl, header->crl_len);
	if (ret < 0)
		goto err;
	
	/* Install contents */
	for (cnt = 0; cnt < tmd_data->num_contents; cnt++) {
		tmd_content *content = &tmd_data->contents[cnt];

		u32 idx = 0, len;
		s32 cfd;

		Con_ClearLine();

		printf("\r\t\t>> Instalando contenido #%02ld...", content->cid);
		fflush(stdout);

		/* Encrypted content size */
		len = round_up(content->size, 64);

		/* Install content */
		cfd = ES_AddContentStart(tmd_data->title_id, content->cid);
		if (cfd < 0) {
			ret = cfd;
			goto err;
		}

		/* Install content data */
		while (idx < len) {
			u32 size;

			/* Data length */
			size = (len - idx);
			if (size > BLOCK_SIZE)
				size = BLOCK_SIZE;

			/* Read data */
			ret = __Wad_ReadFile(fp, &wadBuffer, offset, size);
			if (ret < 0)
				goto err;

			/* Install data */
			ret = ES_AddContentData(cfd, wadBuffer, size);
			if (ret < 0)
				goto err;

			/* Increase variables */
			idx    += size;
			offset += size;
		}

		/* Finish content installation */
		ret = ES_AddContentFinish(cfd);
		if (ret < 0)
			goto err;
	}

	Con_ClearLine();

	printf("\r\t\t>> Terminando instalacion...");
	fflush(stdout);

	/* Finish title install */
	ret = ES_AddTitleFinish();
	if (ret >= 0) {
		printf(" LISTO!\n");
		goto out;
	}

err:
	printf(" ERROR! (ret = %d)\n", ret);

	/* Cancel install */
	ES_AddTitleCancel();

out:
	/* Free memory */
	if (header)
		free(header);
	if (p_certs)
		free(p_certs);
	if (p_crl)
		free(p_crl);
	if (p_tik)
		free(p_tik);
	if (p_tmd)
		free(p_tmd);

	return ret;
}

s32 Wad_Uninstall(FILE *fp)
{
	wadHeader *header   = NULL;
	tikview   *viewData = NULL;

	u64 tid;
	u32 viewCnt;
	int ret;

	printf("\t\t>> Leyendo datos de WAD...");
	fflush(stdout);

	/* WAD header */
	ret = __Wad_ReadAlloc(fp, (void *)&header, 0, sizeof(wadHeader));
	if (ret < 0) {
		printf(" ERROR! (ret = %d)\n", ret);
		goto out;
	}

	/* Get title ID */
	ret =  __Wad_GetTitleID(fp, header, &tid);
	if (ret < 0) {
		printf(" ERROR! (ret = %d)\n", ret);
		goto out;
	}
	//Assorted Checks
	if (TITLE_UPPER(tid) == 1 && get_title_ios(TITLE_ID(1, 2)) == 0)
	{
		printf("\n    Imposible determinar IOS del menu de sistema\nEliminacion de titulos del sistema deshabilitada\n");
		ret = -999;
		goto out;
	}
	if (tid == TITLE_ID(1, 1))
	{
		printf("\n    No se desinstalara boot2\n");
		ret = -999;
		goto out;
	}
	if (tid == TITLE_ID(1, 2))
	{
		printf("\n    No se desinstalara el menu de sistema\n");
		ret = -999;
		goto out;
	}
	if(get_title_ios(TITLE_ID(1, 2)) == tid)
	{
		printf("\n    No se desinstalara IOS del menu de sistema\n");
		ret = -999;
		goto out;
	}
	if (tid == get_title_ios(TITLE_ID(0x10001, 0x48415858)) || tid == get_title_ios(TITLE_ID(0x10001, 0x4A4F4449)))
	{
		printf("\n    Este IOS es usado por el HBC, desinstalarlo impedira su uso!\n");
		printf("\n    Presiona A para continuar.\n");
		printf("    Presiona B para saltar.");
		
		u32 buttons = WaitButtons();
		
		if (!(buttons & WPAD_BUTTON_A))
		{
			ret = -998;
			goto out;
		}
	}
	if((tid  == TITLE_ID(0x10008, 0x48414B00 | 'E') || tid  == TITLE_ID(0x10008, 0x48414B00 | 'P') || tid  == TITLE_ID(0x10008, 0x48414B00 | 'J') || tid  == TITLE_ID(0x10008, 0x48414B00 | 'K') 
		|| (tid  == TITLE_ID(0x10008, 0x48414C00 | 'E') || tid  == TITLE_ID(0x10008, 0x48414C00 | 'P') || tid  == TITLE_ID(0x10008, 0x48414C00 | 'J') || tid  == TITLE_ID(0x10008, 0x48414C00 | 'K'))) && get_sm_region_basic() == 0)
	{
		printf("\n    No se pudo obtener region del sistema\n    Por favor, revisa el sitio para actualizaciones");
		ret = -999;
		goto out;
	}
	if(tid  == TITLE_ID(0x10008, 0x48414B00 | get_sm_region_basic()))
	{
		printf("\n    No se desinstalara EULA\n");
		ret = -999;
		goto out;
	}	
	if(tid  == TITLE_ID(0x10008, 0x48414C00 | get_sm_region_basic()))
	{
		printf("\n    No se desinstalara rgsel\n");
		ret = -999;
		goto out;
	}	
	if(tid  == get_title_ios(TITLE_ID(0x10008, 0x48414B00 | get_sm_region_basic())))
	{
		printf("\n    No se desinstalara IOS del EULA\n");
		ret = -999;
		goto out;
	}	
	if(tid  == get_title_ios(TITLE_ID(0x10008, 0x48414C00 | get_sm_region_basic())))
	{
		printf("\n    No se desinstalara IOS de rgsel\n");
		ret = -999;
		goto out;
	}

	Con_ClearLine();

	printf("\t\t>> Eliminando tickets...");
	fflush(stdout);

	/* Get ticket views */
	ret = Title_GetTicketViews(tid, &viewData, &viewCnt);
	if (ret < 0)
		printf(" ERROR! (ret = %d)\n", ret);

	/* Delete tickets */
	if (ret >= 0) {
		u32 cnt;

		/* Delete all tickets */
		for (cnt = 0; cnt < viewCnt; cnt++) {
			ret = ES_DeleteTicket(&viewData[cnt]);
			if (ret < 0)
				break;
		}

		if (ret < 0)
			printf(" ERROR! (ret = %d\n", ret);
		else
			printf(" LISTO!\n");
	}

	printf("\t\t>> Eliminando contenido del titulo...");
	fflush(stdout);

	/* Delete title contents */
	ret = ES_DeleteTitleContent(tid);
	if (ret < 0)
		printf(" ERROR! (ret = %d)\n", ret);
	else
		printf(" LISTO!\n");


	printf("\t\t>> Eliminando titulo...");
	fflush(stdout);

	/* Delete title */
	ret = ES_DeleteTitle(tid);
	if (ret < 0)
		printf(" ERROR! (ret = %d)\n", ret);
	else
		printf(" LISTO!\n");

out:
	/* Free memory */
	if (header)
		free(header);
	return ret;
}
