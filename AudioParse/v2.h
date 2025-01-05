#pragma once

#include <fstream>
#include <vector>
#include <stdio.h>
#include <math.h>
#include <iostream>
#include "midi.h"

//AUDIO PARSE V2
//https://www.desmos.com/calculator/hhbs4bdfsc
//pitch + timing + volume

float toHz(char name[3]) {
	if (name[0] == '0') return 0;

	int off = 0;
	if (name[1] == 's') off++;
	if (name[1] == 'f') off--;
	int o = name[off ? 2 : 1] - '0';

	int note = 0;
	switch (name[0]) {
	case 'A': note = 0; break;
	case 'B': note = 2; break;
	case 'C': note = 3; break;
	case 'D': note = 5; break;
	case 'E': note = 7; break;
	case 'F': note = 8; break;
	case 'G': note = 10; break;
	default: throw 3;
	}

	note += off;

	if (name[0] >= 'C') note -= 12;
	note += (o - 4) * 12;
	return 440.0f * powf(2, note / 12.0f);
}

float toHZ(unsigned char midi) {
	if (midi == 0) return 0;
	int note = midi - 69;
	return 440.0f * powf(2, note / 12.0f);
}

struct Note {
	float HZ;
	float length;
	float volume = 0.5;
	Note() {
		HZ = 0;
		length = 0;
	}
	Note(char name[3], float length) {
		this->length = length;
		HZ = toHz(name);
	}

	Note(int midiNote, float length, int volume) {
		this->length = length;
		this->volume = volume / 127.0f;
		HZ = toHZ(midiNote);
	}

	int Append(char* str, size_t s, float& time, float& hz, float& volume) {
		float t = time;
		time += length;
		float j = HZ - hz;
		hz = HZ;
		float k = this->volume;
		volume = this->volume;
		return sprintf_s(str, s, "%g,%.1f,%.3f", t, j, k);
	}
};

std::vector<Note> notes{};
char out[100000];

void fixNotes() {
	if (notes.size() == 0) return;
	for (size_t i = 0; i < notes.size() - 1; i++) {
		if (notes[i].HZ == 0 && notes[i + 1].HZ == 0) {
			notes[i].length += notes[i + 1].length;
			notes.erase(i + 1 + notes.begin());
			i--;
		}
	}
}

void printNotes() {
	int index = 0;
	out[index++] = '[';
	float hz = 0;
	float time = 0;
	float vel = 0;
	fixNotes();
	try {
		for (size_t i = 0; i < notes.size(); i++) {
			index += notes[i].Append(out + index, sizeof(out) - index, time, hz, vel);
			if (i != notes.size() - 1) out[index++] = ',';
		}
	}
	catch (int e) {}
	out[index++] = ']';
	out[index] = 0;
	printf(out);
}

void parseTxt(std::fstream& s) {
	while (s.good()) {
		char str[256];
		s.getline(str, 256);

		char name[3];
		float length;

		if (str[0] == 0) continue;
		if (sscanf_s(str, "%3c %f", name, 3, &length) != 2)
			throw 1;

		Note n{ name, length };
		notes.push_back(n);
	}

	printNotes();
}

int parseVariedLengthValue(const uint8_t* ptr, int& v) {
	v = 0;
	int a = 0;
	do {
		v <<= 7;
		v |= *ptr & 0x7F;
	} while (*(ptr + a++) & 0x80);
	return a;
}

struct Tempo {
	int time;
	float spt;//seconds per tick
	int endTime = -1;
};


uint8_t keySigneture = 0;
uint8_t keyMode = 0;
int totalTicks = 0;
int prevTotalTicks = 0;
int depth = 0;
int depthTarget = 0;
int channelTarget = 0;
int timeDivision = 0;
std::vector<Tempo> tempos{ };
int ticksPerBeat = 0;
float totalTime = 0;
float prevTotalTime = 0;
int currentVolume = 64;

int maxDepth = 0;
int maxChannel = 0;

struct {
	uint8_t note;
	uint8_t volume;
	float start;
} buffer[32] = {};

void clearBuffer() {
	memset(buffer, 0, sizeof(buffer));
}

void AddNoteBuffer(uint8_t value, uint8_t volume, float start) {
	for (int i = 0; i < sizeof(buffer) / sizeof(buffer[0]); i++) {
		if (buffer[i].note == 0) {
			if (i == depthTarget && start - buffer[i].start > 0) {
				notes.push_back(Note{ (uint8_t)0, start - buffer[i].start, buffer[i].volume });
			}

			buffer[i].note = value;
			buffer[i].volume = volume;
			buffer[i].start = start;
			if (i > maxDepth) maxDepth = i;

			return;
		}
	}
	std::cout << "BUFFER OVERFLOW" << std::endl;
}
void RemoveNoteBuffer(uint8_t value, uint8_t volume, float end) {
	for (int i = 0; i < sizeof(buffer) / sizeof(buffer[0]); i++) {
		if (buffer[i].note == value) {

			if (i == depthTarget) {
				notes.push_back(Note{ (uint8_t)value, end - buffer[i].start, buffer[i].volume });
			}
			buffer[i].note = 0;
			buffer[i].volume = volume;
			buffer[i].start = end;
			return;
		}
	}
	std::cout << "NON NOTE" << std::endl;
}

unsigned short swapByteOrderWord(unsigned short s) {
	return (((unsigned char*)&s)[0] << 8) | (((unsigned char*)&s)[1] << 0);
}

Tempo getTempo(int tick) {
	for (int i = 0; i < tempos.size(); i++) {
		if (tick < tempos[i].time) return tempos[i - 1];
	}
	return tempos[tempos.size() - 1];
}

int parseMidiMessage(uint8_t status, uint8_t* k) {
	int high = status >> 4;
	int low = status & 0xF;

	float timeGap = (totalTime - prevTotalTime);
	switch (high) {
	case 0x8:
		if (low == channelTarget) {
			RemoveNoteBuffer(k[0], currentVolume, totalTime);
			prevTotalTime = totalTime;
		}
		if (low > maxChannel) maxChannel = low;
		return 2;
	case 0x9:
		if (low == channelTarget) {
			if (k[1] == 0) { //volume is zero
				RemoveNoteBuffer(k[0], currentVolume, totalTime);
			}
			else {
				AddNoteBuffer(k[0], currentVolume, totalTime);
			}
		}
		if (low > maxChannel) maxChannel = low;
		return 2;
	case 0xA:
	case 0xB: {//control change
		int controller = k[0];
		int value = k[0];
		switch (controller) {
		case 0x07:
		case 0x27:
			currentVolume = value;
			break;
		}
	} return 2;
	case 0xE:
		return 2;
	case 0xC:
	case 0xD:
		return 1;
	case 0xF:
		switch (low) {
		case 0x0: {
			int i = 2;
			while (k[i] == k[1] && k[i + 1] == 0xF7) i++;
			return i;
		}
		case 0x2:
			return 2;
		case 0x3:
			return 1;
		default:
			return 0;
		}
	}
}

void parseMidiMetaEvent(uint8_t code, int length, uint8_t* k) {
	switch (code) {
	case 0x51://tempo
		float mspb = (k[0] << 16) | (k[1] << 8) | (k[2]);
		float spt = mspb / 1e+6 / timeDivision;
		if (tempos.size() != 0) tempos[tempos.size() - 1].endTime = totalTicks;
		tempos.push_back(Tempo{ totalTicks, spt, -1 });
		break;
	}
}
void parseMidiMetaEventTrack(uint8_t code, int length, uint8_t* k) {
	switch (code) {
	case 0x03: {//track name 
		if (length >= 127) length = 127;
		char c[128];
		memcpy(c, k, length);
		c[length] = 0;
		std::cout << c << std::endl;
	} break;
	case 0x04: {//instrument name 
		if (length >= 127) length = 127;
		char c[128];
		memcpy(c, k, length);
		c[length] = 0;
		std::cout << c << std::endl;
	} break;
	}
}

void parseMetaTrack(MidiChunk& chunk) {
	tempos.clear();
	uint8_t prevMsg = 0;
	totalTicks = 0;
	while (MidiIsReadable(chunk)) {
		totalTicks += MidiPopVL(chunk);
		uint8_t ev = MidiPopByte(chunk);
		if (ev == 0xFF) {//meta event
			uint8_t code = MidiPopByte(chunk);
			int length = MidiPopVL(chunk);
			parseMidiMetaEvent(code, length, MidiPointer(chunk));
			chunk.index += length;
		}
		else if (ev & 0x80) {
			prevMsg = ev;
			chunk.index += parseMidiMessage(prevMsg, MidiPointer(chunk));
		}
		else {
			chunk.index += parseMidiMessage(prevMsg, MidiPointer(chunk) - 1) - 1;
		}
	}
	notes.clear();
	totalTicks = 0;
	prevTotalTicks = 0;
	depth = 0;
	maxDepth = 0;
	maxChannel = 0;
	totalTime = 0;
	prevTotalTime = 0;
	clearBuffer();
}

void parseTrack(MidiChunk& chunk) {
	uint8_t prevMsg = 0;
	totalTicks = 0;
	while (MidiIsReadable(chunk)) {
		totalTicks += MidiPopVL(chunk);
		while (prevTotalTicks != totalTicks) {
			Tempo t = getTempo(prevTotalTicks);
			int maxTicks = 0;
			if (totalTicks < t.endTime || t.endTime == -1) {
				maxTicks = totalTicks - prevTotalTicks;
			}
			else {
				maxTicks = t.endTime - prevTotalTicks;
			}
			totalTime += maxTicks * t.spt;
			prevTotalTicks += maxTicks;
		}
		uint8_t ev = MidiPopByte(chunk);
		if (ev == 0xFF) {//meta event
			uint8_t code = MidiPopByte(chunk);
			int length = MidiPopVL(chunk);
			parseMidiMetaEventTrack(code, length, MidiPointer(chunk));
			chunk.index += length;
		}
		else if (ev & 0x80) {
			prevMsg = ev;
			chunk.index += parseMidiMessage(prevMsg, MidiPointer(chunk));
		}
		else {
			chunk.index += parseMidiMessage(prevMsg, MidiPointer(chunk) - 1) - 1;
		}
	}

	RemoveNoteBuffer((uint8_t)buffer[depthTarget].note, 0, totalTime - buffer[depthTarget].start);
}

void parseMidi(std::fstream& s) {
	s.seekg(0, s.end);
	int length = s.tellg();
	s.seekg(0, s.beg);

	uint8_t* a = new uint8_t[length];
	s.read((char*)a, length);

	MidiChunk chunk = { (MidiChunkData*)a };
	int formatType = MidiPopWord(chunk);
	int tracks = MidiPopWord(chunk);
	std::cout << "track (" << tracks << "): " << std::endl;
	int trackTarget;
	std::cin >> trackTarget;
	if (trackTarget > tracks) trackTarget = 1;
	timeDivision = MidiPopWord(chunk);
	std::cout << "(drums only on channel 10) " << std::endl;
	std::cout << "channel Target (>=1): " << std::endl;
	std::cin >> channelTarget;
	channelTarget--;
	std::cout << "parallel depth Target (>=1): " << std::endl;
	std::cin >> depthTarget;
	depthTarget--;

	MidiChunk c = MidiNextChunk(chunk);
	parseMetaTrack(c);
	for (int i = 0; i < trackTarget; i++) {
		chunk = MidiNextChunk(chunk);
	}

	std::cout << std::endl;
	parseTrack(chunk);
	std::cout << std::endl << "TRACK " << (trackTarget) << std::endl;
	std::cout << "Max Channel: " << (maxChannel + 1) << std::endl;
	maxChannel = 0;
	std::cout << "Max Depth: " << (maxDepth + 1) << std::endl;
	maxDepth = 0;
	printNotes();
	notes.clear();
}

int main(int argc, const char* argv[]) {
	char file[256];
	if (argc >= 2) {
		strcpy_s(file, argv[1]);
	}
	else {
		std::cout << "enter file path:" << std::endl;
		std::cin.getline(file, 256);
	}
	std::fstream stream{ file,std::ios::in | std::ios::binary };
	if (!stream)
		throw 2;

	int i = 0;
	for (; file[i]; i++);
	for (; i > 0 && file[i] != '.'; i--);
	if (i == 0)
		throw 3;
	i++;
	while (true) {
		system("cls");
		stream.seekg(0, std::ios::beg);
		notes = {};
		if (!strcmp(file + i, "txt")) parseTxt(stream);
		if (!strcmp(file + i, "mid")) parseMidi(stream);
		std::cout << std::endl;
		std::cin.get();
		std::cin.ignore();

	}
}