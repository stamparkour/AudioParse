#pragma once

#include <stdint.h>
#include <istream>
#include <vector>

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

struct MidiChunkData {
	char chunkID[4];
	uint32_t chunkSize;
	uint8_t data[0];
};

struct MidiHeaderChunkData {
	char chunkID[4];
	uint32_t chunkSize;
	uint16_t formatType;
	uint16_t tracks;
	uint16_t divisions;

	double SecondsPerTick(int tempo) {
		if (divisions & 0x8000) {
			char f = divisions >> 8;
			uint8_t tpf = divisions;
			double fps = 0;
			if (f == 24) fps = 24;
			else if (f == 25) fps = 25;
			else if (f == 29) fps = 30 / 1.001;
			else if (f == 30) fps = 30;
			return 1 / (tpf * fps);
		}
		else {//secs / mics * mics / qu / (tick / qu)
			return 0.000001 * tempo / divisions;
		}
	}

	double QuarternotesPerTick(int tempo) {
		// qu / mics * mics / sec * sec / tick
		if (divisions & 0x8000) {
			char f = divisions >> 8;
			uint8_t tpf = divisions;
			double fps = 0;
			if (f == 24) fps = 24;
			else if (f == 25) fps = 25;
			else if (f == 29) fps = 30 / 1.001;
			else if (f == 30) fps = 30;
			return 1000000.0 / tempo / (tpf * fps);
		}
		else {
			return 1.0 / divisions;
		}
	}
};

enum struct MidiEventType {
	//8 bit midi event command
	MIDI_EVENT, 
	//8 bit sysex event command
	SYSEX_EVENT, 
	//7 bit message type
	META_EVENT
};

struct MidiEvent {
	uint8_t* data;
	int deltaTime;
	int length;
	MidiEventType type;
	uint8_t command;
};

struct MidiChunk {
	MidiChunkData* data{};
	size_t index{};

	MidiChunk() = default;
	MidiChunk(uint8_t* v) {
		data = (MidiChunkData*)v;
		index = 0;
	}
	MidiChunk(MidiChunkData* data, size_t index) {
		this->data = data;
		this->index = index;
	}

	MidiChunk MidiNextChunk() {
		return MidiChunk{ (MidiChunkData*)((char*)(data + 1) + swapEndian32(data->chunkSize)), 0 };
	}
	template<typename T>
	T* GetChunk() {
		return (T*)data;
	}

	void reset() {
		index = 0;
	}
	uint32_t MidiPopVL() {
		int v = 0;
		do {
			v <<= 7;
			v |= data->data[index] & 0x7F;
		} while (data->data[index++] & 0x80);
		return v;
	}
	uint16_t MidiPopWord() {
		uint16_t v = (data->data[index] << 8) | data->data[index + 1];
		index += 2;
		return v;
	}
	uint8_t MidiPopByte() {
		uint8_t v = data->data[index];
		index += 1;
		return v;
	}
	uint32_t MidiPop3Byte() {
		uint32_t v = (data->data[index] << 16) | (data->data[index + 1] << 8) | data->data[index + 2];
		index += 1;
		return v;
	}
	MidiEvent MidiPopEvent() {
		MidiEvent event{};
		event.deltaTime = MidiPopVL();
		event.data = MidiPointer();
		if (event.data[0] == 0xFF) {
			event.type = MidiEventType::META_EVENT;
			event.command = event.data[1];
			index += 2;
			event.data = MidiPointer();
			event.length = MidiPopVL();
		}
		else if ((event.data[0] & 0xF0) == 0xF0) {
			event.type = MidiEventType::SYSEX_EVENT;
			event.command = event.data[0];
			index += 1;
			event.data = MidiPointer();
			event.length = MidiPopVL();
		}
		else {
			event.type = MidiEventType::MIDI_EVENT;
			event.command = event.data[0];
			index += 1;
			event.data = MidiPointer();
			event.length = 0;
			int delta = ((event.data[0] & 0xE0) == 0xC0 ? 1 : 2);
			while (!(event.data[event.length] & 0x80)) {
				event.length += delta;
			}
		}
		index += event.length;
		return event;
	}
	uint8_t* MidiPointer() {
		return data->data + index;
	}
	bool eoc() {
		return index >= swapEndian32(data->chunkSize);
	}
};

struct MidiFile {
	uint8_t* data;
	MidiHeaderChunkData* header;
	MidiFile() = default;
	MidiFile(std::istream& stream) {
		stream.seekg(0, stream.end);
		int length = stream.tellg();
		stream.seekg(0, stream.beg);

		data = new uint8_t[length];
		stream.read((char*)data, length);
		header = getMidiChunk(0).GetChunk< MidiHeaderChunkData>();
	}

	MidiChunk getMidiChunk(int index = 0) {
		MidiChunk c = MidiChunk{ data };
		for (int i = 0; i < index; i++) c = c.MidiNextChunk();
		return c;
	}
};

struct Tempo {
	uint32_t micPerQuarternote;
};
struct TimeSignature {
	uint8_t numerator;
	uint8_t denomenator;
	uint8_t midiClockPerMetronomeClick;
	uint8_t _32ndnotesPerQuarternote;
};
struct KeySignature {
	uint8_t key;
	uint8_t isMinor;
};

template<typename T>
struct TimeCodedValue {
	uint32_t timecode;
	T value;
};

struct MidiMetaData {
	std::string trackName;
	std::string instrumentName;
	std::vector<TimeCodedValue<Tempo>> tempos;
	std::vector<TimeCodedValue<TimeSignature>> timeSignature;
	std::vector<TimeCodedValue<KeySignature>> keySignature;

	MidiMetaData() = default;
	MidiMetaData(MidiChunk& meta) {
		if (!meta.eoc()) {
			MidiEvent ev = meta.MidiPopEvent();
			if (ev.type != MidiEventType::META_EVENT) continue;
		}
	}
};

struct Note {
	uint8_t note;
	uint8_t volume;
	uint8_t velocity;
	double length;
};

class MidiTrack {
	MidiFile& file;
	MidiChunk meta;
	MidiChunk chunk;
	std::vector<Note> notes;
public:
	//1 - maxChunk
	MidiTrack(MidiFile& file, int chunkIndex) : file(file) {
		meta = file.getMidiChunk(1);
		chunk = file.getMidiChunk(chunkIndex);
	}
};