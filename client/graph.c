//-----------------------------------------------------------------------------
// Copyright (C) 2010 iZsh <izsh at fail0verflow.com>
//
// This code is licensed to you under the terms of the GNU GPL, version 2 or,
// at your option, any later version. See the LICENSE.txt file for the text of
// the license.
//-----------------------------------------------------------------------------
// Graph utilities
//-----------------------------------------------------------------------------

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "ui.h"
#include "graph.h"
#include "lfdemod.h"

int GraphBuffer[MAX_GRAPH_TRACE_LEN];
int GraphTraceLen;

/* write a bit to the graph */
void AppendGraph(int redraw, int clock, int bit)
{
  int i;

  for (i = 0; i < (int)(clock / 2); ++i)
    GraphBuffer[GraphTraceLen++] = bit ^ 1;

  for (i = (int)(clock / 2); i < clock; ++i)
    GraphBuffer[GraphTraceLen++] = bit;

  if (redraw)
    RepaintGraphWindow();
}

// clear out our graph window
int ClearGraph(int redraw)
{
  int gtl = GraphTraceLen;
  memset(GraphBuffer, 0x00, GraphTraceLen);

  GraphTraceLen = 0;

  if (redraw)
    RepaintGraphWindow();

  return gtl;
}

// DETECT CLOCK NOW IN LFDEMOD.C

void setGraphBuf(uint8_t *buff, size_t size)
{
	if ( buff == NULL ) return;
	
	uint16_t i = 0;  
	if ( size > MAX_GRAPH_TRACE_LEN )
		size = MAX_GRAPH_TRACE_LEN;
	ClearGraph(0);
	for (; i < size; ++i){
		GraphBuffer[i]=buff[i]-128;
	}
	GraphTraceLen=size;
	RepaintGraphWindow();
	return;
}
size_t getFromGraphBuf(uint8_t *buff)
{
	if (buff == NULL ) return 0;
	uint32_t i;
	for (i=0;i<GraphTraceLen;++i){
		if (GraphBuffer[i]>127) GraphBuffer[i]=127; //trim
		if (GraphBuffer[i]<-127) GraphBuffer[i]=-127; //trim
		buff[i]=(uint8_t)(GraphBuffer[i]+128);
	}
	return i;
}

// A simple test to see if there is any data inside Graphbuffer. 
bool HasGraphData(){

	if ( GraphTraceLen <= 0) {
		PrintAndLog("No data available, try reading something first");
		return false;
	}
	return true;	
}

// Detect high and lows in Grapbuffer.
// Only loops the first 256 values. 
void DetectHighLowInGraph(int *high, int *low, bool addFuzz) {

	uint8_t loopMax = 255;
	if ( loopMax > GraphTraceLen)
		loopMax = GraphTraceLen;
  
	for (uint8_t i = 0; i < loopMax; ++i) {
		if (GraphBuffer[i] > *high)
			*high = GraphBuffer[i];
		else if (GraphBuffer[i] < *low)
			*low = GraphBuffer[i];
	}
	
	//12% fuzz in case highs and lows aren't clipped
	if (addFuzz) {
		*high = (int)(*high * .88);
		*low  = (int)(*low  * .88);
	}
}

// Get or auto-detect ask clock rate
int GetAskClock(const char str[], bool printAns, bool verbose)
{
	int clock;
	sscanf(str, "%i", &clock);
	if (!strcmp(str, ""))
		clock = 0;

	if (clock != 0) 
		return clock;
	// Auto-detect clock
	uint8_t grph[MAX_GRAPH_TRACE_LEN]={0};
	size_t size = getFromGraphBuf(grph);
	if (size == 0) {
		if (verbose)
			PrintAndLog("Failed to copy from graphbuffer");
		return -1;
	}
	DetectASKClock(grph, size, &clock, 20);
	// Only print this message if we're not looping something
	if (printAns){
		PrintAndLog("Auto-detected clock rate: %d", clock);
	}
	return clock;
}

int GetPskClock(const char str[], bool printAns, bool verbose)
{
	int clock;
	sscanf(str, "%i", &clock);
	if (!strcmp(str, "")) 
		clock = 0;

	if (clock!=0) 
		return clock;
	// Auto-detect clock
	uint8_t grph[MAX_GRAPH_TRACE_LEN]={0};
	size_t size = getFromGraphBuf(grph);
	if ( size == 0 ) {
		if (verbose) 
			PrintAndLog("Failed to copy from graphbuffer");
		return -1;
	}
	clock = DetectPSKClock(grph,size,0);
	// Only print this message if we're not looping something
	if (printAns){
		PrintAndLog("Auto-detected clock rate: %d", clock);
	}
	return clock;
}

uint8_t GetNrzClock(const char str[], bool printAns, bool verbose)
{
	int clock;
	sscanf(str, "%i", &clock);
	if (!strcmp(str, ""))
		clock = 0;

	if (clock!=0) 
		return clock;
	// Auto-detect clock
	uint8_t grph[MAX_GRAPH_TRACE_LEN]={0};
	size_t size = getFromGraphBuf(grph);
	if ( size == 0 ) {
		if (verbose) 
			PrintAndLog("Failed to copy from graphbuffer");
		return -1;
	}
	clock = DetectNRZClock(grph, size, 0);
	// Only print this message if we're not looping something
	if (printAns){
		PrintAndLog("Auto-detected clock rate: %d", clock);
	}
	return clock;
}
//by marshmellow
//attempt to detect the field clock and bit clock for FSK
uint8_t GetFskClock(const char str[], bool printAns, bool verbose)
{
	int clock;
	sscanf(str, "%i", &clock);
	if (!strcmp(str, ""))
		clock = 0;
	if (clock != 0) return (uint8_t)clock;

	uint8_t BitStream[MAX_GRAPH_TRACE_LEN]={0};
	size_t size = getFromGraphBuf(BitStream);
	if (size==0) return 0;
	uint8_t dummy = 0;
	uint16_t ans = countFC(BitStream, size, &dummy); 
	if (ans==0) {
		if (verbose) PrintAndLog("DEBUG: No data found");
		return 0;
	}
	uint8_t fc1, fc2;
	fc1 = (ans >> 8) & 0xFF;
	fc2 = ans & 0xFF;

	uint8_t rf1 = detectFSKClk(BitStream, size, fc1, fc2);
	if (rf1==0) {
		if (verbose) PrintAndLog("DEBUG: Clock detect error");
		return 0;
	}
	if ((fc1==10 && fc2==8) || (fc1==8 && fc2==5)){
		if (printAns) PrintAndLog("Detected Field Clocks: FC/%d, FC/%d - Bit Clock: RF/%d", fc1, fc2, rf1);
		return rf1;
	}
	if (verbose){
		PrintAndLog("DEBUG: unknown fsk field clock detected");
		PrintAndLog("Detected Field Clocks: FC/%d, FC/%d - Bit Clock: RF/%d", fc1, fc2, rf1);
	}
	return 0;
}
