#include <bzlib.h>
#include "defines.h"
#include "powder.h"
#include "save.h"
#include "gravity.h"
#include "BSON.h"

void *build_save(int *size, int orig_x0, int orig_y0, int orig_w, int orig_h, unsigned char bmap[YRES/CELL][XRES/CELL], float vx[YRES/CELL][XRES/CELL], float vy[YRES/CELL][XRES/CELL], float pv[YRES/CELL][XRES/CELL], float fvx[YRES/CELL][XRES/CELL], float fvy[YRES/CELL][XRES/CELL], sign signs[MAXSIGNS], void* partsptr)
{
#ifdef SAVE_OPS
	return build_save_OPS(size, orig_x0, orig_y0, orig_w, orig_h, bmap, vx, vy, pv, fvx, fvy, signs, partsptr);
#else
	return build_save_PSv(size, orig_x0, orig_y0, orig_w, orig_h, bmap, fvx, fvy, signs, partsptr);
#endif
}

int parse_save(void *save, int size, int replace, int x0, int y0, unsigned char bmap[YRES/CELL][XRES/CELL], float vx[YRES/CELL][XRES/CELL], float vy[YRES/CELL][XRES/CELL], float pv[YRES/CELL][XRES/CELL], float fvx[YRES/CELL][XRES/CELL], float fvy[YRES/CELL][XRES/CELL], sign signs[MAXSIGNS], void* partsptr, unsigned pmap[YRES][XRES])
{
	unsigned char * saveData = save;
	if(saveData[0] == 'O' && saveData[1] == 'P' && saveData[2] == 'S')
	{
		return parse_save_OPS(save, size, replace, x0, y0, bmap, vx, vy, pv, fvx, fvy, signs, partsptr, pmap);
	}
	else if((saveData[0]==0x66 && saveData[1]==0x75 && saveData[2]==0x43) || (saveData[0]==0x50 && saveData[1]==0x53 && saveData[2]==0x76))
	{
		return parse_save_PSv(save, size, replace, x0, y0, bmap, fvx, fvy, signs, partsptr, pmap);
	}
}

void *build_save_OPS(int *size, int orig_x0, int orig_y0, int orig_w, int orig_h, unsigned char bmap[YRES/CELL][XRES/CELL], float vx[YRES/CELL][XRES/CELL], float vy[YRES/CELL][XRES/CELL], float pv[YRES/CELL][XRES/CELL], float fvx[YRES/CELL][XRES/CELL], float fvy[YRES/CELL][XRES/CELL], sign signs[MAXSIGNS], void* o_partsptr)
{
	particle *partsptr = o_partsptr;
	unsigned char *partsData = NULL, *fanData = NULL, *wallData = NULL, *finalData = NULL, *outputData = NULL;
	int partsDataLen, fanDataLen, wallDataLen, finalDataLen, outputDataLen;
	int blockX, blockY, blockW, blockH, fullX, fullY, fullW, fullH;
	int x, y, i, wallDataFound = 0;
	
	//Get coords in blocks
	blockX = orig_x0/CELL;
	blockY = orig_y0/CELL;
	blockW = orig_w/CELL;
	blockH = orig_h/CELL;
	
	//Snap full coords to block size
	fullX = blockX*CELL;
	fullY = blockY*CELL;
	fullW = blockW*CELL;
	fullH = blockH*CELL;
	
	//Copy fan and wall data
	wallData = malloc(blockW*blockH);
	wallDataLen = blockW*blockH;
	fanData = malloc(blockW*blockH);
	fanDataLen = 0;
	for(x = blockX; x < blockX+blockW; x++)
	{
		for(y = blockY; y < blockY+blockH; y++)
		{
			wallData[y*blockW+x] = bmap[y][x];
			if(bmap[y][x] && !wallDataFound)
				wallDataFound = 1;
			if(bmap[y][x]==WL_FAN || bmap[y][x]==4)
			{
				i = (int)(fvy[y][x]*64.0f+127.5f);
				if (i<0) i=0;
				if (i>255) i=255;
				fanData[fanDataLen++] = i;
			}
		}
	}
	if(!fanDataLen)
	{
		free(fanData);
		fanData = NULL;
	}
	if(!wallDataFound)
	{
		free(wallData);
		wallData = NULL;
	}
	
	//Copy parts data
	/* Field descriptor format:
	|		0		|		0		|		0		|		0		|		0		|		0		|		0		|		0		|
	|		vy		|		vx		|	dcololour	|	ctype		|		tmp[2]	|		tmp[1]	|		life[2]	|		life[1]	|
	life[2] means a second byte (for a 16 bit field) if life[1] is present
	*/
	partsData = malloc(NPART * (sizeof(particle)+1));
	partsDataLen = 0;
	for(i = 0; i < NPART; i++)
	{
		if(parts[i].type)
		{
			x = (int)(parts[i].x+0.5f);
			y = (int)(parts[i].y+0.5f);
			if(x >= fullX && x <= fullX+fullW && y >= fullY && y <= fullY+fullH)
			{
				unsigned char fieldDesc = 0;
				int fieldDescLoc = 0, tempTemp, vTemp;
				
				//Type (required)
				partsData[partsDataLen++] = partsptr[i].type;
				
				//X and Y coord (required), 2 bytes each
				partsData[partsDataLen++] = x;
				partsData[partsDataLen++] = x >> 8;
				printf("Saved: %d, %d", (char)x, (char)(x>>8));
				partsData[partsDataLen++] = y;
				partsData[partsDataLen++] = y >> 8;
				
				//Temperature (required), 2 bytes
				tempTemp = partsptr[i].temp;
				partsData[partsDataLen++] = tempTemp;
				partsData[partsDataLen++] = tempTemp >> 8;
				
				//Location of the field descriptor
				fieldDescLoc = partsDataLen++;
				
				//Life (optional), 1 to 2 bytes
				if(partsptr[i].life)
				{
					fieldDesc |= 1;
					partsData[partsDataLen++] = partsptr[i].life;
					if(partsptr[i].life > 255)
					{
						fieldDesc |= 1 << 1;
						partsData[partsDataLen++] = partsptr[i].life >> 8;
					}
				}
				
				//Tmp (optional), 1 to 2 bytes
				if(partsptr[i].tmp)
				{
					fieldDesc |= 1 << 2;
					partsData[partsDataLen++] = partsptr[i].tmp;
					if(partsptr[i].tmp > 255)
					{
						fieldDesc |= 1 << 3;
						partsData[partsDataLen++] = partsptr[i].tmp >> 8;
					}
				}
				
				//Ctype (optional), 1 byte
				if(partsptr[i].ctype)
				{
					fieldDesc |= 1 << 4;
					partsData[partsDataLen++] = partsptr[i].ctype;
				}
				
				//Dcolour (optional), 4 bytes
				if(partsptr[i].dcolour && (partsptr[i].dcolour & 0xFF000000))
				{
					fieldDesc |= 1 << 5;
					partsData[partsDataLen++] = (partsptr[i].dcolour&0xFF000000)>>24;
					partsData[partsDataLen++] = (partsptr[i].dcolour&0x00FF0000)>>16;
					partsData[partsDataLen++] = (partsptr[i].dcolour&0x0000FF00)>>8;
					partsData[partsDataLen++] = (partsptr[i].dcolour&0x000000FF);
				}
				
				//VX (optional), 1 byte
				if(fabs(partsptr[i].vx) > 0.001f)
				{
					fieldDesc |= 1 << 6;
					vTemp = (int)(partsptr[i].vx*16.0f+127.5f);
					if (vTemp<0) vTemp=0;
					if (vTemp>255) vTemp=255;
					partsData[partsDataLen++] = vTemp;
				}
				
				//VY (optional), 1 byte
				if(fabs(partsptr[i].vy) > 0.001f)
				{
					fieldDesc |= 1 << 7;
					vTemp = (int)(partsptr[i].vy*16.0f+127.5f);
					if (vTemp<0) vTemp=0;
					if (vTemp>255) vTemp=255;
					partsData[partsDataLen++] = vTemp;
				}
				
				//Write the field descriptor;
				partsData[fieldDescLoc] = fieldDesc;
			}
		}
	}
	
	bson b;
	bson_init(&b);
	/* These fields are in the "outer" header, don't bother saving here
	bson_append_int(&b, "majorVersion", SAVE_VERSION);
	bson_append_int(&b, "xRes", fullW);
	bson_append_int(&b, "yRes", fullH);
	bson_append_int(&b, "cellSize", CELL);*/
	//Save stuff like gravity, heat, blah states
	bson_append_int(&b, "partsDataBytes", partsDataLen);	//For debugging, remove eventually
	if(partsData)
		bson_append_binary(&b, "parts", BSON_BIN_USER, partsData, partsDataLen);
	if(wallData)
		bson_append_binary(&b, "wallMap", BSON_BIN_USER, wallData, wallDataLen);
	if(fanData)
		bson_append_binary(&b, "fanMap", BSON_BIN_USER, fanData, fanDataLen);
	bson_finish(&b);
	bson_print(&b);
	
	finalData = bson_data(&b);
	finalDataLen = bson_size(&b);
	outputDataLen = finalDataLen*2+12;
	outputData = malloc(outputDataLen);

	outputData[0] = 'O';
	outputData[1] = 'P';
	outputData[2] = 'S';
	outputData[3] = '1';
	outputData[4] = SAVE_VERSION;
	outputData[5] = CELL;
	outputData[6] = blockW;
	outputData[7] = blockH;
	outputData[8] = finalDataLen;
	outputData[9] = finalDataLen >> 8;
	outputData[10] = finalDataLen >> 16;
	outputData[11] = finalDataLen >> 24;
	
	if (BZ2_bzBuffToBuffCompress(outputData+12, &outputDataLen, finalData, bson_size(&b), 9, 0, 0) != BZ_OK)
	{
		puts("Save Error\n");
		free(outputData);
		*size = 0;
		outputData = NULL;
		goto fin;
	}
	
	*size = outputDataLen + 12;
	
fin:
	bson_destroy(&b);
	if(partsData)
		free(partsData);
	if(wallData)
		free(wallData);
	if(fanData)
		free(fanData);
	
	return outputData;
}

int parse_save_OPS(void *save, int size, int replace, int x0, int y0, unsigned char bmap[YRES/CELL][XRES/CELL], float vx[YRES/CELL][XRES/CELL], float vy[YRES/CELL][XRES/CELL], float pv[YRES/CELL][XRES/CELL], float fvx[YRES/CELL][XRES/CELL], float fvy[YRES/CELL][XRES/CELL], sign signs[MAXSIGNS], void* o_partsptr, unsigned pmap[YRES][XRES])
{
	particle *partsptr = o_partsptr;
	unsigned char * inputData = save, *bsonData = NULL, *partsData = NULL, *fanData = NULL, *wallData = NULL;
	int inputDataLen = size, bsonDataLen = 0, partsDataLen, fanDataLen, wallDataLen;
	int i, freeIndicesCount, x, y, returnCode = 0;
	int *freeIndices = NULL;
	
	bsonDataLen = ((unsigned)inputData[8]);
	bsonDataLen |= ((unsigned)inputData[9]) << 8;
	bsonDataLen |= ((unsigned)inputData[10]) << 16;
	bsonDataLen |= ((unsigned)inputData[11]) << 24;
	
	bsonData = malloc(bsonDataLen);
	if(!bsonData)
	{
		fprintf(stderr, "Internal error while parsing save: could not allocate buffer\n");
		return 3;
	}
	
	if (BZ2_bzBuffToBuffDecompress(bsonData, &bsonDataLen, inputData+12, inputDataLen-12, 0, 0))
		return 1;
	
	bson b;
	bson_iterator iter;
	bson_init_data(&b, bsonData);
	bson_iterator_init(&iter, &b);
	while(bson_iterator_more(&iter))
	{
		bson_iterator_next(&iter);
		if(strcmp(bson_iterator_key(&iter), "parts")==0)
		{
			if(bson_iterator_type(&iter)==BSON_BINDATA && ((unsigned char)bson_iterator_bin_type(&iter))==BSON_BIN_USER && (partsDataLen = bson_iterator_bin_len(&iter)) > 0)
			{
				partsData = bson_iterator_bin_data(&iter);
			}
			else
			{
				fprintf(stderr, "Invalid datatype of particle data: %d[%d] %d[%d] %d[%d]\n", bson_iterator_type(&iter), bson_iterator_type(&iter)==BSON_BINDATA, (unsigned char)bson_iterator_bin_type(&iter), ((unsigned char)bson_iterator_bin_type(&iter))==BSON_BIN_USER, bson_iterator_bin_len(&iter), bson_iterator_bin_len(&iter)>0);
			}
		}
	}
	
	//Read particle data
	if(partsData)
	{
		int newIndex = 0, fieldDescriptor, tempTemp;
		puts("Have particle data");
		parts_lastActiveIndex = NPART-1;
		freeIndicesCount = 0;
		freeIndices = calloc(sizeof(int), NPART);
		for (i = 0; i<NPART; i++)
		{
			//Ensure ALL parts (even photons) are in the pmap so we can overwrite, keep a track of indices we can use
			if (partsptr[i].type)
			{
				x = (int)(partsptr[i].x+0.5f);
				y = (int)(partsptr[i].y+0.5f);
				pmap[y][x] = (i<<8)|1;
			}
			else
				freeIndices[freeIndicesCount++] = i;
		}
		i = 0;
		//i+7 because we have 8 bytes of required fields (type (1), x (2), y (2), temp (2), descriptor (1))
		while(i+7 < partsDataLen)
		{
			x = partsData[i+1] | (((unsigned)partsData[i+2])<<8);
			y = partsData[i+3] | (((unsigned)partsData[i+4])<<8);
			fieldDescriptor = partsData[i+7];
			if(x >= XRES || x < 0 || y >= YRES || y < 0)
			{
				fprintf(stderr, "Out of range [%d]: %d %d, [%d, %d], [%d, %d]\n", i, x, y, (unsigned)partsData[i+1], (unsigned)partsData[i+2], (unsigned)partsData[i+3], (unsigned)partsData[i+4]);
				goto fail;
			}
			if(partsData[i] > NPART)
				partsData[i+1] = PT_DMND;	//Replace all invalid powders with diamond
			if(pmap[y][x])
			{
				//Replace existing particle or allocated block
				newIndex = pmap[y][x]>>8;
			}
			else if(freeIndicesCount)
			{
				//Create new particle
				newIndex = freeIndices[--freeIndicesCount];
			}
			else
			{
				//Nowhere to put new particle, tpt is sad :(
				break;
			}
			if(newIndex < 0 || newIndex >= NPART)
				goto fail;
				
			//Clear the particle, ready for our new properties
			memset(&(partsptr[newIndex]), 0, sizeof(particle));
			
			//Required fields
			partsptr[newIndex].type = partsData[i];
			partsptr[newIndex].x = x;
			partsptr[newIndex].y = y;
			partsptr[newIndex].temp = (partsData[i+5] | (partsData[i+6]<<8));
			i+=8;
			
			//Read life
			if(fieldDescriptor & 0x01)
			{
				if(i >= partsDataLen) goto fail;
				partsptr[newIndex].life = partsData[i++];
				//Read 2nd byte
				if(fieldDescriptor & 0x02)
				{
					if(i >= partsDataLen) goto fail;
					partsptr[newIndex].life |= partsData[i++];
				}
			}
			
			//Read tmp
			if(fieldDescriptor & 0x04)
			{
				if(i >= partsDataLen) goto fail;
				partsptr[newIndex].tmp = partsData[i++];
				//Read 2nd byte
				if(fieldDescriptor & 0x08)
				{
					if(i >= partsDataLen) goto fail;
					partsptr[newIndex].tmp |= partsData[i++];
				}
			}
			
			//Read ctype
			if(fieldDescriptor & 0x10)
			{
				if(i >= partsDataLen) goto fail;
				partsptr[newIndex].ctype = partsData[i++];
			}
			
			//Read dcolour
			if(fieldDescriptor & 0x20)
			{
				if(i+3 >= partsDataLen) goto fail;
				partsptr[newIndex].dcolour = partsData[i++];
				partsptr[newIndex].dcolour = partsData[i++];
				partsptr[newIndex].dcolour = partsData[i++];
				partsptr[newIndex].dcolour = partsData[i++];
			}
			
			//Read vx
			if(fieldDescriptor & 0x40)
			{
				if(i >= partsDataLen) goto fail;
				partsptr[newIndex].vx = (partsData[i++]-127.0f)/16.0f;
			}
			
			//Read vy
			if(fieldDescriptor & 0x80)
			{
				if(i >= partsDataLen) goto fail;
				partsptr[newIndex].vy = (partsData[i++]-127.0f)/16.0f;
			}
		}
	}
	goto fin;
fail:
	//Clean up everything
	returnCode = 1;
fin:
	bson_destroy(&b);
	if(freeIndices)
		free(freeIndices);
	return returnCode;
}

//the old saving function
void *build_save_PSv(int *size, int orig_x0, int orig_y0, int orig_w, int orig_h, unsigned char bmap[YRES/CELL][XRES/CELL], float fvx[YRES/CELL][XRES/CELL], float fvy[YRES/CELL][XRES/CELL], sign signs[MAXSIGNS], void* partsptr)
{
	unsigned char *d=calloc(1,3*(XRES/CELL)*(YRES/CELL)+(XRES*YRES)*15+MAXSIGNS*262), *c;
	int i,j,x,y,p=0,*m=calloc(XRES*YRES, sizeof(int));
	int x0, y0, w, h, bx0=orig_x0/CELL, by0=orig_y0/CELL, bw, bh;
	particle *parts = partsptr;
	bw=(orig_w+orig_x0-bx0*CELL+CELL-1)/CELL;
	bh=(orig_h+orig_y0-by0*CELL+CELL-1)/CELL;

	// normalize coordinates
	x0 = bx0*CELL;
	y0 = by0*CELL;
	w  = bw *CELL;
	h  = bh *CELL;

	// save the required air state
	for (y=by0; y<by0+bh; y++)
		for (x=bx0; x<bx0+bw; x++)
			d[p++] = bmap[y][x];
	for (y=by0; y<by0+bh; y++)
		for (x=bx0; x<bx0+bw; x++)
			if (bmap[y][x]==WL_FAN||bmap[y][x]==4)
			{
				i = (int)(fvx[y][x]*64.0f+127.5f);
				if (i<0) i=0;
				if (i>255) i=255;
				d[p++] = i;
			}
	for (y=by0; y<by0+bh; y++)
		for (x=bx0; x<bx0+bw; x++)
			if (bmap[y][x]==WL_FAN||bmap[y][x]==4)
			{
				i = (int)(fvy[y][x]*64.0f+127.5f);
				if (i<0) i=0;
				if (i>255) i=255;
				d[p++] = i;
			}

	// save the particle map
	for (i=0; i<NPART; i++)
		if (parts[i].type)
		{
			x = (int)(parts[i].x+0.5f);
			y = (int)(parts[i].y+0.5f);
			if (x>=orig_x0 && x<orig_x0+orig_w && y>=orig_y0 && y<orig_y0+orig_h) {
				if (!m[(x-x0)+(y-y0)*w] ||
				        parts[m[(x-x0)+(y-y0)*w]-1].type == PT_PHOT ||
				        parts[m[(x-x0)+(y-y0)*w]-1].type == PT_NEUT)
					m[(x-x0)+(y-y0)*w] = i+1;
			}
		}
	for (j=0; j<w*h; j++)
	{
		i = m[j];
		if (i)
			d[p++] = parts[i-1].type;
		else
			d[p++] = 0;
	}

	// save particle properties
	for (j=0; j<w*h; j++)
	{
		i = m[j];
		if (i)
		{
			i--;
			x = (int)(parts[i].vx*16.0f+127.5f);
			y = (int)(parts[i].vy*16.0f+127.5f);
			if (x<0) x=0;
			if (x>255) x=255;
			if (y<0) y=0;
			if (y>255) y=255;
			d[p++] = x;
			d[p++] = y;
		}
	}
	for (j=0; j<w*h; j++)
	{
		i = m[j];
		if (i) {
			//Everybody loves a 16bit int
			//d[p++] = (parts[i-1].life+3)/4;
			int ttlife = (int)parts[i-1].life;
			d[p++] = ((ttlife&0xFF00)>>8);
			d[p++] = (ttlife&0x00FF);
		}
	}
	for (j=0; j<w*h; j++)
	{
		i = m[j];
		if (i) {
			//Now saving tmp!
			//d[p++] = (parts[i-1].life+3)/4;
			int tttmp = (int)parts[i-1].tmp;
			d[p++] = ((tttmp&0xFF00)>>8);
			d[p++] = (tttmp&0x00FF);
		}
	}
	for (j=0; j<w*h; j++)
	{
		i = m[j];
		if (i && (parts[i-1].type==PT_PBCN)) {
			//Save tmp2
			d[p++] = parts[i-1].tmp2;
		}
	}
	for (j=0; j<w*h; j++)
	{
		i = m[j];
		if (i) {
			//Save colour (ALPHA)
			d[p++] = (parts[i-1].dcolour&0xFF000000)>>24;
		}
	}
	for (j=0; j<w*h; j++)
	{
		i = m[j];
		if (i) {
			//Save colour (RED)
			d[p++] = (parts[i-1].dcolour&0x00FF0000)>>16;
		}
	}
	for (j=0; j<w*h; j++)
	{
		i = m[j];
		if (i) {
			//Save colour (GREEN)
			d[p++] = (parts[i-1].dcolour&0x0000FF00)>>8;
		}
	}
	for (j=0; j<w*h; j++)
	{
		i = m[j];
		if (i) {
			//Save colour (BLUE)
			d[p++] = (parts[i-1].dcolour&0x000000FF);
		}
	}
	for (j=0; j<w*h; j++)
	{
		i = m[j];
		if (i)
		{
			//New Temperature saving uses a 16bit unsigned int for temperatures, giving a precision of 1 degree versus 36 for the old format
			int tttemp = (int)parts[i-1].temp;
			d[p++] = ((tttemp&0xFF00)>>8);
			d[p++] = (tttemp&0x00FF);
		}
	}
	for (j=0; j<w*h; j++)
	{
		i = m[j];
		if (i && (parts[i-1].type==PT_CLNE || parts[i-1].type==PT_PCLN || parts[i-1].type==PT_BCLN || parts[i-1].type==PT_SPRK || parts[i-1].type==PT_LAVA || parts[i-1].type==PT_PIPE || parts[i-1].type==PT_LIFE || parts[i-1].type==PT_PBCN || parts[i-1].type==PT_WIRE || parts[i-1].type==PT_STOR || parts[i-1].type==PT_CONV))
			d[p++] = parts[i-1].ctype;
	}

	j = 0;
	for (i=0; i<MAXSIGNS; i++)
		if (signs[i].text[0] &&
		        signs[i].x>=x0 && signs[i].x<x0+w &&
		        signs[i].y>=y0 && signs[i].y<y0+h)
			j++;
	d[p++] = j;
	for (i=0; i<MAXSIGNS; i++)
		if (signs[i].text[0] &&
		        signs[i].x>=x0 && signs[i].x<x0+w &&
		        signs[i].y>=y0 && signs[i].y<y0+h)
		{
			d[p++] = (signs[i].x-x0);
			d[p++] = (signs[i].x-x0)>>8;
			d[p++] = (signs[i].y-y0);
			d[p++] = (signs[i].y-y0)>>8;
			d[p++] = signs[i].ju;
			x = strlen(signs[i].text);
			d[p++] = x;
			memcpy(d+p, signs[i].text, x);
			p+=x;
		}

	i = (p*101+99)/100 + 612;
	c = malloc(i);

	//New file header uses PSv, replacing fuC. This is to detect if the client uses a new save format for temperatures
	//This creates a problem for old clients, that display and "corrupt" error instead of a "newer version" error

	c[0] = 0x50;	//0x66;
	c[1] = 0x53;	//0x75;
	c[2] = 0x76;	//0x43;
	c[3] = legacy_enable|((sys_pause<<1)&0x02)|((gravityMode<<2)&0x0C)|((airMode<<4)&0x70)|((ngrav_enable<<7)&0x80);
	c[4] = SAVE_VERSION;
	c[5] = CELL;
	c[6] = bw;
	c[7] = bh;
	c[8] = p;
	c[9] = p >> 8;
	c[10] = p >> 16;
	c[11] = p >> 24;

	i -= 12;

	if (BZ2_bzBuffToBuffCompress((char *)(c+12), (unsigned *)&i, (char *)d, p, 9, 0, 0) != BZ_OK)
	{
		free(d);
		free(c);
		free(m);
		return NULL;
	}
	free(d);
	free(m);

	*size = i+12;
	return c;
}

int parse_save_PSv(void *save, int size, int replace, int x0, int y0, unsigned char bmap[YRES/CELL][XRES/CELL], float fvx[YRES/CELL][XRES/CELL], float fvy[YRES/CELL][XRES/CELL], sign signs[MAXSIGNS], void* partsptr, unsigned pmap[YRES][XRES])
{
	unsigned char *d=NULL,*c=save;
	int q,i,j,k,x,y,p=0,*m=NULL, ver, pty, ty, legacy_beta=0, tempGrav = 0;
	int bx0=x0/CELL, by0=y0/CELL, bw, bh, w, h;
	int nf=0, new_format = 0, ttv = 0;
	particle *parts = partsptr;
	int *fp = malloc(NPART*sizeof(int));

	//New file header uses PSv, replacing fuC. This is to detect if the client uses a new save format for temperatures
	//This creates a problem for old clients, that display and "corrupt" error instead of a "newer version" error

	if (size<16)
		return 1;
	if (!(c[2]==0x43 && c[1]==0x75 && c[0]==0x66) && !(c[2]==0x76 && c[1]==0x53 && c[0]==0x50))
		return 1;
	if (c[2]==0x76 && c[1]==0x53 && c[0]==0x50) {
		new_format = 1;
	}
	if (c[4]>SAVE_VERSION)
		return 2;
	ver = c[4];

	if (ver<34)
	{
		legacy_enable = 1;
	}
	else
	{
		if (ver>=44) {
			legacy_enable = c[3]&0x01;
			if (!sys_pause) {
				sys_pause = (c[3]>>1)&0x01;
			}
			if (ver>=46 && replace) {
				gravityMode = ((c[3]>>2)&0x03);// | ((c[3]>>2)&0x01);
				airMode = ((c[3]>>4)&0x07);// | ((c[3]>>4)&0x02) | ((c[3]>>4)&0x01);
			}
			if (ver>=49 && replace) {
				tempGrav = ((c[3]>>7)&0x01);		
			}
		} else {
			if (c[3]==1||c[3]==0) {
				legacy_enable = c[3];
			} else {
				legacy_beta = 1;
			}
		}
	}

	bw = c[6];
	bh = c[7];
	if (bx0+bw > XRES/CELL)
		bx0 = XRES/CELL - bw;
	if (by0+bh > YRES/CELL)
		by0 = YRES/CELL - bh;
	if (bx0 < 0)
		bx0 = 0;
	if (by0 < 0)
		by0 = 0;

	if (c[5]!=CELL || bx0+bw>XRES/CELL || by0+bh>YRES/CELL)
		return 3;
	i = (unsigned)c[8];
	i |= ((unsigned)c[9])<<8;
	i |= ((unsigned)c[10])<<16;
	i |= ((unsigned)c[11])<<24;
	d = malloc(i);
	if (!d)
		return 1;

	if (BZ2_bzBuffToBuffDecompress((char *)d, (unsigned *)&i, (char *)(c+12), size-12, 0, 0))
		return 1;
	size = i;

	if (size < bw*bh)
		return 1;

	// normalize coordinates
	x0 = bx0*CELL;
	y0 = by0*CELL;
	w  = bw *CELL;
	h  = bh *CELL;

	if (replace)
	{
		if (ver<46) {
			gravityMode = 0;
			airMode = 0;
		}
		clear_sim();
	}
	parts_lastActiveIndex = NPART-1;
	m = calloc(XRES*YRES, sizeof(int));

	// make a catalog of free parts
	//memset(pmap, 0, sizeof(pmap)); "Using sizeof for array given as function argument returns the size of pointer."
	memset(pmap, 0, sizeof(unsigned)*(XRES*YRES));
	for (i=0; i<NPART; i++)
		if (parts[i].type)
		{
			x = (int)(parts[i].x+0.5f);
			y = (int)(parts[i].y+0.5f);
			pmap[y][x] = (i<<8)|1;
		}
		else
			fp[nf++] = i;

	// load the required air state
	for (y=by0; y<by0+bh; y++)
		for (x=bx0; x<bx0+bw; x++)
		{
			if (d[p])
			{
				bmap[y][x] = d[p];
				if (bmap[y][x]==1)
					bmap[y][x]=WL_WALL;
				if (bmap[y][x]==2)
					bmap[y][x]=WL_DESTROYALL;
				if (bmap[y][x]==3)
					bmap[y][x]=WL_ALLOWLIQUID;
				if (bmap[y][x]==4)
					bmap[y][x]=WL_FAN;
				if (bmap[y][x]==5)
					bmap[y][x]=WL_STREAM;
				if (bmap[y][x]==6)
					bmap[y][x]=WL_DETECT;
				if (bmap[y][x]==7)
					bmap[y][x]=WL_EWALL;
				if (bmap[y][x]==8)
					bmap[y][x]=WL_WALLELEC;
				if (bmap[y][x]==9)
					bmap[y][x]=WL_ALLOWAIR;
				if (bmap[y][x]==10)
					bmap[y][x]=WL_ALLOWSOLID;
				if (bmap[y][x]==11)
					bmap[y][x]=WL_ALLOWALLELEC;
				if (bmap[y][x]==12)
					bmap[y][x]=WL_EHOLE;
				if (bmap[y][x]==13)
					bmap[y][x]=WL_ALLOWGAS;
			}

			p++;
		}
	for (y=by0; y<by0+bh; y++)
		for (x=bx0; x<bx0+bw; x++)
			if (d[(y-by0)*bw+(x-bx0)]==4||d[(y-by0)*bw+(x-bx0)]==WL_FAN)
			{
				if (p >= size)
					goto corrupt;
				fvx[y][x] = (d[p++]-127.0f)/64.0f;
			}
	for (y=by0; y<by0+bh; y++)
		for (x=bx0; x<bx0+bw; x++)
			if (d[(y-by0)*bw+(x-bx0)]==4||d[(y-by0)*bw+(x-bx0)]==WL_FAN)
			{
				if (p >= size)
					goto corrupt;
				fvy[y][x] = (d[p++]-127.0f)/64.0f;
			}

	// load the particle map
	i = 0;
	pty = p;
	for (y=y0; y<y0+h; y++)
		for (x=x0; x<x0+w; x++)
		{
			if (p >= size)
				goto corrupt;
			j=d[p++];
			if (j >= PT_NUM) {
				//TODO: Possibly some server side translation
				j = PT_DUST;//goto corrupt;
			}
			gol[x][y]=0;
			if (j)
			{
				if (pmap[y][x])
				{
					k = pmap[y][x]>>8;
				}
				else if (i<nf)
				{
					k = fp[i];
					i++;
				}
				else
				{
					m[(x-x0)+(y-y0)*w] = NPART+1;
					continue;
				}
				memset(parts+k, 0, sizeof(particle));
				parts[k].type = j;
				if (j == PT_COAL)
					parts[k].tmp = 50;
				if (j == PT_FUSE)
					parts[k].tmp = 50;
				if (j == PT_PHOT)
					parts[k].ctype = 0x3fffffff;
				if (j == PT_SOAP)
					parts[k].ctype = 0;
				if (j==PT_BIZR || j==PT_BIZRG || j==PT_BIZRS)
					parts[k].ctype = 0x47FFFF;
				parts[k].x = (float)x;
				parts[k].y = (float)y;
				m[(x-x0)+(y-y0)*w] = k+1;
			}
		}

	// load particle properties
	for (j=0; j<w*h; j++)
	{
		i = m[j];
		if (i)
		{
			i--;
			if (p+1 >= size)
				goto corrupt;
			if (i < NPART)
			{
				parts[i].vx = (d[p++]-127.0f)/16.0f;
				parts[i].vy = (d[p++]-127.0f)/16.0f;
			}
			else
				p += 2;
		}
	}
	for (j=0; j<w*h; j++)
	{
		i = m[j];
		if (i)
		{
			if (ver>=44) {
				if (p >= size) {
					goto corrupt;
				}
				if (i <= NPART) {
					ttv = (d[p++])<<8;
					ttv |= (d[p++]);
					parts[i-1].life = ttv;
				} else {
					p+=2;
				}
			} else {
				if (p >= size)
					goto corrupt;
				if (i <= NPART)
					parts[i-1].life = d[p++]*4;
				else
					p++;
			}
		}
	}
	if (ver>=44) {
		for (j=0; j<w*h; j++)
		{
			i = m[j];
			if (i)
			{
				if (p >= size) {
					goto corrupt;
				}
				if (i <= NPART) {
					ttv = (d[p++])<<8;
					ttv |= (d[p++]);
					parts[i-1].tmp = ttv;
					if (ver<53 && !parts[i-1].tmp)
						for (q = 1; q<=NGOLALT; q++) {
							if (parts[i-1].type==goltype[q-1] && grule[q][9]==2)
								parts[i-1].tmp = grule[q][9]-1;
						}
					if (ver>=51 && ver<53 && parts[i-1].type==PT_PBCN)
					{
						parts[i-1].tmp2 = parts[i-1].tmp;
						parts[i-1].tmp = 0;
					}
				} else {
					p+=2;
				}
			}
		}
	}
	if (ver>=53) {
		for (j=0; j<w*h; j++)
		{
			i = m[j];
			ty = d[pty+j];
			if (i && ty==PT_PBCN)
			{
				if (p >= size)
					goto corrupt;
				if (i <= NPART)
					parts[i-1].tmp2 = d[p++];
				else
					p++;
			}
		}
	}
	//Read ALPHA component
	for (j=0; j<w*h; j++)
	{
		i = m[j];
		if (i)
		{
			if (ver>=49) {
				if (p >= size) {
					goto corrupt;
				}
				if (i <= NPART) {
					parts[i-1].dcolour = d[p++]<<24;
				} else {
					p++;
				}
			}
		}
	}
	//Read RED component
	for (j=0; j<w*h; j++)
	{
		i = m[j];
		if (i)
		{
			if (ver>=49) {
				if (p >= size) {
					goto corrupt;
				}
				if (i <= NPART) {
					parts[i-1].dcolour |= d[p++]<<16;
				} else {
					p++;
				}
			}
		}
	}
	//Read GREEN component
	for (j=0; j<w*h; j++)
	{
		i = m[j];
		if (i)
		{
			if (ver>=49) {
				if (p >= size) {
					goto corrupt;
				}
				if (i <= NPART) {
					parts[i-1].dcolour |= d[p++]<<8;
				} else {
					p++;
				}
			}
		}
	}
	//Read BLUE component
	for (j=0; j<w*h; j++)
	{
		i = m[j];
		if (i)
		{
			if (ver>=49) {
				if (p >= size) {
					goto corrupt;
				}
				if (i <= NPART) {
					parts[i-1].dcolour |= d[p++];
				} else {
					p++;
				}
			}
		}
	}
	for (j=0; j<w*h; j++)
	{
		i = m[j];
		ty = d[pty+j];
		if (i)
		{
			if (ver>=34&&legacy_beta==0)
			{
				if (p >= size)
				{
					goto corrupt;
				}
				if (i <= NPART)
				{
					if (ver>=42) {
						if (new_format) {
							ttv = (d[p++])<<8;
							ttv |= (d[p++]);
							if (parts[i-1].type==PT_PUMP) {
								parts[i-1].temp = ttv + 0.15;//fix PUMP saved at 0, so that it loads at 0.
							} else {
								parts[i-1].temp = ttv;
							}
						} else {
							parts[i-1].temp = (d[p++]*((MAX_TEMP+(-MIN_TEMP))/255))+MIN_TEMP;
						}
					} else {
						parts[i-1].temp = ((d[p++]*((O_MAX_TEMP+(-O_MIN_TEMP))/255))+O_MIN_TEMP)+273;
					}
				}
				else
				{
					p++;
					if (new_format) {
						p++;
					}
				}
			}
			else
			{
				parts[i-1].temp = ptypes[parts[i-1].type].heat;
			}
		}
	} 
	for (j=0; j<w*h; j++)
	{
		int gnum = 0;
		i = m[j];
		ty = d[pty+j];
		if (i && (ty==PT_CLNE || (ty==PT_PCLN && ver>=43) || (ty==PT_BCLN && ver>=44) || (ty==PT_SPRK && ver>=21) || (ty==PT_LAVA && ver>=34) || (ty==PT_PIPE && ver>=43) || (ty==PT_LIFE && ver>=51) || (ty==PT_PBCN && ver>=52) || (ty==PT_WIRE && ver>=55) || (ty==PT_STOR && ver>=59) || (ty==PT_CONV && ver>=60)))
		{
			if (p >= size)
				goto corrupt;
			if (i <= NPART)
				parts[i-1].ctype = d[p++];
			else
				p++;
		}
		// no more particle properties to load, so we can change type here without messing up loading
		if (i && i<=NPART)
		{
			if ((player.spwn == 1 && ty==PT_STKM) || (player2.spwn == 1 && ty==PT_STKM2))
			{
				parts[i-1].type = PT_NONE;
			}
			else if (parts[i-1].type == PT_STKM)
			{
				STKM_init_legs(&player, i-1);
				player.spwn = 1;
				player.elem = PT_DUST;
			}
			else if (parts[i-1].type == PT_STKM2)
			{
				STKM_init_legs(&player2, i-1);
				player2.spwn = 1;
				player2.elem = PT_DUST;
			}
			else if (parts[i-1].type == PT_FIGH)
			{
				unsigned char fcount = 0;
				while (fcount < 100 && fcount < (fighcount+1) && fighters[fcount].spwn==1) fcount++;
				if (fcount < 100 && fighters[fcount].spwn==0)
				{
					parts[i-1].tmp = fcount;
					fighters[fcount].spwn = 1;
					fighters[fcount].elem = PT_DUST;
					fighcount++;
					STKM_init_legs(&(fighters[fcount]), i-1);
				}
			}

			if (ver<48 && (ty==OLD_PT_WIND || (ty==PT_BRAY&&parts[i-1].life==0)))
			{
				// Replace invisible particles with something sensible and add decoration to hide it
				x = (int)(parts[i-1].x+0.5f);
				y = (int)(parts[i-1].y+0.5f);
				parts[i-1].dcolour = 0xFF000000;
				parts[i-1].type = PT_DMND;
			}
			if(ver<51 && ((ty>=78 && ty<=89) || (ty>=134 && ty<=146 && ty!=141))){
				//Replace old GOL
				parts[i-1].type = PT_LIFE;
				for (gnum = 0; gnum<NGOLALT; gnum++){
					if (ty==goltype[gnum])
						parts[i-1].ctype = gnum;
				}
				ty = PT_LIFE;
			}
			if(ver<52 && (ty==PT_CLNE || ty==PT_PCLN || ty==PT_BCLN)){
				//Replace old GOL ctypes in clone
				for (gnum = 0; gnum<NGOLALT; gnum++){
					if (parts[i-1].ctype==goltype[gnum])
					{
						parts[i-1].ctype = PT_LIFE;
						parts[i-1].tmp = gnum;
					}
				}
			}
			if(ty==PT_LCRY){
				if(ver<67)
				{
					//New LCRY uses TMP not life
					if(parts[i-1].life>=10)
					{
						parts[i-1].life = 10;
						parts[i-1].tmp2 = 10;
						parts[i-1].tmp = 3;
					}
					else if(parts[i-1].life<=0)
					{
						parts[i-1].life = 0;
						parts[i-1].tmp2 = 0;
						parts[i-1].tmp = 0;
					}
					else if(parts[i-1].life < 10 && parts[i-1].life > 0)
					{
						parts[i-1].tmp = 1;
					}
				}
				else
				{
					parts[i-1].tmp2 = parts[i-1].life;
				}
			}
			if (!ptypes[parts[i-1].type].enabled)
				parts[i-1].type = PT_NONE;
		}
	}

	#ifndef RENDERER
	//Change the gravity state
	if(ngrav_enable != tempGrav && replace)
	{
		if(tempGrav)
			start_grav_async();
		else
			stop_grav_async();
	}
	#endif
	
	gravity_mask();

	if (p >= size)
		goto version1;
	j = d[p++];
	for (i=0; i<j; i++)
	{
		if (p+6 > size)
			goto corrupt;
		for (k=0; k<MAXSIGNS; k++)
			if (!signs[k].text[0])
				break;
		x = d[p++];
		x |= ((unsigned)d[p++])<<8;
		if (k<MAXSIGNS)
			signs[k].x = x+x0;
		x = d[p++];
		x |= ((unsigned)d[p++])<<8;
		if (k<MAXSIGNS)
			signs[k].y = x+y0;
		x = d[p++];
		if (k<MAXSIGNS)
			signs[k].ju = x;
		x = d[p++];
		if (p+x > size)
			goto corrupt;
		if (k<MAXSIGNS)
		{
			memcpy(signs[k].text, d+p, x);
			signs[k].text[x] = 0;
			clean_text(signs[k].text, 158-14 /* Current max sign length */);
		}
		p += x;
	}

version1:
	if (m) free(m);
	if (d) free(d);
	if (fp) free(fp);

	return 0;

corrupt:
	if (m) free(m);
	if (d) free(d);
	if (fp) free(fp);
	if (replace)
	{
		legacy_enable = 0;
		clear_sim();
	}
	return 1;
}

void *build_thumb(int *size, int bzip2)
{
	unsigned char *d=calloc(1,XRES*YRES), *c;
	int i,j,x,y;
	for (i=0; i<NPART; i++)
		if (parts[i].type)
		{
			x = (int)(parts[i].x+0.5f);
			y = (int)(parts[i].y+0.5f);
			if (x>=0 && x<XRES && y>=0 && y<YRES)
				d[x+y*XRES] = parts[i].type;
		}
	for (y=0; y<YRES/CELL; y++)
		for (x=0; x<XRES/CELL; x++)
			if (bmap[y][x])
				for (j=0; j<CELL; j++)
					for (i=0; i<CELL; i++)
						d[x*CELL+i+(y*CELL+j)*XRES] = 0xFF;
	j = XRES*YRES;

	if (bzip2)
	{
		i = (j*101+99)/100 + 608;
		c = malloc(i);

		c[0] = 0x53;
		c[1] = 0x68;
		c[2] = 0x49;
		c[3] = 0x74;
		c[4] = PT_NUM;
		c[5] = CELL;
		c[6] = XRES/CELL;
		c[7] = YRES/CELL;

		i -= 8;

		if (BZ2_bzBuffToBuffCompress((char *)(c+8), (unsigned *)&i, (char *)d, j, 9, 0, 0) != BZ_OK)
		{
			free(d);
			free(c);
			return NULL;
		}
		free(d);
		*size = i+8;
		return c;
	}

	*size = j;
	return d;
}