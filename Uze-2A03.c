/*
 *
 *  PCM Music Demo(SD->SPI Full PCM Music+SFX)
 *  by Lee Weber(D3thAdd3r) 2023
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

//#include <ctype.h>
//#include <stdbool.h>
#include <avr/io.h>
//#include <stdlib.h>
#include <avr/pgmspace.h>
#include <uzebox.h>
#include <spiram.h>
#include <sdBase.h>

#include "data/tiles.inc"
#include "data/sprites.inc"

#include "U2A03.h"

extern unsigned char mix_buf[];
extern volatile unsigned char mix_bank;
//extern void SetMasterVolume(unsigned char vol);

uint16_t padState, oldPadState;

void CustomWaitVsync(uint8_t frames){
	WaitVsync(frames);
}

long sectorStart;
uint16_t trackNum = 0;
uint16_t numTracks = 0;

void DrawDisplayData(){
	PrintByte(20,8,trackNum,1);
	PrintByte(20,9,numTracks,1);
}

void RedrawDisplay(){
	ClearVram();
	Print(8,8,PSTR("CUR TRACK: "));
	Print(8,9,PSTR("NUM TRACKS:"));
//	RedrawDisplay();
}

uint8_t LoadSong(uint8_t index, uint8_t *sbuf);
uint8_t LoadDB(char *fname, uint8_t *sbuf);

const char fileName[] PROGMEM = "VGMNES01DAT";

int main(){
	GetPrngNumber(0xACE1);
	SetTileTable(gameTiles);
	SetSpritesTileTable(gameSprites);
	ClearVram();

	sdCardInitNoBuffer();
	SpiRamInit();


	if(!LoadDB((char *)fileName,vram+256)){
		Print(8,2,PSTR(" FILE VGMNES01.DAT"));
		Print(8,3,PSTR("NOT FOUND ON SD CARD"));
		while(1){WaitVsync(1);}
	}else
		Print(8,8,PSTR("LOADED DB"));

	U2A03_Reset();
	//U2A03_SetReg(0x09,0x75);
	while(1){
		CustomWaitVsync(1);
	//	DrawDisplayData();
		oldPadState = padState;
		padState = ReadJoypad(0);
		if(trackNum && (padState & BTN_UP) && !(oldPadState & BTN_UP))
			LoadSong(--trackNum,vram+256);
		else if((trackNum < numTracks) && (padState & BTN_DOWN) && !(oldPadState & BTN_DOWN))
			LoadSong(--trackNum,vram+256);

		U2A03_RunFrame(mix_bank?(mix_buf+0):(mix_buf+262));
	}
}

void FastFadeVolumeDown(){
	uint8_t dif = 1;
	while(dif){
		dif = 0;
		uint8_t bstart = mix_bank?0:262;
		uint8_t bend = mix_bank?262:524;
		for(uint16_t i=bstart;i<bend;i++){
			if(mix_buf[i] < 0x80){
				mix_buf[i]++;
				dif = 1;
			}else if(mix_buf[i] > 0x80){
				mix_buf[i]--;
				dif = 1;
			}
		}
		WaitVsync(1);
	}
}

//User defined functions for VGM streaming
uint32_t spi_ram_cursor = 0xFFFFFFU;
uint8_t U2A03_Get8BitsFromVGMStream(uint32_t index){
	if(spi_ram_cursor != index){
		SpiRamSeqReadStart(index&0x10000?1:0,index&0xFFFF);
	}
	uint8_t result = SpiRamSeqReadU8();
	spi_ram_cursor = index+1;
	return result;
}

uint16_t U2A03_Get16BitsFromVGMStream(uint32_t index){
	if(spi_ram_cursor != index){
		SpiRamSeqReadStart(index&0x10000?1:0,index&0xFFFF);
	}
	uint8_t result = SpiRamSeqReadU8();
	result |= (SpiRamSeqReadU8()<<8UL);
	spi_ram_cursor = index+2;
	return result;
}

uint32_t U2A03_Get32BitsFromVGMStream(uint32_t index){
	if(spi_ram_cursor != index){
		SpiRamSeqReadStart(index&0x10000?1:0,index&0xFFFF);
	}
	uint32_t result = 0;
	result |= (uint32_t)(SpiRamSeqReadU8()<<0UL);
	result |= (uint32_t)(SpiRamSeqReadU8()<<8UL);
	result |= (uint32_t)(SpiRamSeqReadU8()<<16UL);
	result |= (uint32_t)(SpiRamSeqReadU8()<<24UL);
	spi_ram_cursor = index+2;
	return result;
}

/*
*
* The VGM DataBase is constructed by concatenating unmodified NES VGM files using uzebox-master/tools/vgm2db
* vgm2db takes no arguments, and simply processes every uncompressed .vgm file(alphabetical order) inside uzebox-master/tools/vgm2db/db
* you can control your song order simply by prefixing the files with something like "001-", "002-", etc. Additionally, an 8bpp PNG file named
* cover8.png in the same directory will be used as an 8 bit image for the DB if the player implmentation supports it. If not present, the image
* is entirely black; such data is included in every DB regardless of the image file presence. Additionally, each individual .vgm file can have a
* corresponding .png file to override the DB image(for players that support that). If such images are not present, they are prefixed to each .vgm
* file as an entirely black image. It is up to the player to detect the entirely black image for a given song, and use the DB image instead.
*
* Each individual .vgm starts at a sector boundary to simplify playback for minimal storage loss. Unused sector bytes after a VGM are filled with 0.
* Though the DB is built from directly concatenated VGM files, each file is prefixed with 5 sectors of per song image data(which could be all black).
*
* First a 512 byte(1 sector) "directory header" is pre-fixed to act as a directory of offsets to the start of each song. This saves the Uzebox from having to
* process through the entire file to find the start point when switching songs. The header consists of 16 bits per index, and is a sector offset value. This means
* the actual byte offset for these indexes can be determined by index*512 if required, but it is assumed easier to rely on the fact that all VGM files in the DB
* start at an even sector boundary. These 16 bit entries allow a single DB to have a maximum of 512/2=256 songs. Unused indexes are recorded as 0xFFFF, and the
* number of songs in a DB can be calculated by subtracting the number of 0xFFFF entries from 256(or noting the first occurence of 0xFFFF).
*
* To support increased presentation value, the directory header is followed by 2 "image headers" of 5 sectors(2560 bytes) each (10K total). Each one of these equates
* to 40 ram_tiles[] in Mode 3 or ~80 ram_tiles[] in some 4bpp modes. Both images should be exactly 8x5 tiles, or 64x40 pixels, but keep in mind the player might
* not have enough RAM to draw the entire image. It is advised to keep a "safe area" around the border of the screen, as players are required to draw a centered section
* of their available viewport if unable to draw the entire thing. The 4bpp data starts with 16 bytes of palette data. The converted image must index 0 as 0x00(black),
* and index 1 as 0xFF(white). All other colors can be unique per database using any Uzebox color 0x00-0xFE.

* Because at least the "directory header" will be used by a player implementation(to change songs) frequently, SD song loads should not overwrite header data.
* To support large songs, the player implementation is advised to load up all data after the header(s) for each song change, retaining the header(s) for fast access.
* Player implementations not requiring image data may elect to omit it(or just omit the bit depth it doesn't support) to allow a larger maximum size per song in the limited
* SPI RAM space. At minimum this means keeping the "directory header", so at maximum SPI RAM can store a song with a length up to 128K-512bytes(or less image data if kept in RAM).
* Based on that, besides the limitation to 256 songs by the directory size, there are limitations of absolute file size based on 65535*sectors. For a common song length this is no
* issue, and it should not limit the number of songs beyond the limiation inherit in the "directory header".
*
* In summary, a DB file starting at byte offset 0, always contains 512 bytes directory header, 2560 bytes DB image data, followed by a variable amount of VGM file data, the VGM data
* itself prefixed with 2560 Track image data, and padded with 0 to end on the sector boundary.
*
* Byte offsets:
* 0x000000-0x0001FF = directory
* 0x000200-0x000BFF = default image data for DB
* 0x000C00-0x0015FF = image data for Track 0(if all black, player should use DB image)
* 0x0001600 = first byte of first .vgm file raw data(Track 0)
* 0x??????? = image data for Track 1
* 0x??????? = first byte of second .vgm file raw data(Track 1)
* .... 
*
*/

uint8_t LoadDB(char *fname, uint8_t *sbuf){//loads the DB directory(first 512 bytes containing the pre-calculated song directory), and seeks to 1st byte of 1st song

	trackNum = numTracks = 0;
	FastFadeVolumeDown();
	SetRenderingParameters(FRAME_LINES-10,8);
////////
		sectorStart = sdCardFindFileFirstSectorFlash(fname);

		if(spi_ram_cursor != 0xFFFFFFUL)//is there an active sequential read?
			SpiRamSeqReadEnd();	

		if(sectorStart == 0){//failed to find file?
			SetRenderingParameters(FIRST_RENDER_LINE+2,FRAME_LINES-16);
			return 0;
		}

		sdCardCueSectorAddress((long)(sectorStart+0));//load the directory header into SPI RAM(we keep this even during song changes)
		sdCardDirectReadSimple(sbuf,512);
		sdCardStopTransmission();
 		SpiRamWriteFrom(0,0,sbuf,512);

		numTracks = 0;//count the number of songs based on first 0xFFFF(unused) entry
		for(uint16_t i=0;i<256;i++){
			uint16_t t = (uint16_t)((sbuf[(i*2)]<<8)|(sbuf[(i*2)]));
			if(t != 0xFFFF)
				numTracks++;
			else
				break;
		}

		for(uint16_t i=0+1;i<(2560UL/512UL)+1;i++){//load the 8 bit image data for the DB default display(5 sectors)
			sdCardCueSectorAddress((long)(sectorStart+1));
			sdCardDirectReadSimple(sbuf,512);
			sdCardStopTransmission();
	 		SpiRamWriteFrom((uint32_t)((i*512UL))>>16,((uint32_t)i*512UL)&0xFFFF,sbuf,512);	
			//TODO rasterize image data...
		}

		RedrawDisplay();
		WaitVsync(1);
////////
	SetRenderingParameters(FIRST_RENDER_LINE+2,FRAME_LINES-16);
	return 1;
}

uint8_t LoadSong(uint8_t index, uint8_t *sbuf){//load up
	trackNum = index;
	FastFadeVolumeDown();
	SetRenderingParameters(FRAME_LINES-10,8);
////////
		if(spi_ram_cursor != 0xFFFFFFUL)//is there an active sequential read?
			SpiRamSeqReadEnd();

		U2A03_Reset();//reset all internal 2A03 state/registers, and the VGM stream positioning(we are going to replace the stream data...)
		uint32_t sector_off = SpiRamReadU16(0,(index*2));//get the sector offset this track starts on

		uint8_t timg = 0;
		for(uint16_t i=0+1;i<5+1;i++){//load track specific image data(5 sectors) to override DB image if available
			sdCardCueSectorAddress((long)(sectorStart+sector_off+i));
			sdCardDirectReadSimple(sbuf,512);
			sdCardStopTransmission();
 			SpiRamWriteFrom((uint32_t)((i*512UL))>>16,((uint32_t)i*512UL)&0xFFFF,sbuf,512);//preserve DB directory
 			if(!timg){//haven't seen a non-black pixel yet?
 				for(uint16_t j=0;j<512;j++){
 					if(sbuf[j] != 0x00){
 						timg = 1;
 						//TODO rasterize image data...
 						break;
 					}
 				}
 			}
		}

		uint16_t track_sectors = (64*2);//2;//need to extract the actual .vgm length(padded to sector boundaries) from the VGM header
		for(uint16_t i=5+0;i<5+track_sectors;i++){//load 512 bytes at a time from SD into ram, then from ram into SPI ram, for a total of 128K or 256 sectors
			sdCardCueSectorAddress((long)(sectorStart+sector_off+i));
			sdCardDirectReadSimple(sbuf,512);
			sdCardStopTransmission();
 			SpiRamWriteFrom((uint32_t)((i*512UL))>>16,((uint32_t)i*512UL)&0xFFFF,sbuf,512);
 //TODO DONT LOAD MORE THAN WE NEED!
 //			if(i == 5+0){//first read from the actual VGM?
 //				track_sectors = SpiRamReadU32();//Get EOF offset(-4) from the VGM header to determine song length
 //				track_sectors = (track_sectors+512UL+4UL)/512UL;//don't load more than we need to...
 //			}
		}

		spi_ram_cursor = (6*512UL);
		SpiRamSeqReadStart(spi_ram_cursor&0x10000?1:0,spi_ram_cursor&0xFFFF);//we will stay in sequential mode until the next song or DB change
		U2A03_Start();
		RedrawDisplay();
		WaitVsync(1);
////////
	SetRenderingParameters(FIRST_RENDER_LINE+2,FRAME_LINES-16);
	//SetMasterVolume(0xFF);
	
	return 1;
}
