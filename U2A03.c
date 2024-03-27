/*
 *
 *  NES 2A03 Demo
 *  by Lee Weber(D3thAdd3r) 2024
 *
 *  Directly based on Arduino Library(GPLv3) by Connor Nishijima
 *  https://github.com/connornishijima/Cartridge/tree/master
 * 
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

#include <avr/io.h>
#ifdef U2A03_FLASH_TABLES
	#include <avr/pgmspace.h>
#endif
#include "U2A03.h"

//const uint32_t nes_clocks_per_frame = 29781UL; 
const uint32_t apu_clocks_per_frame = 14890UL;
const uint32_t apu_clocks_per_sample = 59UL;//(14890UL / 262UL);

//const uint32_t NES_APU_FREQ =  (1789773UL / 2UL / 2UL); // APU is half speed of NES CPU, and we are running half the resolution of that to stay light.
const uint32_t cycle_period = (28636360UL / (1789773UL / 2UL / 2UL));

//const uint16_t audio_rate = 44100;
//const uint32_t audio_period = F_CPU / audio_rate;
const uint32_t audio_period = (28636360UL / 44100UL);
uint32_t next_audio = 0;

uint32_t next_cycle = 0;
uint32_t cpu_cycles = 0;
uint32_t apu_cycles = 0;

uint32_t t_last = 0;
uint32_t cycles_delta = 0;
uint32_t cycles_so_far = 0;

const uint8_t audio_divisor = 2;
uint8_t audio_counter = 0;

#ifdef U2A03_FLASH_TABLES
const uint8_t length_table[2][16] PROGMEM = {
#else
const uint8_t length_table[2][16] = {
#endif
{0x0A, 0x14, 0x28, 0x50, 0xA0, 0x3C, 0x0E, 0x1A, 0x0C, 0x18, 0x30, 0x60, 0xC0, 0x48, 0x10, 0x20},
{0xFE, 0x02, 0x04, 0x06, 0x08, 0x0A, 0x0C, 0x0E, 0x10, 0x12, 0x14, 0x16, 0x18, 0x1A, 0x1C, 0x1E}
};

#ifdef U2A03_FLASH_TABLES
const uint8_t duty_table[4][8] PROGMEM = {
#else
const uint8_t duty_table[4][8] = {
#endif
{0, 0, 0, 0, 0, 0, 0, 1}, // 12.5%
{0, 0, 0, 0, 0, 0, 1, 1}, // 25%
{0, 0, 0, 0, 1, 1, 1, 1}, // 50%
{1, 1, 1, 1, 1, 1, 0, 0}, // 25% (inv.)
};

#ifdef U2A03_FLASH_TABLES
const uint16_t noise_table[16] PROGMEM = {
#else
const uint16_t noise_table[16] = {
#endif
4, 8, 16, 32, 64, 96, 128, 160, 202, 254, 380, 508, 762, 1016, 2034, 4068
};

#ifdef U2A03_FLASH_TABLES
const uint8_t tri_table[32] PROGMEM = {
#else
const uint8_t tri_table[32] = {
#endif
15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0,
 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
 };

// Pulse 1 Variables
uint8_t p1_output = 0;
int16_t p1_11_bit_timer = 0;
uint8_t p1_wave_index = 0;
uint16_t p1_length_counter = 0;
uint8_t p1_envelope_divider = 0;
uint8_t p1_decay_counter = 0;
uint8_t p1_volume = 0;
uint8_t p1_channel = 0;
uint8_t p1_pin = 12;

// Pulse 2 Variables
uint8_t p2_output = 0;
int16_t p2_11_bit_timer = 0;
uint8_t p2_wave_index = 0;
uint16_t p2_length_counter = 0;
uint8_t p2_envelope_divider = 0;
uint8_t p2_decay_counter = 0;
uint8_t p2_volume = 0;
uint8_t p2_channel = 1;
uint8_t p2_pin = 14;

// Noise Variables
uint8_t n_output = 0;
int16_t n_timer = 0;
uint16_t n_length_counter = 0;
uint8_t n_envelope_divider = 0;
uint8_t n_decay_counter = 0;
uint8_t n_volume = 0;
uint8_t n_xor = 0;
uint16_t n_lfsr = 1;
uint8_t n_channel = 2;
uint8_t n_pin = 27;

// Triangle Variables
uint8_t t_output = 0;
int16_t t_11_bit_timer = 0;
uint8_t t_wave_index = 0;
uint16_t t_length_counter = 0;
uint16_t t_linear_counter = 0;
uint8_t t_channel = 3;
uint8_t t_pin = 26;

uint32_t vgm_index = 0;
uint16_t vgm_wait = 0;
uint8_t NES_PLAYING = 0;

uint32_t VGM_EOF_OFFSET = 0;
uint32_t VGM_TOTAL_NUM_SAMPLES = 0;
uint32_t VGM_RATE = 0;
uint32_t VGM_DATA_OFFSET = 0;
uint32_t VGM_NES_APU_CLOCK = 0;
uint32_t VGM_LOOP_OFFSET = 0;


void U2A03_RunFrame(uint8_t* abuf){
	uint16_t samples_left = 262;
	uint8_t i;
U2A03_FRAME_TOP:
	if(!samples_left)
		return;
	samples_left--;
	
//	U2A03_ParseVGMHeader(music);

//	next_cycle = ESP.getCycleCount();
//	next_audio = ESP.getCycleCount();
//	uint32_t t_now = 0;//ESP.getCycleCount();
//	if(t_last > t_now){
//		next_cycle = t_now;
//		next_audio = t_now;
//	}

//	if(t_now >= next_cycle){
//		next_cycle += cycle_period;
		for(i=0; i<apu_clocks_per_sample; i++)
			U2A03_ClockAPU();
//	}

//	if(t_now >= next_audio){
//		next_audio += audio_period;
		U2A03_ParseVGMStream(1);

//		audio_counter++;
//		if(audio_counter >= audio_divisor){
//			audio_counter = 0;
			U2A03_SampleAudio(abuf++);
			//render_audio();
//		}
//	}
goto U2A03_FRAME_TOP;
//	t_last = t_now;
}

extern uint8_t U2A03_Get8BitsFromVGMStream(uint32_t index);//user defined function(commonly, read 8 bits sequential from SPI RAM)
extern uint16_t U2A03_Get16BitsFromVGMStream(uint32_t index);//user defined function(commonly, read 16 bits sequential from SPI RAM)
extern uint32_t U2A03_Get32BitsFromVGMStream(uint32_t index);//user defined function(commonly, read 32 bits sequential from SPI RAM)

void U2A03_Start(){
	U2A03_Reset();
	U2A03_ParseVGMHeader();
}


void U2A03_ParseVGMHeader(){
	VGM_EOF_OFFSET = U2A03_Get32BitsFromVGMStream(0x04) + 0x04;
	VGM_TOTAL_NUM_SAMPLES = U2A03_Get32BitsFromVGMStream(0x18);
	VGM_RATE = U2A03_Get32BitsFromVGMStream(0x24);
	VGM_DATA_OFFSET = U2A03_Get32BitsFromVGMStream(0x34) + 0x34;
	VGM_NES_APU_CLOCK = U2A03_Get32BitsFromVGMStream(0x84);
	VGM_LOOP_OFFSET = U2A03_Get32BitsFromVGMStream(0x1C) + 0x1C;

	vgm_index = VGM_DATA_OFFSET;
}


void U2A03_ParseVGMStream(uint8_t looping){
	uint8_t extra = 0;
	if(vgm_wait == 0){
		uint8_t command = U2A03_Get8BitsFromVGMStream(vgm_index+0);//music[vgm_index];

		if(command == 0x61){// Wait nL nH samples
/*			uint16_t result = U2A03_Get8BitsFromVGMStream(vgm_index+2);//music[vgm_index + 2];
			result = result << 8;
			result += U2A03_Get8BitsFromVGMStream(vgm_index+1);//music[vgm_index + 1];
*/
			vgm_wait = U2A03_Get16BitsFromVGMStream(vgm_index+1);
			//vgm_wait = result;
			extra = 2;
		}else if(command == 0x62){// Wait 735 samples
			vgm_wait = 735;
		}else if(command == 0x63){// Wait 882 samples
			vgm_wait = 882;
		}else if(command == 0x67 && U2A03_Get8BitsFromVGMStream(vgm_index+1)){//music[vgm_index + 1] == 0x66){// Start of data block
//			uint32_t block_size = (music[vgm_index + 3]			) + 
//									(music[vgm_index + 4] <<	8UL) + 
//									(music[vgm_index + 5] << 16UL) + 
//									(music[vgm_index + 6] << 24UL); 
			uint32_t block_size = U2A03_Get8BitsFromVGMStream(vgm_index+3);
			vgm_index += block_size - 1;// Basically ignore all data block contents
		}else if(command == 0x66 || vgm_index == VGM_EOF_OFFSET){
			//Serial.println("END");
			if(looping){
				if(VGM_LOOP_OFFSET == 0){// Loop offset is not built into file
					vgm_index = VGM_DATA_OFFSET - 1;// Restart the song entirely
				}else{
					vgm_index = VGM_LOOP_OFFSET - 1;// Music loops, reset to looping byte
				}
			}else{
				U2A03_Reset();// Reset if loop opt-out
			}
		}else if(command >= 0x70 && command < 0x80){// Wait (0x7)n+1 samples
			uint8_t result = command;
			result = result << 4;
			result = result >> 4;
			vgm_wait = result + 1;
		}else if(command == 0xB4){// NES APU write dd to aa
			uint16_t linear_data = U2A03_Get16BitsFromVGMStream(vgm_index+1);//optimize for SPI RAM//music[vgm_index + 1];
			uint8_t reg = linear_data<<8UL;//U2A03_Get8BitsFromVGMStream(vgm_index+1);//music[vgm_index + 1];
			uint8_t value = linear_data&0xFF;//U2A03_Get8BitsFromVGMStream(vgm_index+2);//music[vgm_index + 2];


			U2A03_SetReg(reg, value);
			extra = 2;
		}

		vgm_index += 1;
		vgm_index += extra;
	}else{
		vgm_wait--;
	}
}


void U2A03_ReadVGMHeader(){
	VGM_EOF_OFFSET = U2A03_Get32BitsFromVGMStream(0x04) + 0x04;
	VGM_TOTAL_NUM_SAMPLES = U2A03_Get32BitsFromVGMStream(0x18);
	VGM_RATE = U2A03_Get32BitsFromVGMStream(0x24);
	VGM_DATA_OFFSET = U2A03_Get32BitsFromVGMStream(0x34) + 0x34;
	VGM_NES_APU_CLOCK = U2A03_Get32BitsFromVGMStream(0x84);
	VGM_LOOP_OFFSET = U2A03_Get32BitsFromVGMStream(0x1C) + 0x1C;

	vgm_index = VGM_DATA_OFFSET;
}


void U2A03_Reset(){
	for (uint8_t i = 0; i < 24; i++){
		NES_REG[i] = 0b00000000;
	}

	// Pulse 1 Variables
	p1_output = 0;
	p1_11_bit_timer = 0;
	p1_wave_index = 0;
	p1_length_counter = 0;
	p1_envelope_divider = 0;
	p1_decay_counter = 0;
	p1_volume = 0;
	p1_channel = 0;

	// Pulse 2 Variables
	p2_output = 0;
	p2_11_bit_timer = 0;
	p2_wave_index = 0;
	p2_length_counter = 0;
	p2_envelope_divider = 0;
	p2_decay_counter = 0;
	p2_volume = 0;
	p2_channel = 1;

	// Noise Variables
	n_output = 0;
	n_timer = 0;
	n_length_counter = 0;
	n_envelope_divider = 0;
	n_decay_counter = 0;
	n_volume = 0;
	n_xor = 0;
	n_lfsr = 1;
	n_channel = 2;

	// Triangle Variables
	t_output = 0;
	t_11_bit_timer = 0;
	t_wave_index = 0;
	t_length_counter = 0;
	t_linear_counter = 0;
	t_channel = 3;

	vgm_index = 0;
	vgm_wait = 0;
	NES_PLAYING = 0;

	next_audio = 0;
	next_cycle = 0;
	cpu_cycles = 0;
	apu_cycles = 0;

	t_last = 0;

	audio_counter = 0;
}

void U2A03_SampleAudio(uint8_t *abuf){
	// Pulse 1
	if(p1_length_counter > 0){
		if(p1_11_bit_timer >= 8){
#ifdef U2A03_FLASH_TABLES
			p1_output = p1_volume * pgm_read_byte(&duty_table[U2A03_GetDuty(0x00)][p1_wave_index]);
#else
			p1_output = p1_volume * duty_table[U2A03_GetDuty(0x00)][p1_wave_index];
#endif
		}
	}
	else{
		p1_output = 0;
	}

	// Pulse 2
	if(p2_length_counter > 0){
		if(p2_11_bit_timer >= 8){
#ifdef U2A03_FLASH_TABLES
			p2_output = p2_volume * pgm_read_byte(&duty_table[U2A03_GetDuty(0x04)][p2_wave_index]);
#else
			p2_output = p2_volume * duty_table[U2A03_GetDuty(0x04)][p2_wave_index];
#endif
		}
	}
	else{
		p2_output = 0;
	}

	// Noise
	if(n_length_counter > 0){
		n_output = n_volume * n_xor;
	}
	else{
		n_output = 0;
	}

	// Triangle
	if(t_length_counter > 0){
#ifdef U2A03_FLASH_TABLES
		t_output = pgm_read_byte(&tri_table[t_wave_index]);
#else
		t_output = tri_table[t_wave_index];
#endif
	}
	else{
		t_output = 0;
	}
 //render_audio contained this mixing:
 /*	sigmaDeltaWrite(p1_channel, p1_output * 12);
	sigmaDeltaWrite(p2_channel, p2_output * 12);
	sigmaDeltaWrite(n_channel, n_output * 6);
	sigmaDeltaWrite(t_channel, t_output * 16);
*/
	abuf[0] = p1_output+p2_output+n_output+t_output;
}


void U2A03_ClockAPU(){
	U2A03_ClockFrameCounter();
	U2A03_DecrementTimers();
	apu_cycles += 2;
}

void U2A03_DecrementTimers(){
	// Pulse 1
	if(p1_11_bit_timer <= 0){
		p1_11_bit_timer = U2A03_Get11BitTimer(0x02, 0x03);
		if(p1_wave_index == 0){
			p1_wave_index = 7;
		}
		else{
			p1_wave_index--;
		}
	}
	else{
		p1_11_bit_timer -= 2;
	}

	// Pulse 2
	if(p2_11_bit_timer <= 0){
		p2_11_bit_timer = U2A03_Get11BitTimer(0x06, 0x07);
		if(p2_wave_index == 0){
			p2_wave_index = 7;
		}
		else{
			p2_wave_index--;
		}
	}
	else{
		p2_11_bit_timer -= 2;
	}

	// Noise
	if(n_timer <= 0){
		uint8_t index = U2A03_GetReg(0x0E);
		index = index << 4;
		index = index >> 4;
#ifdef U2A03_FLASH_TABLES
		n_timer = pgm_read_word(&noise_table[index]);
#else
		n_timer = noise_table[index];
#endif
		U2A03_ClockLFSR();
	}
	else{
		n_timer -= 2;
	}

	// Triangle
	if(t_11_bit_timer <= 0){
		t_11_bit_timer = U2A03_Get11BitTimer(0x0A, 0x0B) + 1;

		if(t_length_counter > 0 && t_linear_counter > 0){
			if(t_wave_index == 0){
				t_wave_index = 31;
			}
			else{
				t_wave_index--;
			}
		}
	}
	else{
		t_11_bit_timer -= 2;
		t_11_bit_timer -= 2; // Triangle timer clocked on CPU not APU (2x speed)
	}
}

void U2A03_ClockLFSR(){
	uint8_t bit1 = 0;
	uint8_t bit2 = 1;
	if(U2A03_GetBit(0x0E, 7) == 1){
		bit2 = 6;
	}
	uint8_t val1 = (n_lfsr&(1<<bit1))?1:0;//bitRead(n_lfsr, bit1);
	uint8_t val2 = (n_lfsr&(1<<bit2))?1:0;//bitRead(n_lfsr, bit2);
	n_xor = val1 ^ val2;

	n_lfsr = n_lfsr >> 1;
	n_lfsr |= (n_xor<<14UL);//bitWrite(n_lfsr, 14, n_xor);
}

void U2A03_ClockFrameCounter(){
	if(U2A03_GetFrameCounterMode() == 0){ // 4-step sequence
		if(apu_cycles == 3728){
			U2A03_ClockEnvelopes();
			U2A03_ClockLinearCounter();

//			if(fc_callback)
//				fc_callback();
		}
		else if(apu_cycles == 7456){
			U2A03_ClockEnvelopes();
			U2A03_ClockLinearCounter();
			U2A03_ClockLengthCounters();
//			U2A03_ClockSweepUnits();
		}
		else if(apu_cycles == 11186){
			U2A03_ClockEnvelopes();
			U2A03_ClockLinearCounter();
		}
		else if(apu_cycles == 14914){
			U2A03_ClockEnvelopes();
			U2A03_ClockLinearCounter();
			U2A03_ClockLengthCounters();
//			U2A03_ClockSweepUnits();
			apu_cycles = 0;
		}
	}
	else if(U2A03_GetFrameCounterMode() == 1){ // 5-step sequence
		if(apu_cycles == 3728){
			U2A03_ClockEnvelopes();
			U2A03_ClockLinearCounter();
			
//			if(fc_callback)
//				fc_callback();
		}
		else if(apu_cycles == 7456){
			U2A03_ClockEnvelopes();
			U2A03_ClockLinearCounter();
			U2A03_ClockLengthCounters();
//			U2A03_ClockSweepUnits();
		}
		else if(apu_cycles == 11186){
			U2A03_ClockEnvelopes();
			U2A03_ClockLinearCounter();
		}
		else if(apu_cycles == 18640){
			U2A03_ClockEnvelopes();
			U2A03_ClockLinearCounter();
			U2A03_ClockLengthCounters();
//			U2A03_ClockSweepUnits();
			apu_cycles = 0;
		}
	}
}

uint8_t U2A03_GetFrameCounterMode(){
	return U2A03_GetBit(0x17, 7);
}

uint8_t U2A03_GetBit(uint8_t reg, uint8_t b){
	//return bitRead(U2A03_GetReg(reg), b);
	return (U2A03_GetReg(reg) & (1<<b))?1:0;
}

void U2A03_SetBit(uint8_t reg, uint8_t b, uint8_t val){
	uint8_t r = U2A03_GetReg(reg);
	r |= (val<<b);//bitWrite(r, b, val);
	U2A03_SetReg(reg, r); // used to make sure conditions defined in U2A03_SetReg() are used here also
}

uint8_t U2A03_GetReg(uint8_t reg){
	return NES_REG[reg];
}

void U2A03_SetReg(uint8_t reg, uint8_t val){
	// PULSE 1 ----------------------------------------
	if(reg == 0x03){
		// Pulse 1 load length counter from table address
		uint8_t level = (val&(1<<3))?1:0;//bitRead(val, 3);
		uint8_t index = val >> 4;
#ifdef U2A03_FLASH_TABLES
		p1_length_counter = pgm_read_byte(&length_table[level][index]);
#else
		p1_length_counter = length_table[level][index];
#endif
		p1_wave_index = 0;

		p1_decay_counter = 15;
		p1_envelope_divider = 0;

	}
	else if(reg == 0x15){
		// Pulse 1
//		if(bitRead(val, 0) == 0){ // if p1 enabled bit clear, empty length counter
		if((val&1) == 0){
			U2A03_SetLengthCounter(0x03, 0);
		}

		//clear DMC interrupt flag
	}

	// PULSE 2 ----------------------------------------
	if(reg == 0x07){
		// Pulse 2 load length counter from table address
		uint8_t level = (val&(1<<3))?1:0;//bitRead(val, 3);
		uint8_t index = val >> 4;
#ifdef U2A03_FLASH_TABLES
		p2_length_counter = pgm_read_byte(&length_table[level][index]);	
#else
		p2_length_counter = length_table[level][index];
#endif
		p2_wave_index = 0;

		p2_decay_counter = 15;
		p2_envelope_divider = 0;

	}
	else if(reg == 0x15){
		// Pulse 2
//		if(bitRead(val, 1) == 0){ // if p2 enabled bit clear, empty length counter
		if(val&2){
			U2A03_SetLengthCounter(0x07, 0);
		}

		//clear DMC interrupt flag
	}

	// NOISE ----------------------------------------
	if(reg == 0x0F){
		// Noise load length counter from table address
		uint8_t level = (val&(1<<3))?1:0;//bitRead(val, 3);
		uint8_t index = val >> 4;
#ifdef U2A03_FLASH_TABLES
		n_length_counter = pgm_read_byte(&length_table[level][index]);
#else
		n_length_counter = length_table[level][index];
#endif
		n_decay_counter = 15;
		n_envelope_divider = 0;

	}
	else if(reg == 0x15){
		// Noise
//		if(bitRead(val, 3) == 0){ // if noise enabled bit clear, empty length counter
		if((val&(1<<3)) == 0){
			U2A03_SetLengthCounter(0x0F, 0);
		}

		//clear DMC interrupt flag
	}

	// TRIANGLE ----------------------------------------
	if(reg == 0x0B){
		// Triangle load length counter from table address
		uint8_t level = (val&(1<<3));//bitRead(val, 3);
		uint8_t index = val >> 4;
#ifdef U2A03_FLASH_TABLES
		t_length_counter = pgm_read_byte(&length_table[level][index]);
#else
		t_length_counter = length_table[level][index];
#endif
		// Triangle load linear counter from register
		uint8_t linc = U2A03_GetReg(0x08);
		linc = linc << 1;
		linc = linc >> 1;
		t_linear_counter = linc;

		U2A03_SetBit(0x08, 7, 1);
	}
	else if(reg == 0x15){

		//clear DMC interrupt flag
	}

	NES_REG[reg] = val;
}

void U2A03_ClockEnvelopes(){
	// Pulse 1
	uint8_t p1_envelope_disable = U2A03_GetReg(0x00)&(1<<4);//bitRead(U2A03_GetReg(0x00), 4);
	if(p1_envelope_disable){
		uint8_t v = U2A03_GetReg(0x00);
		v = v << 4;
		v = v >> 4;
		p1_volume = v;
	}
	else{
		uint8_t p1_envelope_loop = U2A03_GetReg(0x00)&(1<<5);//bitRead(U2A03_GetReg(0x00), 5);

		if(p1_envelope_divider > 0){
			p1_envelope_divider--;
		}
		else{
			uint8_t d = U2A03_GetReg(0x00);
			d = d << 4;
			d = d >> 4;
			p1_envelope_divider = d;

			//clock decay counter
			if(p1_decay_counter > 0){
				p1_decay_counter--;
			}
			else{
				if(p1_envelope_loop == 1){
					p1_decay_counter = 15;
				}
			}
		}

		p1_volume = p1_decay_counter;
	}

	// Pulse 2
	uint8_t p2_envelope_disable = U2A03_GetReg(0x04)&(1<<4);//bitRead(U2A03_GetReg(0x04), 4);
	if(p2_envelope_disable){
		uint8_t v = U2A03_GetReg(0x04);
		v = v << 4;
		v = v >> 4;
		p2_volume = v;
	}
	else{
		uint8_t p2_envelope_loop = U2A03_GetReg(0x04)&(1<<5);//bitRead(U2A03_GetReg(0x04), 5);

		if(p2_envelope_divider > 0){
			p2_envelope_divider--;
		}
		else{
			uint8_t d = U2A03_GetReg(0x04);
			d = d << 4;
			d = d >> 4;
			p2_envelope_divider = d;

			//clock decay counter
			if(p2_decay_counter > 0){
				p2_decay_counter--;
			}
			else{
				if(p2_envelope_loop == 1){
					p2_decay_counter = 15;
				}
			}
		}

		p2_volume = p2_decay_counter;
	}

	// Noise
	uint8_t n_envelope_disable = U2A03_GetReg(0x0C)&(1<<4);//bitRead(U2A03_GetReg(0x0C), 4);
	if(n_envelope_disable){
		uint8_t v = U2A03_GetReg(0x0C);
		v = v << 4;
		v = v >> 4;
		n_volume = v;
	}
	else{
		uint8_t n_envelope_loop = U2A03_GetReg(0x0C)&(1<<5);//bitRead(U2A03_GetReg(0x0C), 5);

		if(n_envelope_divider > 0){
			n_envelope_divider--;
		}
		else{
			uint8_t d = U2A03_GetReg(0x0C);
			d = d << 4;
			d = d >> 4;
			n_envelope_divider = d;

			//clock decay counter
			if(n_decay_counter > 0){
				n_decay_counter--;
			}
			else{
				if(n_envelope_loop == 1){
					n_decay_counter = 15;
				}
			}
		}

		n_volume = n_decay_counter;
	}
}

void U2A03_ClockLinearCounter(){
	if(U2A03_GetBit(0x08, 7) == 1){
		if(t_linear_counter > 0){
			t_linear_counter--;
		}
	}
}

void U2A03_ClockLengthCounters(){
	uint8_t length_counter_halt_flag = 0;

	// Pulse 1
	length_counter_halt_flag = U2A03_GetBit(0x00, 5);
	if(length_counter_halt_flag == 0){ // Pulse 1 halt flag not set? decrement length counter
		if(p1_length_counter > 0){
			p1_length_counter--;
		}
	}

	// Pulse 2
	length_counter_halt_flag = U2A03_GetBit(0x04, 5);
	if(length_counter_halt_flag == 0){ // Pulse 2 halt flag not set? decrement length counter
		if(p2_length_counter > 0){
			p2_length_counter--;
		}
	}

	// Noise
	length_counter_halt_flag = U2A03_GetBit(0x0C, 5);
	if(length_counter_halt_flag == 0){ // Noise halt flag not set? decrement length counter
		if(n_length_counter > 0){
			n_length_counter--;
		}
	}

	// Triangle
	length_counter_halt_flag = U2A03_GetBit(0x08, 7);
	if(length_counter_halt_flag == 0){ // Triangle halt flag not set? decrement length counter
		if(t_length_counter > 0){
			t_length_counter--;
		}
	}
}

uint8_t U2A03_GetDuty(uint8_t reg){
	return (U2A03_GetReg(reg)>>6);
}

uint16_t U2A03_Get11BitTimer(uint8_t reg_low, uint8_t reg_high){
	uint8_t rlow = U2A03_GetReg(reg_low);
	uint8_t rhigh = U2A03_GetReg(reg_high);
	rhigh = rhigh << 5;
	rhigh = rhigh >> 5;
	uint16_t r16 = rhigh;
	r16 = r16 << 8;
	r16 = r16 + rlow;
	return r16;
}

void U2A03_Set11BitTimer(uint8_t reg_low, uint8_t reg_high, uint16_t val){
	uint16_t out_low = val;
	out_low = out_low << 8;
	out_low = out_low >> 8;
	uint8_t ol = out_low;

	uint16_t out_high = val;
	out_high = out_high >> 8;
	uint8_t oh = out_high;

	uint8_t r_high = U2A03_GetReg(reg_high);

	uint16_t val_high = r_high;
	val_high = val_high >> 3;
	val_high = val_high << 3;
	val_high = val_high + oh;

	U2A03_SetReg(reg_low, ol);
	U2A03_SetReg(reg_high, val_high);
}

uint8_t U2A03_GetLengthCounter(uint8_t reg){
	uint8_t r = U2A03_GetReg(reg);
	r = r >> 3;
	return r;
}

uint8_t U2A03_SetLengthCounter(uint8_t reg, uint8_t val){
	uint8_t r = U2A03_GetReg(reg);
	r = r << 5;
	r = r >> 5;
	val = val << 3;
	uint8_t result = r + val;
	U2A03_SetReg(reg, result);
	return result;
}

void U2A03_ClockSweepUnits(){
	asm("nop");
}
