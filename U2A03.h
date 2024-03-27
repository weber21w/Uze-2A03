/*
  Library for parsing VGM into an NES APU emulation!
  Created by Connor Nishijima, October 14th 2018.
  Released under the GPLv3 license.
*/

#ifndef U2A03_H
#define U2A03_H

uint8_t U2A03_LoadSong(uint8_t index);//user defined function
uint8_t U2A03_Get8BitsFromVGMStream(uint32_t index);//user defined function
uint16_t U2A03_Get16BitsFromVGMStream(uint32_t index);//user defined function
uint32_t U2A03_Get32BitsFromVGMStream(uint32_t index);//user defined function
void U2A03_ParseVGMHeader();
void U2A03_ParseVGMStream(uint8_t looping);
void U2A03_Reset();
void U2A03_Start();//reset A203 and VGM stream, and parse VGM header
void U2A03_RunFrame(uint8_t *abuf);
void U2A03_SampleAudio(uint8_t *abuf);
void U2A03_ClockAPU();
void U2A03_DecrementTimers();
void U2A03_ClockLFSR();
void U2A03_ClockFrameCounter();
uint8_t U2A03_GetFrameCounterMode();
uint8_t U2A03_GetBit(uint8_t reg, uint8_t b);
void U2A03_SetBit(uint8_t reg, uint8_t b, uint8_t val);
uint8_t U2A03_GetReg(uint8_t reg);
void U2A03_SetReg(uint8_t reg, uint8_t val);
void U2A03_ClockEnvelopes();
void U2A03_ClockLinearCounter();
void U2A03_ClockLengthCounters();
uint8_t U2A03_GetDuty(uint8_t reg);
uint16_t U2A03_Get11BitTimer(uint8_t reg_low, uint8_t reg_high);
void U2A03_Set11BitTimer(uint8_t reg_low, uint8_t reg_high, uint16_t val);
uint8_t U2A03_GetLengthCounter(uint8_t reg);
uint8_t U2A03_SetLengthCounter(uint8_t reg, uint8_t val);
//void U2A03_ClockSweepUnits();

uint8_t NES_REG[24]; /*= {
// PULSE 1
0b00000000, // $4000
0b00000000, // $4001
0b00000000, // $4002
0b00000000, // $4003

// PULSE 2
0b00000000, // $4004
0b00000000, // $4005
0b00000000, // $4006
0b00000000, // $4007

// TRIANGLE
0b00000000, // $4008

0b00000000, // $4009 (UNUSED)

0b00000000, // $400A
0b00000000, // $400B

// NOISE
0b00000000, // $400C
0b00000000, // $400E
0b00000000, // $400F

// DMC
0b00000000, // $4010
0b00000000, // $4011
0b00000000, // $4012
0b00000000, // $4013

0b00000000, // $4014 (UNUSED)

// Control/Status
0b00000000, // $4015

0b00000000, // $4016 (UNUSED)

// Frame Counter
0b00000000, // $4017
};*/

#endif



