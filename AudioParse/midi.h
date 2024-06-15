#pragma once
#include <cstdint>

extern "C" {

	typedef struct {
		char chunkID[4];
		uint32_t chunkSize;
		uint8_t data[];
	} MidiChunkData;

	typedef struct {
		MidiChunkData* data;
		size_t index;
	} MidiChunk;

	uint32_t swapEndian32(uint32_t v) {
		int k = 0;
		k = (k << 8) | (v & 0xFF);
		v >>= 8;
		k = (k << 8) | (v & 0xFF);
		v >>= 8;
		k = (k << 8) | (v & 0xFF);
		v >>= 8;
		k = (k << 8) | (v & 0xFF);
		v >>= 8;
		return k;
	}

	MidiChunk MidiNextChunk(MidiChunk& chunk) {
		return MidiChunk{ (MidiChunkData*)((char*)(chunk.data+1) + swapEndian32(chunk.data->chunkSize)), 0};
	}

	uint32_t MidiPopVL(MidiChunk& chunk) {
		int v = 0;
		do {
			v <<= 7;
			v |= chunk.data->data[chunk.index] & 0x7F;
		} while (chunk.data->data[chunk.index++] & 0x80);
		return v;
	}
	uint16_t MidiPopWord(MidiChunk& chunk) {
		uint16_t v = (chunk.data->data[chunk.index] << 8) | chunk.data->data[chunk.index + 1];
		chunk.index += 2;
		return v;
	}
	uint8_t MidiPopByte(MidiChunk& chunk) {
		uint8_t v = chunk.data->data[chunk.index];
		chunk.index += 1;
		return v;
	}
	uint32_t MidiPop3Byte(MidiChunk& chunk) {
		uint32_t v = (chunk.data->data[chunk.index] << 16) | (chunk.data->data[chunk.index + 1] << 8) | chunk.data->data[chunk.index + 2];
		chunk.index += 1;
		return v;
	}
	uint8_t* MidiPointer(MidiChunk& chunk) {
		return chunk.data->data + chunk.index;
	}
	bool MidiIsReadable(MidiChunk& chunk) {
		return chunk.index < swapEndian32(chunk.data->chunkSize);
	}
};